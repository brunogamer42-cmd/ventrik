#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>

#include <gsl/gsl>

#include "client/mi_identidad.h"
#include "common/cifrado/boveda.h"
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
  std::uint32_t generacion = 0;
  const auto clave = ventrik::cliente::leer_contenido_de_copia(datos, &generacion);
  if (!clave) {
    exigir(generacion == 0);
    return;
  }
  const auto huella = ventrik::cifrado::huella_de_publica(clave->publica());
  exigir(huella && *huella == clave->huella());
}

void contenido_de_llaves(gsl::span<const std::uint8_t> datos) {
  const auto llaves = ventrik::cliente::leer_contenido_de_llaves(datos);
  if (!llaves) {
    return;
  }
  exigir(llaves->llaves.size() <= ventrik::cliente::kMaximoLlaves);
  exigir(ventrik::cifrado::huella_de_publica(llaves->identidad).has_value());
  for (const auto& llave : llaves->llaves) {
    exigir(ventrik::cifrado::armar_llave(llave.publica.algoritmo, llave.publica.clave).has_value());
  }
  const auto otra_vez = ventrik::cliente::armar_contenido_de_llaves(*llaves);
  exigir(otra_vez && std::equal(otra_vez->begin(), otra_vez->end(), datos.begin(), datos.end()));
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

void boveda(gsl::span<const std::uint8_t> datos) {
  const auto abierta = ventrik::cifrado::abrir_boveda(datos, std::nullopt);
  exigir(abierta.boveda.has_value() == (abierta.resultado == ventrik::cifrado::AperturaBoveda::abierta));
  if (abierta.boveda) {
    const auto otra_vez = abierta.boveda->armar_archivo(std::nullopt);
    exigir(otra_vez && ventrik::cifrado::abrir_boveda(*otra_vez, std::nullopt).boveda.has_value());
    exigir(abierta.boveda->derivar(ventrik::cifrado::kEtiquetaHistorial).has_value());
  }
}

void clave_cifrada(gsl::span<const std::uint8_t> datos) {
  static const ventrik::cifrado::Boveda fija = [] {
    ventrik::cifrado::Secreto clave(ventrik::cifrado::kBytesBoveda);
    std::fill(clave.bytes().begin(), clave.bytes().end(), std::uint8_t{7});
    return ventrik::cifrado::Boveda(std::move(clave));
  }();
  const auto identidad = ventrik::cifrado::abrir_clave(datos, fija);
  if (!identidad) {
    return;
  }
  const auto cerrada = ventrik::cifrado::cerrar_clave(*identidad, fija);
  exigir(cerrada.has_value());
  const auto otra_vez = ventrik::cifrado::abrir_clave(*cerrada, fija);
  exigir(otra_vez && otra_vez->huella() == identidad->huella());
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

}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  cifrado = std::make_unique<ventrik::cifrado::Inicio>();
  exigir(cifrado->ok());
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const gsl::span<const std::uint8_t> entrada(datos, largo);
  clave_del_servidor(entrada);
  contenido_de_copia(entrada);
  contenido_de_llaves(entrada);
  const auto cargadas = ventrik::cliente::leer_archivo_de_llaves(entrada);
  exigir(cargadas.danadas == !cargadas.llaves.has_value() && (!cargadas.viejas || cargadas.danadas));
  exigir(!cargadas.llaves || cargadas.llaves->llaves.size() <= ventrik::cliente::kMaximoLlaves);
  sobre(entrada);
  publica_y_firma(entrada);
  boveda(entrada);
  clave_cifrada(entrada);
  const auto mia = ventrik::cliente::leer_archivo_de_identidad(entrada);
  if (mia.clave) {
    exigir(ventrik::cifrado::huella_de_publica(mia.clave->publica()).has_value());
  }
  exigir((mia.estado == ventrik::cliente::EstadoIdentidad::con_contrasena) == !mia.cerrada.empty());
  exigir(mia.cerrada.empty() || ventrik::cifrado::leer_cabecera_sobre("VKMM", mia.cerrada).has_value());
  if (const auto equipo = ventrik::cliente::leer_archivo_de_equipo(entrada)) {
    exigir(equipo->size() == ventrik::cliente::kBytesSecretoDeEquipo);
  }
  return 0;
}
