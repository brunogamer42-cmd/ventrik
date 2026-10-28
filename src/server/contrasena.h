#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "common/cifrado/sobre.h"
#include "common/protocol/pedidos.h"

namespace ventrik::servidor {

constexpr std::uint32_t kMilisegundosArgon2Servidor = 400;

enum class Comprobado : std::uint8_t { coincide, distinta, fallo };

struct ContrasenaGuardada {
  cifrado::Argon2 argon2;
  std::array<std::uint8_t, cifrado::kBytesSal> sal{};
  std::array<std::uint8_t, cifrado::kBytesClaveSimetrica> resumen{};
};

std::optional<std::string> guardar_contrasena(std::string_view contrasena,
                                              std::optional<cifrado::Argon2> fijos = std::nullopt);
std::optional<ContrasenaGuardada> leer_contrasena(std::string_view texto);
Comprobado comprobar(const ContrasenaGuardada& guardada, std::string_view contrasena);

}
