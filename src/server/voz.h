#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/secreto.h"
#include "common/cifrado/tls.h"
#include "common/protocol/pedidos.h"
#include "common/red/red.h"
#include "server/proteccion.h"

namespace ventrik::servidor {
struct OpcionesVoz {
  std::chrono::seconds rotacion{3600};
  std::size_t pendientes_por_ip = 4;
  std::size_t pendientes_en_total = 64;
  std::chrono::seconds plazo_saludo{10};
  std::chrono::seconds sin_datos{60};
  double rafaga_paquetes = 25;
  double paquetes_por_segundo = 60;
  double rafaga_datagramas = 50;
  double datagramas_por_segundo = 120;
  double rafaga_saludos_por_ip = 20;
  double saludos_por_segundo_por_ip = 10;
  double rafaga_escucha = 400;
  double escucha_por_segundo = 200;
};

using Derivar = std::function<bool(gsl::span<const std::uint8_t>, gsl::span<std::uint8_t>)>;

class Voz {
 public:
  Voz(OpcionesVoz opciones, cifrado::EnviarDatagrama enviar);
  Voz(const Voz&) = delete;
  Voz& operator=(const Voz&) = delete;

  bool ok() const { return config_ != nullptr; }
  std::optional<protocolo::ClaveVoz> alta(std::uint32_t id, std::uint32_t ip, std::uint32_t canal,
                                          Instante ahora, const Derivar& derivar);
  std::optional<protocolo::ClaveVoz> renovar(std::uint32_t id, Instante ahora,
                                             const Derivar& derivar);
  void mover(std::uint32_t id, std::uint32_t canal);
  void baja(std::uint32_t id);
  std::optional<std::uint32_t> recibir(const red::Direccion& origen,
                                       gsl::span<const std::uint8_t> datagrama, Instante ahora);
  std::vector<std::uint32_t> revisar(Instante ahora);

  bool tiene_voz(std::uint32_t id) const;
  std::size_t pendientes() const { return pendientes_.size(); }

 private:
  struct Persona {
    std::uint32_t id = 0;
    std::uint32_t ip = 0;
    std::uint32_t canal = 0;
    std::array<std::uint8_t, 16> identidad{};
    cifrado::Secreto clave{32};
    Instante rotar_en;
    std::unique_ptr<cifrado::SesionTls> sesion;
    red::Direccion direccion;
    Cubeta paquetes;
    Cubeta datagramas;
    Cubeta saludos;
    Instante ultimo;
  };
  struct Pendiente {
    red::Direccion origen;
    std::unique_ptr<cifrado::SesionTls> sesion;
    Instante limite;
  };

  std::optional<protocolo::ClaveVoz> nueva_clave(Persona& persona, Instante ahora,
                                                  const Derivar& derivar);
  gsl::span<const std::uint8_t> elegir(cifrado::SesionTls& sesion,
                                       gsl::span<const std::uint8_t> identidad);
  void atender(Persona& persona, gsl::span<const std::uint8_t> datagrama, Instante ahora);
  void reenviar(const Persona& hablante, protocolo::Medio medio);
  void escribir(cifrado::SesionTls& sesion, const std::vector<std::uint8_t>& datagrama);
  std::optional<std::uint32_t> saludar(gsl::span<const std::uint8_t> datagrama,
                                       const red::Direccion& origen, Instante ahora);
  Persona* buscar(std::uint32_t id);

  OpcionesVoz opciones_;
  cifrado::EnviarDatagrama enviar_;
  std::unique_ptr<cifrado::ConfigTls> config_;
  std::unique_ptr<cifrado::SesionTls> escucha_;
  Cubeta cubeta_escucha_;
  std::vector<Persona> personas_;
  std::vector<std::uint8_t> datagrama_;
  std::vector<Pendiente> pendientes_;
};

}
