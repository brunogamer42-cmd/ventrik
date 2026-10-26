#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace prueba {

inline void tono(std::vector<std::int16_t>& muestras, std::size_t& muestra, unsigned frecuencia) {
  for (auto& valor : muestras) {
    const double segundos = static_cast<double>(muestra++) / frecuencia;
    valor = static_cast<std::int16_t>(8000 * std::sin(2 * 3.14159265 * 440 * segundos) *
                                      (0.55 + 0.45 * std::sin(2 * 3.14159265 * 4 * segundos)));
  }
}

inline double energia(const std::vector<std::int16_t>& muestras, std::size_t desde) {
  double suma = 0;
  for (std::size_t i = desde; i < muestras.size(); ++i) {
    suma += static_cast<double>(muestras[i]) * muestras[i];
  }
  return suma / static_cast<double>(muestras.size() - desde);
}

}
