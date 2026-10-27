#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include <gsl/gsl>

#include "client/paquete_de_sonidos.h"

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const gsl::span<const std::uint8_t> entrada(datos, largo);
  if (const auto indice = ventrik::cliente::leer_indice(std::string_view(reinterpret_cast<const char*>(datos), largo))) {
    exigir(indice->size() <= largo);
  }
  if (const auto paquete = ventrik::cliente::leer_paquete(entrada)) {
    exigir(paquete->size() <= ventrik::cliente::kSonidos);
    const auto otra_vez = ventrik::cliente::leer_paquete(ventrik::cliente::armar_paquete(*paquete));
    exigir(otra_vez && otra_vez->size() == paquete->size());
  }
  return 0;
}
