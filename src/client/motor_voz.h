#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <gsl/gsl>

#include "client/buffer_voz.h"
#include "client/puente_voz.h"

struct OpusEncoder;
struct OpusDecoder;

namespace ventrik::cliente {
struct Calidad {
  unsigned frecuencia = 48000;
  int complejidad = 10;
  int kbps = 24;
};

Calidad calidad_automatica();

class MotorVoz {
 public:
  MotorVoz(PuenteVoz& puente, const Calidad& calidad);
  MotorVoz(const MotorVoz&) = delete;
  MotorVoz& operator=(const MotorVoz&) = delete;

  bool ok() const { return codificador_ && decodificador_; }
  unsigned frecuencia() const { return calidad_.frecuencia; }
  void transmitir(bool si) { transmitiendo_ = si; }
  bool transmitiendo() const { return transmitiendo_; }
  std::uint32_t espera_ms() const { return espera_ms_.load(std::memory_order_relaxed); }

  void procesar(gsl::span<const std::int16_t> entrada, gsl::span<std::int16_t> salida) noexcept;
  static void procesar_audio(void* motor, gsl::span<const std::int16_t> entrada,
                             gsl::span<std::int16_t> salida) {
    static_cast<MotorVoz*>(motor)->procesar(entrada, salida);
  }

 private:
  struct Borrar {
    void operator()(OpusEncoder* codificador) const;
    void operator()(OpusDecoder* decodificador) const;
  };

  void codificar();
  void decodificar();

  PuenteVoz& puente_;
  Calidad calidad_;
  std::size_t por_paquete_;
  std::unique_ptr<OpusEncoder, Borrar> codificador_;
  std::unique_ptr<OpusDecoder, Borrar> decodificador_;
  std::array<std::int16_t, 960> capturado_{};
  std::size_t capturadas_ = 0;
  std::array<std::int16_t, 5760> reproducir_{};
  std::size_t por_reproducir_ = 0;
  std::size_t reproducidas_ = 0;
  BufferVoz buffer_;
  std::uint32_t hablante_ = 0;
  unsigned quietos_ = 0;
  std::uint16_t numero_ = 0;
  std::atomic<bool> transmitiendo_{false};
  std::atomic<std::uint32_t> espera_ms_{0};
};

}
