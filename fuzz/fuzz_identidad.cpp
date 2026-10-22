#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>

#include <gsl/gsl>

#include "common/cifrado/huella.h"
#include "common/cifrado/identidad.h"
#include "common/cifrado/inicio.h"

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

std::unique_ptr<ventrik::cifrado::Inicio> cifrado;

}  // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  cifrado = std::make_unique<ventrik::cifrado::Inicio>();
  exigir(cifrado->ok());
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const auto identidad =
      ventrik::cifrado::IdentidadServidor::desde_der(gsl::span<const std::uint8_t>(datos, largo));
  if (!identidad) {
    return 0;
  }
  const auto der = identidad->clave_der();
  exigir(der.has_value());
  const auto otra_vez = ventrik::cifrado::IdentidadServidor::desde_der(der->bytes());
  exigir(otra_vez && otra_vez->huella() == identidad->huella());
  return 0;
}
