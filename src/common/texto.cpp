#include "common/texto.h"

#include <cstdint>
#include <vector>

#include <windows.h>

#include "common/utf.h"

namespace ventrik {
namespace {
template <typename Revisar>
bool cada_caracter(std::string_view texto, Revisar&& revisar) {
  std::size_t i = 0;
  while (i < texto.size()) {
    const auto primero = static_cast<unsigned char>(texto[i]);
    std::size_t largo = 1;
    std::uint32_t valor = primero;
    if (primero >= 0xF0) {
      largo = 4;
      valor = primero & 0x07u;
    } else if (primero >= 0xE0) {
      largo = 3;
      valor = primero & 0x0Fu;
    } else if (primero >= 0x80) {
      largo = 2;
      valor = primero & 0x1Fu;
    }
    for (std::size_t k = 1; k < largo; ++k) {
      valor = (valor << 6) | (static_cast<unsigned char>(texto[i + k]) & 0x3Fu);
    }
    if (!revisar(valor)) {
      return false;
    }
    i += largo;
  }
  return true;
}

bool es_control(std::uint32_t c) {
  return c < 0x20 || (c >= 0x7F && c <= 0x9F);
}

bool da_vuelta_el_texto(std::uint32_t c) {
  return (c >= 0x202A && c <= 0x202E) || (c >= 0x2066 && c <= 0x2069);
}

bool no_se_ve(std::uint32_t c) {
  switch (c) {
    case 0x00A0:
    case 0x00AD:
    case 0x034F:
    case 0x061C:
    case 0x115F:
    case 0x1160:
    case 0x1680:
    case 0x2800:
    case 0x17B4:
    case 0x17B5:
    case 0x3000:
    case 0x3164:
    case 0xFEFF:
    case 0xFFA0:
      return true;
    default:
      break;
  }
  return (c >= 0x180B && c <= 0x180F) || (c >= 0x2000 && c <= 0x200F) ||
         (c >= 0x2028 && c <= 0x202F) || (c >= 0x205F && c <= 0x206F) ||
         (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xFFF0 && c <= 0xFFFB) ||
         (c >= 0x1D173 && c <= 0x1D17A) || (c >= 0xE0000 && c <= 0xE0FFF) ||
         (c >= 0x13430 && c <= 0x1343F) || (c >= 0x1BCA0 && c <= 0x1BCA3) ||
         (c >= 0xFDD0 && c <= 0xFDEF) || (c & 0xFFFEu) == 0xFFFEu;
}

}

bool es_nombre_valido(std::string_view nombre) {
  if (nombre.empty() || nombre.size() > kMaximoBytesNombre || !es_utf8_valido(nombre) ||
      nombre.front() == ' ' || nombre.back() == ' ' ||
      nombre.find("  ") != std::string_view::npos) {
    return false;
  }
  std::size_t caracteres = 0;
  return cada_caracter(nombre,
                       [&caracteres](std::uint32_t c) {
                         ++caracteres;
                         return !es_control(c) && !da_vuelta_el_texto(c) && !no_se_ve(c);
                       }) &&
         caracteres <= kMaximoCaracteresNombre;
}

bool es_chat_valido(std::string_view texto) {
  if (texto.empty() || texto.size() > kMaximoBytesChat || !es_utf8_valido(texto)) {
    return false;
  }
  bool hay_algo = false;
  return cada_caracter(texto,
                       [&hay_algo](std::uint32_t c) {
                         if (c == '\n') {
                           return true;
                         }
                         if (es_control(c) || da_vuelta_el_texto(c)) {
                           return false;
                         }
                         hay_algo = hay_algo || c != ' ';
                         return true;
                       }) &&
         hay_algo;
}

template <typename Mapear>
std::wstring mapear(const std::wstring& texto, Mapear&& funcion) {
  const int largo = static_cast<int>(texto.size());
  const int necesario = texto.empty() ? 0 : funcion(texto.data(), largo, nullptr, 0);
  if (necesario <= 0) {
    return {};
  }
  std::wstring resultado(static_cast<std::size_t>(necesario), L' ');
  return funcion(texto.data(), largo, &resultado[0], necesario) == necesario ? resultado
                                                                               : std::wstring();
}

std::wstring nombre_para_comparar(std::string_view nombre) {
  const std::wstring plegado =
      mapear(a_utf16(nombre), [](const wchar_t* texto, int largo, wchar_t* salida, int lugar) {
        return FoldStringW(MAP_FOLDCZONE | MAP_PRECOMPOSED, texto, largo, salida, lugar);
      });
  return mapear(plegado, [](const wchar_t* texto, int largo, wchar_t* salida, int lugar) {
    return LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, texto, largo, salida, lugar);
  });
}

}
