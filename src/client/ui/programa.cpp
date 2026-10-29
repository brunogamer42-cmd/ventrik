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
#include "client/mis_servidores.h"
#include "client/motor_voz.h"
#include "client/arranque.h"
#include "client/prueba_de_audio.h"
#include "client/sala.h"
#include "client/salida_de_windows.h"
#include "client/sesion.h"
#include "client/vigia.h"
#include "client/webauthn.h"
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
#include "common/protocol/hostear.h"
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

constexpr auto kEsperaParaArrancar = std::chrono::minutes(2);
constexpr auto kCadaCuantoMirarMios = 2s;
constexpr std::uint64_t kMemoriaDeServidorLimitado = 2ull * 1024 * 1024 * 1024;

bool desde_windows(DWORD mayor, DWORD menor) {
  OSVERSIONINFOEXW pedido = {};
  pedido.dwOSVersionInfoSize = sizeof(pedido);
  pedido.dwMajorVersion = mayor;
  pedido.dwMinorVersion = menor;
  return VerifyVersionInfoW(&pedido, VER_MAJORVERSION | VER_MINORVERSION,
                            VerSetConditionMask(VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL),
                                                VER_MINORVERSION, VER_GREATER_EQUAL)) != FALSE;
}

bool servidor_limitado() {
  SYSTEM_INFO sistema = {};
  GetSystemInfo(&sistema);
  MEMORYSTATUSEX memoria = {};
  memoria.dwLength = sizeof(memoria);
  return sistema.dwNumberOfProcessors <= 1 ||
         (GlobalMemoryStatusEx(&memoria) != 0 && memoria.ullTotalPhys < kMemoriaDeServidorLimitado);
}

enum class Tarea : std::uint8_t { exportar, importar, desbloquear, activar, cambiar, quitar, crear_llave };
enum class Encargo : std::uint8_t { crear, arrancar, exportar, importar, cambiar_clave };

struct EncargoEnCurso {
  Encargo cual;
  std::wstring carpeta;
  cliente::ProcesoDelServidor proceso;
  std::chrono::steady_clock::time_point desde;
  cifrado::Secreto arranque;
  std::wstring archivo;
  bool configurar = false;
  bool con_arranque = false;
};

struct TrabajoDeCopia {
  Tarea tarea = Tarea::exportar;
  std::wstring ruta;
  std::optional<cifrado::Huella> huella;
  cifrado::Secreto contrasena;
  cifrado::Secreto nueva;
  cifrado::Secreto entrada;
  std::uint32_t generacion = 0;
  std::optional<std::vector<std::uint8_t>> armada;
  cliente::CopiaAbierta abierta;
  std::optional<cifrado::LlaveDeSobre> llave;
  std::wstring nombre;
  std::vector<cliente::IdDeLlave> excluir;
  cliente::RespuestaDeWebAuthn<cifrado::LlaveCreada> creada;
  std::atomic<bool> listo{false};
};

constexpr HRESULT kCanceladoPorLaPersona = _HRESULT_TYPEDEF_(0x80090036L);

Clave explicar_webauthn(HRESULT error) {
  return error == kCanceladoPorLaPersona || error == HRESULT_FROM_WIN32(ERROR_CANCELLED) ? Clave::llave_cancelada
         : error == NTE_EXISTS                                                       ? Clave::llave_repetida
         : error == NTE_NOT_SUPPORTED || error == NTE_BAD_DATA                       ? Clave::llave_sin_es256
                                                                                     : Clave::llave_error;
}

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
    refrescar_mios();
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
    ui::borrar_contrasenas(pantalla_.dialogo);
    ui::borrar_contrasenas(hostear_);
    cerrar();
    soltar_clics_de_windows();
    if ((copia_ && !copia_->listo) || std::any_of(tocando_.begin(), tocando_.end(), [](const auto& sesion) {
          return sesion->tocando_llave();
        })) {
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
    if (sesion_) {
      sesion_->abandonar();
    }
    if (sesion_ && sesion_->tocando_llave()) {
      tocando_.push_back(std::move(sesion_));
    }
    sesion_.reset();
    sala_.reset();
    en_hostear_ = en_hostear_ && !hostear_.configurando;
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
    ajustes_.hay_bajar_otros = desde_windows(6, 1);
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
    hostear_.ocupado = ocupado();
    en_hostear_ = true;
  }

  void abrir_asistente() {
    PantallaHostear pantalla = asistente(pantalla_.idioma);
    pantalla.xp = !desde_windows(6, 0);
    pantalla.limitado = servidor_limitado();
    if (cliente::exe_del_servidor().empty()) {
      pantalla.error = texto(pantalla_.idioma, Clave::hostear_falta_servidor);
    }
    carpeta_config_.clear();
    abrir_hostear(std::move(pantalla));
  }

  const cliente::MiServidorEnDisco* buscar_mio(const std::wstring& carpeta) const {
    const auto mio = std::find_if(mios_.begin(), mios_.end(),
                                  [&carpeta](const cliente::MiServidorEnDisco& cual) { return cual.carpeta == carpeta; });
    return mio == mios_.end() ? nullptr : &*mio;
  }

  void abrir_configuracion() {
    const auto es_este = [this](const cliente::MiServidorEnDisco& cual) { return cual.huella == pendiente_.huella; };
    const auto andando = std::find_if(mios_.begin(), mios_.end(), [&](const cliente::MiServidorEnDisco& cual) {
      return es_este(cual) && cual.puerto == pendiente_.direccion.puerto && cliente::esta_andando(cual.carpeta);
    });
    const auto mio = andando != mios_.end() ? andando : std::find_if(mios_.begin(), mios_.end(), es_este);
    guardando_contrasena_ = false;
    confirmar_quitar_ = false;
    const bool aca = mio != mios_.end();
    PantallaHostear pantalla = configuracion(servidor_.idioma, servidor_.nombre_servidor, aca);
    pantalla.huella = cifrado::Huella(pendiente_.huella).a_texto();
    pantalla.exportada = aca && mio->exportada;
    pantalla.xp = aca && !desde_windows(6, 0);
    pantalla.limitado = aca && servidor_limitado();
    carpeta_config_ = aca ? mio->carpeta : std::wstring();
    pantalla.cargando = true;
    abrir_hostear(std::move(pantalla));
    config_recibida_ = false;
    ordenar({protocolo::PedirConfigSeguridad{}, protocolo::PedirRegistro{}});
  }

  void refrescar_mios() {
    mios_ = cliente::leer_mis_servidores(raiz_servidores_);
    pantalla_.mis_servidores.clear();
    for (const cliente::MiServidorEnDisco& mio : mios_) {
      MiServidor tarjeta{mio.nombre, mio.puerto, cliente::esta_andando(mio.carpeta),
                         encargo_ && encargo_->carpeta == mio.carpeta, mio.huella};
      const auto nota = notas_.find(mio.carpeta);
      if (nota != notas_.end()) {
        std::tie(tarjeta.nota, tarjeta.nota_es_error) = nota->second;
      }
      pantalla_.mis_servidores.push_back(std::move(tarjeta));
    }
  }

  bool mirar_mios() {
    if (sala_ || en_ajustes_ || en_hostear_ || bloqueado_ || Reloj::now() < mirar_mios_) {
      return false;
    }
    mirar_mios_ = Reloj::now() + kCadaCuantoMirarMios;
    bool cambio = false;
    for (std::size_t i = 0; i < mios_.size(); ++i) {
      if (i >= pantalla_.mis_servidores.size() ||
          cliente::esta_andando(mios_[i].carpeta) != pantalla_.mis_servidores[i].andando) {
        notas_.erase(mios_[i].carpeta);
        cambio = true;
      }
    }
    if (cambio) {
      refrescar_mios();
    }
    return cambio;
  }

  void anotar_mio(const std::wstring& carpeta, std::string_view nota, bool error) {
    notas_[carpeta] = {std::string(nota), error};
    refrescar_mios();
  }

  std::string nombre_propio() const {
    if (es_nombre_valido(guardados_.nombre)) {
      return guardados_.nombre;
    }
    std::array<wchar_t, 257> usuario{};
    DWORD largo = static_cast<DWORD>(usuario.size());
    const std::string de_windows = GetUserNameW(usuario.data(), &largo) != 0 ? a_utf8(usuario.data()) : std::string();
    return es_nombre_valido(de_windows) ? de_windows : std::string(texto(pantalla_.idioma, Clave::hostear_nombre_dueno));
  }

  bool lanzar(Encargo cual, const std::wstring& carpeta, std::vector<std::wstring> argumentos,
              const cifrado::Secreto& entrada, cifrado::Secreto arranque = {}, std::wstring archivo = {}) {
    const std::wstring exe = cliente::exe_del_servidor();
    if (encargo_ || exe.empty() || (entrada.size() == 0 && cual != Encargo::arrancar)) {
      return false;
    }
    argumentos.insert(argumentos.begin(), {L"--carpeta", carpeta});
    auto proceso = cliente::ProcesoDelServidor::lanzar(exe, argumentos, entrada.bytes(), carpeta);
    if (!proceso) {
      return false;
    }
    encargo_.emplace(EncargoEnCurso{cual, carpeta, std::move(*proceso), Reloj::now(), std::move(arranque),
                                    std::move(archivo)});
    return true;
  }

  bool ocupado() const { return encargo_.has_value() && encargo_->cual != Encargo::arrancar; }

  void avisar_hostear(Clave clave, bool error) {
    hostear_.aviso = texto(hostear_.idioma, clave);
    hostear_.aviso_es_error = error;
    hostear_.ocupado = ocupado();
    hostear_.error.clear();
  }

  void crear_mi_servidor(Creacion creacion) {
    const Clave problema = !identidad_ && !en_llave()           ? Clave::hostear_sin_identidad
                           : encargo_                           ? Clave::hostear_espera
                           : cliente::exe_del_servidor().empty() ? Clave::hostear_falta_servidor
                                                                : Clave::titulo_ventana;
    const std::wstring carpeta =
        problema == Clave::titulo_ventana ? cliente::carpeta_nueva(raiz_servidores_, creacion.nombre) : std::wstring();
    protocolo::ServidorNuevo nuevo;
    nuevo.nombre = creacion.nombre;
    nuevo.seguridad = creacion.seguridad;
    nuevo.dueno = identidad_ ? identidad_->publica() : en_llave() ? mis_llaves_->identidad : cifrado::ClavePublica{};
    nuevo.nombre_dueno = nombre_propio();
    nuevo.contrasena = std::move(creacion.contrasena);
    nuevo.arranque = cifrado::copiar(creacion.arranque.bytes());
    const bool con_arranque = creacion.arranque.size() > 0;
    if (!carpeta.empty() && lanzar(Encargo::crear, carpeta, {L"--crear"}, protocolo::armar_servidor_nuevo(nuevo),
                                   std::move(creacion.arranque))) {
      encargo_->con_arranque = con_arranque;
      avisar_hostear(Clave::hostear_creando, false);
      return;
    }
    if (!carpeta.empty()) {
      cliente::borrar_carpeta_vacia(carpeta);
    }
    hostear_.aviso.clear();
    hostear_.error = texto(hostear_.idioma, problema == Clave::titulo_ventana ? Clave::hostear_no_se_creo : problema);
  }

  void iniciar_mio(std::wstring carpeta, cifrado::Secreto arranque, bool configurar) {
    if (cliente::esta_andando(carpeta)) {
      entrar_a_mio(carpeta, configurar);
      return;
    }
    const bool con_arranque = arranque.size() > 0;
    if (!lanzar(Encargo::arrancar, carpeta, {L"--sin-consola"},
                protocolo::armar_entrada(cifrado::texto_de(arranque), {}))) {
      anotar_mio(carpeta, texto(pantalla_.idioma, encargo_ ? Clave::hostear_espera : Clave::mio_no_arranco), true);
      return;
    }
    encargo_->configurar = configurar;
    encargo_->con_arranque = con_arranque;
    anotar_mio(carpeta, {}, false);
  }

  void entrar_a_mio(std::wstring carpeta, bool configurar) {
    refrescar_mios();
    const cliente::MiServidorEnDisco* mio = buscar_mio(carpeta);
    if (mio == nullptr || !mio->huella) {
      anotar_mio(carpeta, texto(pantalla_.idioma, Clave::mio_no_arranco), true);
      return;
    }
    configurar_al_entrar_ = configurar ? carpeta : std::wstring();
    entrar(red::Direccion{0x7F000001, mio->puerto}, cifrado::Huella(*mio->huella), {nombre_propio(), std::nullopt},
           false);
  }

  bool atender_encargo() {
    if (!encargo_) {
      return false;
    }
    const auto salida = encargo_->proceso.salida();
    const bool listo = encargo_->cual == Encargo::arrancar && cliente::esta_andando(encargo_->carpeta);
    const bool vencido = encargo_->cual == Encargo::arrancar && Reloj::now() - encargo_->desde > kEsperaParaArrancar;
    if (!salida && !listo && !vencido) {
      return false;
    }
    EncargoEnCurso hecho = std::move(*encargo_);
    encargo_.reset();
    const protocolo::SalidaServidor codigo = salida.value_or(protocolo::SalidaServidor::error);
    const bool bien = codigo == protocolo::SalidaServidor::bien;
    hostear_.ocupado = false;
    switch (hecho.cual) {
      case Encargo::crear:
        if (!bien) {
          cliente::borrar_carpeta_vacia(hecho.carpeta);
          hostear_.aviso.clear();
          hostear_.error = texto(hostear_.idioma, Clave::hostear_no_se_creo);
          break;
        }
        ui::borrar_contrasenas(hostear_);
        en_hostear_ = false;
        iniciar_mio(hecho.carpeta, std::move(hecho.arranque), false);
        break;
      case Encargo::arrancar:
        if (listo || codigo == protocolo::SalidaServidor::andando) {
          anotar_mio(hecho.carpeta, {}, false);
          entrar_a_mio(hecho.carpeta, hecho.configurar);
        } else if (codigo == protocolo::SalidaServidor::arranque && !vencido) {
          arrancar_pendiente_ = hecho.carpeta;
          configurar_pendiente_ = hecho.configurar;
          anotar_mio(hecho.carpeta, {}, false);
          pedir_contrasenas(pantalla_.dialogo, PideContrasena::arranque, true);
          if (hecho.con_arranque) {
            pantalla_.dialogo.error = texto(pantalla_.idioma, Clave::hostear_error_arranque);
          }
        } else {
          anotar_mio(hecho.carpeta, texto(pantalla_.idioma, Clave::mio_no_arranco), true);
        }
        break;
      case Encargo::exportar: {
        const std::wstring temporal = hecho.archivo + L".exportando";
        const cliente::MiServidorEnDisco* mio = buscar_mio(hecho.carpeta);
        hostear_.exportada = bien && MoveFileExW(temporal.c_str(), hecho.archivo.c_str(),
                                                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (hostear_.exportada && mio != nullptr && mio->huella) {
          escribir_archivo_completo(hecho.carpeta + protocolo::kArchivoExportada, *mio->huella);
        }
        if (!bien) {
          DeleteFileW(temporal.c_str());
        }
        terminar_en_consola(hostear_.exportada, codigo, Clave::hostear_exportado);
        break;
      }
      case Encargo::importar:
        if (!bien) {
          cliente::borrar_carpeta_vacia(hecho.carpeta);
          terminar_en_consola(false, codigo, Clave::hostear_importando);
          break;
        }
        ui::borrar_contrasenas(hostear_);
        en_hostear_ = false;
        refrescar_mios();
        iniciar_mio(hecho.carpeta, {}, false);
        break;
      case Encargo::cambiar_clave:
        terminar_en_consola(bien, codigo, Clave::hostear_clave_cambiada);
        break;
    }
    refrescar_mios();
    return true;
  }

  void terminar_en_consola(bool bien, protocolo::SalidaServidor codigo, Clave hecho) {
    avisar_hostear(bien                                              ? hecho
                   : codigo == protocolo::SalidaServidor::arranque    ? Clave::hostear_error_arranque
                   : codigo == protocolo::SalidaServidor::contrasena ? Clave::hostear_error_archivo
                                                                      : Clave::hostear_error_consola,
                   !bien);
  }

  void elegir_exportacion() {
    const auto ruta = carpeta_config_.empty() ? std::nullopt
                                              : elegir_archivo(Clave::archivo_exportar_servidor,
                                                               Clave::archivo_filtro_servidor, L"*.vkservidor", true,
                                                               Clave::archivo_servidor_nuevo, L".vkservidor");
    if (ruta) {
      exportar_a_ = *ruta;
      pedir_contrasenas(hostear_.dialogo, PideContrasena::exportar, cliente::pide_arranque(carpeta_config_));
    }
  }

  void elegir_importacion() {
    const auto ruta = elegir_archivo(Clave::archivo_importar_servidor, Clave::archivo_filtro_servidor, L"*.vkservidor",
                                     false, Clave::archivo_servidor_nuevo, L".vkservidor");
    if (ruta) {
      importar_de_ = *ruta;
      pedir_contrasenas(hostear_.dialogo, PideContrasena::importar, false);
    }
  }

  void usar_contrasenas(Contrasenas contrasenas) {
    const auto entrada = protocolo::armar_entrada(cifrado::texto_de(contrasenas.arranque),
                                                  cifrado::texto_de(contrasenas.contrasena));
    if (contrasenas.cual == PideContrasena::exportar) {
      const std::wstring temporal = exportar_a_ + L".exportando";
      DeleteFileW(temporal.c_str());
      if (lanzar(Encargo::exportar, carpeta_config_, {L"--sin-consola", L"--exportar", temporal}, entrada, {},
                 exportar_a_)) {
        avisar_hostear(Clave::hostear_exportando, false);
        return;
      }
    } else if (contrasenas.cual == PideContrasena::importar) {
      const std::size_t barra = importar_de_.find_last_of(L"\\/") + 1;
      const std::wstring archivo = importar_de_.substr(barra, importar_de_.find_last_of(L'.') - barra);
      const std::wstring carpeta = encargo_ ? std::wstring() : cliente::carpeta_nueva(raiz_servidores_, a_utf8(archivo));
      if (!carpeta.empty() &&
          lanzar(Encargo::importar, carpeta, {L"--sin-consola", L"--importar", importar_de_}, entrada)) {
        avisar_hostear(Clave::hostear_importando, false);
        return;
      }
      if (!carpeta.empty()) {
        cliente::borrar_carpeta_vacia(carpeta);
      }
    } else if (contrasenas.cual == PideContrasena::cambiar_clave) {
      if (lanzar(Encargo::cambiar_clave, carpeta_config_, {L"--sin-consola", L"--cambiar-clave"}, entrada)) {
        avisar_hostear(Clave::hostear_cambiando, false);
        return;
      }
    }
    avisar_hostear(encargo_ ? Clave::hostear_espera : Clave::hostear_error_consola, true);
  }

  void atender_hostear(PedidoHostear pedido) {
    if (pedido.crear) {
      crear_mi_servidor(std::move(*pedido.crear));
    }
    const bool en_vivo = hostear_.configurando && sala_ != nullptr;
    if (pedido.nombre && en_vivo && *pedido.nombre != servidor_.nombre_servidor) {
      ordenar({protocolo::CambiarNombreServidor{*pedido.nombre}});
      avisar_hostear(Clave::hostear_guardando, false);
    }
    if (pedido.contrasena && en_vivo && (pedido.contrasena->size() > 0 || hostear_.con_contrasena)) {
      if (pedido.contrasena->size() == 0 && !std::exchange(confirmar_quitar_, true)) {
        avisar_hostear(Clave::hostear_confirmar_quitar, true);
      } else {
        confirmar_quitar_ = false;
        std::vector<cliente::Orden> orden;
        orden.emplace_back(protocolo::CambiarContrasenaServidor{std::string(cifrado::texto_de(*pedido.contrasena))});
        ordenar(std::move(orden));
        guardando_contrasena_ = true;
        avisar_hostear(Clave::hostear_guardando, false);
      }
    }
    if (pedido.seguridad && en_vivo && config_recibida_) {
      hostear_.error.clear();
      ordenar({protocolo::CambiarConfigSeguridad{*pedido.seguridad}});
    }
    if (pedido.sacar_admin && en_vivo && *pedido.sacar_admin < hostear_.admins.size()) {
      ordenar({protocolo::SacarAdminGuardado{hostear_.admins[*pedido.sacar_admin].nombre}});
    }
    if (pedido.exportar) {
      despues_del_cuadro_ = [this] { elegir_exportacion(); };
    }
    if (pedido.importar) {
      despues_del_cuadro_ = [this] { elegir_importacion(); };
    }
    if (pedido.cambiar_clave && !carpeta_config_.empty()) {
      pedir_contrasenas(hostear_.dialogo, PideContrasena::cambiar_clave, cliente::pide_arranque(carpeta_config_),
                        cliente::esta_andando(carpeta_config_));
    }
    if (pedido.contrasenas) {
      usar_contrasenas(std::move(*pedido.contrasenas));
    }
  }

  void atender_configuracion(const cliente::Novedad& novedad) {
    if (const auto* nombre = std::get_if<protocolo::NombreServidor>(&novedad)) {
      servidor_.nombre_servidor = nombre->nombre;
      if (hostear_.configurando) {
        avisar_hostear(Clave::hostear_nombre_guardado, false);
      }
    } else if (const auto* registro = std::get_if<protocolo::RegistroDeSeguridad>(&novedad);
               registro != nullptr && hostear_.configurando) {
      hostear_.con_contrasena = registro->con_contrasena;
      hostear_.admins.clear();
      for (const protocolo::AdminGuardado& admin : registro->admins) {
        const auto huella = cifrado::huella_de_publica(admin.clave);
        hostear_.admins.push_back({admin.nombre, huella ? cliente::identidad_corta(*huella) : std::string()});
      }
      hostear_.registro.clear();
      for (const protocolo::AvisoDeSeguridad& entrada : registro->entradas) {
        hostear_.registro.push_back({cliente::fecha_y_hora_local(entrada.hora_ms), cliente::explicar(hostear_.idioma, entrada)});
      }
      if (std::exchange(guardando_contrasena_, false)) {
        avisar_hostear(registro->con_contrasena ? Clave::hostear_contrasena_guardada : Clave::hostear_contrasena_quitada,
                       false);
      }
    } else if (const auto* negado = std::get_if<protocolo::Negado>(&novedad);
               negado != nullptr && hostear_.configurando &&
               (negado->pedido == protocolo::TipoMensaje::cambiar_nombre_servidor ||
                negado->pedido == protocolo::TipoMensaje::cambiar_contrasena_servidor ||
                negado->pedido == protocolo::TipoMensaje::sacar_admin_guardado ||
                negado->pedido == protocolo::TipoMensaje::pedir_registro)) {
      guardando_contrasena_ = false;
      hostear_.aviso.clear();
      hostear_.error = cliente::explicar(hostear_.idioma, negado->motivo);
    }
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
    if (pedido.agregar_llave) {
      empezar_llave();
    }
    if (pedido.cancelar_identidad) {
      if (!llaves_nuevas_.empty() || std::exchange(copia_para_llaves_, false)) {
        soltar_llaves();
      }
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
    cliente::LlavesCargadas llaves = cliente::cargar_mis_llaves(carpeta_);
    if (llaves.llaves) {
      mis_llaves_ = std::make_shared<const cliente::MisLlaves>(std::move(*llaves.llaves));
    }
    ajustes_.llaves_posibles = cliente::autenticadores_disponibles() != cliente::Autenticadores::ninguno;
    cliente::MiIdentidad cargada = cliente::cargar_o_crear_mi_identidad(
        carpeta_, !llaves.danadas && (!mis_llaves_ || mis_llaves_->llaves.empty()));
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
    ui::borrar_contrasenas(pantalla_.dialogo);
    hostear_.dialogo.cual = pantalla_.dialogo.cual = PideContrasena::ninguna;
    en_hostear_ = false;
    cambiar_bloqueo(true);
  }

  bool en_llave() const { return !identidad_ && cerrada_.empty() && mis_llaves_ && !mis_llaves_->llaves.empty(); }

  std::optional<cifrado::Huella> mi_huella() const {
    return identidad_ ? std::optional<cifrado::Huella>(identidad_->huella())
           : mis_llaves_ ? cifrado::huella_de_publica(mis_llaves_->identidad)
                         : std::nullopt;
  }

  std::uint32_t generacion() const { return mis_llaves_ ? mis_llaves_->generacion : 0; }

  void aplicar_identidad() {
    const bool con_llave = en_llave();
    const bool lista = identidad_ != nullptr || con_llave;
    const auto huella = lista ? mi_huella() : std::nullopt;
    ajustes_.estado_identidad = !identidad_cargada_ ? EstadoDeIdentidad::cargando
                                : lista             ? EstadoDeIdentidad::lista
                                : estado_identidad_ == cliente::EstadoIdentidad::danada ? EstadoDeIdentidad::danada
                                                                                        : EstadoDeIdentidad::error;
    ajustes_.identidad = huella ? cliente::identidad_corta(*huella) : std::string();
    ajustes_.copia_hecha =
        con_llave || (huella && cliente::ajuste_texto(guardados_, cliente::Ajuste::copia_de_identidad) == huella->a_texto());
    ajustes_.llaves = con_llave ? mis_llaves_->llaves.size() : 0;
    ajustes_.llaves_nuevas = llaves_nuevas_.size();
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
    } else if (std::exchange(copia_para_llaves_, false)) {
      soltar_llaves();
      cerrar_dialogo_identidad();
    }
  }

  void empezar_llave() {
    if (identidad_ && !copia_ && llaves_nuevas_.size() < cliente::kMaximoLlaves) {
      if (llaves_nuevas_.empty()) {
        generacion_nueva_ = std::max(cifrado::generacion_de_la_hora(cliente::ahora_ms()), generacion());
      }
      if (ajustes_.dialogo_identidad != DialogoIdentidad::llave) {
        abrir_dialogo_identidad(DialogoIdentidad::llave);
      }
      empezar_copia(Tarea::crear_llave, {});
    }
  }

  void terminar_llave(const TrabajoDeCopia& hecho) {
    const auto& creada = hecho.creada.valor;
    const bool repetida = creada && std::any_of(llaves_nuevas_.begin(), llaves_nuevas_.end(), [&](const auto& llave) {
                            return llave.publica == creada->publica;
                          });
    const auto permiso = creada && !repetida && identidad_
                             ? identidad_->firmar(cifrado::lo_que_firma_el_permiso(identidad_->publica(),
                                                                                    creada->publica, hecho.generacion))
                             : std::nullopt;
    if (permiso) {
      llaves_nuevas_.push_back({creada->publica, creada->id, *permiso});
    } else {
      ajustes_.error_identidad =
          texto(ajustes_.idioma, repetida ? Clave::llave_repetida : explicar_webauthn(hecho.creada.error));
    }
    aplicar_identidad();
  }

  void soltar_llaves() {
    llaves_nuevas_.clear();
    avisar_identidad(Clave::llave_no_agregada, true);
    aplicar_identidad();
  }

  void pasar_a_llaves() {
    const auto huella = mi_huella();
    cliente::MisLlaves nuevas{generacion_nueva_, identidad_ ? identidad_->publica() : cifrado::ClavePublica{},
                              llaves_nuevas_};
    if (!identidad_ || !huella || !cliente::guardar_mis_llaves(carpeta_, nuevas) ||
        !cliente::borrar_identidad_de_la_pc(carpeta_, *huella)) {
      avisar_identidad(Clave::seguridad_no_se_guardo, true);
      return;
    }
    mis_llaves_ = std::make_shared<const cliente::MisLlaves>(std::move(nuevas));
    identidad_.reset();
    maestra_.reset();
    cerrada_.clear();
    recordar_maestra();
    avisar_identidad(Clave::llave_lista, false);
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
      case DialogoIdentidad::llave:
        if (!llaves_nuevas_.empty()) {
          copia_para_llaves_ = true;
          despues_del_cuadro_ = [this] { exportar_identidad(); };
        }
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
    trabajo->generacion = tarea == Tarea::crear_llave || copia_para_llaves_ ? generacion_nueva_ : generacion();
    trabajo->nombre = a_utf16("Ventrik " + ajustes_.identidad);
    for (const auto& llave : llaves_nuevas_) {
      trabajo->excluir.push_back(llave.id);
    }
    borrar_contrasenas();
    ajustes_.error_identidad.clear();
    try {
      hilo_de_copia_ = std::thread([trabajo, ventana = ventana_.hwnd()] {
        const std::string_view contrasena = texto_de(trabajo->contrasena);
        switch (trabajo->tarea) {
          case Tarea::exportar:
            trabajo->armada =
                cliente::armar_copia(trabajo->entrada.bytes(), contrasena, std::nullopt, trabajo->generacion);
            break;
          case Tarea::crear_llave:
            trabajo->creada = cliente::crear_llave(ventana, trabajo->nombre, trabajo->excluir);
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
    if (hecho.tarea == Tarea::crear_llave) {
      terminar_llave(hecho);
      return;
    }
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
      if (bien && std::exchange(copia_para_llaves_, false)) {
        pasar_a_llaves();
      } else {
        avisar_identidad(bien ? Clave::seguridad_exportada : Clave::seguridad_no_se_guardo, !bien);
      }
      copia_para_llaves_ = false;
      llaves_nuevas_.clear();
      cerrar_dialogo_identidad();
      aplicar_identidad();
      return;
    }
    generacion_de_copia_ = hecho.abierta.generacion;
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
    if (const auto mia = mi_huella(); mia && hecho.abierta.clave->huella() != *mia) {
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
    const bool misma = mis_llaves_ && mis_llaves_->identidad == identidad_->publica();
    const std::uint32_t anterior = std::max(generacion_de_copia_, misma ? mis_llaves_->generacion : 0);
    cliente::MisLlaves sin_llaves{
        desde_copia ? std::max(cifrado::generacion_de_la_hora(cliente::ahora_ms()),
                               anterior == UINT32_MAX ? anterior : anterior + 1)
                    : 0,
        identidad_->publica(),
        {}};
    const bool anotada = cliente::guardar_mis_llaves(carpeta_, sin_llaves);
    if (anotada) {
      mis_llaves_ = std::make_shared<const cliente::MisLlaves>(std::move(sin_llaves));
    }
    generacion_de_copia_ = 0;
    if (bloqueado_) {
      cerrada_.clear();
      al_desbloquear_ = nullptr;
      cambiar_bloqueo(false);
    }
    cliente::poner_texto(guardados_, cliente::Ajuste::copia_de_identidad,
                         desde_copia ? identidad_->huella().a_texto() : std::string());
    guardar();
    const bool sin_anular = desde_copia && !anotada;
    avisar_identidad(sin_anular ? Clave::llave_sin_anular : aviso, sin_anular,
                     cliente::identidad_corta(identidad_->huella()));
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
    tocando_.erase(std::remove_if(tocando_.begin(), tocando_.end(),
                                  [](const auto& sesion) { return !sesion->tocando_llave(); }),
                   tocando_.end());
    cambio = atender_encargo() || cambio;
    cambio = mirar_mios() || cambio;
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
      for (cliente::Novedad& novedad : sesion_->esperar_novedades(0ms)) {
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
          atender_configuracion(novedad);
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
        if (auto* oferta = std::get_if<protocolo::ConfigurarSegundoPaso>(&novedad)) {
          mbedtls_platform_zeroize(oferta->secreto.data(), oferta->secreto.size());
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
      PedidoHostear pedido = dibujar_hostear(hostear_);
      en_hostear_ = !pedido.cerrar;
      atender_hostear(std::move(pedido));
      if (!en_hostear_) {
        refrescar_mios();
      }
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
        abrir_configuracion();
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
        abrir_asistente();
        break;
      case Accion::segundo_paso: {
        cliente::Presentacion quien{pendiente_.nombre, std::nullopt};
        quien.segundo_paso = pedido.codigo;
        entrar(pendiente_.direccion, cifrado::Huella(pendiente_.huella), std::move(quien),
               pantalla_.intento.desde_formulario);
        break;
      }
      case Accion::iniciar_mio:
      case Accion::configurar_mio:
        if (pedido.cual < mios_.size()) {
          iniciar_mio(mios_[pedido.cual].carpeta, {}, pedido.accion == Accion::configurar_mio);
        }
        break;
      case Accion::detener_mio:
        if (pedido.cual < mios_.size() && cliente::detener_servidor(mios_[pedido.cual].carpeta)) {
          anotar_mio(mios_[pedido.cual].carpeta, texto(pantalla_.idioma, Clave::mio_deteniendo), false);
        }
        break;
      case Accion::ninguna:
        break;
    }
    if (auto contrasenas = std::exchange(pantalla_.contrasenas, std::nullopt)) {
      iniciar_mio(arrancar_pendiente_, std::move(contrasenas->arranque), configurar_pendiente_);
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
    } else if (const auto* oferta = std::get_if<protocolo::ConfigurarSegundoPaso>(&novedad)) {
      mostrar_segundo_paso(servidor_,
                           con_datos(texto(pantalla_.idioma, Clave::segundo_paso_cuenta),
                                     {pendiente_.nombre, servidor_.nombre_servidor}),
                           oferta->secreto);
    } else if (const auto* estado = std::get_if<protocolo::EstadoSegundoPaso>(&novedad)) {
      using protocolo::FaltaSegundoPaso;
      if (estado->falta == FaltaSegundoPaso::nada) {
        ordenar(sala_->volver_a_pedir());
      } else if (estado->falta != FaltaSegundoPaso::activar || estado->todos) {
        mostrar_seguridad(std::string(texto(
            pantalla_.idioma, estado->falta == FaltaSegundoPaso::volver_a_entrar ? Clave::segundo_paso_falta_volver
                              : estado->falta == FaltaSegundoPaso::activar     ? Clave::segundo_paso_falta_activar_todos
                              : estado->todos ? Clave::segundo_paso_falta_permiso_todos
                                              : Clave::segundo_paso_falta_permiso_admins)));
      }
    } else if (const auto* aviso = std::get_if<protocolo::AvisoDeSeguridad>(&novedad)) {
      if (aviso->tipo == protocolo::Seguridad::segundo_paso_configurado) {
        olvidar_segundo_paso(servidor_);
      }
      mostrar_seguridad(cliente::explicar(pantalla_.idioma, *aviso));
      avisos_.seguridad(servidor_.seguridad.back());
      alerta_ = alerta_ || (aviso->tipo != protocolo::Seguridad::nombre_liberado &&
                            aviso->tipo != protocolo::Seguridad::segundo_paso_quitado &&
                            aviso->tipo != protocolo::Seguridad::segundo_paso_configurado);
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
    } else if (negado != nullptr && negado->pedido == protocolo::TipoMensaje::segundo_paso &&
               servidor_.dialogo == Dialogo::segundo_paso) {
      servidor_.error_dialogo = cliente::explicar(pantalla_.idioma, negado->motivo);
    } else if (negado != nullptr && (negado->pedido == protocolo::TipoMensaje::pedir_config_seguridad ||
                                     negado->pedido == protocolo::TipoMensaje::cambiar_config_seguridad)) {
      hostear_.error = cliente::explicar(pantalla_.idioma, negado->motivo);
      if (negado->pedido == protocolo::TipoMensaje::cambiar_config_seguridad) {
        config_recibida_ = false;
        hostear_.cargando = true;
        ordenar({protocolo::PedirConfigSeguridad{}});
      }
    } else if (const auto* config = std::get_if<protocolo::ConfigSeguridad>(&novedad);
               config != nullptr && en_hostear_ && hostear_.configurando) {
      hostear_.seguridad = config->seguridad;
      hostear_.xp = config->xp;
      hostear_.cargando = false;
      config_recibida_ = true;
    }
  }

  void mostrar_seguridad(std::string linea) {
    auto& lista = servidor_.seguridad;
    if (lista.size() >= kTopeAvisosDeSeguridad) {
      lista.erase(lista.begin());
    }
    lista.push_back(std::move(linea));
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
      quien.segundo_paso.reset();
      al_desbloquear_ = [this, direccion, huella, quien, desde_formulario] {
        entrar(direccion, huella, quien, desde_formulario);
      };
      return;
    }
    cerrar();
    intento_con_codigo_ = quien.segundo_paso.has_value();
    quien.identidad = identidad_;
    quien.equipo = equipo_;
    quien.generacion = mis_llaves_ ? mis_llaves_->generacion : 0;
    if (en_llave()) {
      quien.llaves = mis_llaves_;
      quien.tocar_llave = [ventana = ventana_.hwnd()](const cliente::IdDeLlave& id, gsl::span<const std::uint8_t> desafio) {
        auto firmada = cliente::firmar_con_llave(ventana, {id}, desafio);
        return firmada.valor ? std::optional<cliente::LlaveTocada>(
                                   cliente::LlaveTocada{std::move(firmada.valor->datos), std::move(firmada.valor->firma)})
                             : std::nullopt;
      };
    }
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
    if (intento.estado == cliente::EstadoConexion::falta_segundo_paso ||
        intento.estado == cliente::EstadoConexion::segundo_paso_incorrecto ||
        (intento.estado == cliente::EstadoConexion::esperar && intento_con_codigo_)) {
      const bool desde_formulario = pantalla_.intento.desde_formulario;
      std::wstring configurar = std::move(configurar_al_entrar_);
      desconectar();
      configurar_al_entrar_ = std::move(configurar);
      pantalla_.intento.desde_formulario = desde_formulario;
      pantalla_.pedir_segundo_paso = true;
      pantalla_.error_segundo_paso =
          intento.estado == cliente::EstadoConexion::falta_segundo_paso ? std::string()
          : intento.estado == cliente::EstadoConexion::esperar
              ? con_datos(texto(pantalla_.idioma, Clave::conectar_esperar_segundo_paso), {std::to_string(intento.espera)})
              : cliente::explicar(pantalla_.idioma, intento);
      return;
    }
    if (intento.estado != cliente::EstadoConexion::conectado || !intento.presentado) {
      configurar_al_entrar_.clear();
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
    if (!std::exchange(configurar_al_entrar_, std::wstring()).empty() &&
        intento.presentado->rol == protocolo::Rol::dueno) {
      abrir_configuracion();
    }
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
    configurar_al_entrar_.clear();
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
  std::shared_ptr<const cliente::MisLlaves> mis_llaves_;
  std::vector<cliente::LlaveRegistrada> llaves_nuevas_;
  bool copia_para_llaves_ = false;
  std::uint32_t generacion_de_copia_ = 0;
  std::uint32_t generacion_nueva_ = 0;
  std::vector<std::unique_ptr<cliente::Sesion>> tocando_;
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
  bool intento_con_codigo_ = false;
  bool config_recibida_ = false;
  bool midiendo_ = false;
  std::ofstream registro_;
  std::shared_ptr<cliente::PuenteVoz> puente_;
  std::unique_ptr<cliente::MotorVoz> motor_;
  std::unique_ptr<cliente::Audio> audio_;
  std::unique_ptr<cliente::Vigia> vigia_;
  std::unique_ptr<cliente::Sala> sala_;
  std::unique_ptr<cliente::Sesion> sesion_;
  const std::wstring raiz_servidores_ = cliente::carpeta_de_mis_servidores(carpeta_, integrar_);
  std::vector<cliente::MiServidorEnDisco> mios_;
  std::map<std::wstring, std::pair<std::string, bool>> notas_;
  std::optional<EncargoEnCurso> encargo_;
  Reloj::time_point mirar_mios_{};
  std::wstring carpeta_config_;
  std::wstring configurar_al_entrar_;
  std::wstring arrancar_pendiente_;
  bool configurar_pendiente_ = false;
  std::wstring exportar_a_;
  std::wstring importar_de_;
  bool guardando_contrasena_ = false;
  bool confirmar_quitar_ = false;
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
