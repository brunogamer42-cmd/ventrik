#pragma once

#include <atomic>
#include <cstdint>

#include <gsl/gsl>

#include "client/audio.h"

namespace ventrik::cliente {
class PruebaDeAudio {
 public:
  PruebaDeAudio() = default;
  PruebaDeAudio(const PruebaDeAudio&) = delete;
  PruebaDeAudio& operator=(const PruebaDeAudio&) = delete;

  bool abrir(const ElegidosDeAudio& elegidos, bool tono);
  void escucharme(bool si) { escucharme_ = si; }
  int nivel(int segmentos) const;
  static void procesar(void* yo, gsl::span<const std::int16_t> entrada, gsl::span<std::int16_t> salida);

 private:
  std::atomic<int> pico_{0};
  std::atomic<bool> escucharme_{false};
  bool tono_ = false;
  std::uint64_t muestra_ = 0;
  Audio audio_;
};

}
