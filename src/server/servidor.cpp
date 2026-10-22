#include "server/servidor.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <variant>

#include <winsock2.h>

namespace ventrik::servidor {

namespace {

using Reloj = std::chrono::steady_clock;

constexpr std::chrono::milliseconds kEsperaMaxima{500};
constexpr std::chrono::milliseconds kPausaSiFallaAceptar{100};
// Una ola de conexiones no puede dejar sin atender a las que ya están.
constexpr std::size_t kAceptadasPorVuelta = 64;
// Un programa de verdad manda menos de 2 KiB antes de saludar; más que esto es un ataque.
constexpr std::size_t kMaximoAntesDelSaludo = 16 * 1024;

enum class Estado { cifrando, saludando, conectada, cerrando };

}  // namespace

struct Servidor::Conexion {
  bool pendiente() const { return estado == Estado::cifrando || estado == Estado::saludando; }
  bool tiene_limite() const { return pendiente() || estado == Estado::cerrando; }

  red::Socket socket;
  red::Direccion origen;
  std::unique_ptr<cifrado::SesionTls> tls;
  Estado estado = Estado::cifrando;
  Reloj::time_point limite;
  protocolo::Ensamblador ensamblador{protocolo::kMaximoCuerpoSaludo};
  std::vector<std::uint8_t> salida;
  bool turno_global = false;
  bool quiere_escribir = false;
  bool terminar = false;
};

Servidor::Servidor(cifrado::IdentidadServidor& identidad, Opciones opciones, Avisar avisar)
    : identidad_(identidad),
      opciones_(std::move(opciones)),
      avisar_(std::move(avisar)),
      proteccion_(opciones_.proteccion) {
  // Más sockets que los que entran en un fd_set quedarían afuera de select y nunca vencerían.
  opciones_.conexiones_maximas =
      std::min(opciones_.conexiones_maximas, static_cast<std::size_t>(FD_SETSIZE - 1));
}

Servidor::~Servidor() = default;

bool Servidor::escuchar() {
  config_ = cifrado::ConfigTls::para_servidor(identidad_);
  if (!config_) {
    return false;
  }
  escucha_ = red::escuchar(opciones_.escuchar);
  const auto local = escucha_.valido() ? red::direccion_local(escucha_) : std::nullopt;
  if (!local) {
    escucha_ = red::Socket();
    return false;
  }
  puerto_ = local->puerto;
  return true;
}

void Servidor::avisar(Evento evento, const red::Direccion& origen) {
  if (avisar_) {
    avisar_(evento, origen);
  }
}

void Servidor::falla(const Conexion& conexion) {
  if (proteccion_.fallo(conexion.origen.ipv4, Reloj::now())) {
    avisar(Evento::bloqueada, conexion.origen);
  }
}

void Servidor::correr(const std::atomic<bool>& seguir) {
  while (seguir.load() && escucha_.valido()) {
    fd_set lectura;
    fd_set escritura;
    FD_ZERO(&lectura);
    FD_ZERO(&escritura);
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
      // Mientras no puede escribir, no se le lee: si no, lo que espera ser leído despierta el bucle sin parar.
      FD_SET(s, conexion->quiere_escribir ? &escritura : &lectura);
      if (conexion->tiene_limite()) {
        const auto falta =
            std::chrono::duration_cast<std::chrono::milliseconds>(conexion->limite - ahora);
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

    for (auto& conexion : conexiones_) {
      const auto s = static_cast<SOCKET>(conexion->socket.get());
      if (FD_ISSET(s, &lectura) || FD_ISSET(s, &escritura)) {
        avanzar(*conexion);
      }
    }
    if (FD_ISSET(static_cast<SOCKET>(escucha_.get()), &lectura)) {
      aceptar_nuevas();
    }

    const auto despues = Reloj::now();
    for (auto& conexion : conexiones_) {
      if (!conexion->terminar && conexion->tiene_limite() && despues >= conexion->limite) {
        if (conexion->pendiente()) {
          avisar(Evento::tiempo_vencido, conexion->origen);
          falla(*conexion);
        }
        conexion->terminar = true;
      }
    }
    conexiones_.erase(std::remove_if(conexiones_.begin(), conexiones_.end(),
                                     [](const auto& conexion) { return conexion->terminar; }),
                      conexiones_.end());
  }
  conexiones_.clear();
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
    // Antes que nada, y sin gastar en el cifrado: lo más barato de rechazar es lo primero.
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

void Servidor::avanzar(Conexion& conexion) {
  conexion.quiere_escribir = false;
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
  while (!conexion.terminar && conexion.estado != Estado::cerrando) {
    std::size_t leidos = 0;
    const auto paso = conexion.tls->leer(recibido, leidos);
    if (paso == cifrado::Paso::listo) {
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
      if (conexion.estado == Estado::conectada) {
        avisar(Evento::desconectada, conexion.origen);
      } else {
        // Hacer el cifrado entero (lo caro) y cortar sin saludar también es una falla.
        avisar(Evento::rechazo_saludo, conexion.origen);
        falla(conexion);
      }
      conexion.terminar = true;
    }
    break;
  }
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
  if (conexion.estado != Estado::saludando) {
    // Después del saludo todavía no hay nada que pedir (llega en el paso 2.3).
    avisar(Evento::desconectada, conexion.origen);
    conexion.terminar = true;
    return;
  }
  std::optional<protocolo::Hola> hola;
  if (marco.tipo == static_cast<std::uint16_t>(protocolo::TipoMensaje::hola)) {
    hola = protocolo::leer_hola(marco.cuerpo);
  }
  const auto respuesta =
      hola ? protocolo::negociar(*hola, opciones_.protocolo)
           : std::variant<protocolo::Bienvenida, protocolo::Rechazo>(
                 protocolo::Rechazo{protocolo::MotivoRechazo::saludo_invalido, {}});
  protocolo::Escritor cuerpo;
  if (const auto* bienvenida = std::get_if<protocolo::Bienvenida>(&respuesta)) {
    protocolo::escribir(cuerpo, *bienvenida);
    encolar(conexion, protocolo::TipoMensaje::bienvenida, cuerpo);
    conexion.estado = Estado::conectada;
    conexion.ensamblador.cambiar_maximo(protocolo::kMaximoCuerpo);
    proteccion_.saludo_completo(conexion.origen.ipv4, Reloj::now());
    avisar(Evento::conectada, conexion.origen);
    return;
  }
  const auto& rechazo = std::get<protocolo::Rechazo>(respuesta);
  // Una versión vieja no es un ataque; un saludo que no se puede leer, puede serlo.
  if (rechazo.motivo == protocolo::MotivoRechazo::saludo_invalido) {
    falla(conexion);
  }
  protocolo::escribir(cuerpo, rechazo);
  encolar(conexion, protocolo::TipoMensaje::rechazo, cuerpo);
  conexion.estado = Estado::cerrando;
  conexion.limite = Reloj::now() + opciones_.tiempo_para_cerrar;
  avisar(Evento::rechazo_saludo, conexion.origen);
}

void Servidor::encolar(Conexion& conexion, protocolo::TipoMensaje tipo,
                       const protocolo::Escritor& cuerpo) {
  if (!cuerpo.ok() ||
      !protocolo::armar_marco(static_cast<std::uint16_t>(tipo), cuerpo.datos(), conexion.salida)) {
    conexion.terminar = true;
  }
}

bool Servidor::enviar_pendiente(Conexion& conexion) {
  while (!conexion.salida.empty() && !conexion.terminar) {
    std::size_t escritos = 0;
    const auto paso = conexion.tls->escribir(conexion.salida, escritos);
    if (paso == cifrado::Paso::listo) {
      conexion.salida.erase(conexion.salida.begin(),
                            conexion.salida.begin() + static_cast<std::ptrdiff_t>(escritos));
      continue;
    }
    if (paso == cifrado::Paso::esperar_escritura) {
      conexion.quiere_escribir = true;
    } else if (paso != cifrado::Paso::esperar_lectura) {
      conexion.terminar = true;
    }
    return false;
  }
  if (conexion.estado == Estado::cerrando && !conexion.terminar) {
    conexion.tls->cerrar();
    conexion.terminar = true;
  }
  return !conexion.terminar;
}

}  // namespace ventrik::servidor
