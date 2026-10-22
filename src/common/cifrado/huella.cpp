#include "common/cifrado/huella.h"

#include <gsl/gsl>
#include <mbedtls/sha256.h>

// Este encabezado de Mbed TLS 3.6 no avisa que es de C.
extern "C" {
#include <mbedtls/constant_time.h>
}

namespace ventrik::cifrado {

namespace {

constexpr char kAlfabeto[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
constexpr std::size_t kLetras = (Huella::kBytes * 8 + 4) / 5;
constexpr std::size_t kGrupo = 4;
// Una huella escrita con espacios de más sigue siendo corta; esto frena textos enormes.
constexpr std::size_t kMaximoTexto = 256;

int valor_de_letra(char letra) {
  if (letra >= 'a' && letra <= 'z') {
    letra = static_cast<char>(letra - 'a' + 'A');
  }
  if (letra >= 'A' && letra <= 'Z') {
    return letra - 'A';
  }
  if (letra >= '2' && letra <= '7') {
    return letra - '2' + 26;
  }
  return -1;
}

}  // namespace

std::string Huella::a_texto() const {
  std::string texto;
  std::uint32_t acumulado = 0;
  int bits = 0;
  std::size_t letras = 0;
  const auto agregar = [&](std::uint32_t valor) {
    if (letras > 0 && letras % kGrupo == 0) {
      texto.push_back('-');
    }
    texto.push_back(kAlfabeto[valor & 31]);
    ++letras;
  };
  for (const std::uint8_t byte : bytes_) {
    acumulado = (acumulado << 8) | byte;
    bits += 8;
    while (bits >= 5) {
      bits -= 5;
      agregar(acumulado >> bits);
      acumulado &= (1u << bits) - 1;
    }
  }
  if (bits > 0) {
    agregar(acumulado << (5 - bits));
  }
  return texto;
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
  // Mbed TLS escribe al final del espacio que recibe.
  const auto publica = gsl::span<const unsigned char>(der).last(static_cast<std::size_t>(largo));
  Huella::Bytes bytes{};
  if (mbedtls_sha256(publica.data(), publica.size(), bytes.data(), 0) != 0) {
    return std::nullopt;
  }
  return Huella(bytes);
}

std::optional<Huella> leer_huella(std::string_view texto) {
  if (texto.size() > kMaximoTexto) {
    return std::nullopt;
  }
  Huella::Bytes bytes{};
  std::size_t llenos = 0;
  std::size_t letras = 0;
  std::uint32_t acumulado = 0;
  int bits = 0;
  for (const char letra : texto) {
    if (letra == '-' || letra == ' ') {
      continue;
    }
    const int valor = valor_de_letra(letra);
    if (valor < 0 || letras == kLetras) {
      return std::nullopt;
    }
    ++letras;
    acumulado = (acumulado << 5) | static_cast<std::uint32_t>(valor);
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      if (llenos == bytes.size()) {
        return std::nullopt;
      }
      bytes[llenos++] = static_cast<std::uint8_t>(acumulado >> bits);
    }
    acumulado &= (1u << bits) - 1;
  }
  // Los bits que sobran al final tienen que ser 0: una sola forma de escribir cada huella.
  if (letras != kLetras || llenos != bytes.size() || acumulado != 0) {
    return std::nullopt;
  }
  return Huella(bytes);
}

}  // namespace ventrik::cifrado
