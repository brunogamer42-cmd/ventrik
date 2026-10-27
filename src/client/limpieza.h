#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gsl/gsl>

#include "client/voz_primero.h"

struct SpeexEchoState_;
struct SpeexPreprocessState_;

namespace ventrik::cliente {
enum class Ruido : std::uint8_t { apagado, baja, media, alta };

struct Limpiar {
  Ruido ruido = Ruido::apagado;
  bool eco = false;
  bool volumen = false;
};

class Limpieza {
 public:
  explicit Limpieza(unsigned frecuencia);
  ~Limpieza();
  Limpieza(const Limpieza&) = delete;
  Limpieza& operator=(const Limpieza&) = delete;

  std::size_t bloque() const { return bloque_; }
  void usar(const Limpiar& limpiar);
  void escuchar(gsl::span<const std::int16_t> salida);
  int limpiar(gsl::span<std::int16_t> captura);
  Tramo rnnoise() { return {rnnoise_.data(), rnnoise_.size() * sizeof(std::uint64_t)}; }

 private:
  void aplicar(Ruido ruido);

  std::size_t bloque_;
  SpeexEchoState_* eco_ = nullptr;
  SpeexPreprocessState_* ruido_previo_ = nullptr;
  SpeexPreprocessState_* volumen_previo_ = nullptr;
  std::vector<std::uint64_t> rnnoise_;
  std::array<float, 480> entrada_{};
  std::array<float, 480> salida_{};
  std::array<std::int16_t, 480> sin_eco_{};
  float ganancia_ = 1;
  Ruido ruido_puesto_ = Ruido::apagado;
  std::atomic<Ruido> ruido_{Ruido::apagado};
  std::atomic<bool> eco_prendido_{false};
  std::atomic<bool> volumen_{false};
};

}
