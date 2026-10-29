#include "common/cifrado/base32.h"

namespace ventrik::cifrado {
namespace {
constexpr char kAlfabeto[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
constexpr std::size_t kGrupo = 4;
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

}

std::string a_base32(gsl::span<const std::uint8_t> bytes, bool con_guiones) {
  std::string texto;
  std::uint32_t acumulado = 0;
  int bits = 0;
  std::size_t letras = 0;
  const auto agregar = [&](std::uint32_t valor) {
    if (con_guiones && letras > 0 && letras % kGrupo == 0) {
      texto.push_back('-');
    }
    texto.push_back(kAlfabeto[valor & 31]);
    ++letras;
  };
  for (const std::uint8_t byte : bytes) {
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

bool de_base32(std::string_view texto, gsl::span<std::uint8_t> destino) {
  if (texto.size() > kMaximoTexto) {
    return false;
  }
  const std::size_t letras_esperadas = (destino.size() * 8 + 4) / 5;
  std::size_t llenos = 0;
  std::size_t letras = 0;
  std::uint32_t acumulado = 0;
  int bits = 0;
  for (const char letra : texto) {
    if (letra == '-' || letra == ' ') {
      continue;
    }
    const int valor = valor_de_letra(letra);
    if (valor < 0 || letras == letras_esperadas) {
      return false;
    }
    ++letras;
    acumulado = (acumulado << 5) | static_cast<std::uint32_t>(valor);
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      if (llenos == destino.size()) {
        return false;
      }
      destino[llenos++] = static_cast<std::uint8_t>(acumulado >> bits);
    }
    acumulado &= (1u << bits) - 1;
  }
  return letras == letras_esperadas && llenos == destino.size() && acumulado == 0;
}

}
