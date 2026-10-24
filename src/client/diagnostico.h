#pragma once

#include <cstdint>
#include <utility>

namespace ventrik::cliente {
struct Medicion {
  double procesador = 0;
  double ventana = 0;
  double sistema = 0;
  std::uint64_t ram_bytes = 0;
};

class Diagnostico {
 public:
  Diagnostico();
  Medicion medir();

 private:
  std::uint64_t proceso_anterior_ = 0;
  std::uint64_t ventana_anterior_ = 0;
  std::pair<std::uint64_t, std::uint64_t> sistema_anterior_;
  std::uint64_t reloj_anterior_ = 0;
};

}
