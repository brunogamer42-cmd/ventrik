#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace prueba {

inline void tono(std::vector<std::int16_t>& muestras, std::size_t& muestra, unsigned frecuencia, double hz = 440,
                 double amplitud = 8000) {
  for (auto& valor : muestras) {
    const double segundos = static_cast<double>(muestra++) / frecuencia;
    valor = static_cast<std::int16_t>(amplitud * std::sin(2 * 3.14159265 * hz * segundos) *
                                      (0.55 + 0.45 * std::sin(2 * 3.14159265 * 4 * segundos)));
  }
}

inline double potencia_en(const std::vector<std::int16_t>& muestras, std::size_t desde, double hz,
                          unsigned frecuencia) {
  const double coseno = 2 * std::cos(2 * 3.14159265 * hz / frecuencia);
  double anterior = 0;
  double previo = 0;
  for (std::size_t i = desde; i < muestras.size(); ++i) {
    const double actual = muestras[i] + coseno * anterior - previo;
    previo = anterior;
    anterior = actual;
  }
  const double largo = static_cast<double>(muestras.size() - desde);
  return (anterior * anterior + previo * previo - coseno * anterior * previo) / (largo * largo);
}

inline double energia(const std::vector<std::int16_t>& muestras, std::size_t desde) {
  double suma = 0;
  for (std::size_t i = desde; i < muestras.size(); ++i) {
    suma += static_cast<double>(muestras[i]) * muestras[i];
  }
  return suma / static_cast<double>(muestras.size() - desde);
}

}
