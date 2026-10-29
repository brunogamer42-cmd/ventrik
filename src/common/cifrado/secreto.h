#pragma once

#include <cstddef>
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <gsl/gsl>
#include <mbedtls/platform_util.h>

namespace ventrik::cifrado {

inline void borrar(gsl::span<std::uint8_t> bytes) {
  if (!bytes.empty()) {
    mbedtls_platform_zeroize(bytes.data(), bytes.size());
  }
}

class Secreto {
 public:
  Secreto() = default;
  explicit Secreto(std::size_t largo) : datos_(largo) {}
  ~Secreto() { cifrado::borrar(datos_); }
  Secreto(const Secreto&) = delete;
  Secreto& operator=(const Secreto&) = delete;
  Secreto(Secreto&& otro) noexcept : datos_(std::move(otro.datos_)) {}
  Secreto& operator=(Secreto&& otro) noexcept {
    cifrado::borrar(datos_);
    datos_ = std::move(otro.datos_);
    return *this;
  }

  gsl::span<std::uint8_t> bytes() { return datos_; }
  gsl::span<const std::uint8_t> bytes() const { return datos_; }
  std::size_t size() const { return datos_.size(); }

 private:
  std::vector<std::uint8_t> datos_;
};

inline Secreto copiar(gsl::span<const std::uint8_t> original) {
  Secreto copia(original.size());
  std::copy(original.begin(), original.end(), copia.bytes().begin());
  return copia;
}

inline Secreto secreto_de(std::string_view escrita) {
  return copiar(gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(escrita.data()), escrita.size()));
}

inline std::string_view texto_de(const Secreto& secreto) {
  return std::string_view(reinterpret_cast<const char*>(secreto.bytes().data()), secreto.size());
}

}
