#include "common/cifrado/huella.h"

#include <gsl/gsl>
#include <mbedtls/sha256.h>

#include "common/cifrado/base32.h"

extern "C" {
#include <mbedtls/constant_time.h>
}

namespace ventrik::cifrado {
std::string Huella::a_texto() const {
  return a_base32(bytes_);
}

bool operator==(const Huella& a, const Huella& b) {
  return mbedtls_ct_memcmp(a.bytes().data(), b.bytes().data(), Huella::kBytes) == 0;
}

bool operator!=(const Huella& a, const Huella& b) {
  return !(a == b);
}

std::optional<Huella> huella_de_clave(const mbedtls_pk_context& clave) {
  std::array<unsigned char, 512> der{};
  const int largo = mbedtls_pk_write_pubkey_der(&clave, der.data(), der.size());
  if (largo <= 0 || static_cast<std::size_t>(largo) > der.size()) {
    return std::nullopt;
  }
  const auto publica = gsl::span<const unsigned char>(der).last(static_cast<std::size_t>(largo));
  Huella::Bytes bytes{};
  if (mbedtls_sha256(publica.data(), publica.size(), bytes.data(), 0) != 0) {
    return std::nullopt;
  }
  return Huella(bytes);
}

std::optional<Huella> leer_huella(std::string_view texto) {
  Huella::Bytes bytes{};
  if (!de_base32(texto, bytes)) {
    return std::nullopt;
  }
  return Huella(bytes);
}

}
