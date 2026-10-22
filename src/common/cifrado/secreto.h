#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <gsl/gsl>
#include <mbedtls/platform_util.h>

namespace ventrik::cifrado {

// Las claves en memoria se borran apenas dejan de usarse (diseño: "Claves en la memoria").
// Tamaño fijo desde el principio: al agrandarse, vector dejaría copias sin borrar.
class Secreto {
 public:
  Secreto() = default;
  explicit Secreto(std::size_t largo) : datos_(largo) {}
  ~Secreto() { borrar(); }
  Secreto(const Secreto&) = delete;
  Secreto& operator=(const Secreto&) = delete;
  Secreto(Secreto&& otro) noexcept : datos_(std::move(otro.datos_)) {}
  Secreto& operator=(Secreto&& otro) noexcept {
    borrar();
    datos_ = std::move(otro.datos_);
    return *this;
  }

  gsl::span<std::uint8_t> bytes() { return datos_; }
  gsl::span<const std::uint8_t> bytes() const { return datos_; }
  std::size_t size() const { return datos_.size(); }

 private:
  void borrar() {
    if (!datos_.empty()) {
      mbedtls_platform_zeroize(datos_.data(), datos_.size());
    }
  }

  std::vector<std::uint8_t> datos_;
};

}  // namespace ventrik::cifrado
