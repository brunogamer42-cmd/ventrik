#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/clave.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/llave.h"
#include "common/protocol/bytes.h"

namespace cifrado = ventrik::cifrado;
using ventrik::cifrado::ResultadoLlave;
using ventrik::protocolo::Lector;

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

std::unique_ptr<cifrado::Inicio> inicio;
std::optional<cifrado::Clave> fija;
std::vector<std::uint8_t> aplicacion;

std::vector<std::uint8_t> con_aplicacion(gsl::span<const std::uint8_t> resto) {
  std::vector<std::uint8_t> datos = aplicacion;
  datos.insert(datos.end(), resto.begin(), resto.end());
  return datos;
}

void cose_y_creada(gsl::span<const std::uint8_t> entrada) {
  if (const auto publica = cifrado::leer_clave_cose(entrada)) {
    exigir(entrada.size() == 77 && (*publica)[0] == 0x04 && cifrado::huella_de_publica(*publica));
  }
  for (const auto& datos : {std::vector<std::uint8_t>(entrada.begin(), entrada.end()), con_aplicacion(entrada)}) {
    if (const auto creada = cifrado::leer_llave_creada(datos)) {
      exigir(!creada->id.empty() && creada->id.size() <= cifrado::kMaximoIdDeLlave &&
             cifrado::huella_de_publica(creada->publica));
    }
  }
}

void llave_inventada(gsl::span<const std::uint8_t> entrada) {
  Lector lector(entrada);
  const std::uint32_t guardado = lector.leer_u32();
  const auto desafio = lector.leer_bytes(lector.leer_u8());
  const auto datos = lector.leer_bytes(lector.leer_u8());
  const auto firma = lector.leer_bytes(lector.restante());
  if (!lector.ok()) {
    return;
  }
  for (const auto& probados : {std::vector<std::uint8_t>(datos.begin(), datos.end()), con_aplicacion(datos)}) {
    exigir(cifrado::verificar_llave(fija->publica(), guardado, desafio, probados, firma).resultado !=
           ResultadoLlave::valida);
  }
}

void llave_firmada(gsl::span<const std::uint8_t> entrada) {
  Lector lector(entrada);
  const auto cabecera = lector.leer_bytes(5);
  const std::uint32_t guardado = lector.leer_u32();
  const auto desafio = lector.leer_bytes(lector.restante());
  if (!lector.ok()) {
    return;
  }
  const auto datos = con_aplicacion(cabecera);
  const std::string texto = cifrado::texto_de_cliente(cifrado::Ceremonia::firmar, desafio);
  const auto resumen = cifrado::resumir(gsl::span<const std::uint8_t>(
      reinterpret_cast<const std::uint8_t*>(texto.data()), texto.size()));
  exigir(resumen.has_value());
  auto firmado = datos;
  firmado.insert(firmado.end(), resumen->begin(), resumen->end());
  const auto firma = fija->firmar(firmado);
  exigir(firma.has_value());
  const auto resultado = cifrado::verificar_llave(fija->publica(), guardado, desafio, datos, *firma);
  const std::uint8_t marcas = cabecera[0];
  const std::uint32_t contador = (static_cast<std::uint32_t>(cabecera[1]) << 24) |
                                 (static_cast<std::uint32_t>(cabecera[2]) << 16) |
                                 (static_cast<std::uint32_t>(cabecera[3]) << 8) | cabecera[4];
  if (desafio.size() != cifrado::kBytesDesafioDeLlave || (marcas & cifrado::kLlavePresente) == 0 ||
      (marcas & (cifrado::kLlaveConCredencial | cifrado::kLlaveConExtensiones)) != 0) {
    exigir(resultado.resultado == ResultadoLlave::datos_invalidos);
  } else if ((contador == 0 && guardado == 0) || contador > guardado) {
    exigir(resultado.resultado == ResultadoLlave::valida && resultado.contador == contador &&
           resultado.segundo_paso == ((marcas & cifrado::kLlaveVerificada) != 0));
  } else {
    exigir(resultado.resultado == ResultadoLlave::posible_llave_clonada && !resultado.segundo_paso);
  }
}

void permiso(gsl::span<const std::uint8_t> entrada) {
  Lector lector(entrada);
  cifrado::ClavePublica otra{};
  const auto bytes = lector.leer_bytes(otra.size());
  const std::uint32_t generacion = lector.leer_u32();
  const std::uint32_t minima = lector.leer_u32();
  const auto firma = lector.leer_bytes(lector.restante());
  if (!lector.ok()) {
    return;
  }
  std::copy(bytes.begin(), bytes.end(), otra.begin());
  exigir(!cifrado::verificar_permiso(fija->publica(), otra, generacion, minima, firma));
  cifrado::verificar_permiso(otra, fija->publica(), generacion, minima, firma);
}

}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  inicio = std::make_unique<cifrado::Inicio>();
  exigir(inicio->ok());
  fija = cifrado::Clave::crear_nueva();
  const std::string nombre = cifrado::kAplicacionDeLlave;
  const auto resumen = cifrado::resumir(gsl::span<const std::uint8_t>(
      reinterpret_cast<const std::uint8_t*>(nombre.data()), nombre.size()));
  exigir(fija.has_value() && resumen.has_value());
  aplicacion.assign(resumen->begin(), resumen->end());
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const gsl::span<const std::uint8_t> entrada(datos, largo);
  cose_y_creada(entrada);
  llave_inventada(entrada);
  if (entrada.empty()) {
    return 0;
  }
  if ((entrada[0] & 1) != 0) {
    llave_firmada(entrada.subspan(1));
  } else {
    permiso(entrada.subspan(1));
  }
  return 0;
}
