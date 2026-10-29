#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include <gsl/gsl>

#include "common/cifrado/boveda.h"
#include "common/protocol/hostear.h"
#include "common/texto.h"
#include "common/utf.h"

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

bool iguales(gsl::span<const std::uint8_t> a, gsl::span<const std::uint8_t> b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end());
}

void probar_servidor_nuevo(gsl::span<const std::uint8_t> datos) {
  using namespace ventrik;
  const auto nuevo = protocolo::leer_servidor_nuevo(datos);
  if (!nuevo) {
    return;
  }
  exigir(es_nombre_valido(nuevo->nombre) && es_nombre_valido(nuevo->nombre_dueno) && es_valida(nuevo->seguridad));
  exigir(nuevo->dueno[0] == 0x04);
  exigir(nuevo->contrasena.size() <= protocolo::kMaximoBytesContrasena);
  exigir(protocolo::es_contrasena_valida(cifrado::texto_de(nuevo->contrasena)));
  exigir(nuevo->arranque.size() == 0 ||
         cantidad_de_letras(cifrado::texto_de(nuevo->arranque)) >= cifrado::kMinimoContrasena);
  exigir(iguales(protocolo::armar_servidor_nuevo(*nuevo).bytes(), datos));
}

void probar_entrada(gsl::span<const std::uint8_t> datos) {
  using namespace ventrik;
  const auto entrada = protocolo::leer_entrada(datos);
  if (!entrada) {
    return;
  }
  for (const cifrado::Secreto* secreto : {&entrada->arranque, &entrada->contrasena}) {
    exigir(protocolo::es_contrasena_valida(cifrado::texto_de(*secreto), protocolo::kMaximoContrasenaDeArranque));
  }
  exigir(iguales(
      protocolo::armar_entrada(cifrado::texto_de(entrada->arranque), cifrado::texto_de(entrada->contrasena)).bytes(),
      datos));
}

void probar_publicos(gsl::span<const std::uint8_t> datos) {
  using namespace ventrik;
  const auto publicos = protocolo::leer_datos_publicos(datos);
  if (!publicos) {
    return;
  }
  exigir(publicos->puerto != 0 && es_nombre_valido(publicos->nombre));
  exigir(iguales(protocolo::armar_datos_publicos(*publicos), datos));
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  if (largo == 0) {
    return 0;
  }
  const gsl::span<const std::uint8_t> resto(datos + 1, largo - 1);
  switch (datos[0] % 4) {
    case 0:
      probar_servidor_nuevo(resto);
      break;
    case 1:
      probar_entrada(resto);
      break;
    case 2:
      probar_publicos(resto);
      break;
    default:
      exigir(!ventrik::cifrado::pide_contrasena_de_arranque(resto) ||
             (resto.size() >= 6 && resto[4] == 1 && resto[5] == 1));
      break;
  }
  return 0;
}
