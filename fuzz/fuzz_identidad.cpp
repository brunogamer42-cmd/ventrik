#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>

#include <gsl/gsl>

#include "client/mi_identidad.h"
#include "common/cifrado/clave.h"
#include "common/cifrado/huella.h"
#include "common/cifrado/identidad.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/sobre.h"

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

std::unique_ptr<ventrik::cifrado::Inicio> cifrado;

void clave_del_servidor(gsl::span<const std::uint8_t> datos) {
  const auto identidad = ventrik::cifrado::IdentidadServidor::desde_der(datos);
  if (!identidad) {
    return;
  }
  const auto der = identidad->clave_der();
  exigir(der.has_value());
  const auto otra_vez = ventrik::cifrado::IdentidadServidor::desde_der(der->bytes());
  exigir(otra_vez && otra_vez->huella() == identidad->huella());
}

void contenido_de_copia(gsl::span<const std::uint8_t> datos) {
  const auto clave = ventrik::cliente::leer_contenido_de_copia(datos);
  if (!clave) {
    return;
  }
  const auto huella = ventrik::cifrado::huella_de_publica(clave->publica());
  exigir(huella && *huella == clave->huella());
}

void sobre(gsl::span<const std::uint8_t> datos) {
  const auto cabecera = ventrik::cifrado::leer_cabecera_sobre("VKID", datos);
  if (!cabecera) {
    return;
  }
  exigir(cabecera->argon2.memoria_kib >= ventrik::cifrado::kMinimoMemoriaArgon2 &&
         cabecera->argon2.memoria_kib <= ventrik::cifrado::kMaximoMemoriaArgon2 && cabecera->argon2.vueltas >= 1 &&
         cabecera->argon2.vueltas <= ventrik::cifrado::kMaximoVueltasArgon2);
  exigir(cabecera->asociados.size() + cabecera->sellado.size() == datos.size());
  const std::array<std::uint8_t, ventrik::cifrado::kBytesClaveSimetrica> clave{};
  const auto abierto = ventrik::cifrado::abrir_sellado(clave, cabecera->asociados, cabecera->sellado);
  if (abierto) {
    exigir(abierto->size() + ventrik::cifrado::kBytesNonce + ventrik::cifrado::kBytesEtiqueta ==
           cabecera->sellado.size());
  }
}

void publica_y_firma(gsl::span<const std::uint8_t> datos) {
  if (datos.size() <= ventrik::cifrado::kBytesClavePublica) {
    return;
  }
  ventrik::cifrado::ClavePublica publica{};
  const auto primeros = datos.first(publica.size());
  std::copy(primeros.begin(), primeros.end(), publica.begin());
  const auto resto = datos.subspan(publica.size() + 1);
  const std::size_t largo_firmado = std::min<std::size_t>(datos[publica.size()], resto.size());
  const bool valida = ventrik::cifrado::huella_de_publica(publica).has_value();
  if (ventrik::cifrado::verificar_firma(publica, resto.first(largo_firmado), resto.subspan(largo_firmado))) {
    exigir(valida);
  }
}

}  // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  cifrado = std::make_unique<ventrik::cifrado::Inicio>();
  exigir(cifrado->ok());
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const gsl::span<const std::uint8_t> entrada(datos, largo);
  clave_del_servidor(entrada);
  contenido_de_copia(entrada);
  sobre(entrada);
  publica_y_firma(entrada);
  if (const auto mia = ventrik::cliente::leer_archivo_de_identidad(entrada)) {
    exigir(ventrik::cifrado::huella_de_publica(mia->publica()).has_value());
  }
  if (const auto equipo = ventrik::cliente::leer_archivo_de_equipo(entrada)) {
    exigir(equipo->size() == ventrik::cliente::kBytesSecretoDeEquipo);
  }
  return 0;
}
