#pragma once

#include <cstdint>

namespace ventrik::cliente {
struct Medicion {
  double procesador = 0;
  std::uint64_t ram_bytes = 0;
};

class Diagnostico {
 public:
  Diagnostico();
  Medicion medir();

 private:
  std::uint64_t proceso_anterior_ = 0;
  std::uint64_t reloj_anterior_ = 0;
};

}
