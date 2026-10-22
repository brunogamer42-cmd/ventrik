#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <mbedtls/pk.h>

namespace ventrik::cifrado {

// SHA-256 de la clave pública, no del certificado: rehacer el certificado no la cambia.
class Huella {
 public:
  static constexpr std::size_t kBytes = 32;
  using Bytes = std::array<std::uint8_t, kBytes>;

  explicit Huella(const Bytes& bytes) : bytes_(bytes) {}

  const Bytes& bytes() const { return bytes_; }
  // Base32 en grupos de 4: sin 0, 1, 8 ni 9, que se confunden con O, I, B y g.
  std::string a_texto() const;

 private:
  Bytes bytes_;
};

// En tiempo constante.
bool operator==(const Huella& a, const Huella& b);
bool operator!=(const Huella& a, const Huella& b);

std::optional<Huella> huella_de_clave(const mbedtls_pk_context& clave);

// Acepta minúsculas, espacios y guiones; cualquier otra cosa, no.
std::optional<Huella> leer_huella(std::string_view texto);

}  // namespace ventrik::cifrado
