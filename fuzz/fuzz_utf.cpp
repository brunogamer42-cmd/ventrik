#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include <windows.h>

#include "common/utf.h"

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

// Escrita distinto a es_utf8_valido a propósito: decodifica cada carácter y revisa su valor.
bool utf8_valido_segun_referencia(std::string_view texto) {
  static const std::uint32_t kMinimo[] = {0, 0x80, 0x800, 0x10000};
  std::size_t i = 0;
  while (i < texto.size()) {
    const auto primero = static_cast<unsigned char>(texto[i]);
    std::size_t siguen = 0;
    std::uint32_t valor = 0;
    if (primero < 0x80) {
      valor = primero;
    } else if ((primero & 0xE0) == 0xC0) {
      siguen = 1;
      valor = primero & 0x1Fu;
    } else if ((primero & 0xF0) == 0xE0) {
      siguen = 2;
      valor = primero & 0x0Fu;
    } else if ((primero & 0xF8) == 0xF0) {
      siguen = 3;
      valor = primero & 0x07u;
    } else {
      return false;
    }
    if (texto.size() - i <= siguen) {
      return false;
    }
    for (std::size_t k = 1; k <= siguen; ++k) {
      const auto byte = static_cast<unsigned char>(texto[i + k]);
      if ((byte & 0xC0) != 0x80) {
        return false;
      }
      valor = (valor << 6) | (byte & 0x3Fu);
    }
    if (valor < kMinimo[siguen] || valor > 0x10FFFF || (valor >= 0xD800 && valor <= 0xDFFF)) {
      return false;
    }
    i += siguen + 1;
  }
  return true;
}

std::string de_vuelta_a_utf8(const std::wstring& texto) {
  const int largo = static_cast<int>(texto.size());
  const int necesario = WideCharToMultiByte(CP_UTF8, 0, texto.data(), largo, nullptr, 0, nullptr, nullptr);
  exigir(necesario > 0);
  std::string resultado(static_cast<std::size_t>(necesario), '\0');
  exigir(WideCharToMultiByte(CP_UTF8, 0, texto.data(), largo, resultado.data(), necesario,
                             nullptr, nullptr) == necesario);
  return resultado;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const std::string texto(datos, datos + largo);
  const bool valido = ventrik::es_utf8_valido(texto);
  exigir(valido == utf8_valido_segun_referencia(texto));

  const std::wstring convertido = ventrik::a_utf16(texto);
  if (!valido || texto.empty()) {
    exigir(convertido.empty());
    return 0;
  }
  exigir(!convertido.empty());
  exigir(de_vuelta_a_utf8(convertido) == texto);
  return 0;
}
