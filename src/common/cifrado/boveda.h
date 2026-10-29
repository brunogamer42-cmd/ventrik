#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/secreto.h"
#include "common/cifrado/sobre.h"

namespace ventrik::cifrado {

constexpr std::size_t kBytesBoveda = 32;
constexpr std::size_t kMaximoArchivoBoveda = 4096;
constexpr std::size_t kMinimoContrasenaArranque = 8;
constexpr std::string_view kEtiquetaClaveServidor = "ventrik clave del servidor v1";
constexpr std::string_view kEtiquetaHistorial = "ventrik historial v1";

class Boveda {
 public:
  static std::optional<Boveda> crear_nueva();
  explicit Boveda(Secreto clave) : clave_(std::move(clave)) {}

  std::optional<Secreto> derivar(std::string_view etiqueta) const;
  std::optional<std::vector<std::uint8_t>> armar_archivo(std::optional<std::string_view> contrasena,
                                                         std::optional<Argon2> fijos = std::nullopt) const;

 private:
  Secreto clave_;
};

enum class AperturaBoveda { abierta, falta_contrasena, contrasena, otro_windows, danada, sin_memoria };

struct BovedaAbierta {
  AperturaBoveda resultado = AperturaBoveda::danada;
  std::optional<Boveda> boveda;
};

BovedaAbierta abrir_boveda(gsl::span<const std::uint8_t> archivo, std::optional<std::string_view> contrasena);

}
