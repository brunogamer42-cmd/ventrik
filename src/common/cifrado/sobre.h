#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/secreto.h"

namespace ventrik::cifrado {

constexpr std::uint32_t kMinimoMemoriaArgon2 = 8 * 1024;
constexpr std::uint32_t kMaximoMemoriaArgon2 = 64 * 1024;
constexpr std::uint8_t kMaximoVueltasArgon2 = 10;
constexpr std::size_t kBytesSal = 16;
constexpr std::size_t kBytesClaveSimetrica = 32;
constexpr std::size_t kBytesNonce = 12;
constexpr std::size_t kBytesEtiqueta = 16;
constexpr std::size_t kBytesMarca = 4;
constexpr std::uint32_t kMilisegundosArgon2 = 1000;

struct Argon2 {
  std::uint32_t memoria_kib = 0;
  std::uint8_t vueltas = 0;
};

bool es_argon2_valido(const Argon2& valores);

bool derivar_argon2(std::string_view contrasena, gsl::span<const std::uint8_t> sal, const Argon2& valores,
                    gsl::span<std::uint8_t> salida);

std::optional<Argon2> derivar_a_tiempo(std::string_view contrasena, gsl::span<const std::uint8_t> sal,
                                       std::uint32_t milisegundos, gsl::span<std::uint8_t> salida);

Argon2 memoria_para_esta_pc();

std::optional<std::vector<std::uint8_t>> sellar(gsl::span<const std::uint8_t> clave,
                                                gsl::span<const std::uint8_t> asociados,
                                                gsl::span<const std::uint8_t> datos);

std::optional<Secreto> abrir_sellado(gsl::span<const std::uint8_t> clave, gsl::span<const std::uint8_t> asociados,
                                     gsl::span<const std::uint8_t> sellado);

struct CabeceraSobre {
  Argon2 argon2;
  gsl::span<const std::uint8_t> sal;
  gsl::span<const std::uint8_t> asociados;
  gsl::span<const std::uint8_t> sellado;
};

std::optional<CabeceraSobre> leer_cabecera_sobre(std::string_view marca, gsl::span<const std::uint8_t> sobre);

enum class AperturaSobre { abierto, contrasena, danado, sin_memoria };

struct SobreAbierto {
  AperturaSobre resultado = AperturaSobre::danado;
  std::optional<Secreto> contenido;
};

std::optional<std::vector<std::uint8_t>> cerrar_sobre(std::string_view marca, gsl::span<const std::uint8_t> contenido,
                                                      std::string_view contrasena,
                                                      std::optional<Argon2> fijos = std::nullopt);

SobreAbierto abrir_sobre(std::string_view marca, gsl::span<const std::uint8_t> sobre, std::string_view contrasena);

}  // namespace ventrik::cifrado
