#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "client/puente_voz.h"

namespace ventrik::cliente {
class BufferVoz {
 public:
  enum class Salida { paquete, perdido, nada };

  explicit BufferVoz(std::size_t espera = 2) : espera_(espera == 0 ? 1 : espera) {}

  void llegar(const PaqueteVoz& paquete);
  Salida sacar(PaqueteVoz& paquete);
  void reiniciar();

 private:
  static constexpr std::size_t kLugares = 64;

  struct Lugar {
    bool ocupado = false;
    PaqueteVoz paquete;
  };

  void vaciar();

  std::size_t espera_;
  std::array<Lugar, kLugares> lugares_{};
  bool sincronizado_ = false;
  bool llenando_ = true;
  std::uint16_t siguiente_ = 0;
  std::size_t cantidad_ = 0;
  bool hay_ultimo_ = false;
  std::uint16_t ultimo_ = 0;
};

}
