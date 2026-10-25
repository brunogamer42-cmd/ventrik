#include "client/canal_voz.h"

#include <algorithm>
#include <array>
#include <vector>

#include "common/cifrado/inicio.h"

namespace ventrik::cliente {
namespace {
using namespace std::chrono_literals;

constexpr std::chrono::seconds kCadaCuantoPing{5};
constexpr std::chrono::seconds kSinRespuesta{15};
constexpr std::chrono::seconds kPlazoSaludo{10};
constexpr std::chrono::milliseconds kPrimeraEspera{2000};
constexpr std::chrono::milliseconds kEsperaMaxima{60000};
constexpr std::chrono::milliseconds kDuranteElSaludo{100};
constexpr int kLecturasPorVuelta = 32;
constexpr int kMaximoSalto = 50;

std::uint64_t milisegundos(CanalVoz::Reloj::time_point momento) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(momento.time_since_epoch()).count());
}

}

CanalVoz::CanalVoz(const red::Direccion& servidor, PuenteVoz& puente)
    : servidor_(servidor), puente_(puente) {}

void CanalVoz::clave(const protocolo::ClaveVoz& nueva, cifrado::Secreto secreto) {
  sesion_.reset();
  clave_ = nueva;
  secreto_ = std::move(secreto);
  lista_ = false;
  espera_ = kPrimeraEspera;
  reintentar_en_ = {};
}

std::chrono::milliseconds CanalVoz::hasta_lo_proximo(Reloj::time_point ahora) const {
  const auto hasta = [ahora](Reloj::time_point cuando) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(cuando - ahora);
  };
  return lista_    ? std::min(hasta(proximo_ping_), hasta(ultimo_dato_ + kSinRespuesta))
         : sesion_ ? kDuranteElSaludo
         : clave_  ? hasta(reintentar_en_)
                   : kEsperaMaxima;
}

void CanalVoz::atender(Reloj::time_point ahora) {
  PaqueteVoz paquete;
  if (clave_ && !sesion_ && ahora >= reintentar_en_) {
    empezar(ahora);
  }
  if (sesion_ && !lista_) {
    const auto paso = sesion_->apreton();
    if (paso == cifrado::Paso::listo) {
      lista_ = true;
      puente_.estado = EstadoVoz::lista;
      ultimo_dato_ = ahora;
      proximo_ping_ = ahora;
      espera_ = kPrimeraEspera;
    } else if (paso == cifrado::Paso::error || paso == cifrado::Paso::cerrado ||
               ahora - inicio_saludo_ > kPlazoSaludo) {
      fallar(ahora);
    }
  }
  if (!lista_) {
    while (puente_.subida.sacar(paquete)) {
    }
    return;
  }
  leer(ahora);
  while (lista_ && puente_.subida.sacar(paquete)) {
    enviar(protocolo::MedioSubida{
        {protocolo::TipoFlujo::voz, 0, paquete.numero,
         std::vector<std::uint8_t>(paquete.opus.begin(), paquete.opus.begin() + paquete.largo)}});
  }
  if (lista_ && ahora >= proximo_ping_) {
    enviar(protocolo::Ping{milisegundos(ahora)});
    proximo_ping_ = ahora + kCadaCuantoPing;
  }
  if (lista_ && ahora - ultimo_dato_ > kSinRespuesta) {
    fallar(ahora);
  }
}

template <typename Mensaje>
void CanalVoz::enviar(const Mensaje& mensaje) {
  std::vector<std::uint8_t> datagrama;
  std::size_t escritos = 0;
  if (protocolo::armar_datagrama(mensaje, datagrama)) {
    sesion_->escribir(datagrama, escritos);
  }
}

void CanalVoz::empezar(Reloj::time_point ahora) {
  config_ = cifrado::ConfigTls::para_voz_programa(clave_->identidad, secreto_.bytes());
  socket_ = red::conectar_udp(servidor_);
  if (config_ && socket_.valido()) {
    sesion_ = std::make_unique<cifrado::SesionTls>(*config_, socket_.get());
  }
  if (!sesion_ || !sesion_->ok()) {
    fallar(ahora);
    return;
  }
  inicio_saludo_ = ahora;
  puente_.estado = EstadoVoz::conectando;
}

void CanalVoz::fallar(Reloj::time_point ahora) {
  sesion_.reset();
  socket_ = red::Socket();
  lista_ = false;
  puente_.estado = EstadoVoz::sin_conexion;
  unsigned char azar = 0;
  cifrado::aleatorio(nullptr, &azar, 1);
  reintentar_en_ = ahora + espera_ + espera_ * azar / 1024;
  espera_ = std::min(espera_ * 2, kEsperaMaxima);
}

void CanalVoz::leer(Reloj::time_point ahora) {
  std::array<std::uint8_t, protocolo::kMaximoDatagrama> leido{};
  for (int vuelta = 0; vuelta < kLecturasPorVuelta; ++vuelta) {
    std::size_t largo = 0;
    const auto paso = sesion_->leer(leido, largo);
    if (paso != cifrado::Paso::listo) {
      if (paso != cifrado::Paso::esperar_lectura) {
        fallar(ahora);
      }
      return;
    }
    recibir(gsl::span<const std::uint8_t>(leido).first(largo), ahora);
  }
}

void CanalVoz::recibir(gsl::span<const std::uint8_t> registro, Reloj::time_point ahora) {
  ultimo_dato_ = ahora;
  const auto marco = protocolo::separar_datagrama(registro);
  if (marco) {
    if (marco->tipo == static_cast<std::uint16_t>(protocolo::TipoMensaje::medio_bajada)) {
      if (const auto bajada = protocolo::leer_mensaje<protocolo::MedioBajada>(marco->cuerpo)) {
        contar(*bajada);
        PaqueteVoz paquete;
        paquete.hablante = bajada->origen;
        paquete.numero = bajada->medio.numero;
        paquete.largo = static_cast<std::uint16_t>(bajada->medio.datos.size());
        std::copy(bajada->medio.datos.begin(), bajada->medio.datos.end(), paquete.opus.begin());
        puente_.bajada.poner(paquete);
      }
    } else if (marco->tipo == static_cast<std::uint16_t>(protocolo::TipoMensaje::pong)) {
      const auto pong = protocolo::leer_mensaje<protocolo::Pong>(marco->cuerpo);
      const auto ahora_ms = milisegundos(ahora);
      if (pong && pong->marca <= ahora_ms) {
        puente_.ping_ms = static_cast<std::uint32_t>(std::min<std::uint64_t>(ahora_ms - pong->marca,
                                                                             0xFFFFFFFFu));
      }
    }
  }
}

void CanalVoz::contar(const protocolo::MedioBajada& bajada) {
  const auto anterior = siguiente_.find(bajada.origen);
  if (anterior != siguiente_.end()) {
    const auto salto = static_cast<std::int16_t>(bajada.medio.numero - anterior->second);
    if (salto < 0) {
      return;
    }
    if (salto < kMaximoSalto) {
      puente_.perdidos += static_cast<std::uint32_t>(salto);
    }
  }
  ++puente_.recibidos;
  siguiente_[bajada.origen] = static_cast<std::uint16_t>(bajada.medio.numero + 1);
}

}
