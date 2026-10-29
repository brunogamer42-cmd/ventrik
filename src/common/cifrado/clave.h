#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <gsl/gsl>
#include <mbedtls/pk.h>

#include "common/cifrado/huella.h"
#include "common/cifrado/secreto.h"

namespace ventrik::cifrado {

constexpr std::size_t kBytesClavePublica = 65;
constexpr std::size_t kMaximoFirma = 72;
using ClavePublica = std::array<std::uint8_t, kBytesClavePublica>;
using Resumen = std::array<std::uint8_t, 32>;

class Clave {
 public:
  static std::optional<Clave> crear_nueva();
  static std::optional<Clave> desde_der(gsl::span<const std::uint8_t> der);

  Clave(Clave&&) noexcept;
  Clave& operator=(Clave&&) noexcept;
  ~Clave();

  std::optional<Secreto> der() const;
  const Huella& huella() const;
  const ClavePublica& publica() const;
  std::optional<std::vector<std::uint8_t>> firmar(gsl::span<const std::uint8_t> datos) const;
  mbedtls_pk_context* contexto() const;

 private:
  struct Datos;
  explicit Clave(std::unique_ptr<Datos> datos);
  static std::optional<Clave> completar(std::unique_ptr<Datos> datos);

  std::unique_ptr<Datos> datos_;
};

std::optional<Resumen> resumir(gsl::span<const std::uint8_t> datos);

std::optional<Huella> huella_de_publica(const ClavePublica& publica);

bool verificar_firma(const ClavePublica& publica, gsl::span<const std::uint8_t> datos,
                     gsl::span<const std::uint8_t> firma);

}
