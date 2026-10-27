#include "client/prueba_de_audio.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "client/motor_voz.h"

namespace ventrik::cliente {
namespace {
constexpr unsigned kFrecuencia = 48000;
constexpr double kTono = 660;
constexpr double kFuerza = 6000;

}

bool PruebaDeAudio::abrir(const ElegidosDeAudio& elegidos, bool tono) {
  tono_ = tono;
  muestra_ = 0;
  pico_ = 0;
  return audio_.abrir(kFrecuencia, &PruebaDeAudio::procesar, this, elegidos, !tono);
}

int PruebaDeAudio::nivel(int segmentos) const { return segmentos_de(pico_.load(std::memory_order_relaxed), segmentos); }

void PruebaDeAudio::procesar(void* yo, gsl::span<const std::int16_t> entrada, gsl::span<std::int16_t> salida) {
  auto& prueba = *static_cast<PruebaDeAudio*>(yo);
  int pico = 0;
  for (const std::int16_t muestra : entrada) {
    pico = std::max(pico, std::abs(static_cast<int>(muestra)));
  }
  prueba.pico_.store(pico, std::memory_order_relaxed);
  std::fill(salida.begin(), salida.end(), std::int16_t{0});
  if (prueba.tono_) {
    for (auto& muestra : salida) {
      const double segundos = static_cast<double>(prueba.muestra_++) / kFrecuencia;
      muestra = static_cast<std::int16_t>(kFuerza * std::sin(2 * 3.14159265358979 * kTono * segundos));
    }
  } else if (prueba.escucharme_) {
    std::copy_n(entrada.begin(), std::min(entrada.size(), salida.size()), salida.begin());
  }
}

}
