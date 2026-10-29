#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/clave.h"
#include "common/cifrado/huella.h"
#include "common/protocol/bytes.h"

namespace ventrik::cifrado {

constexpr std::size_t kMaximoEslabones = 8;

struct Eslabon {
  ClavePublica vieja{};
  ClavePublica nueva{};
  std::uint32_t numero = 0;
  std::vector<std::uint8_t> firma;
};

using Cadena = std::vector<Eslabon>;

struct CadenaSeguida {
  Huella nueva;
  std::vector<Huella> atras;
};

std::optional<Eslabon> firmar_eslabon(const Clave& vieja, const ClavePublica& nueva, std::uint32_t numero);

bool agregar_eslabon(Cadena& cadena, Eslabon eslabon);

void escribir_cadena(protocolo::Escritor& escritor, const Cadena& cadena, bool vacia_vale = false);
void leer_cadena(protocolo::Lector& lector, Cadena& cadena, bool vacia_vale = false);
std::vector<std::uint8_t> escribir_cadena(const Cadena& cadena);
std::optional<Cadena> leer_cadena(gsl::span<const std::uint8_t> datos);

std::optional<CadenaSeguida> seguir_cadena(const Cadena& cadena, const Huella& guardada, const Huella& de_la_conexion,
                                           gsl::span<const Huella> atras);

std::optional<Huella> origen_de_la_cadena(const Cadena& cadena, const Huella& de_la_conexion);

}
