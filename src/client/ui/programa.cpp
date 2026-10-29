#include "client/ui/programa.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <windows.h>

#include <commdlg.h>

#include <cwchar>

#include "client/audio.h"
#include "client/cuando_suena.h"
#include "client/paquete_de_sonidos.h"
#include "client/sonidos.h"
#include "client/avisos.h"
#include "client/diagnostico.h"
#include "client/guardados.h"
#include "client/horas.h"
#include "client/mi_identidad.h"
#include "client/motor_voz.h"
#include "client/arranque.h"
#include "client/prueba_de_audio.h"
#include "client/sala.h"
#include "client/salida_de_windows.h"
#include "client/sesion.h"
#include "client/vigia.h"
#include "client/ui/ajustes.h"
#include "client/ui/aviso_ventana.h"
#include "client/ui/hostear.h"
#include "client/ui/inicio.h"
#include "client/ui/servidor.h"
#include "client/ui/ventana.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/secreto.h"
#include "common/cifrado/tls.h"
#include "common/archivos.h"
#include "common/proceso.h"
#include "common/texto.h"
#include "common/utf.h"

namespace ventrik::ui {
namespace {
using namespace std::chrono_literals;
using Reloj = std::chrono::steady_clock;
constexpr auto kEntreAperturasDeVoz = 1s;
constexpr auto kSilencioParaDevolver = 1s;
constexpr UINT_PTR kTicDePrueba = 3;
constexpr UINT kCadaNivelDePrueba = 250;
constexpr auto kDuracionDelTono = 1200ms;
constexpr std::uint32_t kSinSaber = UINT32_MAX;
constexpr int kSegmentosDePrueba = 20;

bool desde_windows_7() {
  OSVERSIONINFOEXW pedido = {};
  pedido.dwOSVersionInfoSize = sizeof(pedido);
  pedido.dwMajorVersion = 6;
  pedido.dwMinorVersion = 1;
  return VerifyVersionInfoW(&pedido, VER_MAJORVERSION | VER_MINORVERSION,
                            VerSetConditionMask(VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL),
                                                VER_MINORVERSION, VER_GREATER_EQUAL)) != FALSE;
}

enum class Tarea : std::uint8_t { exportar, importar, desbloquear, activar, cambiar, quitar };

struct TrabajoDeCopia {
  Tarea tarea = Tarea::exportar;
  std::wstring ruta;
  std::optional<cifrado::Huella> huella;
  cifrado::Secreto contrasena;
  cifrado::Secreto nueva;
  cifrado::Secreto entrada;
  std::optional<std::vector<std::uint8_t>> armada;
  cliente::CopiaAbierta abierta;
  std::optional<cifrado::LlaveDeSobre> llave;
  std::atomic<bool> listo{false};
};

using cifrado::copiar;
using cifrado::secreto_de;
using cifrado::texto_de;

template <typename... Valores>
void anotar(std::ostream& salida, const Valores&... valores) {
  const char* separador = "";
  ((salida << separador << valores, separador = ","), ...);
  salida << std::endl;
}

class Programa {
 public:
  Programa(Idioma idioma, Ventana& ventana, std::wstring carpeta, const std::vector<std::string>& entrada)
      : ventana_(ventana),
        carpeta_(std::move(carpeta)),
        integrar_(carpeta_ == carpeta_de_datos(L"Programa")),
        avisos_(idioma) {
    pantalla_.idioma = idioma;
    if (integrar_) {
      escuchar_clics_de_windows(ventana.hwnd());
    }
    const cliente::LecturaGuardados lectura = cliente::cargar_guardados(guardados_, carpeta_);
    se_puede_guardar_ = lectura == cliente::LecturaGuardados::leidos || lectura == cliente::LecturaGuardados::danados;
    const Clave aviso = lectura == cliente::LecturaGuardados::version_nueva ? Clave::inicio_lista_nueva
                        : lectura == cliente::LecturaGuardados::error   ? Clave::inicio_lista_ilegible
                        : lectura == cliente::LecturaGuardados::danados ? Clave::inicio_lista_danada
                                                                        : Clave::titulo_ventana;
    if (aviso != Clave::titulo_ventana) {
      pantalla_.aviso = texto(idioma, aviso);
    }
    poner(pantalla_.nombre, guardados_.nombre);
    bloqueado_ = cliente::ajuste_o(guardados_, cliente::Ajuste::maestra, 0) == 1;
    aplicar_idioma();
    aplicar_avisos();
    if (entrada.size() == 3 || entrada.size() == 4) {
      poner(pantalla_.direccion, entrada[0]);
      poner(pantalla_.huella, entrada[1]);
      poner(pantalla_.nombre, entrada[2]);
      poner(pantalla_.codigo, entrada.size() == 4 ? entrada[3] : std::string());
      entrar_con_formulario();
    } else if (cliente::ajuste_o(guardados_, cliente::Ajuste::entrar_al_ultimo, 1) == 1 &&
               !guardados_.servidores.empty()) {
      entrar_a(guardados_.servidores[0]);
    }
  }

  ~Programa() {
    mbedtls_platform_zeroize(pantalla_.contrasena.data(), pantalla_.contrasena.size());
    borrar_contrasenas();
    cerrar();
    soltar_clics_de_windows();
    if (copia_ && !copia_->listo) {
      TerminateProcess(GetCurrentProcess(), 0);
    }
    if (hilo_de_copia_.joinable()) {
      hilo_de_copia_.join();
    }
  }

  void registrar(const std::wstring& ruta) {
    registro_.open(ruta.c_str(), std::ios::trunc);
    if (!registro_) {
      MessageBoxW(ventana_.hwnd(),
                  a_utf16(con_datos(texto(pantalla_.idioma, Clave::registro_sin_archivo), {a_utf8(ruta)})).c_str(),
                  a_utf16(texto(pantalla_.idioma, Clave::titulo_ventana)).c_str(), MB_OK | MB_ICONWARNING);
      return;
    }
    registro_ << std::fixed << std::setprecision(1) << "# placa=" << ventana_.placa() << ";grafica="
              << ventana_.grafica() << ";vertices_por_procesador=" << ventana_.por_procesador()
              << ";frecuencia=" << calidad_.frecuencia
              << ";complejidad=" << calidad_.complejidad << '\n'
              << "ms,ping_ms,ping_voz_ms,cpu_total,cpu_ventana,cpu_sistema,ocupado_ms,pasada_max_ms,imgui_ms,"
                 "direct3d_ms,presentados,vertices_max,cuadros,colchon_ms,recortes_total,perdidos,recibidos,salida_ms,"
                 "salida_max_ms,descartes_salida_total,descartes_con_voz_total,vacios_salida_total,"
                 "vaciados_colchon_total,faltas_colchon_total,transmitiendo,voces,voz_lista,avisos,mensajes,"
                 "mensaje_comun,ram_mb,despertares_ventana,despertares_audio,despertares_red,tcp_sube,tcp_baja,"
                 "udp_sube,udp_baja,memoria_propia_kb,fallas_por_segundo,arranque_ms,pixeles,foco,juego,"
                 "bloqueada,ahorro,atlas_kb,silencio,notificaciones,objetivo_ms,acelerado_ms,estirado_ms,tardes,limpieza_us,"
                 "reinicios_fallidos,error_direct3d,aviso_de_sesion"
              << std::endl;
  }

  void cerrar() {
    if (auto* guardado = sala_ ? cliente::buscar_guardado(guardados_, pendiente_.direccion) : nullptr) {
      guardado->leidos = sala_->leidos();
      guardado->texto = sala_->ultimo_texto();
      if (const auto* canal = sala_->canal(servidor_.ultima_voz)) {
        guardado->nombre_canal = canal->nombre;
      }
      guardar();
    }
    if (sin_guardar_) {
      guardar();
    }
    sesion_.reset();
    sala_.reset();
    listas_de_susurro();
    canal_de_voz_ = 0;
    avisos_.desconectado();
    cerrar_voz();
  }

  void abrir_voz() {
    puente_ = std::make_shared<cliente::PuenteVoz>();
    motor_ = std::make_unique<cliente::MotorVoz>(*puente_, calidad_);
    motor_->sensibilidad(guardados_.sensibilidad);
    motor_->por_voz(guardados_.por_voz);
    aplicar_personas();
    aplicar_audio();
    aplicar_limpieza();
    prueba_.reset();
    audio_ = std::make_unique<cliente::Audio>();
    servidor_.hay_audio = motor_->ok() && audio_->abrir(motor_->frecuencia(), cliente::MotorVoz::procesar_audio,
                                                       motor_.get(), elegidos());
    vigia_ = std::make_unique<cliente::Vigia>(*motor_, *puente_, cliente::tecla_para_hablar(guardados_.tecla_hablar),
                                              ventana_.hwnd(), [this] { avisar(); });
    vigia_->por_voz(guardados_.por_voz);
    aplicar_teclas();
    despertares_audio_ = audio_->despertares();
    abrir_desde_ = Reloj::now() + kEntreAperturasDeVoz;
    sesion_->voz(puente_);
  }

  void aplicar_audio() {
    if (motor_) {
      motor_->silenciar(guardados_.silencio != 0 || bloqueado_, (guardados_.silencio & protocolo::kSilencioAudio) != 0);
      motor_->volumenes(guardados_.volumen_microfono, guardados_.volumen_salida);
    }
  }

  cliente::ElegidosDeAudio elegidos() const {
    return {cliente::ajuste_texto(guardados_, cliente::Ajuste::microfono),
            cliente::ajuste_texto(guardados_, cliente::Ajuste::salida)};
  }

  void aplicar_idioma() {
    const std::string codigo = cliente::ajuste_texto(guardados_, cliente::Ajuste::idioma);
    const Idioma idioma = codigo.empty() ? idioma_de_windows() : elegir_idioma(codigo);
    pantalla_.idioma = servidor_.idioma = ajustes_.idioma = hostear_.idioma = idioma;
    avisos_.idioma(idioma);
    ventana_.bandeja(cliente::ajuste_o(guardados_, cliente::Ajuste::al_cerrar, 0) == 0,
                     cliente::ajuste_o(guardados_, cliente::Ajuste::al_minimizar, 0) == 1,
                     a_utf16(texto(idioma, Clave::bandeja_abrir)), a_utf16(texto(idioma, Clave::bandeja_salir)));
  }

  void aplicar_avisos() {
    const auto prendido = [this](cliente::Ajuste ajuste, std::uint8_t de_fabrica) {
      return cliente::ajuste_o(guardados_, ajuste, de_fabrica) == 1;
    };
    avisos_.opciones({true, prendido(cliente::Ajuste::avisos_de_texto, 1), prendido(cliente::Ajuste::avisos_de_voz, 1),
                      prendido(cliente::Ajuste::avisos_de_conexion, 1),
                      prendido(cliente::Ajuste::avisos_privados, 0) || bloqueado_, false});
    mostrar_minimizada_ = prendido(cliente::Ajuste::avisos_minimizada, 1);
  }

  void aplicar_teclas() {
    std::array<std::uint8_t, protocolo::kLugaresDeSusurro> teclas{};
    for (std::size_t lugar = 0; lugar < teclas.size(); ++lugar) {
      const auto susurro = cliente::susurro_de(guardados_, lugar);
      teclas[lugar] = susurro ? susurro->tecla : 0;
      const Idioma idioma = servidor_.idioma;
      servidor_.susurros[lugar] =
          !susurro                    ? std::string()
          : !susurro->canal.empty()    ? con_datos(texto(idioma, Clave::servidor_susurrando_canal), {susurro->canal})
          : !susurro->personas.empty() ? con_datos(texto(idioma, Clave::servidor_susurrando_a),
                                                   {lista_de_nombres(idioma, susurro->personas)})
                                       : std::string(texto(idioma, Clave::servidor_susurrando_nadie));
    }
    if (vigia_) {
      vigia_->solo_con_foco(cliente::ajuste_o(guardados_, cliente::Ajuste::teclas_sin_foco, 1) == 0);
      vigia_->seguir_al_soltar(std::chrono::milliseconds(
          cliente::kSeguirAlSoltarMs[cliente::ajuste_o(guardados_, cliente::Ajuste::seguir_al_soltar, 1)]));
      vigia_->susurros(teclas);
    }
    aplicar_susurros();
  }

  void quitar_tecla(std::uint8_t tecla) {
    for (std::size_t lugar = 0; lugar < protocolo::kLugaresDeSusurro; ++lugar) {
      auto susurro = cliente::susurro_de(guardados_, lugar);
      if (susurro && susurro->tecla == tecla) {
        susurro->tecla = 0;
        cliente::poner_susurro(guardados_, lugar, susurro);
      }
    }
  }

  void aplicar_susurros() {
    if (!sala_) {
      return;
    }
    for (std::size_t lugar = 0; lugar < susurros_enviados_.size(); ++lugar) {
      const auto susurro = cliente::susurro_de(guardados_, lugar);
      protocolo::DestinosDeSusurro destinos =
          susurro ? cliente::destinos_de(*susurro, lugar, sala_->tu_id(), sala_->canales(), sala_->usuarios())
                  : protocolo::DestinosDeSusurro{static_cast<std::uint8_t>(lugar + 1), 0, {}};
      protocolo::DestinosDeSusurro& enviados = susurros_enviados_[lugar];
      if (std::tie(destinos.canal, destinos.personas) != std::tie(enviados.canal, enviados.personas)) {
        enviados = destinos;
        ordenar({std::move(destinos)});
      }
    }
  }

  void listas_de_susurro() {
    ajustes_.personas.clear();
    ajustes_.canales.clear();
    if (!sala_) {
      return;
    }
    for (const auto& [id, usuario] : sala_->usuarios()) {
      if (id != sala_->tu_id()) {
        ajustes_.personas.push_back(usuario.nombre);
      }
    }
    for (const auto& canal : sala_->canales()) {
      if (canal.tipo == protocolo::TipoCanal::voz) {
        ajustes_.canales.push_back(canal.nombre);
      }
    }
  }

  std::string te_susurra(const cliente::Hablando& ids) const {
    std::vector<std::string> nombres;
    for (const std::uint32_t id : ids) {
      const auto usuario = sala_->usuarios().find(id);
      if (id != 0 && usuario != sala_->usuarios().end() &&
          std::find(nombres.begin(), nombres.end(), usuario->second.nombre) == nombres.end()) {
        nombres.push_back(usuario->second.nombre);
      }
    }
    return nombres.empty() ? std::string()
                           : con_datos(texto(servidor_.idioma, Clave::servidor_te_susurra),
                                       {lista_de_nombres(servidor_.idioma, nombres)});
  }

  void abrir_ajustes() {
    en_ajustes_ = true;
    if (vigia_ && servidor_.eligiendo_tecla) {
      vigia_->no_elegir();
      servidor_.eligiendo_tecla = false;
    }
    ajustes_.dispositivos = cliente::dispositivos_de_audio();
    ajustes_.arranca_con_windows = integrar_ && cliente::arranca_con_windows();
    ajustes_.hay_bajar_otros = desde_windows_7();
    ajustes_.un_nucleo = calidad_.frecuencia != cliente::Calidad{}.frecuencia;
    listas_de_susurro();
  }

  void cerrar_ajustes() {
    en_ajustes_ = false;
    probar(false, false);
    eligiendo_desde_.reset();
    ajustes_.eligiendo_tecla = false;
  }

  void probar(bool microfono, bool tono) {
    KillTimer(ventana_.hwnd(), kTicDePrueba);
    prueba_.reset();
    ajustes_.probando_microfono = microfono;
    ajustes_.nivel = 0;
    if (!microfono && !tono) {
      return;
    }
    SetTimer(ventana_.hwnd(), kTicDePrueba, kCadaNivelDePrueba, nullptr);
    fin_de_prueba_ = Reloj::now() + kDuracionDelTono;
    if (!motor_ || tono) {
      prueba_ = std::make_unique<cliente::PruebaDeAudio>();
      prueba_->escucharme(ajustes_.escuchandome);
      if (!prueba_->abrir(elegidos(), tono)) {
        prueba_.reset();
        if (!microfono) {
          KillTimer(ventana_.hwnd(), kTicDePrueba);
        }
      }
    }
  }

  bool atender_ajustes() {
    bool cambio = false;
    if (eligiendo_desde_ && GetForegroundWindow() == ventana_.hwnd()) {
      const cliente::Teclas ahora = cliente::teclas_apretadas();
      const int tecla = cliente::tecla_nueva(*eligiendo_desde_, ahora);
      eligiendo_desde_ = ahora;
      if (tecla >= 0) {
        eligiendo_desde_.reset();
        ajustes_.eligiendo_tecla = false;
        cambio = true;
        auto susurro = ajustes_.tecla_de == 0 ? std::nullopt : cliente::susurro_de(guardados_, ajustes_.tecla_de - 1);
        if (tecla > 0 && susurro && tecla != cliente::tecla_para_hablar(guardados_.tecla_hablar)) {
          quitar_tecla(static_cast<std::uint8_t>(tecla));
          susurro->tecla = static_cast<std::uint8_t>(tecla);
          cliente::poner_susurro(guardados_, ajustes_.tecla_de - 1, susurro);
          aplicar_teclas();
          guardar();
        } else if (tecla > 0 && ajustes_.tecla_de == 0) {
          quitar_tecla(static_cast<std::uint8_t>(tecla));
          aplicar_teclas();
          guardados_.tecla_hablar = static_cast<std::uint8_t>(tecla);
          servidor_.tecla = guardados_.tecla_hablar;
          if (vigia_) {
            vigia_->usar_tecla(guardados_.tecla_hablar);
          }
          guardar();
        }
      }
    }
    if (ajustes_.probando_microfono || prueba_) {
      const int nivel = prueba_   ? prueba_->nivel(kSegmentosDePrueba)
                        : motor_ ? motor_->nivel(kSegmentosDePrueba)
                                 : 0;
      cambio = std::exchange(ajustes_.nivel, nivel) != nivel || cambio;
      if (!ajustes_.probando_microfono && Reloj::now() >= fin_de_prueba_) {
        probar(false, false);
      }
    }
    return cambio;
  }

  void aplicar_ajustes(cliente::Guardados nuevos) {
    const bool otra_voz = nuevos.por_voz != guardados_.por_voz;
    const bool otros_dispositivos =
        cliente::ajuste_texto(nuevos, cliente::Ajuste::microfono) !=
            cliente::ajuste_texto(guardados_, cliente::Ajuste::microfono) ||
        cliente::ajuste_texto(nuevos, cliente::Ajuste::salida) != cliente::ajuste_texto(guardados_, cliente::Ajuste::salida);
    guardados_ = std::move(nuevos);
    const auto antes = limpieza_;
    const auto ahora = cliente::limpieza_de(guardados_, calidad_.frecuencia != cliente::Calidad{}.frecuencia);
    const bool otra_limpieza = std::tie(antes.ruido, antes.eco, antes.volumen, antes.bajar) !=
                               std::tie(ahora.ruido, ahora.eco, ahora.volumen, ahora.bajar);
    aplicar_idioma();
    aplicar_avisos();
    aplicar_teclas();
    servidor_.por_voz = guardados_.por_voz;
    servidor_.sensibilidad = guardados_.sensibilidad;
    servidor_.volumen_microfono = guardados_.volumen_microfono;
    servidor_.volumen_salida = guardados_.volumen_salida;
    if (motor_) {
      motor_->sensibilidad(guardados_.sensibilidad);
      motor_->por_voz(guardados_.por_voz);
      if (otra_voz) {
        vigia_->por_voz(guardados_.por_voz);
      }
      aplicar_audio();
      if (otros_dispositivos && audio_) {
        servidor_.hay_audio = audio_->abrir(motor_->frecuencia(), cliente::MotorVoz::procesar_audio, motor_.get(),
                                            elegidos());
      }
    }
    if (otra_limpieza) {
      aplicar_limpieza();
    }
    sin_guardar_ = true;
  }

  void abrir_hostear(PantallaHostear pantalla) {
    ui::borrar_contrasenas(hostear_);
    hostear_ = std::move(pantalla);
    en_hostear_ = true;
  }

  void cuadro_de_bloqueo() {
    ajustes_.con_conexion = sesion_ != nullptr;
    const PedidoAjustes pedido = dibujar_bloqueo(ajustes_, guardados_);
    if (pedido.desbloquear && !cerrada_.empty()) {
      empezar_copia(Tarea::desbloquear, copiar(cerrada_));
    }
    if (pedido.importar_identidad && !sesion_) {
      despues_del_cuadro_ = [this] { importar_identidad(); };
    }
    if (pedido.confirmar_identidad && !sesion_) {
      confirmar_identidad();
    }
    if (pedido.cancelar_identidad) {
      olvidar_copia();
    }
  }

  void cuadro_de_ajustes() {
    PedidoAjustes pedido = dibujar_ajustes(ajustes_, guardados_);
    if (pedido.guardados) {
      aplicar_ajustes(std::move(*pedido.guardados));
    }
    if (pedido.arrancar_con_windows && integrar_) {
      cliente::arrancar_con_windows(*pedido.arrancar_con_windows);
      ajustes_.arranca_con_windows = cliente::arranca_con_windows();
    }
    if (pedido.elegir_tecla) {
      ajustes_.eligiendo_tecla = !ajustes_.eligiendo_tecla;
      ajustes_.tecla_de = pedido.tecla_de;
      eligiendo_desde_ = ajustes_.eligiendo_tecla ? std::optional<cliente::Teclas>(cliente::teclas_apretadas())
                                                  : std::nullopt;
    }
    if (pedido.probar_microfono) {
      probar(*pedido.probar_microfono, false);
    }
    if (pedido.escucharme) {
      ajustes_.escuchandome = *pedido.escucharme;
      if (prueba_) {
        prueba_->escucharme(ajustes_.escuchandome);
      }
    }
    if (pedido.probar_salida) {
      probar(false, true);
    }
    if (pedido.avisos) {
      cambiar_avisos(guardados_.servidores[pedido.avisos->first].direccion, std::move(pedido.avisos->second));
    }
    if (pedido.probar_sonido) {
      sonar_ya(static_cast<cliente::Sonido>(*pedido.probar_sonido));
    }
    if (pedido.elegir_sonido) {
      despues_del_cuadro_ = [this, sonido = static_cast<cliente::Sonido>(*pedido.elegir_sonido)] {
        elegir_sonido(sonido);
      };
    }
    if (pedido.sonido_de_fabrica) {
      volver_a_fabrica(static_cast<cliente::Sonido>(*pedido.sonido_de_fabrica));
      guardar();
    }
    if (pedido.sonidos_de_fabrica) {
      for (std::size_t cual = 0; cual < cliente::kSonidos; ++cual) {
        volver_a_fabrica(static_cast<cliente::Sonido>(cual));
      }
      guardar();
    }
    if (pedido.importar_sonidos) {
      despues_del_cuadro_ = [this] { importar_sonidos(); };
    }
    if (pedido.exportar_sonidos) {
      despues_del_cuadro_ = [this] { exportar_sonidos(); };
    }
    if (pedido.exportar_identidad) {
      despues_del_cuadro_ = [this] { exportar_identidad(); };
    }
    if (pedido.importar_identidad) {
      despues_del_cuadro_ = [this] { importar_identidad(); };
    }
    if (pedido.confirmar_identidad) {
      confirmar_identidad();
    }
    if (pedido.cancelar_identidad) {
      olvidar_copia();
    }
    if (pedido.guardar || (sin_guardar_ && !ImGui::IsAnyItemActive())) {
      guardar();
    }
    if (pedido.volver) {
      cerrar_ajustes();
    }
  }

  void sonar_ya(cliente::Sonido sonido) {
    const std::string sonar_por = cliente::ajuste_texto(guardados_, cliente::Ajuste::sonar_por);
    sonidos_.sonar(sonido, cliente::ajuste_o(guardados_, cliente::Ajuste::volumen_avisos, cliente::kVolumenPorDefecto),
                   !cliente::ajuste_texto(guardados_, cliente::ajuste_de_propio(static_cast<std::size_t>(sonido))).empty(),
                   sonar_por.empty() ? cliente::ajuste_texto(guardados_, cliente::Ajuste::salida) : sonar_por,
                   Reloj::now());
  }

  void sonar(std::optional<cliente::Sonido> sonido) {
    if (sonido && cliente::sonido_prendido(guardados_, static_cast<std::size_t>(*sonido)) &&
        cliente::suena_con(cliente::ajuste_o(guardados_, cliente::Ajuste::no_molestar, cliente::kNoMolestarAlJugar),
                           ventana_.situacion().juego) &&
        (!cliente::es_sonido_de_voz(*sonido) || servidor_.avisos.sonidos_de_voz)) {
      sonar_ya(*sonido);
    }
  }

  std::optional<std::wstring> elegir_archivo(Clave titulo, Clave filtro, std::wstring_view patron, bool guardar_como,
                                             Clave nombre_nuevo = Clave::archivo_paquete_nuevo,
                                             std::wstring_view extension = L".vksonidos") {
    const Idioma idioma = ajustes_.idioma;
    std::wstring filtros = a_utf16(texto(idioma, filtro));
    filtros.push_back(L'\0');
    filtros += patron;
    filtros.push_back(L'\0');
    const std::wstring titulo_ventana = a_utf16(texto(idioma, titulo));
    std::vector<wchar_t> ruta(32768);
    if (guardar_como) {
      const std::wstring nombre = a_utf16(texto(idioma, nombre_nuevo));
      std::copy(nombre.begin(), nombre.end(), ruta.begin());
    }
    OPENFILENAMEW pedido = {};
    pedido.lStructSize = sizeof(pedido);
    pedido.hwndOwner = ventana_.hwnd();
    pedido.lpstrFilter = filtros.c_str();
    pedido.lpstrFile = ruta.data();
    pedido.nMaxFile = static_cast<DWORD>(ruta.size());
    pedido.lpstrTitle = titulo_ventana.c_str();
    pedido.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST |
                   (guardar_como ? DWORD{OFN_OVERWRITEPROMPT} : DWORD{OFN_FILEMUSTEXIST});
    if (!(guardar_como ? GetSaveFileNameW(&pedido) : GetOpenFileNameW(&pedido))) {
      return std::nullopt;
    }
    std::wstring elegida = ruta.data();
    if (guardar_como && (elegida.size() < extension.size() ||
                         _wcsicmp(elegida.c_str() + elegida.size() - extension.size(), extension.data()) != 0)) {
      elegida += extension;
      if (GetFileAttributesW(elegida.c_str()) != INVALID_FILE_ATTRIBUTES &&
          MessageBoxW(ventana_.hwnd(),
                      a_utf16(con_datos(texto(idioma, Clave::archivo_reemplazar), {a_utf8(elegida)})).c_str(),
                      titulo_ventana.c_str(), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return std::nullopt;
      }
    }
    return elegida;
  }

  void avisar_sonidos(Clave clave, std::size_t cuantos, bool error) {
    ajustes_.aviso_de_sonidos = con_datos(texto(ajustes_.idioma, clave), {std::to_string(cuantos)});
    ajustes_.aviso_es_error = error;
  }

  bool guardar_propio(cliente::Sonido sonido, const cliente::Muestras& muestras, std::string nombre) {
    const auto vksn = cliente::codificar_vksn(muestras);
    while (!nombre.empty() && (nombre.size() > cliente::kMaximoNombreDePropio || !es_utf8_valido(nombre))) {
      nombre.pop_back();
    }
    if (vksn.empty() || nombre.empty() || !crear_carpetas(carpeta_ + L"\\sonidos") ||
        !escribir_archivo_completo(cliente::archivo_propio(carpeta_, sonido), vksn)) {
      return false;
    }
    cliente::poner_texto(guardados_, cliente::ajuste_de_propio(static_cast<std::size_t>(sonido)), nombre);
    sonidos_.olvidar(sonido);
    return true;
  }

  void volver_a_fabrica(cliente::Sonido sonido) {
    cliente::poner_texto(guardados_, cliente::ajuste_de_propio(static_cast<std::size_t>(sonido)), {});
    DeleteFileW(cliente::archivo_propio(carpeta_, sonido).c_str());
    sonidos_.olvidar(sonido);
  }

  void elegir_sonido(cliente::Sonido sonido) {
    const auto ruta = elegir_archivo(Clave::archivo_elegir_sonido, Clave::archivo_filtro_sonidos, L"*.wav;*.mp3", false);
    if (!ruta) {
      return;
    }
    cifrado::Secreto contenido;
    const auto muestras = leer_archivo(*ruta, cliente::kMaximoArchivoDeSonido, contenido) == LecturaArchivo::leido
                              ? cliente::decodificar_archivo(contenido.bytes(), cliente::maximo_de_muestras(sonido))
                              : std::nullopt;
    if (muestras && guardar_propio(sonido, *muestras, a_utf8(ruta->substr(ruta->find_last_of(L"\\/") + 1)))) {
      ajustes_.aviso_de_sonidos.clear();
      guardar();
      sonar_ya(sonido);
    } else {
      avisar_sonidos(Clave::sonidos_archivo_malo, cliente::maximo_de_muestras(sonido) / cliente::kFrecuenciaDeSonidos, true);
    }
  }

  void importar_sonidos() {
    const auto ruta =
        elegir_archivo(Clave::archivo_importar_paquete, Clave::archivo_filtro_paquetes, L"*.vksonidos", false);
    if (!ruta) {
      return;
    }
    cifrado::Secreto contenido;
    const auto paquete = leer_archivo(*ruta, cliente::kMaximoPaquete, contenido) == LecturaArchivo::leido
                             ? cliente::leer_paquete(contenido.bytes())
                             : std::nullopt;
    std::size_t importados = 0;
    for (const cliente::SonidoDePaquete& sonido : paquete ? *paquete : std::vector<cliente::SonidoDePaquete>()) {
      importados += guardar_propio(sonido.sonido, sonido.muestras, sonido.nombre) ? 1 : 0;
    }
    avisar_sonidos(importados == 0 ? Clave::sonidos_paquete_malo : Clave::sonidos_importados, importados, importados == 0);
    guardar();
  }

  void exportar_sonidos() {
    std::vector<cliente::SonidoDePaquete> sonidos;
    for (std::size_t cual = 0; cual < cliente::kSonidos; ++cual) {
      const auto sonido = static_cast<cliente::Sonido>(cual);
      const std::string nombre = cliente::ajuste_texto(guardados_, cliente::ajuste_de_propio(cual));
      cifrado::Secreto contenido;
      auto muestras = !nombre.empty() && leer_archivo(cliente::archivo_propio(carpeta_, sonido),
                                                      cliente::kMaximoArchivoDeSonido, contenido) == LecturaArchivo::leido
                          ? cliente::decodificar_vksn(contenido.bytes(), cliente::maximo_de_muestras(sonido))
                          : std::nullopt;
      if (muestras) {
        sonidos.push_back({sonido, nombre, std::move(*muestras)});
      }
    }
    const auto ruta = sonidos.empty() ? std::nullopt
                                      : elegir_archivo(Clave::archivo_exportar_paquete, Clave::archivo_filtro_paquetes,
                                                       L"*.vksonidos", true);
    if (!ruta) {
      return;
    }
    const auto paquete = cliente::armar_paquete(sonidos);
    const bool bien = !paquete.empty() && escribir_archivo_completo(*ruta, paquete);
    avisar_sonidos(bien ? Clave::sonidos_exportados : Clave::sonidos_no_se_guardo, sonidos.size(), !bien);
  }

  void cargar_identidad() {
    identidad_cargada_ = true;
    cliente::MiIdentidad cargada = cliente::cargar_o_crear_mi_identidad(carpeta_);
    estado_identidad_ = cargada.estado;
    identidad_ = cargada.clave ? std::make_shared<const cifrado::Clave>(std::move(*cargada.clave)) : nullptr;
    cerrada_ = std::move(cargada.cerrada);
    if (auto secreto = cliente::secreto_de_equipo(carpeta_)) {
      equipo_ = std::make_shared<const cifrado::Secreto>(std::move(*secreto));
    }
    cambiar_bloqueo(!cerrada_.empty());
  }

  void recordar_maestra() {
    const std::uint8_t con = bloqueado_ || maestra_ ? 1 : 0;
    if (cliente::ajuste_o(guardados_, cliente::Ajuste::maestra, 0) != con) {
      cliente::poner_ajuste(guardados_, cliente::Ajuste::maestra, con);
      guardar();
    }
  }

  void cambiar_bloqueo(bool bloquear) {
    bloqueado_ = bloquear;
    if (bloquear) {
      olvidar_copia();
      ajustes_.cerrar_dialogo_identidad = ajustes_.dialogo_identidad != DialogoIdentidad::ninguno;
      mbedtls_platform_zeroize(pantalla_.contrasena.data(), pantalla_.contrasena.size());
    }
    ajustes_.error_identidad.clear();
    ajustes_.enfocar_contrasena = bloquear;
    aplicar_audio();
    aplicar_avisos();
    recordar_maestra();
    aplicar_identidad();
  }

  void bloquear() {
    auto cerrada = cliente::cerrar_con_maestra(*identidad_, *maestra_);
    if (!cerrada) {
      return;
    }
    cerrada_ = std::move(*cerrada);
    identidad_.reset();
    maestra_.reset();
    if (vigia_ && servidor_.eligiendo_tecla) {
      vigia_->no_elegir();
      servidor_.eligiendo_tecla = false;
    }
    cerrar_ajustes();
    ui::borrar_contrasenas(hostear_);
    en_hostear_ = false;
    cambiar_bloqueo(true);
  }

  void aplicar_identidad() {
    const bool lista = identidad_ != nullptr;
    ajustes_.estado_identidad = !identidad_cargada_ ? EstadoDeIdentidad::cargando
                                : lista             ? EstadoDeIdentidad::lista
                                : estado_identidad_ == cliente::EstadoIdentidad::danada ? EstadoDeIdentidad::danada
                                                                                        : EstadoDeIdentidad::error;
    ajustes_.identidad = lista ? cliente::identidad_corta(identidad_->huella()) : std::string();
    ajustes_.copia_hecha = lista && cliente::ajuste_texto(guardados_, cliente::Ajuste::copia_de_identidad) ==
                                        identidad_->huella().a_texto();
    ajustes_.con_maestra = maestra_.has_value();
    pantalla_.copia = !identidad_cargada_    ? Copia::sin_saber
                      : !lista               ? Copia::problema
                      : ajustes_.copia_hecha ? Copia::hecha
                                             : Copia::falta;
  }

  void avisar_identidad(Clave clave, bool error, const std::string& dato = {}) {
    ajustes_.aviso_identidad = con_datos(texto(ajustes_.idioma, clave), {dato});
    ajustes_.aviso_identidad_es_error = error;
  }

  void abrir_dialogo_identidad(DialogoIdentidad cual) {
    ajustes_.dialogo_identidad = cual;
    ajustes_.abrir_dialogo_identidad = true;
    ajustes_.error_identidad.clear();
  }

  void borrar_contrasenas() {
    mbedtls_platform_zeroize(ajustes_.actual.data(), ajustes_.actual.size());
    mbedtls_platform_zeroize(ajustes_.contrasena.data(), ajustes_.contrasena.size());
    mbedtls_platform_zeroize(ajustes_.repetir.data(), ajustes_.repetir.size());
    olvidar_lo_escrito();
  }

  void olvidar_copia() {
    borrar_contrasenas();
    copia_leida_ = cifrado::Secreto();
    por_reemplazar_.reset();
  }

  void cerrar_dialogo_identidad() {
    ajustes_.cerrar_dialogo_identidad = true;
    olvidar_copia();
  }

  void exportar_identidad() {
    const auto ruta = identidad_ ? elegir_archivo(Clave::archivo_exportar_identidad, Clave::archivo_filtro_identidad,
                                                  L"*.vkid", true, Clave::archivo_identidad_nueva, L".vkid")
                                 : std::nullopt;
    if (ruta) {
      ruta_de_copia_ = *ruta;
      ajustes_.aviso_identidad.clear();
      abrir_dialogo_identidad(DialogoIdentidad::exportar);
    }
  }

  void importar_identidad() {
    const auto ruta = elegir_archivo(Clave::archivo_importar_identidad, Clave::archivo_filtro_identidad, L"*.vkid",
                                     false, Clave::archivo_identidad_nueva, L".vkid");
    if (!ruta) {
      return;
    }
    cifrado::Secreto leida;
    if (leer_archivo(*ruta, cliente::kMaximoArchivoIdentidad, leida) != LecturaArchivo::leido) {
      avisar_identidad(Clave::seguridad_archivo_malo, true);
      return;
    }
    copia_leida_ = std::move(leida);
    ajustes_.aviso_identidad.clear();
    abrir_dialogo_identidad(DialogoIdentidad::importar);
  }

  void confirmar_identidad() {
    switch (ajustes_.dialogo_identidad) {
      case DialogoIdentidad::exportar:
        if (auto der = identidad_ ? identidad_->der() : std::nullopt) {
          empezar_copia(Tarea::exportar, std::move(*der));
        }
        break;
      case DialogoIdentidad::importar:
        empezar_copia(Tarea::importar, copiar(copia_leida_.bytes()));
        break;
      case DialogoIdentidad::activar:
        empezar_copia(Tarea::activar, {});
        break;
      case DialogoIdentidad::cambiar:
      case DialogoIdentidad::quitar:
        if (const auto cerrada = identidad_ && maestra_ ? cliente::cerrar_con_maestra(*identidad_, *maestra_)
                                                        : std::nullopt) {
          empezar_copia(ajustes_.dialogo_identidad == DialogoIdentidad::cambiar ? Tarea::cambiar : Tarea::quitar,
                        copiar(*cerrada));
        }
        break;
      case DialogoIdentidad::reemplazar:
        if (por_reemplazar_) {
          usar_identidad(std::move(*por_reemplazar_), Clave::seguridad_importada, true);
        }
        cerrar_dialogo_identidad();
        break;
      case DialogoIdentidad::crear:
        if (auto nueva = cifrado::Clave::crear_nueva()) {
          usar_identidad(std::move(*nueva), Clave::seguridad_creada, false);
        } else {
          avisar_identidad(Clave::seguridad_no_se_guardo, true);
        }
        cerrar_dialogo_identidad();
        break;
      case DialogoIdentidad::ninguno:
        break;
    }
  }

  void empezar_copia(Tarea tarea, cifrado::Secreto entrada) {
    auto trabajo = std::make_shared<TrabajoDeCopia>();
    const bool con_actual = tarea == Tarea::cambiar || tarea == Tarea::quitar;
    trabajo->tarea = tarea;
    trabajo->ruta = ruta_de_copia_;
    trabajo->huella = identidad_ ? std::optional<cifrado::Huella>(identidad_->huella()) : std::nullopt;
    trabajo->contrasena = secreto_de(con_actual ? ajustes_.actual.data() : ajustes_.contrasena.data());
    trabajo->nueva = secreto_de(tarea == Tarea::cambiar ? ajustes_.contrasena.data() : "");
    trabajo->entrada = std::move(entrada);
    borrar_contrasenas();
    ajustes_.error_identidad.clear();
    try {
      hilo_de_copia_ = std::thread([trabajo, ventana = ventana_.hwnd()] {
        const std::string_view contrasena = texto_de(trabajo->contrasena);
        switch (trabajo->tarea) {
          case Tarea::exportar:
            trabajo->armada = cliente::armar_copia(trabajo->entrada.bytes(), contrasena);
            break;
          case Tarea::importar:
            trabajo->abierta = cliente::abrir_copia(trabajo->entrada.bytes(), contrasena);
            break;
          case Tarea::activar:
            trabajo->llave = cliente::nueva_maestra(contrasena);
            break;
          case Tarea::desbloquear:
          case Tarea::cambiar:
          case Tarea::quitar:
            trabajo->abierta = cliente::abrir_con_maestra(trabajo->entrada.bytes(), contrasena);
            if (trabajo->tarea == Tarea::cambiar && trabajo->abierta.clave) {
              trabajo->llave = cliente::nueva_maestra(texto_de(trabajo->nueva));
            }
            break;
        }
        trabajo->contrasena = cifrado::Secreto();
        trabajo->nueva = cifrado::Secreto();
        trabajo->entrada = cifrado::Secreto();
        trabajo->listo = true;
        PostMessageW(ventana, kMensajeNovedades, 0, 0);
      });
    } catch (const std::system_error&) {
      ajustes_.error_identidad = texto(ajustes_.idioma, Clave::seguridad_no_se_guardo);
      return;
    }
    SetThreadPriority(hilo_de_copia_.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
    copia_ = trabajo;
    ajustes_.calculando = true;
  }

  void terminar_copia(TrabajoDeCopia& hecho) {
    ajustes_.calculando = false;
    if (hecho.tarea != Tarea::exportar && hecho.tarea != Tarea::importar) {
      terminar_maestra(hecho);
      return;
    }
    if (hecho.tarea == Tarea::exportar) {
      const bool bien = hecho.huella && hecho.armada && escribir_archivo_completo(hecho.ruta, *hecho.armada);
      if (bien) {
        cliente::poner_texto(guardados_, cliente::Ajuste::copia_de_identidad, hecho.huella->a_texto());
        guardar();
      }
      avisar_identidad(bien ? Clave::seguridad_exportada : Clave::seguridad_no_se_guardo, !bien);
      cerrar_dialogo_identidad();
      aplicar_identidad();
      return;
    }
    const cifrado::AperturaSobre resultado = hecho.abierta.resultado;
    if (resultado != cifrado::AperturaSobre::abierto || !hecho.abierta.clave) {
      ajustes_.error_identidad = texto(ajustes_.idioma, resultado == cifrado::AperturaSobre::contrasena
                                                            ? Clave::seguridad_contrasena_mal
                                                        : resultado == cifrado::AperturaSobre::sin_memoria
                                                            ? Clave::seguridad_sin_memoria
                                                            : Clave::seguridad_archivo_malo);
      ajustes_.enfocar_contrasena = true;
      return;
    }
    if (identidad_ && hecho.abierta.clave->huella() != identidad_->huella()) {
      ajustes_.identidad_nueva = cliente::identidad_corta(hecho.abierta.clave->huella());
      por_reemplazar_ = std::move(hecho.abierta.clave);
      abrir_dialogo_identidad(DialogoIdentidad::reemplazar);
      return;
    }
    usar_identidad(std::move(*hecho.abierta.clave), Clave::seguridad_importada, true);
    cerrar_dialogo_identidad();
  }

  void terminar_maestra(TrabajoDeCopia& hecho) {
    if (hecho.tarea != Tarea::activar && !hecho.abierta.clave) {
      ajustes_.error_identidad =
          texto(ajustes_.idioma, hecho.abierta.resultado == cifrado::AperturaSobre::sin_memoria
                                     ? Clave::seguridad_sin_memoria
                                     : Clave::maestra_mal);
      ajustes_.enfocar_contrasena = true;
      return;
    }
    if (hecho.tarea == Tarea::desbloquear) {
      identidad_ = std::make_shared<const cifrado::Clave>(std::move(*hecho.abierta.clave));
      maestra_ = std::move(hecho.abierta.llave);
      estado_identidad_ = cliente::EstadoIdentidad::lista;
      cerrada_.clear();
      cambiar_bloqueo(false);
      if (auto entrar_luego = std::exchange(al_desbloquear_, nullptr)) {
        entrar_luego();
      }
      return;
    }
    std::optional<cifrado::LlaveDeSobre> llave =
        hecho.tarea == Tarea::quitar ? std::nullopt : std::move(hecho.llave);
    if ((hecho.tarea != Tarea::quitar && !llave) || !identidad_ ||
        !cliente::guardar_mi_identidad(carpeta_, *identidad_, llave ? &*llave : nullptr)) {
      ajustes_.error_identidad = texto(ajustes_.idioma, Clave::seguridad_no_se_guardo);
      return;
    }
    maestra_ = std::move(llave);
    recordar_maestra();
    avisar_identidad(hecho.tarea == Tarea::activar   ? Clave::maestra_activada
                     : hecho.tarea == Tarea::cambiar ? Clave::maestra_cambiada
                                                     : Clave::maestra_quitada,
                     false);
    cerrar_dialogo_identidad();
    aplicar_identidad();
  }

  void usar_identidad(cifrado::Clave nueva, Clave aviso, bool desde_copia) {
    if (bloqueado_ && sesion_) {
      return;
    }
    if (!(identidad_ ? cliente::guardar_mi_identidad(carpeta_, nueva, maestra_ ? &*maestra_ : nullptr)
                     : cliente::guardar_apartando_la_vieja(carpeta_, nueva))) {
      avisar_identidad(Clave::seguridad_no_se_guardo, true);
      return;
    }
    identidad_ = std::make_shared<const cifrado::Clave>(std::move(nueva));
    estado_identidad_ = cliente::EstadoIdentidad::lista;
    if (bloqueado_) {
      cerrada_.clear();
      al_desbloquear_ = nullptr;
      cambiar_bloqueo(false);
    }
    cliente::poner_texto(guardados_, cliente::Ajuste::copia_de_identidad,
                         desde_copia ? identidad_->huella().a_texto() : std::string());
    guardar();
    avisar_identidad(aviso, false, cliente::identidad_corta(identidad_->huella()));
    aplicar_identidad();
  }

  void aplicar_limpieza() {
    limpieza_ = cliente::limpieza_de(guardados_, calidad_.frecuencia != cliente::Calidad{}.frecuencia);
    if (motor_) {
      motor_->limpiar({limpieza_.ruido,
                       limpieza_.eco == cliente::Eco::prendido ||
                           (limpieza_.eco == cliente::Eco::automatico && !cliente::con_auriculares()),
                       limpieza_.volumen});
    }
  }

  void cerrar_voz() {
    otros_.devolver();
    bajando_ = false;
    if (sesion_) {
      sesion_->voz(nullptr);
    }
    vigia_.reset();
    audio_.reset();
    motor_.reset();
    puente_.reset();
    midiendo_ = false;
    perdidos_ = 0;
    recibidos_ = 0;
    servidor_.eligiendo_tecla = false;
    servidor_.ajustando = false;
  }

  bool atender() {
    avisado_ = false;
    bool cambio = false;
    sonidos_.atender(Reloj::now());
    if (despues_del_cuadro_) {
      std::exchange(despues_del_cuadro_, nullptr)();
      cambio = true;
    }
    if (!identidad_cargada_ && cuadros_ > 0) {
      cargar_identidad();
      cambio = true;
    }
    if (copia_ && copia_->listo) {
      hilo_de_copia_.join();
      const auto hecho = std::exchange(copia_, nullptr);
      terminar_copia(*hecho);
      cambio = true;
    }
    const Situacion& situacion = ventana_.situacion();
    const std::uint32_t sin_usar = ventana_.tomar_sin_usar_ms();
    const bool windows_bloqueado = situacion.bloqueada || situacion.bloqueos != bloqueos_vistos_;
    if (!copia_) {
      bloqueos_vistos_ = situacion.bloqueos;
    }
    if (maestra_ && identidad_ && !bloqueado_ && !copia_ &&
        cliente::hay_que_bloquear(cliente::ajuste_o(guardados_, cliente::Ajuste::bloqueo, 0), windows_bloqueado,
                                  sin_usar)) {
      bloquear();
      cambio = true;
    }
    const bool minimizada = situacion.minimizada || situacion.oculta;
    avisos_.fuera(minimizada ? mostrar_minimizada_ : !situacion.con_foco);
    if (en_ajustes_) {
      cambio = atender_ajustes() || cambio;
    }
    avisos_.reloj(cliente::ahora_ms());
    auto tocado = notificador_ ? notificador_->tocado() : std::nullopt;
    if (!tocado) {
      tocado = tomar_clic_de_windows();
    }
    if (tocado) {
      abrir_aviso(*tocado);
      cambio = true;
    }
    bool entraron_o_salieron = false;
    if (sesion_) {
      for (const cliente::Novedad& novedad : sesion_->esperar_novedades(0ms)) {
        cambio = true;
        if (const auto* intento = std::get_if<cliente::IntentoDeConexion>(&novedad)) {
          conectado(*intento);
        } else if (const auto* fin = std::get_if<cliente::Fin>(&novedad)) {
          const std::string explicacion = cliente::explicar(pantalla_.idioma, *fin);
          if (sala_) {
            avisos_.corte(explicacion);
            sonar(cliente::Sonido::conexion_perdida);
          }
          terminar(explicacion);
        } else if (const auto* historial = std::get_if<protocolo::Historial>(&novedad);
                   sala_ && historial != nullptr && historial->desde == cliente::kDesdeAviso &&
                   avisos_.espera(historial->canal)) {
          avisos_.historial(*historial, sala_->leido(historial->canal));
        } else if (const auto* negado = std::get_if<protocolo::Negado>(&novedad);
                   sala_ && negado != nullptr && negado->pedido == protocolo::TipoMensaje::pedir_historial &&
                   avisos_.pidiendo() && !sala_->esperando()) {
          avisos_.negado();
        } else if (sala_) {
          sonar(cliente::sonido_de_gente(*sala_, novedad));
          if (const auto* nuevos = std::get_if<protocolo::MensajesNuevos>(&novedad)) {
            sonar(cliente::sonido_de_nuevos(*sala_, *nuevos, servidor_.avisos, cliente::ahora_ms()));
          }
          ordenar(sala_->aplicar(novedad));
          avisar_de(novedad);
          if (const auto* denegado = std::get_if<protocolo::Negado>(&novedad)) {
            sonar(cliente::sonido_de_negado(*denegado));
          }
          entraron_o_salieron = entraron_o_salieron || std::holds_alternative<protocolo::UsuarioEntro>(novedad) ||
                                std::holds_alternative<protocolo::UsuarioSalio>(novedad) ||
                                std::holds_alternative<protocolo::CanalCreado>(novedad) ||
                                std::holds_alternative<protocolo::CanalCambiado>(novedad) ||
                                std::holds_alternative<protocolo::CanalBorrado>(novedad);
          if (const auto* rechazo = std::get_if<protocolo::Negado>(&novedad);
              rechazo != nullptr && rechazo->pedido == protocolo::TipoMensaje::silenciar_personas) {
            silenciados_enviados_ = {0};
          } else if (rechazo != nullptr && rechazo->pedido == protocolo::TipoMensaje::destinos_de_susurro) {
            susurros_enviados_.fill(protocolo::DestinosDeSusurro{1, kSinSaber, {}});
          }
        }
      }
    }
    if (sala_ && entraron_o_salieron) {
      aplicar_personas();
    }
    if (std::exchange(alerta_, false)) {
      sonar(cliente::Sonido::alerta_seguridad);
    }
    if (sala_) {
      ordenar_si(avisos_.pedir(Reloj::now(), sala_->esperando(), situacion.juego));
    }
    mostrar_avisos();
    if (sala_) {
      std::vector<cliente::Orden> ordenes = sala_->revisar();
      cambio = cambio || !ordenes.empty();
      ordenar(std::move(ordenes));
    }
    const bool ahorrar = minimizada || situacion.juego || situacion.bloqueada;
    if (sesion_ && ahorrar != ahorrando_) {
      ahorrando_ = ahorrar;
      sesion_->ahorrar(ahorrar);
    }
    return (sala_ && mirar_voz()) || cambio;
  }

  void cuadro() {
    ++cuadros_;
    pantalla_.logo = servidor_.logo = ventana_.logo();
    if (bloqueado_) {
      cuadro_de_bloqueo();
      return;
    }
    if (en_ajustes_) {
      cuadro_de_ajustes();
      return;
    }
    if (en_hostear_) {
      en_hostear_ = !dibujar_hostear(hostear_).cerrar;
      return;
    }
    if (sala_) {
      PedidoServidor pedido = dibujar_servidor(servidor_, *sala_);
      ordenar(std::move(pedido.ordenes));
      if (pedido.elegir_tecla && vigia_) {
        vigia_->elegir_tecla();
        servidor_.eligiendo_tecla = true;
      }
      if (pedido.sensibilidad) {
        servidor_.sensibilidad = *pedido.sensibilidad;
        guardados_.sensibilidad = static_cast<std::uint8_t>(*pedido.sensibilidad);
        if (motor_) {
          motor_->sensibilidad(*pedido.sensibilidad);
        }
      }
      if (pedido.por_voz) {
        servidor_.por_voz = guardados_.por_voz = *pedido.por_voz;
        if (motor_) {
          motor_->por_voz(*pedido.por_voz);
          vigia_->por_voz(*pedido.por_voz);
        }
      }
      if (pedido.silencio) {
        sonar(cliente::sonido_de_silencio(guardados_.silencio, *pedido.silencio));
        servidor_.silencio = guardados_.silencio = *pedido.silencio;
        ordenar({protocolo::Silenciar{*pedido.silencio}});
      }
      if (pedido.volumen_microfono) {
        servidor_.volumen_microfono = *pedido.volumen_microfono;
        guardados_.volumen_microfono = static_cast<std::uint8_t>(*pedido.volumen_microfono);
      }
      if (pedido.volumen_salida) {
        servidor_.volumen_salida = *pedido.volumen_salida;
        guardados_.volumen_salida = static_cast<std::uint8_t>(*pedido.volumen_salida);
      }
      aplicar_audio();
      if (pedido.persona) {
        if (auto* guardado = cliente::buscar_guardado(guardados_, pendiente_.direccion)) {
          cliente::ajustar_persona(guardado->personas, *pedido.persona);
        }
        aplicar_personas();
      }
      for (const std::uint32_t canal : pedido.leidos) {
        sala_->marcar_leido(canal);
        avisos_.visto(canal);
      }
      if (pedido.avisos) {
        cambiar_avisos(pendiente_.direccion, std::move(*pedido.avisos));
      }
      if (pedido.por_voz || pedido.silencio || pedido.guardar) {
        guardar();
      }
      if (pedido.desconectar) {
        desconectar();
      }
      if (pedido.ajustes) {
        abrir_ajustes();
      }
      if (pedido.configurar) {
        abrir_hostear(configuracion(servidor_.idioma, servidor_.nombre_servidor, false));
      }
      return;
    }
    Pedido pedido = dibujar_inicio(pantalla_, guardados_);
    if (pedido.avisos) {
      cambiar_avisos(guardados_.servidores[pedido.cual].direccion, std::move(*pedido.avisos));
    }
    if (pedido.guardar) {
      guardar();
    }
    if (pedido.ajustes || pedido.copia) {
      abrir_ajustes();
    }
    if (pedido.copia) {
      ajustes_.seccion = Seccion::seguridad;
      if (identidad_) {
        despues_del_cuadro_ = [this] { exportar_identidad(); };
      }
    }
    switch (pedido.accion) {
      case Accion::entrar_con_formulario:
        entrar_con_formulario();
        break;
      case Accion::entrar_a_guardado:
        entrar_a(guardados_.servidores[pedido.cual]);
        break;
      case Accion::quitar:
        if (pantalla_.intento.direccion == guardados_.servidores[pedido.cual].direccion) {
          desconectar();
        }
        guardados_.servidores.erase(guardados_.servidores.begin() + static_cast<std::ptrdiff_t>(pedido.cual));
        guardar();
        break;
      case Accion::desconectar:
        desconectar();
        break;
      case Accion::hostear:
        abrir_hostear(asistente(pantalla_.idioma));
        break;
      case Accion::configurar_mio:
        abrir_hostear(configuracion(pantalla_.idioma, pantalla_.mis_servidores[pedido.cual].nombre, true));
        break;
      case Accion::iniciar_mio:
      case Accion::detener_mio:
      case Accion::ninguna:
        break;
    }
  }

 private:
  void avisar() {
    if (!avisado_.exchange(true)) {
      PostMessageW(ventana_.hwnd(), kMensajeNovedades, 0, 0);
    }
  }

  void ordenar(std::vector<cliente::Orden> ordenes) { sesion_->ordenar(std::move(ordenes)); }

  void cambiar_avisos(red::Direccion direccion, cliente::AvisosDelServidor avisos) {
    cliente::normalizar(avisos, cliente::ahora_ms());
    if (auto* guardado = cliente::buscar_guardado(guardados_, direccion)) {
      guardado->avisos = avisos;
      sin_guardar_ = true;
    }
    if (sala_ && direccion == pendiente_.direccion) {
      avisos_.ajustes(avisos);
      servidor_.avisos = pendiente_.avisos = std::move(avisos);
    }
  }

  void aplicar_personas() {
    const auto* guardado = cliente::buscar_guardado(guardados_, pendiente_.direccion);
    servidor_.personas = guardado != nullptr ? guardado->personas : std::vector<cliente::PersonaGuardada>();
    const cliente::AjustesDeVoz ajustes = cliente::ajustes_de_voz(servidor_.personas, sala_->usuarios());
    if (motor_) {
      motor_->personas(ajustes.volumenes);
    }
    if (ajustes.silenciados != silenciados_enviados_) {
      silenciados_enviados_ = ajustes.silenciados;
      ordenar({protocolo::SilenciarPersonas{ajustes.silenciados}});
    }
    aplicar_susurros();
    if (en_ajustes_) {
      listas_de_susurro();
    }
  }

  void ordenar_si(std::optional<cliente::Orden> orden) {
    if (orden) {
      ordenar({std::move(*orden)});
    }
  }

  void avisar_de(const cliente::Novedad& novedad) {
    if (const auto* mensaje = std::get_if<protocolo::MensajeChat>(&novedad)) {
      const auto* canal = sala_->canal(mensaje->canal);
      const bool voz = canal != nullptr && canal->tipo == protocolo::TipoCanal::voz;
      if (canal != nullptr && mensaje->canal == (voz ? sala_->tu_canal() : sala_->mirando())) {
        avisos_.mensaje(*mensaje, canal->nombre, voz);
        const Situacion& situacion = ventana_.situacion();
        const bool a_la_vista =
            situacion.dibuja() && situacion.con_foco && !en_ajustes_ && sala_->chat().canal == mensaje->canal;
        sonar(cliente::sonido_de_mensaje(*mensaje, sala_->tu_id(), pendiente_.nombre, a_la_vista, servidor_.avisos,
                                         cliente::ahora_ms()));
      }
    } else if (const auto* aviso = std::get_if<protocolo::AvisoDeSeguridad>(&novedad)) {
      auto& lista = servidor_.seguridad;
      if (lista.size() >= kTopeAvisosDeSeguridad) {
        lista.erase(lista.begin());
      }
      lista.push_back(cliente::explicar(pantalla_.idioma, *aviso));
      avisos_.seguridad(lista.back());
      alerta_ = alerta_ || aviso->tipo != protocolo::Seguridad::nombre_liberado;
    } else if (const auto* nuevos = std::get_if<protocolo::MensajesNuevos>(&novedad)) {
      for (const auto& contador : nuevos->canales) {
        const auto* canal = sala_->canal(contador.canal);
        if (canal != nullptr && canal->tipo == protocolo::TipoCanal::texto && canal->id != sala_->mirando()) {
          avisos_.nuevos(canal->id, canal->nombre, sala_->nuevos(canal->id));
        }
      }
    } else if (const auto* borrado = std::get_if<protocolo::CanalBorrado>(&novedad)) {
      avisos_.visto(borrado->id);
    } else if (const auto* negado = std::get_if<protocolo::Negado>(&novedad);
               negado != nullptr && negado->pedido == protocolo::TipoMensaje::pedir_historial) {
      avisos_.negado();
    }
  }

  void abrir_aviso(const cliente::Notificacion& aviso) {
    ventana_.traer_al_frente();
    if (bloqueado_) {
      return;
    }
    avisos_.visto(aviso.canal);
    const auto direccion = red::leer_direccion(aviso.servidor);
    const auto* guardado = direccion ? cliente::buscar_guardado(guardados_, *direccion) : nullptr;
    if (!sesion_ && guardado != nullptr && aviso.canal != 0 && !aviso.voz) {
      entrar_a(*guardado);
    }
    const bool aca = direccion && *direccion == pendiente_.direccion;
    if (!sala_ && sesion_ && aca && !aviso.voz) {
      canal_del_aviso_ = aviso.canal;
    }
    if (sala_ && aviso.canal != 0 && aca && (!aviso.voz || aviso.canal == sala_->tu_canal())) {
      ordenar(aviso.voz ? sala_->elegir_voz(aviso.canal) : sala_->elegir_texto(aviso.canal));
      servidor_.enfocar_mensaje = true;
    }
  }

  void mostrar_avisos() {
    const bool juego = ventana_.situacion().juego;
    if (avisos_.version() == version_avisos_ && juego == juego_avisos_) {
      return;
    }
    version_avisos_ = avisos_.version();
    juego_avisos_ = juego;
    if (juego) {
      if (notificador_) {
        notificador_->ocultar();
      }
      return;
    }
    const std::vector<cliente::Notificacion> lista = avisos_.lista();
    if (!notificador_ && lista.empty()) {
      return;
    }
    if (!notificador_ && integrar_) {
      notificador_ = notificaciones_de_windows(ventana_.hwnd(), carpeta_);
    }
    if (!notificador_ || !notificador_->mostrar(lista)) {
      notificador_ = std::make_unique<AvisosEnPantalla>(ventana_.hwnd(), ventana_.escala());
      notificador_->mostrar(lista);
    }
  }

  void entrar_con_formulario() {
    const auto direccion = red::leer_direccion(pantalla_.direccion.data());
    const auto huella = cifrado::leer_huella(pantalla_.huella.data());
    cliente::Presentacion quien{pantalla_.nombre.data(), std::nullopt};
    const bool con_codigo = pantalla_.codigo[0] != '\0';
    if (con_codigo) {
      quien.codigo_dueno = cifrado::leer_codigo_dueno(pantalla_.codigo.data());
    }
    const Clave problema = !direccion                          ? Clave::conectar_direccion_invalida
                           : !huella                           ? Clave::conectar_huella_invalida
                           : !es_nombre_valido(quien.nombre)   ? Clave::conectar_nombre_invalido
                           : con_codigo && !quien.codigo_dueno ? Clave::conectar_codigo_invalido
                                                               : Clave::titulo_ventana;
    pantalla_.error_formulario.clear();
    if (problema != Clave::titulo_ventana) {
      pantalla_.error_formulario = texto(pantalla_.idioma, problema);
      return;
    }
    const std::string_view contrasena(pantalla_.contrasena.data());
    if (!contrasena.empty()) {
      auto secreto = std::make_shared<cifrado::Secreto>(contrasena.size());
      std::transform(contrasena.begin(), contrasena.end(), secreto->bytes().begin(),
                     [](char letra) { return static_cast<std::uint8_t>(letra); });
      quien.contrasena = std::move(secreto);
      mbedtls_platform_zeroize(pantalla_.contrasena.data(), pantalla_.contrasena.size());
      olvidar_lo_escrito();
    }
    entrar(*direccion, *huella, std::move(quien), true);
  }

  void entrar_a(const cliente::ServidorGuardado& servidor) {
    mbedtls_platform_zeroize(pantalla_.contrasena.data(), pantalla_.contrasena.size());
    entrar(servidor.direccion, cifrado::Huella(servidor.huella), {servidor.nombre, std::nullopt}, false);
  }

  void entrar(const red::Direccion& direccion, const cifrado::Huella& huella, cliente::Presentacion quien,
              bool desde_formulario) {
    if (!identidad_cargada_) {
      cargar_identidad();
    }
    if (bloqueado_) {
      quien.contrasena.reset();
      al_desbloquear_ = [this, direccion, huella, quien, desde_formulario] {
        entrar(direccion, huella, quien, desde_formulario);
      };
      return;
    }
    cerrar();
    quien.identidad = identidad_;
    quien.equipo = equipo_;
    canal_del_aviso_ = 0;
    pantalla_.intento = {direccion, desde_formulario, Fase::conectando, {}};
    pendiente_ = {direccion, huella.bytes(), quien.nombre, 0, {}, 0};
    if (const auto* guardado = cliente::buscar_guardado(guardados_, direccion);
        guardado != nullptr && cliente::conoce_huella(*guardado, huella.bytes())) {
      pendiente_.huella = guardado->huella;
      pendiente_.atras = guardado->atras;
    }
    cliente::OpcionesSesion opciones;
    opciones.conexion.atras = cliente::huellas_atras(pendiente_);
    opciones.al_haber_novedades = [this] { avisar(); };
    sesion_ = std::make_unique<cliente::Sesion>(direccion, cifrado::Huella(pendiente_.huella), std::move(quien),
                                                std::move(opciones));
    ahorrando_ = false;
  }

  void conectado(const cliente::IntentoDeConexion& intento) {
    if (intento.estado != cliente::EstadoConexion::conectado || !intento.presentado) {
      if (intento.estado == cliente::EstadoConexion::falta_contrasena ||
          intento.estado == cliente::EstadoConexion::contrasena_incorrecta ||
          intento.estado == cliente::EstadoConexion::esperar || intento.estado == cliente::EstadoConexion::ocupado) {
        poner(pantalla_.direccion, red::a_texto(pendiente_.direccion));
        poner(pantalla_.huella, cifrado::Huella(pendiente_.huella).a_texto());
        poner(pantalla_.nombre, pendiente_.nombre);
        pantalla_.enfocar_contrasena = true;
        pantalla_.intento.desde_formulario = true;
      }
      terminar(cliente::explicar(pantalla_.idioma, intento));
      return;
    }
    if (const auto* guardado = cliente::buscar_guardado(guardados_, pendiente_.direccion);
        guardado != nullptr && guardado->huella == pendiente_.huella) {
      pendiente_.texto = guardado->texto;
      pendiente_.nombre_canal = guardado->nombre_canal;
      pendiente_.leidos = guardado->leidos;
      pendiente_.personas = guardado->personas;
      pendiente_.avisos = guardado->avisos;
    }
    if (canal_del_aviso_ != 0) {
      pendiente_.texto = std::exchange(canal_del_aviso_, 0);
    }
    if (intento.cambio) {
      cliente::cambiar_huella(pendiente_, *intento.cambio);
    }
    sala_ =std::make_unique<cliente::Sala>(*intento.presentado, pendiente_.leidos, pendiente_.texto);
    avisos_.conectado(red::a_texto(pendiente_.direccion), sala_->tu_id(), pendiente_.nombre);
    avisos_.ajustes(pendiente_.avisos);
    pendiente_.ultima_vez_ms = cliente::ahora_ms();
    guardados_.nombre = pendiente_.nombre;
    if (cliente::recordar(guardados_, pendiente_)) {
      guardar();
    }
    pantalla_.codigo.fill('\0');
    if (pantalla_.intento.desde_formulario) {
      pantalla_.direccion.fill('\0');
      pantalla_.huella.fill('\0');
    }
    servidor_ = PantallaServidor{};
    servidor_.idioma = pantalla_.idioma;
    servidor_.logo = pantalla_.logo;
    servidor_.direccion = red::a_texto(pendiente_.direccion);
    servidor_.nombre_servidor = intento.presentado->servidor;
    servidor_.calidad_alta = calidad_.frecuencia == cliente::Calidad{}.frecuencia;
    servidor_.tecla = guardados_.tecla_hablar;
    servidor_.por_voz = guardados_.por_voz;
    servidor_.sensibilidad = guardados_.sensibilidad;
    servidor_.silencio = guardados_.silencio;
    servidor_.microfono_antes = guardados_.silencio != 0;
    servidor_.volumen_microfono = guardados_.volumen_microfono;
    servidor_.volumen_salida = guardados_.volumen_salida;
    servidor_.enfocar_mensaje = true;
    servidor_.avisos = pendiente_.avisos;
    if (intento.cambio) {
      servidor_.seguridad.emplace_back(texto(pantalla_.idioma, Clave::conectar_clave_cambiada));
    }
    const std::wstring ultima_voz = nombre_para_comparar(pendiente_.nombre_canal);
    for (const auto& canal : sala_->canales()) {
      if (canal.tipo == protocolo::TipoCanal::voz && !ultima_voz.empty() &&
          nombre_para_comparar(canal.nombre) == ultima_voz) {
        servidor_.ultima_voz = canal.id;
      }
    }
    proxima_muestra_ = Reloj::now() + 1s;
    acumulada_ = {};
    segundos_ = 0;
    diagnostico_ = cliente::Diagnostico();
    cuadros_ = 0;
    ventana_.tomar_uso();
    despertares_red_ = sesion_->despertares();
    trafico_ = cifrado::trafico();
    ordenar(sala_->empezar());
    if (cliente::ajuste_o(guardados_, cliente::Ajuste::al_entrar, 0) == 1 && servidor_.ultima_voz != 0) {
      ordenar(sala_->elegir_voz(servidor_.ultima_voz));
    }
    if (guardados_.silencio != 0) {
      ordenar({protocolo::Silenciar{guardados_.silencio}});
    }
    silenciados_enviados_.clear();
    susurros_enviados_ = {};
    aplicar_personas();
  }

  bool mirar_voz() {
    const std::uint32_t canal = sala_->tu_canal();
    if (canal != canal_de_voz_) {
      sonar(canal != 0 ? cliente::Sonido::entrar_canal : cliente::Sonido::salir_canal);
      canal_de_voz_ = canal;
    }
    const bool cambio_de_voz = canal != 0 ? !motor_ && Reloj::now() >= abrir_desde_ : motor_ != nullptr;
    if (cambio_de_voz && canal != 0) {
      abrir_voz();
    } else if (cambio_de_voz) {
      cerrar_voz();
    }
    servidor_.ultima_voz = canal != 0 ? canal : servidor_.ultima_voz;
    if (motor_) {
      const auto& en_mi_canal = sala_->en_canal(canal);
      motor_->solo_en_el_canal(std::none_of(en_mi_canal.begin(), en_mi_canal.end(),
                                            [this](const auto* usuario) { return usuario->id != sala_->tu_id(); }));
    }
    const auto antes = std::make_tuple(servidor_.transmitiendo, servidor_.hablando, servidor_.nivel,
                                       servidor_.susurrando, servidor_.te_susurra);
    servidor_.transmitiendo = motor_ && motor_->transmitiendo();
    servidor_.susurrando = motor_ ? motor_->susurrando() : std::uint8_t{0};
    servidor_.hablando = puente_ ? puente_->quienes() : cliente::Hablando{};
    servidor_.te_susurra = te_susurra(puente_ ? puente_->susurrantes() : cliente::Hablando{});
    if (!servidor_.por_voz && std::get<0>(antes) != servidor_.transmitiendo) {
      sonar(servidor_.transmitiendo ? cliente::Sonido::transmitir_empieza : cliente::Sonido::transmitir_termina);
    }
    if (std::get<4>(antes).empty() && !servidor_.te_susurra.empty()) {
      sonar(cliente::Sonido::susurro);
    }
    const bool hablan = std::any_of(servidor_.hablando.begin(), servidor_.hablando.end(),
                                    [](std::uint32_t id) { return id != 0; }) &&
                        (guardados_.silencio & protocolo::kSilencioAudio) == 0;
    ultima_voz_ = hablan ? Reloj::now() : ultima_voz_;
    if (hablan && limpieza_.bajar != cliente::Bajar::nada && !bajando_) {
      bajando_ = true;
      otros_.bajar(cliente::factor_de_bajada(static_cast<std::uint8_t>(limpieza_.bajar)));
    } else if (!hablan && bajando_ && Reloj::now() - ultima_voz_ >= kSilencioParaDevolver) {
      bajando_ = false;
      otros_.devolver();
    }
    servidor_.nivel = vigia_ ? vigia_->nivel() : 0;
    const Situacion& situacion = ventana_.situacion();
    if (const bool medir = vigia_ && servidor_.por_voz && servidor_.ajustando && situacion.con_foco && situacion.dibuja();
        medir != midiendo_) {
      midiendo_ = medir;
      vigia_->medir(medir);
    }
    bool cambio = cambio_de_voz || antes != std::make_tuple(servidor_.transmitiendo, servidor_.hablando, servidor_.nivel,
                                                            servidor_.susurrando, servidor_.te_susurra);
    if (const auto elegida = vigia_ ? vigia_->tomar_elegida() : std::nullopt) {
      cambio = true;
      servidor_.eligiendo_tecla = false;
      if (*elegida != 0) {
        quitar_tecla(*elegida);
        aplicar_teclas();
        guardados_.tecla_hablar = *elegida;
        servidor_.tecla = *elegida;
        vigia_->usar_tecla(*elegida);
        guardar();
      }
    }
    const auto ahora = Reloj::now();
    if (ahora < proxima_muestra_) {
      return cambio;
    }
    proxima_muestra_ = ahora + 1s;
    if (const auto yo = sala_->usuarios().find(sala_->tu_id());
        yo != sala_->usuarios().end() && yo->second.silencio != guardados_.silencio) {
      ordenar({protocolo::Silenciar{guardados_.silencio}});
    }
    if (silenciados_enviados_ == std::vector<std::uint32_t>{0}) {
      aplicar_personas();
    }
    if (susurros_enviados_[0].canal == kSinSaber) {
      aplicar_susurros();
    }
    if (cliente::normalizar(servidor_.avisos, cliente::ahora_ms())) {
      cambiar_avisos(pendiente_.direccion, servidor_.avisos);
    }
    const bool tenia_poca_memoria = std::exchange(poca_memoria_, queda_poca_memoria());
    if (poca_memoria_ && !tenia_poca_memoria) {
      sonidos_.soltar();
      ImGui::GetIO().Fonts->CompactCache();
      sala_->soltar_lejanos();
      soltar_lejanos(servidor_);
    }
    const std::uint64_t perdidos = puente_ ? puente_->perdidos.load() : 0;
    const std::uint64_t recibidos = puente_ ? puente_->recibidos.load() : 0;
    const Muestra muestra{sala_->ping().value_or(0), static_cast<std::uint32_t>(perdidos - perdidos_),
                          static_cast<std::uint32_t>(recibidos - recibidos_)};
    perdidos_ = perdidos;
    recibidos_ = recibidos;
    acumulada_ = {muestra.ping, acumulada_.perdidos + muestra.perdidos, acumulada_.recibidos + muestra.recibidos};
    if (++segundos_ % kSegundosPorMuestra == 0) {
      servidor_.grafico.push_back(std::exchange(acumulada_, {}));
    }
    const cliente::Medicion medicion = diagnostico_.medir();
    servidor_.diagnostico =
        motor_ ? DatosDiagnostico{puente_->estado == cliente::EstadoVoz::lista, puente_->ping_ms, medicion.procesador,
                                  medicion.ram_bytes, motor_->espera_ms(), motor_->recortes(),
                                  audio_->espera_de_salida_ms()}
               : DatosDiagnostico{false, 0, medicion.procesador, medicion.ram_bytes};
    if (registro_.is_open()) {
      const bool voz = motor_ != nullptr;
      const DatosDiagnostico& datos = servidor_.diagnostico;
      const ImTextureData& atlas = *ImGui::GetIO().Fonts->TexData;
      UsoVentana uso = ventana_.tomar_uso();
      const cifrado::Trafico trafico = cifrado::trafico();
      auto& mensajes = uso.mensajes;
      const std::uint32_t avisos = mensajes[kMensajeNovedades];
      mensajes.erase(kMensajeNovedades);
      std::pair<UINT, std::uint32_t> comun{0, 0};
      std::uint32_t otros = 0;
      for (const auto& par : mensajes) {
        otros += par.second;
        if (par.second > comun.second) {
          comun = par;
        }
      }
      anotar(registro_, std::chrono::duration_cast<std::chrono::milliseconds>(ahora - inicio_).count(), muestra.ping,
             datos.ping_voz, medicion.procesador, medicion.ventana, medicion.sistema, uso.ocupado_ms,
             uso.pasada_maxima_ms, uso.imgui_ms, uso.direct3d_ms, uso.presentados, uso.vertices_max, cuadros_,
             datos.colchon_ms, datos.recortes, muestra.perdidos, muestra.recibidos, datos.salida_ms,
             voz ? audio_->tomar_espera_maxima_ms() : 0, voz ? audio_->descartados() : 0,
             voz ? audio_->descartados_con_voz() : 0, voz ? audio_->vacios() : 0, voz ? motor_->vaciados() : 0,
             voz ? motor_->faltas() : 0, servidor_.transmitiendo,
             std::count_if(servidor_.hablando.begin(), servidor_.hablando.end(),
                           [](std::uint32_t id) { return id != 0; }),
             datos.voz_lista, avisos, otros,
             comun.first, medicion.ram_bytes / (1024 * 1024), uso.despertares,
             voz ? audio_->despertares() - std::exchange(despertares_audio_, audio_->despertares()) : 0,
             sesion_->despertares() - std::exchange(despertares_red_, sesion_->despertares()),
             trafico.tcp_enviados - trafico_.tcp_enviados, trafico.tcp_recibidos - trafico_.tcp_recibidos,
             trafico.udp_enviados - trafico_.udp_enviados, trafico.udp_recibidos - trafico_.udp_recibidos,
             medicion.memoria_propia / 1024, medicion.fallas, ventana_.arranque_ms(), uso.pixeles,
             situacion.con_foco, situacion.juego, situacion.bloqueada, situacion.ahorro,
             atlas.Width * atlas.Height * atlas.BytesPerPixel / 1024, static_cast<int>(servidor_.silencio),
             notificador_ ? notificador_->mostrados() : 0u, voz ? motor_->objetivo_ms() : 0u,
             voz ? motor_->acelerado_ms() : 0u, voz ? motor_->estirado_ms() : 0u, voz ? motor_->tardes() : 0u,
             voz ? motor_->tomar_limpieza_us() : 0u, uso.reinicios_fallidos, uso.error_direct3d, uso.aviso_de_sesion);
      trafico_ = trafico;
    }
    cuadros_ = 0;
    if (servidor_.grafico.size() > kMuestrasDelGrafico) {
      servidor_.grafico.pop_front();
    }
    return true;
  }

  void terminar(std::string mensaje) {
    cerrar();
    pantalla_.intento.fase = Fase::error;
    pantalla_.intento.mensaje = std::move(mensaje);
  }

  void desconectar() {
    cerrar();
    pantalla_.intento = {};
  }

  void guardar() {
    sin_guardar_ = false;
    if (se_puede_guardar_) {
      pantalla_.aviso = cliente::guardar_guardados(guardados_, carpeta_)
                            ? std::string()
                            : std::string(texto(pantalla_.idioma, Clave::inicio_no_se_guardo));
    }
  }

  Ventana& ventana_;
  const std::wstring carpeta_;
  cliente::Sonidos sonidos_{carpeta_};
  std::function<void()> despues_del_cuadro_;
  std::uint32_t canal_de_voz_ = 0;
  const bool integrar_;
  cliente::Avisos avisos_;
  cliente::OtrosProgramas otros_;
  std::unique_ptr<Notificador> notificador_;
  std::uint32_t canal_del_aviso_ = 0;
  std::vector<std::uint32_t> silenciados_enviados_;
  std::array<protocolo::DestinosDeSusurro, protocolo::kLugaresDeSusurro> susurros_enviados_{};
  std::uint64_t version_avisos_ = 0;
  bool juego_avisos_ = false;
  const cliente::Calidad calidad_ = cliente::calidad_automatica();
  cliente::AjustesDeLimpieza limpieza_;
  Reloj::time_point ultima_voz_{};
  bool bajando_ = false;
  std::atomic<bool> avisado_{false};
  PantallaInicio pantalla_;
  PantallaAjustes ajustes_;
  cliente::EstadoIdentidad estado_identidad_ = cliente::EstadoIdentidad::error;
  bool identidad_cargada_ = false;
  std::shared_ptr<const cifrado::Clave> identidad_;
  std::optional<cifrado::LlaveDeSobre> maestra_;
  std::vector<std::uint8_t> cerrada_;
  bool bloqueado_ = false;
  bool alerta_ = false;
  std::uint32_t bloqueos_vistos_ = 0;
  std::function<void()> al_desbloquear_;
  std::shared_ptr<const cifrado::Secreto> equipo_;
  std::optional<cifrado::Clave> por_reemplazar_;
  std::wstring ruta_de_copia_;
  cifrado::Secreto copia_leida_;
  std::shared_ptr<TrabajoDeCopia> copia_;
  std::thread hilo_de_copia_;
  bool en_ajustes_ = false;
  PantallaHostear hostear_;
  bool en_hostear_ = false;
  bool mostrar_minimizada_ = true;
  std::unique_ptr<cliente::PruebaDeAudio> prueba_;
  std::optional<cliente::Teclas> eligiendo_desde_;
  Reloj::time_point fin_de_prueba_{};
  PantallaServidor servidor_;
  cliente::Guardados guardados_;
  bool se_puede_guardar_ = false;
  bool sin_guardar_ = false;
  cliente::ServidorGuardado pendiente_;
  std::uint64_t perdidos_ = 0;
  std::uint64_t recibidos_ = 0;
  Reloj::time_point proxima_muestra_{};
  Reloj::time_point abrir_desde_{};
  Muestra acumulada_{};
  std::uint32_t segundos_ = 0;
  std::uint32_t despertares_audio_ = 0;
  std::uint32_t despertares_red_ = 0;
  cifrado::Trafico trafico_;
  cliente::Diagnostico diagnostico_;
  const Reloj::time_point inicio_ = Reloj::now();
  std::uint32_t cuadros_ = 0;
  bool poca_memoria_ = false;
  bool ahorrando_ = false;
  bool midiendo_ = false;
  std::ofstream registro_;
  std::shared_ptr<cliente::PuenteVoz> puente_;
  std::unique_ptr<cliente::MotorVoz> motor_;
  std::unique_ptr<cliente::Audio> audio_;
  std::unique_ptr<cliente::Vigia> vigia_;
  std::unique_ptr<cliente::Sala> sala_;
  std::unique_ptr<cliente::Sesion> sesion_;
};

}

int abrir_programa(Idioma idioma, const std::wstring& titulo, int como_mostrar, const std::wstring& carpeta,
                   const std::vector<std::string>& entrar, const std::wstring& registro) {
  const auto unico = ser_el_unico();
  if (!unico) {
    MessageBoxW(nullptr, a_utf16(texto(idioma, Clave::aviso_ya_abierto)).c_str(), titulo.c_str(),
                MB_OK | MB_ICONERROR);
    mostrar_el_otro();
    return 4;
  }
  const cifrado::Inicio cifrado;
  const red::Inicio red;
  Ventana ventana(titulo, como_mostrar);
  if (!ventana.ok()) {
    MessageBoxW(nullptr, a_utf16(texto(idioma, Clave::aviso_sin_direct3d)).c_str(), titulo.c_str(),
                MB_OK | MB_ICONERROR);
    return 3;
  }
  Programa programa(idioma, ventana, carpeta, entrar);
  if (!registro.empty()) {
    programa.registrar(registro);
  }
  ventana.al_terminar_la_sesion([&programa] { programa.cerrar(); });
  return ventana.correr([&programa] { return programa.atender(); }, [&programa] { programa.cuadro(); });
}

}
