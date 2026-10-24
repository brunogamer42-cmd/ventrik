#include "server/servidor.h"

#include <algorithm>
#include <array>
#include <future>
#include <optional>
#include <utility>
#include <variant>

#include <winsock2.h>

#include "common/texto.h"

namespace ventrik::servidor {
namespace {
using Reloj = std::chrono::steady_clock;
using protocolo::MotivoNegado;
using protocolo::TipoCanal;
using protocolo::TipoMensaje;

constexpr std::chrono::milliseconds kEsperaMaxima{500};
constexpr std::chrono::milliseconds kPausaSiFallaAceptar{100};
constexpr std::size_t kAceptadasPorVuelta = 64;
constexpr std::size_t kMaximoAntesDelSaludo = 16 * 1024;
constexpr int kLecturasPorVuelta = 16;
constexpr std::size_t kDatagramasPorVuelta = 256;
constexpr std::chrono::milliseconds kCadaCuantoRevisarVoz{100};
constexpr std::size_t kMaximoHistorial = protocolo::kMaximoCuerpo - 64;
constexpr std::chrono::seconds kEntreAvisos{1};

enum class Estado { cifrando, saludando, presentando, conectada, cerrando };

Derivar derivar_de(cifrado::SesionTls& tls) {
  return [&tls](gsl::span<const std::uint8_t> identidad, gsl::span<std::uint8_t> clave) {
    return tls.exportar(identidad, clave);
  };
}

std::uint64_t ahora_ms() {
  const auto desde_1970 = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(desde_1970).count());
}

protocolo::CanalInfo info_de(const Canal& canal) {
  return {canal.id, canal.nombre, canal.tipo, canal.solo_admins, canal.contador};
}

}

struct Servidor::Conexion {
  bool pendiente() const {
    return estado == Estado::cifrando || estado == Estado::saludando ||
           estado == Estado::presentando;
  }
  bool tiene_limite() const { return pendiente() || estado == Estado::cerrando; }
  Persona persona() const { return Persona{dueno, canal}; }
  std::size_t por_enviar() const { return salida.size() - enviado; }

  red::Socket socket;
  red::Direccion origen;
  std::unique_ptr<cifrado::SesionTls> tls;
  Estado estado = Estado::cifrando;
  Reloj::time_point limite;
  Reloj::time_point ultimo_dato;
  protocolo::Ensamblador ensamblador{protocolo::kMaximoCuerpoSaludo};
  std::vector<std::uint8_t> salida;
  std::size_t enviado = 0;
  bool turno_global = false;
  bool quiere_escribir = false;
  bool datos_guardados = false;
  bool terminar = false;
  int intentos_de_presentacion = 0;
  std::uint32_t id = 0;
  std::string nombre;
  std::wstring comparable;
  bool dueno = false;
  std::uint32_t canal = kCanalGeneral;
  std::uint32_t mirando = 0;
  std::vector<std::uint32_t> novedades;
  Reloj::time_point ultimo_aviso{};
  Cubeta chat;
  Cubeta pedidos;
  Cubeta historial;
};

Servidor::Servidor(cifrado::IdentidadServidor& identidad, Opciones opciones, Avisar avisar)
    : identidad_(identidad),
      opciones_(std::move(opciones)),
      avisar_(std::move(avisar)),
      proteccion_(opciones_.proteccion) {
  opciones_.conexiones_maximas =
      std::min(opciones_.conexiones_maximas, static_cast<std::size_t>(FD_SETSIZE - 3));
}

Servidor::~Servidor() = default;

bool Servidor::escuchar() {
  config_ = cifrado::ConfigTls::para_servidor(identidad_);
  if (!config_ || !despertador_.ok() ||
      !base_.arrancar(opciones_.archivo_base, opciones_.nombre_general, despertador_)) {
    return false;
  }
  std::promise<std::pair<std::vector<protocolo::CanalInfo>, Retencion>> cargado;
  auto futuro = cargado.get_future();
  if (!base_.pedir([&cargado](Base& base) -> std::function<void()> {
        cargado.set_value({base.canales(), base.retencion()});
        return {};
      })) {
    return false;
  }
  auto [canales, retencion] = futuro.get();
  retencion_ = retencion;
  for (auto& canal : canales) {
    canales_.push_back(Canal{canal.id, canal.nombre, nombre_para_comparar(canal.nombre),
                             canal.tipo, canal.solo_admins, canal.contador});
  }
  if (puede_entrar(canales_, kCanalGeneral)) {
    return false;
  }
  for (int intento = 0; intento < 8 && !udp_.valido(); ++intento) {
    escucha_ = red::escuchar(opciones_.escuchar);
    const auto local = escucha_.valido() ? red::direccion_local(escucha_) : std::nullopt;
    if (!local) {
      break;
    }
    puerto_ = local->puerto;
    udp_ = red::abrir_udp({opciones_.escuchar.ipv4, puerto_});
    if (opciones_.escuchar.puerto != 0) {
      break;
    }
  }
  voz_ = std::make_unique<Voz>(opciones_.voz, [this](const red::Direccion& destino,
                                                     gsl::span<const std::uint8_t> datos) {
    red::enviar_a(udp_, destino, datos);
  });
  if (!udp_.valido() || !voz_->ok()) {
    escucha_ = red::Socket();
    return false;
  }
  return true;
}

void Servidor::avisar(Evento evento, const red::Direccion& origen) {
  if (avisar_) {
    avisar_(evento, origen, std::string());
  }
}

void Servidor::avisar(Evento evento, const Conexion& conexion) {
  if (avisar_) {
    avisar_(evento, conexion.origen, conexion.nombre);
  }
}

void Servidor::falla(const Conexion& conexion) {
  if (proteccion_.fallo(conexion.origen.ipv4, Reloj::now())) {
    avisar(Evento::bloqueada, conexion.origen);
  }
}

void Servidor::correr(const std::atomic<bool>& seguir) {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
  while (seguir.load() && escucha_.valido()) {
    fd_set lectura;
    fd_set escritura;
    FD_ZERO(&lectura);
    FD_ZERO(&escritura);
    FD_SET(static_cast<SOCKET>(despertador_.socket().get()), &lectura);
    FD_SET(static_cast<SOCKET>(udp_.get()), &lectura);
    auto espera = kEsperaMaxima;
    const auto ahora = Reloj::now();
    if (ahora >= aceptar_desde_) {
      FD_SET(static_cast<SOCKET>(escucha_.get()), &lectura);
    } else {
      espera = std::min(espera, std::chrono::duration_cast<std::chrono::milliseconds>(
                                    aceptar_desde_ - ahora));
    }
    for (const auto& conexion : conexiones_) {
      const auto s = static_cast<SOCKET>(conexion->socket.get());
      if (conexion->quiere_escribir || conexion->por_enviar() > 0) {
        FD_SET(s, &escritura);
      }
      if (!conexion->quiere_escribir) {
        FD_SET(s, &lectura);
      }
      if (conexion->datos_guardados) {
        espera = std::chrono::milliseconds{0};
      }
      if (!conexion->novedades.empty()) {
        espera = std::min(espera, std::max(std::chrono::milliseconds{0},
                                           std::chrono::ceil<std::chrono::milliseconds>(
                                               conexion->ultimo_aviso + kEntreAvisos - ahora)));
      }
      const auto limite = conexion->tiene_limite() ? conexion->limite
                          : conexion->estado == Estado::conectada
                              ? conexion->ultimo_dato + opciones_.inactividad
                              : Reloj::time_point::max();
      if (limite != Reloj::time_point::max()) {
        const auto falta = std::chrono::duration_cast<std::chrono::milliseconds>(limite - ahora);
        espera = std::max(std::chrono::milliseconds{0}, std::min(espera, falta));
      }
    }
    timeval tiempo = {};
    tiempo.tv_sec = static_cast<long>(espera.count() / 1000);
    tiempo.tv_usec = static_cast<long>((espera.count() % 1000) * 1000);
    if (select(0, &lectura, &escritura, nullptr, &tiempo) < 0) {
      FD_ZERO(&lectura);
      FD_ZERO(&escritura);
      Sleep(50);
    }

    if (FD_ISSET(static_cast<SOCKET>(despertador_.socket().get()), &lectura)) {
      despertador_.vaciar();
    }
    if (FD_ISSET(static_cast<SOCKET>(udp_.get()), &lectura)) {
      atender_udp();
    }
    renovar_claves();
    base_.atender_respuestas();
    for (std::size_t i = 0; i < conexiones_.size(); ++i) {
      auto& conexion = *conexiones_[i];
      const auto s = static_cast<SOCKET>(conexion.socket.get());
      if (!conexion.terminar && (FD_ISSET(s, &lectura) || FD_ISSET(s, &escritura) ||
                                 conexion.datos_guardados)) {
        avanzar(conexion);
      }
    }
    if (FD_ISSET(static_cast<SOCKET>(escucha_.get()), &lectura)) {
      aceptar_nuevas();
    }
    avisar_novedades();

    const auto despues = Reloj::now();
    for (auto& conexion : conexiones_) {
      if (conexion->terminar) {
        continue;
      }
      if (conexion->tiene_limite() && despues >= conexion->limite) {
        if (conexion->pendiente()) {
          avisar(Evento::tiempo_vencido, conexion->origen);
          falla(*conexion);
        }
        conexion->terminar = true;
      } else if (conexion->estado == Estado::conectada &&
                 despues - conexion->ultimo_dato > opciones_.inactividad) {
        avisar(Evento::sin_respuesta, *conexion);
        conexion->terminar = true;
      }
    }
    quitar_terminadas();
  }
  conexiones_.clear();
}

void Servidor::quitar_terminadas() {
  std::vector<std::uint32_t> se_fueron;
  for (const auto& conexion : conexiones_) {
    if (conexion->terminar && conexion->estado == Estado::conectada) {
      se_fueron.push_back(conexion->id);
      avisar(Evento::salio, *conexion);
    }
  }
  conexiones_.erase(std::remove_if(conexiones_.begin(), conexiones_.end(),
                                   [](const auto& conexion) { return conexion->terminar; }),
                    conexiones_.end());
  for (const std::uint32_t id : se_fueron) {
    voz_->baja(id);
    difundir(protocolo::UsuarioSalio{id});
  }
}

void Servidor::aceptar_nuevas() {
  for (std::size_t vuelta = 0; vuelta < kAceptadasPorVuelta; ++vuelta) {
    bool fallo = false;
    auto aceptada = red::aceptar(escucha_, fallo);
    if (!aceptada) {
      if (fallo) {
        aceptar_desde_ = Reloj::now() + kPausaSiFallaAceptar;
      }
      return;
    }
    const red::Direccion origen = aceptada->second;
    const auto veredicto = proteccion_.nueva_conexion(origen.ipv4, Reloj::now());
    if (veredicto.decision != Decision::aceptar) {
      if (veredicto.avisar) {
        avisar(veredicto.decision == Decision::bloqueada  ? Evento::rechazo_bloqueada
               : veredicto.decision == Decision::ocupado ? Evento::ocupado
                                                          : Evento::demasiado_rapido,
               origen);
      }
      continue;
    }
    if (conexiones_.size() >= opciones_.conexiones_maximas) {
      avisar(Evento::lleno, origen);
      continue;
    }
    std::size_t pendientes = 0;
    std::size_t pendientes_de_esta_ip = 0;
    std::size_t de_esta_ip = 0;
    for (const auto& otra : conexiones_) {
      const bool misma_ip = otra->origen.ipv4 == origen.ipv4;
      pendientes += otra->pendiente() ? 1 : 0;
      pendientes_de_esta_ip += otra->pendiente() && misma_ip ? 1 : 0;
      de_esta_ip += misma_ip ? 1 : 0;
    }
    if (de_esta_ip >= opciones_.conexiones_por_ip) {
      avisar(Evento::demasiadas_por_ip, origen);
      continue;
    }
    if (pendientes >= opciones_.pendientes_en_total ||
        pendientes_de_esta_ip >= opciones_.pendientes_por_ip) {
      avisar(Evento::demasiadas_pendientes, origen);
      continue;
    }
    auto conexion = std::make_unique<Conexion>();
    conexion->socket = std::move(aceptada->first);
    conexion->origen = origen;
    conexion->tls = std::make_unique<cifrado::SesionTls>(*config_, conexion->socket.get());
    conexion->limite = Reloj::now() + opciones_.tiempo_para_saludar;
    if (conexion->tls->ok()) {
      conexiones_.push_back(std::move(conexion));
    }
  }
}

void Servidor::atender_udp() {
  std::array<std::uint8_t, protocolo::kMaximoDatagrama> datagrama{};
  for (std::size_t vuelta = 0; vuelta < kDatagramasPorVuelta; ++vuelta) {
    const auto recibido = red::recibir_de(udp_, datagrama);
    if (!recibido) {
      return;
    }
    const auto atada = voz_->recibir(recibido->origen,
                                     gsl::span<const std::uint8_t>(datagrama).first(recibido->largo),
                                     Reloj::now());
    if (const Conexion* conexion = atada ? buscar_conectada(*atada) : nullptr) {
      avisar(Evento::voz_conectada, *conexion);
    }
  }
}

void Servidor::renovar_claves() {
  const auto ahora = Reloj::now();
  if (ahora < revisar_voz_en_) {
    return;
  }
  revisar_voz_en_ = ahora + kCadaCuantoRevisarVoz;
  for (const std::uint32_t id : voz_->revisar(ahora)) {
    Conexion* conexion = buscar_conectada(id);
    const auto clave = conexion != nullptr ? voz_->renovar(id, ahora, derivar_de(*conexion->tls))
                                           : std::nullopt;
    if (clave) {
      encolar(*conexion, *clave);
    }
  }
}

void Servidor::avanzar(Conexion& conexion) {
  conexion.quiere_escribir = false;
  conexion.datos_guardados = false;
  if (conexion.estado == Estado::cifrando) {
    if (!conexion.turno_global) {
      const auto veredicto = proteccion_.empezar_saludo(conexion.origen.ipv4, Reloj::now());
      if (veredicto.decision != Decision::aceptar) {
        if (veredicto.avisar) {
          avisar(Evento::ocupado, conexion.origen);
        }
        conexion.terminar = true;
        return;
      }
      conexion.turno_global = true;
    }
    switch (conexion.tls->apreton()) {
      case cifrado::Paso::listo:
        conexion.estado = Estado::saludando;
        break;
      case cifrado::Paso::esperar_lectura:
        revisar_lo_recibido(conexion);
        return;
      case cifrado::Paso::esperar_escritura:
        conexion.quiere_escribir = true;
        revisar_lo_recibido(conexion);
        return;
      case cifrado::Paso::cerrado:
      case cifrado::Paso::error:
        avisar(Evento::rechazo_cifrado, conexion.origen);
        falla(conexion);
        conexion.terminar = true;
        return;
    }
  }
  if (!enviar_pendiente(conexion)) {
    return;
  }

  std::array<std::uint8_t, 4096> recibido{};
  int lecturas = 0;
  bool corto_por_tope = false;
  while (!conexion.terminar && conexion.estado != Estado::cerrando) {
    if (lecturas++ == kLecturasPorVuelta) {
      corto_por_tope = true;
      break;
    }
    std::size_t leidos = 0;
    const auto paso = conexion.tls->leer(recibido, leidos);
    if (paso == cifrado::Paso::listo) {
      conexion.ultimo_dato = Reloj::now();
      auto datos = gsl::span<const std::uint8_t>(recibido).first(leidos);
      while (auto marco = conexion.ensamblador.siguiente(datos)) {
        atender(conexion, *marco);
        if (conexion.terminar || conexion.estado == Estado::cerrando) {
          break;
        }
      }
      if (conexion.ensamblador.error()) {
        avisar(conexion.estado == Estado::conectada ? Evento::desconectada : Evento::rechazo_saludo,
               conexion.origen);
        falla(conexion);
        conexion.terminar = true;
      }
      continue;
    }
    if (paso == cifrado::Paso::esperar_escritura) {
      conexion.quiere_escribir = true;
    } else if (paso != cifrado::Paso::esperar_lectura) {
      if (conexion.estado != Estado::conectada) {
        avisar(Evento::rechazo_saludo, conexion.origen);
        falla(conexion);
      }
      conexion.terminar = true;
    }
    break;
  }
  conexion.datos_guardados = corto_por_tope && !conexion.terminar &&
                             conexion.tls->hay_datos_guardados();
  enviar_pendiente(conexion);
  revisar_lo_recibido(conexion);
}

void Servidor::revisar_lo_recibido(Conexion& conexion) {
  if (!conexion.terminar && conexion.pendiente() &&
      conexion.tls->bytes_recibidos() > kMaximoAntesDelSaludo) {
    avisar(conexion.estado == Estado::cifrando ? Evento::rechazo_cifrado : Evento::rechazo_saludo,
           conexion.origen);
    falla(conexion);
    conexion.terminar = true;
  }
}

void Servidor::atender(Conexion& conexion, const protocolo::Marco& marco) {
  switch (conexion.estado) {
    case Estado::saludando:
      atender_hola(conexion, marco);
      return;
    case Estado::presentando:
      atender_presentacion(conexion, marco);
      return;
    case Estado::conectada:
      atender_pedido(conexion, marco);
      return;
    case Estado::cifrando:
    case Estado::cerrando:
      return;
  }
}

void Servidor::atender_hola(Conexion& conexion, const protocolo::Marco& marco) {
  std::optional<protocolo::Hola> hola;
  if (marco.tipo == static_cast<std::uint16_t>(TipoMensaje::hola)) {
    hola = protocolo::leer_hola(marco.cuerpo);
  }
  const auto respuesta =
      hola ? protocolo::negociar(*hola, opciones_.protocolo)
           : std::variant<protocolo::Bienvenida, protocolo::Rechazo>(
                 protocolo::Rechazo{protocolo::MotivoRechazo::saludo_invalido, {}});
  if (const auto* bienvenida = std::get_if<protocolo::Bienvenida>(&respuesta)) {
    protocolo::Escritor cuerpo;
    protocolo::escribir(cuerpo, *bienvenida);
    std::vector<std::uint8_t> marco_bienvenida;
    if (!cuerpo.ok() ||
        !protocolo::armar_marco(static_cast<std::uint16_t>(TipoMensaje::bienvenida),
                                cuerpo.datos(), marco_bienvenida)) {
      conexion.terminar = true;
      return;
    }
    encolar_bytes(conexion, marco_bienvenida);
    conexion.estado = Estado::presentando;
    conexion.ensamblador.cambiar_maximo(protocolo::kMaximoCuerpoPresentacion);
    avisar(Evento::conectada, conexion.origen);
    return;
  }
  const auto& rechazo = std::get<protocolo::Rechazo>(respuesta);
  if (rechazo.motivo == protocolo::MotivoRechazo::saludo_invalido) {
    falla(conexion);
  }
  protocolo::Escritor cuerpo;
  protocolo::escribir(cuerpo, rechazo);
  std::vector<std::uint8_t> marco_rechazo;
  if (cuerpo.ok() && protocolo::armar_marco(static_cast<std::uint16_t>(TipoMensaje::rechazo),
                                            cuerpo.datos(), marco_rechazo)) {
    encolar_bytes(conexion, marco_rechazo);
  }
  cerrar_con_respuesta(conexion);
  avisar(Evento::rechazo_saludo, conexion.origen);
}

void Servidor::atender_presentacion(Conexion& conexion, const protocolo::Marco& marco) {
  const auto presentacion =
      marco.tipo == static_cast<std::uint16_t>(TipoMensaje::presentarse)
          ? protocolo::leer_mensaje<protocolo::Presentarse>(marco.cuerpo)
          : std::nullopt;
  if (!presentacion) {
    avisar(Evento::rechazo_saludo, conexion.origen);
    falla(conexion);
    conexion.terminar = true;
    return;
  }
  if (presentacion->codigo_dueno) {
    const bool correcto = opciones_.codigo_dueno &&
                          *opciones_.codigo_dueno == cifrado::CodigoDueno(*presentacion->codigo_dueno);
    if (!correcto) {
      encolar(conexion, protocolo::PresentacionRechazada{
                            protocolo::MotivoPresentacion::codigo_incorrecto});
      cerrar_con_respuesta(conexion);
      avisar(Evento::codigo_incorrecto, conexion.origen);
      falla(conexion);
      return;
    }
  }
  const std::wstring comparable = nombre_para_comparar(presentacion->nombre);
  const bool en_uso =
      comparable.empty() ||
      std::any_of(conexiones_.begin(), conexiones_.end(), [&comparable](const auto& otra) {
        return otra->estado == Estado::conectada && !otra->terminar &&
               otra->comparable == comparable;
      });
  if (en_uso) {
    encolar(conexion,
            protocolo::PresentacionRechazada{protocolo::MotivoPresentacion::nombre_en_uso});
    if (++conexion.intentos_de_presentacion >= opciones_.intentos_de_presentacion) {
      cerrar_con_respuesta(conexion);
    }
    return;
  }

  const auto ahora = Reloj::now();
  conexion.id = proximo_id_++;
  conexion.nombre = presentacion->nombre;
  conexion.comparable = comparable;
  conexion.dueno = presentacion->codigo_dueno.has_value();
  conexion.canal = kCanalGeneral;
  conexion.estado = Estado::conectada;
  conexion.ultimo_dato = ahora;
  conexion.chat = Cubeta(opciones_.rafaga_chat, opciones_.chat_por_segundo, ahora);
  conexion.pedidos = Cubeta(opciones_.rafaga_pedidos, opciones_.pedidos_por_segundo, ahora);
  conexion.historial = Cubeta(opciones_.rafaga_historial, opciones_.historial_por_segundo, ahora);
  conexion.ensamblador.cambiar_maximo(protocolo::kMaximoCuerpo);

  protocolo::Presentado presentado;
  presentado.tu_id = conexion.id;
  presentado.dueno = conexion.dueno;
  for (const Canal& canal : canales_) {
    presentado.canales.push_back(info_de(canal));
  }
  for (const auto& otra : conexiones_) {
    if (otra->estado == Estado::conectada && !otra->terminar) {
      presentado.usuarios.push_back(protocolo::UsuarioInfo{otra->id, otra->nombre, otra->canal});
    }
  }
  encolar(conexion, presentado);
  if (const auto clave = voz_->alta(conexion.id, conexion.origen.ipv4, conexion.canal, ahora,
                                    derivar_de(*conexion.tls))) {
    encolar(conexion, *clave);
  }
  difundir(protocolo::UsuarioEntro{protocolo::UsuarioInfo{conexion.id, conexion.nombre,
                                                         conexion.canal}},
           conexion.id);
  proteccion_.saludo_completo(conexion.origen.ipv4, ahora);
  avisar(Evento::presentada, conexion);
}

void Servidor::atender_pedido(Conexion& conexion, const protocolo::Marco& marco) {
  const auto tipo = static_cast<TipoMensaje>(marco.tipo);
  if (tipo == TipoMensaje::enviar_chat) {
    enviar_chat(conexion, marco);
    return;
  }
  if (!conexion.pedidos.tomar(Reloj::now()) ||
      (tipo == TipoMensaje::pedir_historial && !conexion.historial.tomar(Reloj::now()))) {
    negar(conexion, tipo, MotivoNegado::demasiado_rapido);
    return;
  }
  switch (tipo) {
    case TipoMensaje::entrar_a_canal:
      entrar_a_canal(conexion, marco);
      return;
    case TipoMensaje::pedir_historial:
      pedir_historial(conexion, marco);
      return;
    case TipoMensaje::crear_canal:
      crear_canal(conexion, marco);
      return;
    case TipoMensaje::cambiar_canal:
      cambiar_canal(conexion, marco);
      return;
    case TipoMensaje::mirar_canal:
      mirar_canal(conexion, marco);
      return;
    case TipoMensaje::borrar_canal:
      borrar_canal(conexion, marco);
      return;
    case TipoMensaje::ping:
      if (const auto ping = protocolo::leer_mensaje<protocolo::Ping>(marco.cuerpo)) {
        encolar(conexion, protocolo::Pong{ping->marca});
        return;
      }
      break;
    default:
      break;
  }
  error_de_protocolo(conexion);
}

void Servidor::entrar_a_canal(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::EntrarACanal>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (const auto motivo = puede_entrar(canales_, pedido->canal)) {
    negar(conexion, TipoMensaje::entrar_a_canal, *motivo);
    return;
  }
  if (conexion.canal == pedido->canal) {
    return;
  }
  conexion.canal = pedido->canal;
  voz_->mover(conexion.id, conexion.canal);
  difundir(protocolo::UsuarioCambioCanal{conexion.id, conexion.canal});
}

void Servidor::enviar_chat(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::EnviarChat>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (!conexion.chat.tomar(Reloj::now()) ||
      !proteccion_.puede_chatear(conexion.origen.ipv4, Reloj::now())) {
    negar(conexion, TipoMensaje::enviar_chat, MotivoNegado::demasiado_rapido);
    return;
  }
  if (const auto motivo = puede_escribir(conexion.persona(), canales_, pedido->canal)) {
    negar(conexion, TipoMensaje::enviar_chat, *motivo);
    return;
  }
  Canal* canal = buscar_canal(canales_, pedido->canal);
  protocolo::MensajeChat mensaje;
  mensaje.canal = pedido->canal;
  mensaje.numero = ++canal->contador;
  mensaje.autor_id = conexion.id;
  mensaje.autor = conexion.nombre;
  mensaje.hora_ms = ahora_ms();
  mensaje.texto = pedido->texto;
  const bool guardar = retencion_ != Retencion::no_guardar;
  const bool pedido_aceptado = base_.pedir([this, mensaje, guardar](Base& base) mutable {
    const auto id = base.guardar_mensaje(mensaje.canal, mensaje.numero, mensaje.autor,
                                         mensaje.hora_ms, mensaje.texto, guardar);
    return std::function<void()>([this, mensaje, id]() mutable {
      if (id) {
        mensaje.id = *id;
        difundir_chat(mensaje);
      }
    });
  });
  if (!pedido_aceptado) {
    --canal->contador;
    negar(conexion, TipoMensaje::enviar_chat, MotivoNegado::lleno);
  } else if (!guardar) {
    mensaje.id = proximo_mensaje_sin_guardar_++;
    difundir_chat(mensaje);
  }
}

void Servidor::difundir_chat(const protocolo::MensajeChat& mensaje) {
  std::vector<std::uint8_t> marco;
  const Canal* canal = buscar_canal(canales_, mensaje.canal);
  if (canal == nullptr || !protocolo::armar_mensaje(mensaje, marco)) {
    return;
  }
  const bool texto = canal->tipo == TipoCanal::texto;
  for (auto& conexion : conexiones_) {
    if (conexion->estado != Estado::conectada || conexion->terminar) {
      continue;
    }
    if (texto ? conexion->mirando == mensaje.canal || conexion->id == mensaje.autor_id
              : conexion->canal == mensaje.canal) {
      encolar_bytes(*conexion, marco);
    } else if (texto && std::find(conexion->novedades.begin(), conexion->novedades.end(),
                                  mensaje.canal) == conexion->novedades.end()) {
      conexion->novedades.push_back(mensaje.canal);
    }
  }
}

void Servidor::avisar_novedades() {
  const auto ahora = Reloj::now();
  for (auto& conexion : conexiones_) {
    if (conexion->novedades.empty() || ahora - conexion->ultimo_aviso < kEntreAvisos) {
      continue;
    }
    protocolo::MensajesNuevos aviso;
    for (const std::uint32_t id : conexion->novedades) {
      if (const Canal* canal = buscar_canal(canales_, id)) {
        aviso.canales.push_back(protocolo::Contador{id, canal->contador});
      }
    }
    conexion->novedades.clear();
    conexion->ultimo_aviso = ahora;
    if (!aviso.canales.empty()) {
      encolar(*conexion, aviso);
    }
  }
}

void Servidor::mirar_canal(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::MirarCanal>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (const auto motivo = puede_mirar(canales_, pedido->canal)) {
    negar(conexion, TipoMensaje::mirar_canal, *motivo);
    return;
  }
  conexion.mirando = pedido->canal;
  conexion.novedades.erase(
      std::remove(conexion.novedades.begin(), conexion.novedades.end(), pedido->canal),
      conexion.novedades.end());
}

void Servidor::pedir_historial(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::PedirHistorial>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (const auto motivo = puede_leer(conexion.persona(), canales_, pedido->canal)) {
    negar(conexion, TipoMensaje::pedir_historial, *motivo);
    return;
  }
  const std::uint32_t quien = conexion.id;
  const bool pedido_aceptado = base_.pedir([this, quien, canal = pedido->canal,
                                            antes_de = pedido->antes_de](Base& base) {
    auto historial = base.historial(canal, antes_de, kMaximoHistorial);
    return std::function<void()>([this, quien, historial = std::move(historial)]() {
      Conexion* conexion = buscar_conectada(quien);
      if (conexion != nullptr && !puede_leer(conexion->persona(), canales_, historial.canal)) {
        encolar(*conexion, historial);
      }
    });
  });
  if (!pedido_aceptado) {
    negar(conexion, TipoMensaje::pedir_historial, MotivoNegado::lleno);
  }
}

void Servidor::crear_canal(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::CrearCanal>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  auto motivo = puede_crear(conexion.persona(), canales_, pedido->nombre);
  if (!motivo && canales_.size() + canales_pedidos_ >= kMaximoCanales) {
    motivo = MotivoNegado::lleno;
  }
  if (motivo) {
    negar(conexion, TipoMensaje::crear_canal, *motivo);
    return;
  }
  const std::uint32_t quien = conexion.id;
  const bool pedido_aceptado = base_.pedir([this, quien, nuevo = *pedido](Base& base) {
    const auto id = base.crear_canal(nuevo.nombre, nuevo.tipo, nuevo.solo_admins);
    return std::function<void()>([this, quien, nuevo, id]() {
      --canales_pedidos_;
      if (!id) {
        if (Conexion* conexion = buscar_conectada(quien)) {
          negar(*conexion, TipoMensaje::crear_canal, MotivoNegado::nombre_en_uso);
        }
        return;
      }
      canales_.push_back(Canal{*id, nuevo.nombre, nombre_para_comparar(nuevo.nombre), nuevo.tipo,
                               nuevo.solo_admins});
      difundir(protocolo::CanalCreado{info_de(canales_.back())});
    });
  });
  if (pedido_aceptado) {
    ++canales_pedidos_;
  } else {
    negar(conexion, TipoMensaje::crear_canal, MotivoNegado::lleno);
  }
}

void Servidor::cambiar_canal(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::CambiarCanal>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (const auto motivo = puede_cambiar(conexion.persona(), canales_, *pedido)) {
    negar(conexion, TipoMensaje::cambiar_canal, *motivo);
    return;
  }
  const std::uint32_t quien = conexion.id;
  const bool pedido_aceptado = base_.pedir([this, quien, cambio = *pedido](Base& base) {
    const bool hecho = base.cambiar_canal(cambio.canal, cambio.nombre, cambio.solo_admins);
    return std::function<void()>([this, quien, cambio, hecho]() {
      Canal* encontrado = buscar_canal(canales_, cambio.canal);
      if (!hecho || encontrado == nullptr) {
        if (Conexion* conexion = buscar_conectada(quien)) {
          negar(*conexion, TipoMensaje::cambiar_canal, MotivoNegado::nombre_en_uso);
        }
        return;
      }
      encontrado->nombre = cambio.nombre;
      encontrado->comparable = nombre_para_comparar(cambio.nombre);
      encontrado->solo_admins = cambio.solo_admins;
      difundir(protocolo::CanalCambiado{info_de(*encontrado)});
    });
  });
  if (!pedido_aceptado) {
    negar(conexion, TipoMensaje::cambiar_canal, MotivoNegado::lleno);
  }
}

void Servidor::borrar_canal(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::BorrarCanal>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (const auto motivo = puede_borrar(conexion.persona(), canales_, pedido->canal)) {
    negar(conexion, TipoMensaje::borrar_canal, *motivo);
    return;
  }
  const std::uint32_t quien = conexion.id;
  const bool pedido_aceptado =
      base_.pedir([this, quien, canal = pedido->canal](Base& base) {
        const bool hecho = base.borrar_canal(canal);
        return std::function<void()>([this, quien, canal, hecho]() {
          if (!hecho) {
            if (Conexion* conexion = buscar_conectada(quien)) {
              negar(*conexion, TipoMensaje::borrar_canal, MotivoNegado::no_existe);
            }
            return;
          }
          canales_.erase(std::remove_if(canales_.begin(), canales_.end(),
                                        [canal](const Canal& c) { return c.id == canal; }),
                         canales_.end());
          for (auto& otra : conexiones_) {
            if (otra->canal == canal) {
              otra->canal = kCanalGeneral;
              voz_->mover(otra->id, kCanalGeneral);
            }
          }
          difundir(protocolo::CanalBorrado{canal, kCanalGeneral});
        });
      });
  if (!pedido_aceptado) {
    negar(conexion, TipoMensaje::borrar_canal, MotivoNegado::lleno);
  }
}

void Servidor::error_de_protocolo(Conexion& conexion) {
  falla(conexion);
  conexion.terminar = true;
}

void Servidor::negar(Conexion& conexion, TipoMensaje pedido, MotivoNegado motivo) {
  encolar(conexion, protocolo::Negado{pedido, motivo});
}

void Servidor::cerrar_con_respuesta(Conexion& conexion) {
  conexion.estado = Estado::cerrando;
  conexion.limite = Reloj::now() + opciones_.tiempo_para_cerrar;
}

Servidor::Conexion* Servidor::buscar_conectada(std::uint32_t id) {
  for (auto& conexion : conexiones_) {
    if (conexion->estado == Estado::conectada && !conexion->terminar && conexion->id == id) {
      return conexion.get();
    }
  }
  return nullptr;
}

template <typename Mensaje>
void Servidor::encolar(Conexion& conexion, const Mensaje& mensaje) {
  std::vector<std::uint8_t> marco;
  if (!protocolo::armar_mensaje(mensaje, marco)) {
    conexion.terminar = true;
    return;
  }
  encolar_bytes(conexion, marco);
}

template <typename Mensaje>
void Servidor::difundir(const Mensaje& mensaje, std::uint32_t excepto) {
  std::vector<std::uint8_t> marco;
  if (!protocolo::armar_mensaje(mensaje, marco)) {
    return;
  }
  for (auto& conexion : conexiones_) {
    if (conexion->estado == Estado::conectada && !conexion->terminar && conexion->id != excepto) {
      encolar_bytes(*conexion, marco);
    }
  }
}

void Servidor::encolar_bytes(Conexion& conexion, const std::vector<std::uint8_t>& marco) {
  if (conexion.terminar) {
    return;
  }
  if (conexion.por_enviar() + marco.size() > opciones_.maximo_salida) {
    avisar(Evento::no_lee, conexion);
    conexion.terminar = true;
    return;
  }
  conexion.salida.insert(conexion.salida.end(), marco.begin(), marco.end());
}

bool Servidor::enviar_pendiente(Conexion& conexion) {
  while (conexion.por_enviar() > 0 && !conexion.terminar) {
    std::size_t escritos = 0;
    const auto paso = conexion.tls->escribir(
        gsl::span<const std::uint8_t>(conexion.salida).subspan(conexion.enviado), escritos);
    if (paso == cifrado::Paso::listo) {
      conexion.enviado += escritos;
      conexion.ultimo_dato = Reloj::now();
      continue;
    }
    if (paso == cifrado::Paso::esperar_escritura) {
      conexion.quiere_escribir = true;
    } else if (paso != cifrado::Paso::esperar_lectura) {
      conexion.terminar = true;
    }
    break;
  }
  if (conexion.enviado >= 64 * 1024 || conexion.enviado * 2 >= conexion.salida.size()) {
    conexion.salida.erase(conexion.salida.begin(),
                          conexion.salida.begin() + static_cast<std::ptrdiff_t>(conexion.enviado));
    conexion.enviado = 0;
  }
  if (conexion.por_enviar() > 0 || conexion.terminar) {
    return false;
  }
  if (conexion.estado == Estado::cerrando) {
    conexion.tls->cerrar();
    conexion.terminar = true;
  }
  return !conexion.terminar;
}

}
