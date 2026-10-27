#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gsl/gsl>

#include "client/sonidos.h"

namespace ventrik::cliente {
constexpr std::size_t kMaximoPaquete = 30 * 1024 * 1024;
constexpr std::size_t kMaximoArchivosEnPaquete = 40;
constexpr std::size_t kMaximoIndiceDePaquete = 64 * 1024;
constexpr std::size_t kMaximoNombreEnPaquete = 128;

struct SonidoDePaquete {
  Sonido sonido = Sonido::entrar_canal;
  std::string nombre;
  Muestras muestras;
};

std::optional<std::vector<std::pair<Sonido, std::string>>> leer_indice(std::string_view json);
std::optional<std::vector<SonidoDePaquete>> leer_paquete(gsl::span<const std::uint8_t> datos);
std::vector<std::uint8_t> armar_paquete(const std::vector<SonidoDePaquete>& sonidos);

}
