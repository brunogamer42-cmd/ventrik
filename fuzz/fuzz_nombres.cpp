#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "common/texto.h"
#include "common/utf.h"

namespace {
void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

std::optional<std::vector<std::uint32_t>> decodificar(std::string_view texto) {
  struct Forma {
    std::uint8_t desde, hasta, segundo_desde, segundo_hasta;
    int largo;
  };
  static const Forma kFormas[] = {
      {0x00, 0x7F, 0, 0, 1},       {0xC2, 0xDF, 0x80, 0xBF, 2}, {0xE0, 0xE0, 0xA0, 0xBF, 3},
      {0xE1, 0xEC, 0x80, 0xBF, 3}, {0xED, 0xED, 0x80, 0x9F, 3}, {0xEE, 0xEF, 0x80, 0xBF, 3},
      {0xF0, 0xF0, 0x90, 0xBF, 4}, {0xF1, 0xF3, 0x80, 0xBF, 4}, {0xF4, 0xF4, 0x80, 0x8F, 4},
  };
  std::vector<std::uint32_t> puntos;
  for (std::size_t i = 0; i < texto.size();) {
    const auto byte = [&](std::size_t k) { return static_cast<std::uint8_t>(texto[i + k]); };
    const auto forma = std::find_if(std::begin(kFormas), std::end(kFormas), [&](const Forma& f) {
      return byte(0) >= f.desde && byte(0) <= f.hasta;
    });
    if (forma == std::end(kFormas) || texto.size() - i < static_cast<std::size_t>(forma->largo)) {
      return std::nullopt;
    }
    std::uint32_t punto = byte(0) & (forma->largo == 1 ? 0x7F : 0x7F >> forma->largo);
    for (int k = 1; k < forma->largo; ++k) {
      const auto b = byte(static_cast<std::size_t>(k));
      const bool bien = k == 1 ? b >= forma->segundo_desde && b <= forma->segundo_hasta
                               : b >= 0x80 && b <= 0xBF;
      if (!bien) {
        return std::nullopt;
      }
      punto = (punto << 6) | (b & 0x3Fu);
    }
    puntos.push_back(punto);
    i += static_cast<std::size_t>(forma->largo);
  }
  return puntos;
}

bool en(std::uint32_t punto, std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> rangos) {
  return std::any_of(rangos.begin(), rangos.end(),
                     [punto](const auto& r) { return punto >= r.first && punto <= r.second; });
}

bool es_control(std::uint32_t p) { return en(p, {{0x00, 0x1F}, {0x7F, 0x9F}}); }

bool da_vuelta(std::uint32_t p) { return en(p, {{0x202A, 0x202E}, {0x2066, 0x2069}}); }

bool invisible(std::uint32_t p) {
  return (p & 0xFFFE) == 0xFFFE ||
         en(p, {{0x00A0, 0x00A0}, {0x00AD, 0x00AD}, {0x034F, 0x034F}, {0x061C, 0x061C},
                {0x2800, 0x2800}, {0x13430, 0x1343F}, {0x1BCA0, 0x1BCA3},
                {0x115F, 0x1160}, {0x1680, 0x1680}, {0x17B4, 0x17B5}, {0x180B, 0x180F},
                {0x2000, 0x200F}, {0x2028, 0x202F}, {0x205F, 0x206F}, {0x3000, 0x3000},
                {0x3164, 0x3164}, {0xFDD0, 0xFDEF}, {0xFE00, 0xFE0F}, {0xFEFF, 0xFEFF},
                {0xFFA0, 0xFFA0}, {0xFFF0, 0xFFFB}, {0x1D173, 0x1D17A}, {0xE0000, 0xE0FFF}});
}

bool nombre_esperado(std::string_view texto) {
  const auto puntos = decodificar(texto);
  return puntos && !puntos->empty() && texto.size() <= 64 && puntos->size() <= 32 &&
         puntos->front() != ' ' && puntos->back() != ' ' &&
         std::adjacent_find(puntos->begin(), puntos->end(), [](std::uint32_t a, std::uint32_t b) {
           return a == ' ' && b == ' ';
         }) == puntos->end() &&
         std::none_of(puntos->begin(), puntos->end(), [](std::uint32_t p) {
           return es_control(p) || da_vuelta(p) || invisible(p);
         });
}

bool chat_esperado(std::string_view texto) {
  const auto puntos = decodificar(texto);
  return puntos && texto.size() <= 4000 &&
         std::any_of(puntos->begin(), puntos->end(),
                     [](std::uint32_t p) { return p != ' ' && p != '\n'; }) &&
         std::none_of(puntos->begin(), puntos->end(), [](std::uint32_t p) {
           return p != '\n' && (es_control(p) || da_vuelta(p));
         });
}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const std::string texto(reinterpret_cast<const char*>(datos), largo);
  const bool nombre = ventrik::es_nombre_valido(texto);
  exigir(nombre == nombre_esperado(texto));
  exigir(ventrik::es_chat_valido(texto) == chat_esperado(texto));
  exigir(decodificar(texto).has_value() == ventrik::es_utf8_valido(texto));
  if (nombre) {
    const std::wstring comparable = ventrik::nombre_para_comparar(texto);
    exigir(!comparable.empty());
    exigir(ventrik::nombre_para_comparar(ventrik::a_utf8(comparable)) == comparable);
  }
  return 0;
}
