#include "client/ui/programa.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "client/audio.h"
#include "client/diagnostico.h"
#include "client/guardados.h"
#include "client/horas.h"
#include "client/motor_voz.h"
#include "client/sala.h"
#include "client/sesion.h"
#include "client/vigia.h"
#include "client/ui/inicio.h"
#include "client/ui/servidor.h"
#include "client/ui/ventana.h"
#include "common/cifrado/inicio.h"
#include "common/texto.h"
#include "common/utf.h"

namespace ventrik::ui {
namespace {
using namespace std::chrono_literals;
using Reloj = std::chrono::steady_clock;

template <typename... Valores>
void anotar(std::ostream& salida, const Valores&... valores) {
  const char* separador = "";
  ((salida << separador << valores, separador = ","), ...);
  salida << std::endl;
}

class Programa {
 public:
  Programa(Idioma idioma, Ventana& ventana, const std::vector<std::string>& entrada) : ventana_(ventana) {
    pantalla_.idioma = idioma;
    pantalla_.logo = ventana.logo();
    const cliente::LecturaGuardados lectura = cliente::cargar_guardados(guardados_);
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

  ~Programa() { cerrar(); }

  void registrar(const std::wstring& ruta) {
    registro_.open(ruta.c_str(), std::ios::trunc);
    if (!registro_) {
      MessageBoxW(ventana_.hwnd(),
                  a_utf16(con_datos(texto(pantalla_.idioma, Clave::registro_sin_archivo), {a_utf8(ruta)})).c_str(),
                  a_utf16(texto(pantalla_.idioma, Clave::titulo_ventana)).c_str(), MB_OK | MB_ICONWARNING);
      return;
    }
    registro_ << std::fixed << std::setprecision(1) << "# placa=" << ventana_.placa()
              << ";vertices_por_procesador=" << ventana_.por_procesador() << ";frecuencia=" << calidad_.frecuencia
              << ";complejidad=" << calidad_.complejidad << '\n'
              << "ms,ping_ms,ping_voz_ms,cpu_total,cpu_ventana,cpu_sistema,ocupado_ms,pasada_max_ms,imgui_ms,"
                 "direct3d_ms,presentados,vertices_max,cuadros,colchon_ms,recortes_total,perdidos,recibidos,salida_ms,"
                 "salida_max_ms,descartes_salida_total,descartes_con_voz_total,vacios_salida_total,"
                 "vaciados_colchon_total,faltas_colchon_total,transmitiendo,hablando,voz_lista,avisos,mensajes,"
                 "mensaje_comun,ram_mb"
              << std::endl;
  }

  void cerrar() {
    if (auto* guardado = sala_ ? cliente::buscar_guardado(guardados_, pendiente_.direccion) : nullptr) {
      guardado->leidos = sala_->leidos();
      guardar();
    }
    sesion_.reset();
    sala_.reset();
    vigia_.reset();
    audio_.reset();
    motor_.reset();
    puente_.reset();
  }

  void atender() {
    avisado_ = false;
    if (sesion_) {
      for (const cliente::Novedad& novedad : sesion_->esperar_novedades(0ms)) {
        if (const auto* intento = std::get_if<cliente::IntentoDeConexion>(&novedad)) {
          conectado(*intento);
        } else if (const auto* fin = std::get_if<cliente::Fin>(&novedad)) {
          terminar(cliente::explicar(pantalla_.idioma, *fin));
        } else if (sala_) {
          ordenar(sala_->aplicar(novedad));
        }
      }
    }
    if (sala_) {
      mirar_voz();
    }
  }

  void cuadro() {
    ++cuadros_;
    if (sala_) {
      PedidoServidor pedido = dibujar_servidor(servidor_, *sala_);
      ordenar(std::move(pedido.ordenes));
      if (pedido.elegir_tecla) {
        vigia_->elegir_tecla();
        servidor_.eligiendo_tecla = true;
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
      case Accion::entrar_a_guardado: {
        const cliente::ServidorGuardado& servidor = guardados_.servidores[pedido.cual];
        entrar(servidor.direccion, cifrado::Huella(servidor.huella), {servidor.nombre, std::nullopt}, false);
        break;
      }
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

  void ordenar(std::vector<cliente::Orden> ordenes) {
    for (auto& orden : ordenes) {
      sesion_->ordenar(std::move(orden));
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

  void entrar(const red::Direccion& direccion, const cifrado::Huella& huella, cliente::Presentacion quien,
              bool desde_formulario) {
    cerrar();
    pantalla_.intento = {direccion, desde_formulario, Fase::conectando, {}};
    pendiente_ = {direccion, huella.bytes(), quien.nombre, 0, {}, 0};
    puente_ = std::make_shared<cliente::PuenteVoz>();
    motor_ = std::make_unique<cliente::MotorVoz>(*puente_, calidad_);
    audio_ = std::make_unique<cliente::Audio>();
    hay_audio_ = motor_->ok() && audio_->abrir(motor_->frecuencia(), cliente::MotorVoz::procesar_audio, motor_.get());
    cliente::OpcionesSesion opciones;
    opciones.voz = puente_;
    opciones.al_haber_novedades = [this] { avisar(); };
    sesion_ = std::make_unique<cliente::Sesion>(direccion, huella, std::move(quien), std::move(opciones));
  }

  void conectado(const cliente::IntentoDeConexion& intento) {
    if (intento.estado != cliente::EstadoConexion::conectado || !intento.presentado) {
      terminar(cliente::explicar(pantalla_.idioma, intento));
      return;
    }
    const auto* guardado = cliente::buscar_guardado(guardados_, pendiente_.direccion);
    sala_ = std::make_unique<cliente::Sala>(*intento.presentado, guardado != nullptr && guardado->huella == pendiente_.huella
                                                                     ? guardado->leidos
                                                                     : std::vector<cliente::Leido>{});
    const auto* canal = sala_->canal(sala_->tu_canal());
    pendiente_.canal = sala_->tu_canal();
    pendiente_.nombre_canal = canal != nullptr ? canal->nombre : std::string();
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
    servidor_.hay_audio = hay_audio_;
    servidor_.tecla = guardados_.tecla_hablar;
    servidor_.enfocar_mensaje = true;
    vigia_ = std::make_unique<cliente::Vigia>(*motor_, *puente_, cliente::tecla_para_hablar(guardados_.tecla_hablar),
                                              ventana_.hwnd(), [this] { avisar(); });
    perdidos_ = puente_->perdidos;
    recibidos_ = puente_->recibidos;
    proxima_muestra_ = Reloj::now() + 1s;
    acumulada_ = {};
    segundos_ = 0;
    diagnostico_ = cliente::Diagnostico();
    cuadros_ = 0;
    ventana_.tomar_uso();
    ordenar(sala_->empezar());
  }

  void mirar_voz() {
    servidor_.transmitiendo = motor_->transmitiendo();
    servidor_.hablando = puente_->hablando;
    servidor_.nivel = motor_->nivel(cliente::kSegmentosDelMedidor);
    if (const auto elegida = vigia_->tomar_elegida()) {
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
      return;
    }
    proxima_muestra_ = ahora + 1s;
    const std::uint64_t perdidos = puente_->perdidos;
    const std::uint64_t recibidos = puente_->recibidos;
    const Muestra muestra{sala_->ping().value_or(0), static_cast<std::uint32_t>(perdidos - perdidos_),
                          static_cast<std::uint32_t>(recibidos - recibidos_)};
    perdidos_ = perdidos;
    recibidos_ = recibidos;
    acumulada_ = {muestra.ping, acumulada_.perdidos + muestra.perdidos, acumulada_.recibidos + muestra.recibidos};
    if (++segundos_ % kSegundosPorMuestra == 0) {
      servidor_.grafico.push_back(std::exchange(acumulada_, {}));
    }
    const cliente::Medicion medicion = diagnostico_.medir();
    servidor_.diagnostico = {puente_->estado == cliente::EstadoVoz::lista, puente_->ping_ms, medicion.procesador,
                             medicion.ram_bytes, motor_->espera_ms(), motor_->recortes(), audio_->espera_de_salida_ms()};
    if (registro_.is_open()) {
      const DatosDiagnostico& datos = servidor_.diagnostico;
      UsoVentana uso = ventana_.tomar_uso();
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
             audio_->tomar_espera_maxima_ms(), audio_->descartados(), audio_->descartados_con_voz(), audio_->vacios(),
             motor_->vaciados(), motor_->faltas(), servidor_.transmitiendo, servidor_.hablando, datos.voz_lista, avisos,
             otros, comun.first, medicion.ram_bytes / (1024 * 1024));
    }
    cuadros_ = 0;
    if (servidor_.grafico.size() > kMuestrasDelGrafico) {
      servidor_.grafico.pop_front();
    }
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
      pantalla_.aviso = cliente::guardar_guardados(guardados_)
                            ? std::string()
                            : std::string(texto(pantalla_.idioma, Clave::inicio_no_se_guardo));
    }
  }

  Ventana& ventana_;
  const cliente::Calidad calidad_ = cliente::calidad_automatica();
  std::atomic<bool> avisado_{false};
  PantallaInicio pantalla_;
  PantallaServidor servidor_;
  cliente::Guardados guardados_;
  bool se_puede_guardar_ = false;
  bool hay_audio_ = false;
  cliente::ServidorGuardado pendiente_;
  std::uint64_t perdidos_ = 0;
  std::uint64_t recibidos_ = 0;
  Reloj::time_point proxima_muestra_{};
  Muestra acumulada_{};
  std::uint32_t segundos_ = 0;
  cliente::Diagnostico diagnostico_;
  const Reloj::time_point inicio_ = Reloj::now();
  std::uint32_t cuadros_ = 0;
  std::ofstream registro_;
  std::shared_ptr<cliente::PuenteVoz> puente_;
  std::unique_ptr<cliente::MotorVoz> motor_;
  std::unique_ptr<cliente::Audio> audio_;
  std::unique_ptr<cliente::Vigia> vigia_;
  std::unique_ptr<cliente::Sala> sala_;
  std::unique_ptr<cliente::Sesion> sesion_;
};

}

int abrir_programa(Idioma idioma, const std::wstring& titulo, int como_mostrar,
                   const std::vector<std::string>& entrar, const std::wstring& registro) {
  const cifrado::Inicio cifrado;
  const red::Inicio red;
  Ventana ventana(titulo, como_mostrar);
  if (!ventana.ok()) {
    MessageBoxW(nullptr, a_utf16(texto(idioma, Clave::aviso_sin_direct3d)).c_str(), titulo.c_str(),
                MB_OK | MB_ICONERROR);
    return 3;
  }
  Programa programa(idioma, ventana, entrar);
  if (!registro.empty()) {
    programa.registrar(registro);
  }
  ventana.al_terminar_la_sesion([&programa] { programa.cerrar(); });
  return ventana.correr([&programa] { programa.atender(); }, [&programa] { programa.cuadro(); });
}

}
