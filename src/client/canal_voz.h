#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>

#include "client/puente_voz.h"
#include "common/cifrado/secreto.h"
#include "common/cifrado/tls.h"
#include "common/protocol/pedidos.h"
#include "common/red/red.h"

namespace ventrik::cliente {
class CanalVoz {
 public:
  using Reloj = std::chrono::steady_clock;

  CanalVoz(const red::Direccion& servidor, PuenteVoz& puente);
  CanalVoz(const CanalVoz&) = delete;
  CanalVoz& operator=(const CanalVoz&) = delete;

  void clave(const protocolo::ClaveVoz& nueva, cifrado::Secreto secreto);
  void atender(Reloj::time_point ahora);
  const red::Socket& socket() const { return socket_; }
  void recibir(gsl::span<const std::uint8_t> registro, Reloj::time_point ahora);

 private:
  template <typename Mensaje>
  void enviar(const Mensaje& mensaje);
  void empezar(Reloj::time_point ahora);
  void fallar(Reloj::time_point ahora);
  void leer(Reloj::time_point ahora);
  void contar(const protocolo::MedioBajada& bajada);

  red::Direccion servidor_;
  PuenteVoz& puente_;
  std::optional<protocolo::ClaveVoz> clave_;
  cifrado::Secreto secreto_;
  std::unique_ptr<cifrado::ConfigTls> config_;
  red::Socket socket_;
  std::unique_ptr<cifrado::SesionTls> sesion_;
  bool lista_ = false;
  Reloj::time_point inicio_saludo_;
  Reloj::time_point reintentar_en_;
  Reloj::time_point proximo_ping_;
  Reloj::time_point ultimo_dato_;
  std::chrono::milliseconds espera_{2000};
  std::map<std::uint32_t, std::uint16_t> siguiente_;
};

}
