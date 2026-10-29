#include "common/utf.h"

#include <algorithm>
#include <cstddef>
#include <limits>

#include <windows.h>

namespace ventrik {

bool es_utf8_valido(std::string_view texto) {
  const std::size_t largo = texto.size();
  std::size_t i = 0;
  while (i < largo) {
    const auto primero = static_cast<unsigned char>(texto[i]);
    if (primero <= 0x7F) {
      ++i;
      continue;
    }

    // Cuántos bytes siguen y entre qué valores puede estar el segundo
    // (Unicode, tabla 3-7). El resto tiene que estar entre 0x80 y 0xBF.
    std::size_t siguen = 0;
    unsigned char minimo = 0x80;
    unsigned char maximo = 0xBF;
    if (primero >= 0xC2 && primero <= 0xDF) {
      siguen = 1;
    } else if (primero == 0xE0) {
      siguen = 2;
      minimo = 0xA0;  // si no, sería una forma más larga de lo necesario
    } else if ((primero >= 0xE1 && primero <= 0xEC) || primero == 0xEE || primero == 0xEF) {
      siguen = 2;
    } else if (primero == 0xED) {
      siguen = 2;
      maximo = 0x9F;  // si no, sería una mitad de par UTF-16 (U+D800 a U+DFFF)
    } else if (primero == 0xF0) {
      siguen = 3;
      minimo = 0x90;  // si no, sería una forma más larga de lo necesario
    } else if (primero >= 0xF1 && primero <= 0xF3) {
      siguen = 3;
    } else if (primero == 0xF4) {
      siguen = 3;
      maximo = 0x8F;  // si no, se pasaría de U+10FFFF
    } else {
      return false;  // 0x80 a 0xC1 y 0xF5 a 0xFF nunca empiezan un carácter
    }

    if (largo - i <= siguen) {
      return false;  // el texto se corta en la mitad del carácter
    }
    const auto segundo = static_cast<unsigned char>(texto[i + 1]);
    if (segundo < minimo || segundo > maximo) {
      return false;
    }
    for (std::size_t k = 2; k <= siguen; ++k) {
      const auto byte = static_cast<unsigned char>(texto[i + k]);
      if (byte < 0x80 || byte > 0xBF) {
        return false;
      }
    }
    i += siguen + 1;
  }
  return true;
}

std::wstring a_utf16(std::string_view utf8) {
  if (utf8.empty() ||
      utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
      !es_utf8_valido(utf8)) {
    return {};
  }
  const int largo = static_cast<int>(utf8.size());
  const int necesario = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                            utf8.data(), largo, nullptr, 0);
  if (necesario <= 0) {
    return {};
  }
  std::wstring resultado(static_cast<std::size_t>(necesario), L'\0');
  const int escrito = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          utf8.data(), largo, resultado.data(),
                                          necesario);
  if (escrito != necesario) {
    return {};
  }
  return resultado;
}

std::string a_utf8(std::wstring_view utf16) {
  if (utf16.empty() ||
      utf16.size() > static_cast<std::size_t>(std::numeric_limits<int>::max() / 4)) {
    return {};
  }
  std::wstring limpio(utf16);
  for (std::size_t i = 0; i < limpio.size(); ++i) {
    const bool alta = limpio[i] >= 0xD800 && limpio[i] <= 0xDBFF;
    const bool baja = limpio[i] >= 0xDC00 && limpio[i] <= 0xDFFF;
    if (alta && i + 1 < limpio.size() && limpio[i + 1] >= 0xDC00 && limpio[i + 1] <= 0xDFFF) {
      ++i;
    } else if (alta || baja) {
      limpio[i] = 0xFFFD;
    }
  }
  const int largo = static_cast<int>(limpio.size());
  const int necesario =
      WideCharToMultiByte(CP_UTF8, 0, limpio.data(), largo, nullptr, 0, nullptr, nullptr);
  if (necesario <= 0) {
    return {};
  }
  std::string resultado(static_cast<std::size_t>(necesario), '\0');
  const int escrito = WideCharToMultiByte(CP_UTF8, 0, limpio.data(), largo, resultado.data(),
                                          necesario, nullptr, nullptr);
  if (escrito != necesario) {
    return {};
  }
  return resultado;
}

std::size_t cantidad_de_letras(std::string_view texto) {
  return static_cast<std::size_t>(std::count_if(
      texto.begin(), texto.end(), [](char byte) { return (static_cast<unsigned char>(byte) & 0xC0) != 0x80; }));
}

}  // namespace ventrik
