#include "client/ui/programa.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

#include "client/audio.h"
#include "client/avisos.h"
#include "client/diagnostico.h"
#include "client/guardados.h"
#include "client/horas.h"
#include "client/motor_voz.h"
#include "client/sala.h"
#include "client/sesion.h"
#include "client/vigia.h"
#include "client/ui/aviso_ventana.h"
#include "client/ui/inicio.h"
#include "client/ui/servidor.h"
#include "client/ui/ventana.h"
#include "common/cifrado/inicio.h"
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
    pantalla_.logo = ventana.logo();
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
    if (entrada.size() == 3 || entrada.size() == 4) {
      poner(pantalla_.direccion, entrada[0]);
      poner(pantalla_.huella, entrada[1]);
      poner(pantalla_.nombre, entrada[2]);
      poner(pantalla_.codigo, entrada.size() == 4 ? entrada[3] : std::string());
      entrar_con_formulario();
    }
  }

  ~Programa() {
    cerrar();
    soltar_clics_de_windows();
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
                 "bloqueada,ahorro,atlas_kb,silencio,notificaciones,objetivo_ms,acelerado_ms,estirado_ms,tardes"
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
    sesion_.reset();
    sala_.reset();
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
    audio_ = std::make_unique<cliente::Audio>();
    servidor_.hay_audio =
        motor_->ok() && audio_->abrir(motor_->frecuencia(), cliente::MotorVoz::procesar_audio, motor_.get());
    vigia_ = std::make_unique<cliente::Vigia>(*motor_, *puente_, cliente::tecla_para_hablar(guardados_.tecla_hablar),
                                              ventana_.hwnd(), [this] { avisar(); });
    vigia_->por_voz(guardados_.por_voz);
    despertares_audio_ = audio_->despertares();
    abrir_desde_ = Reloj::now() + kEntreAperturasDeVoz;
    sesion_->voz(puente_);
  }

  void aplicar_audio() {
    if (motor_) {
      motor_->silenciar(guardados_.silencio != 0, (guardados_.silencio & protocolo::kSilencioAudio) != 0);
      motor_->volumenes(guardados_.volumen_microfono, guardados_.volumen_salida);
    }
  }

  void cerrar_voz() {
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
    const Situacion& situacion = ventana_.situacion();
    avisos_.fuera(!situacion.con_foco || situacion.minimizada);
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
          ordenar(sala_->aplicar(novedad));
          avisar_de(novedad);
          entraron_o_salieron = entraron_o_salieron || std::holds_alternative<protocolo::UsuarioEntro>(novedad) ||
                                std::holds_alternative<protocolo::UsuarioSalio>(novedad);
          if (const auto* rechazo = std::get_if<protocolo::Negado>(&novedad);
              rechazo != nullptr && rechazo->pedido == protocolo::TipoMensaje::silenciar_personas) {
            silenciados_enviados_ = {0};
          }
        }
      }
    }
    if (sala_ && entraron_o_salieron) {
      aplicar_personas();
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
    const bool ahorrar = situacion.minimizada || situacion.juego || situacion.bloqueada;
    if (sesion_ && ahorrar != ahorrando_) {
      ahorrando_ = ahorrar;
      sesion_->ahorrar(ahorrar);
    }
    return (sala_ && mirar_voz()) || cambio;
  }

  void cuadro() {
    ++cuadros_;
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
      if (pedido.por_voz || pedido.silencio || pedido.guardar) {
        guardar();
      }
      if (pedido.desconectar) {
        desconectar();
      }
      return;
    }
    const Pedido pedido = dibujar_inicio(pantalla_, guardados_);
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
      }
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
    entrar(*direccion, *huella, std::move(quien), true);
  }

  void entrar_a(const cliente::ServidorGuardado& servidor) {
    entrar(servidor.direccion, cifrado::Huella(servidor.huella), {servidor.nombre, std::nullopt}, false);
  }

  void entrar(const red::Direccion& direccion, const cifrado::Huella& huella, cliente::Presentacion quien,
              bool desde_formulario) {
    cerrar();
    canal_del_aviso_ = 0;
    pantalla_.intento = {direccion, desde_formulario, Fase::conectando, {}};
    pendiente_ = {direccion, huella.bytes(), quien.nombre, 0, {}, 0};
    cliente::OpcionesSesion opciones;
    opciones.al_haber_novedades = [this] { avisar(); };
    sesion_ = std::make_unique<cliente::Sesion>(direccion, huella, std::move(quien), std::move(opciones));
    ahorrando_ = false;
  }

  void conectado(const cliente::IntentoDeConexion& intento) {
    if (intento.estado != cliente::EstadoConexion::conectado || !intento.presentado) {
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
    servidor_.calidad_alta = calidad_.frecuencia == cliente::Calidad{}.frecuencia;
    servidor_.tecla = guardados_.tecla_hablar;
    servidor_.por_voz = guardados_.por_voz;
    servidor_.sensibilidad = guardados_.sensibilidad;
    servidor_.silencio = guardados_.silencio;
    servidor_.microfono_antes = guardados_.silencio != 0;
    servidor_.volumen_microfono = guardados_.volumen_microfono;
    servidor_.volumen_salida = guardados_.volumen_salida;
    servidor_.enfocar_mensaje = true;
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
    if (guardados_.silencio != 0) {
      ordenar({protocolo::Silenciar{guardados_.silencio}});
    }
    silenciados_enviados_.clear();
    aplicar_personas();
  }

  bool mirar_voz() {
    const std::uint32_t canal = sala_->tu_canal();
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
    const auto antes = std::make_tuple(servidor_.transmitiendo, servidor_.hablando, servidor_.nivel);
    servidor_.transmitiendo = motor_ && motor_->transmitiendo();
    servidor_.hablando = puente_ ? puente_->quienes() : cliente::Hablando{};
    servidor_.nivel = vigia_ ? vigia_->nivel() : 0;
    const Situacion& situacion = ventana_.situacion();
    if (const bool medir = vigia_ && servidor_.por_voz && servidor_.ajustando && situacion.con_foco && situacion.dibuja();
        medir != midiendo_) {
      midiendo_ = medir;
      vigia_->medir(medir);
    }
    bool cambio = cambio_de_voz || antes != std::make_tuple(servidor_.transmitiendo, servidor_.hablando, servidor_.nivel);
    if (const auto elegida = vigia_ ? vigia_->tomar_elegida() : std::nullopt) {
      cambio = true;
      servidor_.eligiendo_tecla = false;
      if (*elegida != 0) {
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
    const bool tenia_poca_memoria = std::exchange(poca_memoria_, queda_poca_memoria());
    if (poca_memoria_ && !tenia_poca_memoria) {
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
             voz ? motor_->acelerado_ms() : 0u, voz ? motor_->estirado_ms() : 0u, voz ? motor_->tardes() : 0u);
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
    if (se_puede_guardar_) {
      pantalla_.aviso = cliente::guardar_guardados(guardados_, carpeta_)
                            ? std::string()
                            : std::string(texto(pantalla_.idioma, Clave::inicio_no_se_guardo));
    }
  }

  Ventana& ventana_;
  const std::wstring carpeta_;
  const bool integrar_;
  cliente::Avisos avisos_;
  std::unique_ptr<Notificador> notificador_;
  std::uint32_t canal_del_aviso_ = 0;
  std::vector<std::uint32_t> silenciados_enviados_;
  std::uint64_t version_avisos_ = 0;
  bool juego_avisos_ = false;
  const cliente::Calidad calidad_ = cliente::calidad_automatica();
  std::atomic<bool> avisado_{false};
  PantallaInicio pantalla_;
  PantallaServidor servidor_;
  cliente::Guardados guardados_;
  bool se_puede_guardar_ = false;
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
