#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include "common/cifrado/codigo_dueno.h"
#include "common/cifrado/huella.h"
#include "common/red/red.h"

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const std::string_view texto(reinterpret_cast<const char*>(datos), largo);

  if (const auto huella = ventrik::cifrado::leer_huella(texto)) {
    const std::string escrita = huella->a_texto();
    exigir(escrita.size() == 64);
    const auto otra_vez = ventrik::cifrado::leer_huella(escrita);
    exigir(otra_vez && *otra_vez == *huella);
  }

  if (const auto codigo = ventrik::cifrado::leer_codigo_dueno(texto)) {
    const auto otra_vez = ventrik::cifrado::leer_codigo_dueno(codigo->a_texto());
    exigir(otra_vez && *otra_vez == *codigo);
  }

  if (const auto direccion = ventrik::red::leer_direccion(texto)) {
    exigir(direccion->puerto != 0);
    const auto otra_vez = ventrik::red::leer_direccion(ventrik::red::a_texto(*direccion));
    exigir(otra_vez && *otra_vez == *direccion);
  }
  return 0;
}
