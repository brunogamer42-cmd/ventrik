#include "client/sesion.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

#include <winsock2.h>

#include <gsl/gsl>

#include "client/voz_primero.h"

namespace ventrik::cliente {
namespace {
using Reloj = std::chrono::steady_clock;
using protocolo::TipoMensaje;

constexpr std::size_t kMaximoNovedades = 4096;
constexpr std::size_t kMaximoSalida = 1024 * 1024;
constexpr int kLecturasPorVuelta = 16;

std::uint64_t milisegundos_desde_arranque() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                        Reloj::now().time_since_epoch())
                                        .count());
}

template <typename Mensaje>
bool agregar(std::vector<std::uint8_t>& salida, const Mensaje& mensaje) {
  return salida.size() < kMaximoSalida && protocolo::armar_mensaje(mensaje, salida);
}

}

std::string explicar(Idioma idioma, const IntentoDeConexion& intento) {
  switch (intento.estado) {
    case EstadoConexion::programa_viejo:
      return con_datos(texto(idioma, Clave::conectar_programa_viejo), {a_texto(intento.version_pedida)});
    case EstadoConexion::sin_respuesta: return std::string(texto(idioma, Clave::conectar_sin_respuesta));
    case EstadoConexion::cifrado_rechazado: return std::string(texto(idioma, Clave::conectar_cifrado_rechazado));
    case EstadoConexion::huella_distinta: return std::string(texto(idioma, Clave::conectar_huella_distinta));
    case EstadoConexion::servidor_viejo: return std::string(texto(idioma, Clave::conectar_servidor_viejo));
    case EstadoConexion::rechazo_ilegible: return std::string(texto(idioma, Clave::conectar_rechazo_ilegible));
    case EstadoConexion::cortada: return std::string(texto(idioma, Clave::conectar_cortada));
    case EstadoConexion::nombre_invalido: return std::string(texto(idioma, Clave::conectar_nombre_invalido));
    case EstadoConexion::nombre_en_uso: return std::string(texto(idioma, Clave::conectar_nombre_en_uso));
    case EstadoConexion::codigo_incorrecto: return std::string(texto(idioma, Clave::conectar_codigo_incorrecto));
    case EstadoConexion::servidor_lleno: return std::string(texto(idioma, Clave::conectar_servidor_lleno));
    case EstadoConexion::firma_rechazada: return std::string(texto(idioma, Clave::conectar_firma_rechazada));
    case EstadoConexion::demasiadas: return std::string(texto(idioma, Clave::conectar_demasiadas));
    case EstadoConexion::falta_contrasena: return std::string(texto(idioma, Clave::conectar_falta_contrasena));
    case EstadoConexion::contrasena_incorrecta:
      return con_datos(texto(idioma, Clave::conectar_contrasena_incorrecta), {std::to_string(intento.espera)});
    case EstadoConexion::esperar:
      return con_datos(texto(idioma, Clave::conectar_esperar), {std::to_string(intento.espera)});
    case EstadoConexion::ocupado: return std::string(texto(idioma, Clave::conectar_ocupado));
    case EstadoConexion::sin_identidad: return std::string(texto(idioma, Clave::conectar_sin_identidad));
    case EstadoConexion::falta_segundo_paso: return std::string(texto(idioma, Clave::conectar_falta_segundo_paso));
    case EstadoConexion::segundo_paso_incorrecto:
      return con_datos(texto(idioma, Clave::conectar_segundo_paso_incorrecto), {std::to_string(intento.espera)});
    case EstadoConexion::segundo_paso_bloqueado:
      return std::string(texto(idioma, Clave::conectar_segundo_paso_bloqueado));
    case EstadoConexion::segundo_paso_usado:
      return con_datos(texto(idioma, Clave::conectar_segundo_paso_usado), {std::to_string(intento.espera)});
    case EstadoConexion::sin_equipo: return std::string(texto(idioma, Clave::conectar_sin_equipo));
    case EstadoConexion::llave_anulada: return std::string(texto(idioma, Clave::conectar_llave_anulada));
    case EstadoConexion::llave_clonada: return std::string(texto(idioma, Clave::conectar_llave_clonada));
    case EstadoConexion::sin_llave: return std::string(texto(idioma, Clave::conectar_sin_llave));
    default: return std::string(texto(idioma, Clave::conectar_error_interno));
  }
}

std::string explicar(Idioma idioma, const Fin& fin) {
  return std::string(texto(idioma, fin.motivo == MotivoFin::sin_respuesta     ? Clave::fin_sin_respuesta
                                   : fin.motivo == MotivoFin::mensaje_ilegible ? Clave::fin_ilegible
                                   : fin.motivo == MotivoFin::desbordada       ? Clave::fin_desbordada
                                   : fin.motivo == MotivoFin::desplazada       ? Clave::fin_desplazada
                                                                               : Clave::fin_cortado));
}

std::string explicar(Idioma idioma, protocolo::MotivoNegado motivo) {
  constexpr std::array<Clave, 10> kNegados = {Clave::negado_sin_permiso,       Clave::negado_no_existe,
                                              Clave::negado_texto_invalido,    Clave::negado_demasiado_rapido,
                                              Clave::negado_lleno,             Clave::negado_nombre_en_uso,
                                              Clave::negado_tipo_de_canal,     Clave::negado_nombre_libre,
                                              Clave::negado_codigo_incorrecto, Clave::negado_falta_segundo_paso};
  const auto indice = static_cast<std::size_t>(motivo) - 1;
  return std::string(texto(idioma, indice < kNegados.size() ? kNegados[indice] : Clave::negado_sin_permiso));
}

Sesion::Sesion(const red::Direccion& destino, const cifrado::Huella& huella, Presentacion quien,
               OpcionesSesion opciones)
    : destino_(destino),
      huella_(huella),
      quien_(std::move(quien)),
      opciones_(std::move(opciones)),
      hilo_([this] { correr(); }) {}

Sesion::~Sesion() {
  abandonar();
  hilo_.join();
}

void Sesion::abandonar() {
  {
    const std::lock_guard<std::mutex> candado(candado_);
    terminar_ = true;
  }
  despertador_.despertar();
  cancelar_.despertar();
}

void Sesion::ordenar(Orden orden) {
  {
    const std::lock_guard<std::mutex> candado(candado_);
    ordenes_.push_back(std::move(orden));
  }
  despertador_.despertar();
}

void Sesion::ordenar(std::vector<Orden> ordenes) {
  if (ordenes.empty()) {
    return;
  }
  {
    const std::lock_guard<std::mutex> candado(candado_);
    std::move(ordenes.begin(), ordenes.end(), std::back_inserter(ordenes_));
  }
  despertador_.despertar();
}

void Sesion::ahorrar(bool si) {
  if (ahorrando_.exchange(si) != si) {
    despertador_.despertar();
  }
}

void Sesion::voz(std::shared_ptr<PuenteVoz> puente) {
  {
    const std::lock_guard<std::mutex> candado(candado_);
    voz_pedida_ = std::move(puente);
  }
  despertador_.despertar();
}

void Sesion::cambiar_voz() {
  std::optional<std::shared_ptr<PuenteVoz>> pedida;
  {
    const std::lock_guard<std::mutex> candado(candado_);
    pedida.swap(voz_pedida_);
  }
  if (!pedida) {
    return;
  }
  canal_voz_.reset();
  puente_ = std::move(*pedida);
  if (puente_ && puente_->despertador.ok()) {
    canal_voz_ = std::make_unique<CanalVoz>(destino_, *puente_, opciones_.conexion.origen);
    usar_clave();
  }
}

void Sesion::usar_clave() {
  if (clave_ && puente_) {
    puente_->voz_fija = clave_->voz_fija;
  }
  cifrado::Secreto secreto(32);
  if (clave_ && canal_voz_ && tls_->exportar(clave_->identidad, secreto.bytes())) {
    canal_voz_->clave(*clave_, std::move(secreto));
    puente_->claves.fetch_add(1, std::memory_order_relaxed);
  }
}

std::vector<Novedad> Sesion::esperar_novedades(std::chrono::milliseconds tiempo) {
  std::unique_lock<std::mutex> candado(candado_);
  hay_novedades_.wait_for(candado, tiempo, [this] { return !novedades_.empty(); });
  std::vector<Novedad> listas(std::make_move_iterator(novedades_.begin()),
                              std::make_move_iterator(novedades_.end()));
  novedades_.clear();
  return listas;
}

bool Sesion::publicar(Novedad novedad) {
  {
    const std::lock_guard<std::mutex> candado(candado_);
    if (novedades_.size() >= kMaximoNovedades && !std::holds_alternative<Fin>(novedad)) {
      desbordada_ = true;
      return false;
    }
    novedades_.push_back(std::move(novedad));
  }
  hay_novedades_.notify_all();
  if (opciones_.al_haber_novedades) {
    opciones_.al_haber_novedades();
  }
  return true;
}

std::vector<Orden> Sesion::tomar_ordenes() {
  const std::lock_guard<std::mutex> candado(candado_);
  std::vector<Orden> tomadas(std::make_move_iterator(ordenes_.begin()),
                             std::make_move_iterator(ordenes_.end()));
  ordenes_.clear();
  if (terminar_) {
    tomadas.clear();
  }
  return tomadas;
}

bool Sesion::procesar(const protocolo::Marco& marco) {
  const auto publicar_si = [this, &marco](auto mensaje) {
    using Mensaje = decltype(mensaje);
    auto leido = protocolo::leer_mensaje<Mensaje>(marco.cuerpo);
    if (!leido) {
      return false;
    }
    return publicar(std::move(*leido));
  };
  switch (static_cast<TipoMensaje>(marco.tipo)) {
    case TipoMensaje::usuario_entro:
      return publicar_si(protocolo::UsuarioEntro{});
    case TipoMensaje::usuario_salio:
      return publicar_si(protocolo::UsuarioSalio{});
    case TipoMensaje::usuario_cambio_canal:
      return publicar_si(protocolo::UsuarioCambioCanal{});
    case TipoMensaje::usuario_silencio:
      return publicar_si(protocolo::UsuarioSilencio{});
    case TipoMensaje::canal_creado:
      return publicar_si(protocolo::CanalCreado{});
    case TipoMensaje::canal_cambiado:
      return publicar_si(protocolo::CanalCambiado{});
    case TipoMensaje::canal_borrado:
      return publicar_si(protocolo::CanalBorrado{});
    case TipoMensaje::mensaje_chat:
      return publicar_si(protocolo::MensajeChat{});
    case TipoMensaje::mensajes_nuevos:
      return publicar_si(protocolo::MensajesNuevos{});
    case TipoMensaje::historial:
      return publicar_si(protocolo::Historial{});
    case TipoMensaje::negado:
      return publicar_si(protocolo::Negado{});
    case TipoMensaje::usuario_rol:
      return publicar_si(protocolo::UsuarioRol{});
    case TipoMensaje::aviso_de_seguridad:
      return publicar_si(protocolo::AvisoDeSeguridad{});
    case TipoMensaje::configurar_segundo_paso:
      return publicar_si(protocolo::ConfigurarSegundoPaso{});
    case TipoMensaje::config_seguridad:
      return publicar_si(protocolo::ConfigSeguridad{});
    case TipoMensaje::estado_segundo_paso:
      return publicar_si(protocolo::EstadoSegundoPaso{});
    case TipoMensaje::nombre_servidor:
      return publicar_si(protocolo::NombreServidor{});
    case TipoMensaje::registro_de_seguridad:
      return publicar_si(protocolo::RegistroDeSeguridad{});
    case TipoMensaje::desplazado:
      desplazada_ = protocolo::leer_mensaje<protocolo::Desplazado>(marco.cuerpo).has_value();
      return false;
    case TipoMensaje::clave_voz:
      clave_ = protocolo::leer_mensaje<protocolo::ClaveVoz>(marco.cuerpo);
      usar_clave();
      return clave_.has_value();
    case TipoMensaje::pong: {
      const auto pong = protocolo::leer_mensaje<protocolo::Pong>(marco.cuerpo);
      const std::uint64_t ahora = milisegundos_desde_arranque();
      if (!pong || pong->marca > ahora) {
        return false;
      }
      return publicar(PingMedido{static_cast<std::uint32_t>(
          std::min<std::uint64_t>(ahora - pong->marca, 0xFFFFFFFFu))});
    }
    default:
      return false;
  }
}

void Sesion::correr() {
  if (!despertador_.ok() || !cancelar_.ok()) {
    publicar(IntentoDeConexion{});
    return;
  }
  OpcionesConexion conexion = opciones_.conexion;
  conexion.cancelar = &cancelar_.socket();
  if (quien_.tocar_llave) {
    quien_.tocar_llave = [this, tocar = std::move(quien_.tocar_llave)](const std::vector<std::uint8_t>& id,
                                                                       gsl::span<const std::uint8_t> desafio) {
      {
        const std::lock_guard<std::mutex> candado(candado_);
        if (terminar_) {
          return std::optional<LlaveTocada>();
        }
        tocando_ = true;
      }
      auto tocada = tocar(id, desafio);
      const std::lock_guard<std::mutex> candado(candado_);
      tocando_ = false;
      return terminar_ ? std::nullopt : tocada;
    };
  }
  auto resultado = conectar(destino_, huella_, quien_, conexion);
  quien_.contrasena.reset();
  quien_.identidad.reset();
  quien_.llaves.reset();
  quien_.tocar_llave = nullptr;
  if (resultado.cambio) {
    huella_ = resultado.cambio->nueva;
  }
  publicar(IntentoDeConexion{resultado.estado, resultado.version_pedida, resultado.cifrado,
                             resultado.presentado, resultado.espera, resultado.cambio, resultado.original});
  if (resultado.estado != EstadoConexion::conectado || !resultado.abierta || !resultado.abierta->confirmada()) {
    return;
  }
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
  no_frenar_este_hilo();
  const auto cerrar_voz = gsl::finally([this] {
    canal_voz_.reset();
    if (puente_) {
      puente_->cambiar(puente_->estado, EstadoVoz::sin_conexion);
    }
  });
  ConexionAbierta& abierta = *resultado.abierta;
  cifrado::SesionTls& tls = abierta.tls();
  tls_ = &tls;
  std::vector<std::uint8_t> salida;
  auto ultimo_ping = Reloj::now() - opciones_.cada_cuanto_ping;
  auto ultimo_dato = Reloj::now();

  const auto procesar_bytes = [&](gsl::span<std::uint8_t> llegados) {
    gsl::span<const std::uint8_t> datos = llegados;
    bool bien = true;
    while (auto marco = bien ? abierta.ensamblador().siguiente(datos) : std::nullopt) {
      bien = procesar(*marco);
      if (marco->tipo == static_cast<std::uint16_t>(TipoMensaje::configurar_segundo_paso)) {
        cifrado::borrar(marco->cuerpo);
      }
    }
    cifrado::borrar(llegados);
    return bien && !abierta.ensamblador().error();
  };
  if (!procesar_bytes(abierta.sobrante())) {
    publicar(Fin{desplazada_ ? MotivoFin::desplazada : desbordada_ ? MotivoFin::desbordada : MotivoFin::mensaje_ilegible});
    return;
  }

  for (;;) {
    for (Orden& orden : tomar_ordenes()) {
      std::visit([&salida](const auto& mensaje) { agregar(salida, mensaje); }, orden);
      if (auto* cambio = std::get_if<protocolo::CambiarContrasenaServidor>(&orden); cambio && !cambio->contrasena.empty()) {
        mbedtls_platform_zeroize(&cambio->contrasena[0], cambio->contrasena.size());
      }
    }
    cambiar_voz();
    {
      const std::lock_guard<std::mutex> candado(candado_);
      if (terminar_) {
        return;
      }
    }
    const bool ahorrando = ahorrando_;
    const auto cada_ping = ahorrando ? opciones_.cada_cuanto_ping_ahorrando : opciones_.cada_cuanto_ping;
    const auto ahora = Reloj::now();
    if (ahora >= ultimo_ping + cada_ping) {
      agregar(salida, protocolo::Ping{milisegundos_desde_arranque()});
      ultimo_ping = ahora;
    }

    bool quiere_escribir = false;
    std::size_t enviado = 0;
    while (enviado < salida.size()) {
      std::size_t escritos = 0;
      const auto paso =
          tls.escribir(gsl::span<const std::uint8_t>(salida).subspan(enviado), escritos);
      if (paso == cifrado::Paso::listo) {
        enviado += escritos;
        continue;
      }
      if (paso == cifrado::Paso::esperar_escritura) {
        quiere_escribir = true;
        break;
      }
      if (paso != cifrado::Paso::esperar_lectura) {
        publicar(Fin{MotivoFin::el_servidor_corto});
        return;
      }
      break;
    }
    salida.erase(salida.begin(), salida.begin() + static_cast<std::ptrdiff_t>(enviado));

    std::array<std::uint8_t, 4096> recibido{};
    for (int lectura = 0; lectura < kLecturasPorVuelta; ++lectura) {
      std::size_t leidos = 0;
      const auto paso = tls.leer(recibido, leidos);
      if (paso == cifrado::Paso::listo) {
        ultimo_dato = Reloj::now();
        if (!procesar_bytes(gsl::span<std::uint8_t>(recibido).first(leidos))) {
          publicar(Fin{desplazada_ ? MotivoFin::desplazada : desbordada_ ? MotivoFin::desbordada : MotivoFin::mensaje_ilegible});
          return;
        }
        continue;
      }
      if (paso == cifrado::Paso::esperar_escritura) {
        quiere_escribir = true;
      } else if (paso != cifrado::Paso::esperar_lectura) {
        publicar(Fin{MotivoFin::el_servidor_corto});
        return;
      }
      break;
    }
    if (Reloj::now() - ultimo_dato > opciones_.sin_respuesta) {
      publicar(Fin{MotivoFin::sin_respuesta});
      return;
    }
    if (canal_voz_) {
      canal_voz_->ahorrar(ahorrando);
      canal_voz_->atender(Reloj::now());
    }

    fd_set lectura;
    fd_set escritura;
    FD_ZERO(&lectura);
    FD_ZERO(&escritura);
    const auto canal = static_cast<SOCKET>(abierta.canal().get());
    FD_SET(canal, &lectura);
    if (quiere_escribir) {
      FD_SET(canal, &escritura);
    }
    FD_SET(static_cast<SOCKET>(despertador_.socket().get()), &lectura);
    if (canal_voz_) {
      FD_SET(static_cast<SOCKET>(puente_->despertador.socket().get()), &lectura);
      if (canal_voz_->socket().valido()) {
        FD_SET(static_cast<SOCKET>(canal_voz_->socket().get()), &lectura);
      }
    }
    const auto hasta_el_ping = std::chrono::duration_cast<std::chrono::milliseconds>(
        ultimo_ping + cada_ping - Reloj::now());
    const auto hasta_la_voz = canal_voz_ ? canal_voz_->hasta_lo_proximo(Reloj::now()) : hasta_el_ping;
    const auto espera = tls.hay_datos_guardados()
                            ? std::chrono::milliseconds{0}
                            : std::clamp(std::min(hasta_el_ping, hasta_la_voz), std::chrono::milliseconds{0},
                                         cada_ping);
    timeval tiempo = {};
    tiempo.tv_sec = static_cast<long>(espera.count() / 1000);
    tiempo.tv_usec = static_cast<long>((espera.count() % 1000) * 1000);
    const int listos = select(0, &lectura, &escritura, nullptr, &tiempo);
    despertares_.fetch_add(1, std::memory_order_relaxed);
    if (listos < 0) {
      Sleep(10);
    }
    if (listos > 0) {
      despertador_.vaciar();
      if (canal_voz_) {
        puente_->despertador.vaciar();
      }
    }
  }
}

}
