#include "server/voz.h"

#include <algorithm>

#include "common/cifrado/inicio.h"
#include "common/pendiente.h"

extern "C" {
#include <mbedtls/constant_time.h>
}

VENTRIK_PENDIENTE("el UDP de la voz tiene que venir de la misma IP que el TCP: revisarlo con el relay (etapa 6)")
VENTRIK_PENDIENTE("voz y saludos TLS en el mismo hilo de red: un hilo por nucleo en la etapa 8")

namespace ventrik::servidor {
namespace {
constexpr int kLecturasPorDatagrama = 8;

constexpr std::size_t kCabeceraDtls = 13;
constexpr std::uint8_t kAviso = 21;
constexpr std::uint8_t kDatos = 23;

bool solo_datos_cifrados(gsl::span<const std::uint8_t> datagrama) {
  while (!datagrama.empty()) {
    if (datagrama.size() < kCabeceraDtls || (datagrama[0] != kDatos && datagrama[0] != kAviso) ||
        datagrama[3] != 0 || datagrama[4] != 1) {
      return false;
    }
    const std::size_t largo = kCabeceraDtls + (std::size_t{datagrama[11]} << 8 | datagrama[12]);
    if (largo > datagrama.size()) {
      return false;
    }
    datagrama = datagrama.subspan(largo);
  }
  return true;
}

}

Voz::Voz(OpcionesVoz opciones, cifrado::EnviarDatagrama enviar)
    : opciones_(opciones),
      enviar_(std::move(enviar)),
      config_(cifrado::ConfigTls::para_voz_servidor(
          [this](cifrado::SesionTls& sesion, gsl::span<const std::uint8_t> identidad) {
            return elegir(sesion, identidad);
          })),
      cubeta_escucha_(opciones_.rafaga_escucha, opciones_.escucha_por_segundo) {}

std::optional<protocolo::ClaveVoz> Voz::alta(std::uint32_t id, std::uint32_t ip,
                                             std::uint32_t canal, Instante ahora,
                                             const Derivar& derivar) {
  baja(id);
  Persona persona;
  persona.id = id;
  persona.ip = ip;
  persona.canal = canal;
  persona.saludos = Cubeta(opciones_.rafaga_saludos_por_ip, opciones_.saludos_por_segundo_por_ip, ahora);
  const auto clave = nueva_clave(persona, ahora, derivar);
  if (clave) {
    personas_.push_back(std::move(persona));
  }
  return clave;
}

std::optional<protocolo::ClaveVoz> Voz::renovar(std::uint32_t id, Instante ahora,
                                                const Derivar& derivar) {
  Persona* persona = buscar(id);
  const auto clave = persona != nullptr ? nueva_clave(*persona, ahora, derivar) : std::nullopt;
  if (!clave) {
    baja(id);
  }
  return clave;
}

void Voz::mover(std::uint32_t id, std::uint32_t canal) {
  if (Persona* persona = buscar(id)) {
    persona->canal = canal;
  }
}

void Voz::silencio(std::uint32_t id, std::uint8_t valor) {
  if (Persona* persona = buscar(id)) {
    persona->silencio = valor;
  }
}

void Voz::baja(std::uint32_t id) {
  personas_.erase(std::remove_if(personas_.begin(), personas_.end(),
                                 [id](const Persona& persona) { return persona.id == id; }),
                  personas_.end());
  pendientes_.erase(std::remove_if(pendientes_.begin(), pendientes_.end(),
                                   [id](const Pendiente& pendiente) { return pendiente.sesion->marca() == id; }),
                    pendientes_.end());
}

std::optional<std::uint32_t> Voz::recibir(const red::Direccion& origen,
                                          gsl::span<const std::uint8_t> datagrama, Instante ahora) {
  if (datagrama.empty() || datagrama.size() > protocolo::kMaximoDatagrama || !ok()) {
    return std::nullopt;
  }
  const auto propia = std::find_if(personas_.begin(), personas_.end(), [&](const Persona& persona) {
    return persona.sesion && persona.direccion == origen;
  });
  const bool saludando =
      std::any_of(pendientes_.begin(), pendientes_.end(),
                  [&origen](const Pendiente& pendiente) { return pendiente.origen == origen; });
  if (propia == personas_.end() || saludando || !solo_datos_cifrados(datagrama)) {
    return saludar(datagrama, origen, ahora);
  }
  if (propia->datagramas.tomar(ahora)) {
    atender(*propia, datagrama, ahora);
  }
  return std::nullopt;
}

std::vector<std::uint32_t> Voz::revisar(Instante ahora) {
  for (auto& pendiente : pendientes_) {
    if (ahora >= pendiente.limite || pendiente.sesion->apreton() != cifrado::Paso::esperar_lectura) {
      pendiente.sesion.reset();
    }
  }
  pendientes_.erase(std::remove_if(pendientes_.begin(), pendientes_.end(),
                                   [](const Pendiente& pendiente) { return !pendiente.sesion; }),
                    pendientes_.end());
  std::vector<std::uint32_t> vencidas;
  for (auto& persona : personas_) {
    if (persona.sesion && ahora - persona.ultimo > opciones_.sin_datos) {
      persona.sesion.reset();
    }
    if (ahora >= persona.rotar_en) {
      persona.rotar_en = ahora + opciones_.rotacion;
      vencidas.push_back(persona.id);
    }
  }
  return vencidas;
}

bool Voz::tiene_voz(std::uint32_t id) const {
  return std::any_of(personas_.begin(), personas_.end(), [id](const Persona& persona) {
    return persona.id == id && persona.sesion;
  });
}

std::optional<protocolo::ClaveVoz> Voz::nueva_clave(Persona& persona, Instante ahora,
                                                    const Derivar& derivar) {
  unsigned char azar = 0;
  const auto periodo = std::chrono::duration_cast<std::chrono::milliseconds>(opciones_.rotacion);
  if (cifrado::aleatorio(nullptr, &azar, 1) != 0 ||
      cifrado::aleatorio(nullptr, persona.identidad.data(), persona.identidad.size()) != 0 ||
      !derivar(persona.identidad, persona.clave.bytes())) {
    return std::nullopt;
  }
  persona.rotar_en = ahora + periodo * 9 / 10 + periodo * azar / 1275;
  return protocolo::ClaveVoz{persona.identidad};
}

gsl::span<const std::uint8_t> Voz::elegir(cifrado::SesionTls& sesion,
                                          gsl::span<const std::uint8_t> identidad) {
  for (auto& persona : personas_) {
    if (identidad.size() == persona.identidad.size() && persona.ip == sesion.otro_lado().ipv4 &&
        mbedtls_ct_memcmp(persona.identidad.data(), identidad.data(), identidad.size()) == 0) {
      sesion.marcar(persona.id);
      return persona.clave.bytes();
    }
  }
  return {};
}

void Voz::atender(Persona& persona, gsl::span<const std::uint8_t> datagrama, Instante ahora) {
  persona.sesion->entregar(datagrama);
  std::array<std::uint8_t, protocolo::kMaximoDatagrama> leido{};
  for (int vuelta = 0; vuelta < kLecturasPorDatagrama; ++vuelta) {
    std::size_t largo = 0;
    const auto paso = persona.sesion->leer(leido, largo);
    if (paso != cifrado::Paso::listo) {
      if (paso != cifrado::Paso::esperar_lectura) {
        persona.sesion.reset();
      }
      return;
    }
    persona.ultimo = ahora;
    const auto marco = protocolo::separar_datagrama(gsl::span<const std::uint8_t>(leido).first(largo));
    if (!marco) {
      continue;
    }
    if (marco->tipo == static_cast<std::uint16_t>(protocolo::TipoMensaje::medio_subida)) {
      auto subida = protocolo::leer_mensaje<protocolo::MedioSubida>(marco->cuerpo);
      if (subida && persona.paquetes.tomar(ahora)) {
        reenviar(persona, std::move(subida->medio));
      }
      return;
    } else if (marco->tipo == static_cast<std::uint16_t>(protocolo::TipoMensaje::ping)) {
      std::vector<std::uint8_t> respuesta;
      const auto ping = protocolo::leer_mensaje<protocolo::Ping>(marco->cuerpo);
      if (ping && protocolo::armar_datagrama(protocolo::Pong{ping->marca}, respuesta)) {
        escribir(*persona.sesion, respuesta);
      }
    }
  }
}

void Voz::reenviar(const Persona& hablante, protocolo::Medio medio) {
  if ((hablante.silencio & protocolo::kSilencioMicrofono) != 0 ||
      !protocolo::armar_datagrama(protocolo::MedioBajada{hablante.id, std::move(medio)}, datagrama_)) {
    return;
  }
  for (auto& oyente : personas_) {
    if (oyente.id != hablante.id && oyente.canal == hablante.canal && oyente.sesion &&
        (oyente.silencio & protocolo::kSilencioAudio) == 0) {
      escribir(*oyente.sesion, datagrama_);
    }
  }
}

void Voz::escribir(cifrado::SesionTls& sesion, const std::vector<std::uint8_t>& datagrama) {
  std::size_t escritos = 0;
  sesion.escribir(datagrama, escritos);
}

std::optional<std::uint32_t> Voz::saludar(gsl::span<const std::uint8_t> datagrama,
                                          const red::Direccion& origen, Instante ahora) {
  const auto de_la_ip = std::find_if(personas_.begin(), personas_.end(), [&origen](const Persona& persona) {
    return persona.ip == origen.ipv4;
  });
  if (de_la_ip == personas_.end() || !de_la_ip->saludos.tomar(ahora)) {
    return std::nullopt;
  }
  const auto pendiente =
      std::find_if(pendientes_.begin(), pendientes_.end(),
                   [&](const Pendiente& cual) { return cual.origen == origen; });
  if (pendiente != pendientes_.end()) {
    pendiente->sesion->entregar(datagrama);
    const auto paso = pendiente->sesion->apreton();
    if (paso == cifrado::Paso::esperar_lectura) {
      return std::nullopt;
    }
    Persona* persona = paso == cifrado::Paso::listo ? buscar(pendiente->sesion->marca()) : nullptr;
    std::optional<std::uint32_t> atada;
    if (persona != nullptr && persona->ip == origen.ipv4) {
      persona->sesion = std::move(pendiente->sesion);
      persona->direccion = origen;
      persona->paquetes = Cubeta(opciones_.rafaga_paquetes, opciones_.paquetes_por_segundo, ahora);
      persona->datagramas =
          Cubeta(opciones_.rafaga_datagramas, opciones_.datagramas_por_segundo, ahora);
      persona->ultimo = ahora;
      atada = persona->id;
    }
    pendientes_.erase(pendiente);
    return atada;
  }
  if (static_cast<std::size_t>(std::count_if(pendientes_.begin(), pendientes_.end(),
                                              [&origen](const Pendiente& cual) {
                                                return cual.origen.ipv4 == origen.ipv4;
                                              })) >= opciones_.pendientes_por_ip ||
      pendientes_.size() >= opciones_.pendientes_en_total || !cubeta_escucha_.tomar(ahora)) {
    return std::nullopt;
  }
  if (!escucha_) {
    escucha_ = std::make_unique<cifrado::SesionTls>(*config_, enviar_, origen);
  }
  if (!escucha_->reiniciar(origen)) {
    escucha_.reset();
    return std::nullopt;
  }
  const auto antes = escucha_->datagramas_enviados();
  escucha_->entregar(datagrama);
  if (escucha_->apreton() == cifrado::Paso::esperar_lectura &&
      escucha_->datagramas_enviados() > antes) {
    pendientes_.push_back(Pendiente{origen, std::move(escucha_), ahora + opciones_.plazo_saludo});
  }
  return std::nullopt;
}

Voz::Persona* Voz::buscar(std::uint32_t id) {
  const auto encontrada = std::find_if(personas_.begin(), personas_.end(),
                                       [id](const Persona& persona) { return persona.id == id; });
  return encontrada == personas_.end() ? nullptr : &*encontrada;
}

}
