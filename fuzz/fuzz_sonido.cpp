#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <gsl/gsl>

#include "client/sonidos.h"

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const gsl::span<const std::uint8_t> entrada(datos, largo);
  const std::size_t maximo = ventrik::cliente::kMaximoSegundosDeSonido * ventrik::cliente::kFrecuenciaDeSonidos;
  if (const auto muestras = ventrik::cliente::decodificar_vksn(entrada, maximo)) {
    exigir(!muestras->empty() && muestras->size() <= maximo);
    const auto otra_vez = ventrik::cliente::decodificar_vksn(ventrik::cliente::codificar_vksn(*muestras), maximo);
    exigir(otra_vez && otra_vez->size() == muestras->size());
  }
  if (const auto muestras = ventrik::cliente::decodificar_archivo(entrada, maximo)) {
    exigir(!muestras->empty() && muestras->size() <= maximo);
  }
  return 0;
}
