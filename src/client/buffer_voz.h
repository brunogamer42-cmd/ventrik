#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "client/puente_voz.h"

namespace ventrik::cliente {
class BufferVoz {
 public:
  enum class Salida { paquete, perdido, nada };

  static constexpr std::size_t kLugares = 64;
  static constexpr std::size_t kTope = kLugares / 2;

  explicit BufferVoz(std::size_t espera = 2) { esperar(espera); }

  void esperar(std::size_t paquetes) { espera_ = std::clamp<std::size_t>(paquetes, 1, kTope); }
  bool llegar(const PaqueteVoz& paquete);
  Salida sacar(PaqueteVoz& paquete);
  void reiniciar();
  std::size_t cantidad() const { return cantidad_; }
  std::size_t recortes() const { return recortes_; }
  std::size_t vaciados() const { return vaciados_; }
  std::size_t faltas() const { return faltas_; }

 private:
  struct Lugar {
    bool ocupado = false;
    PaqueteVoz paquete;
  };

  void vaciar();

  std::size_t espera_ = 2;
  std::array<Lugar, kLugares> lugares_{};
  bool sincronizado_ = false;
  bool llenando_ = true;
  std::uint16_t siguiente_ = 0;
  std::size_t cantidad_ = 0;
  std::size_t recortes_ = 0;
  std::size_t vaciados_ = 0;
  std::size_t faltas_ = 0;
  bool hay_ultimo_ = false;
  std::uint16_t ultimo_ = 0;
};

}
