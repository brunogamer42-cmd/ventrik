#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gsl/gsl>
#include <mbedtls/rsa.h>

#include "common/cifrado/clave.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/llave.h"
#include "common/protocol/bytes.h"
#include "../tests/rsa_de_prueba.h"

namespace cifrado = ventrik::cifrado;
using ventrik::cifrado::AlgoritmoDeLlave;
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
cifrado::LlavePublica fija_como_llave;
mbedtls_rsa_context rsa;
cifrado::LlavePublica rsa_como_llave;
std::vector<std::uint8_t> aplicacion;

std::vector<std::uint8_t> con_aplicacion(gsl::span<const std::uint8_t> resto) {
  std::vector<std::uint8_t> datos = aplicacion;
  datos.insert(datos.end(), resto.begin(), resto.end());
  return datos;
}

bool llave_bien_formada(const cifrado::LlavePublica& llave) {
  if (llave.algoritmo == AlgoritmoDeLlave::rs256) {
    return llave.clave.size() == cifrado::kBytesModuloRsa && (llave.clave.front() & 0x80) != 0 &&
           (llave.clave.back() & 0x01) != 0;
  }
  cifrado::ClavePublica publica{};
  if (llave.algoritmo != AlgoritmoDeLlave::es256 || llave.clave.size() != publica.size()) {
    return false;
  }
  std::copy(llave.clave.begin(), llave.clave.end(), publica.begin());
  return publica[0] == 0x04 && cifrado::huella_de_publica(publica).has_value();
}

void cose_y_creada(gsl::span<const std::uint8_t> entrada) {
  if (const auto publica = cifrado::leer_clave_cose(entrada)) {
    exigir(llave_bien_formada(*publica));
    exigir(entrada.size() == (publica->algoritmo == AlgoritmoDeLlave::rs256 ? 272u : 77u));
  }
  for (const auto& datos : {std::vector<std::uint8_t>(entrada.begin(), entrada.end()), con_aplicacion(entrada)}) {
    if (const auto creada = cifrado::leer_llave_creada(datos)) {
      exigir(!creada->id.empty() && creada->id.size() <= cifrado::kMaximoIdDeLlave &&
             llave_bien_formada(creada->publica));
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
    for (const auto* llave : {&fija_como_llave, &rsa_como_llave}) {
      exigir(cifrado::verificar_llave(*llave, guardado, desafio, probados, firma).resultado != ResultadoLlave::valida);
    }
  }
}

std::vector<std::uint8_t> firmar_con_rsa(const std::vector<std::uint8_t>& firmado) {
  const auto resumen = cifrado::resumir(firmado);
  std::vector<std::uint8_t> firma(cifrado::kBytesModuloRsa);
  exigir(resumen && mbedtls_rsa_rsassa_pkcs1_v15_sign(&rsa, cifrado::aleatorio, nullptr, MBEDTLS_MD_SHA256,
                                                      static_cast<unsigned int>(resumen->size()), resumen->data(),
                                                      firma.data()) == 0);
  return firma;
}

void llave_firmada(gsl::span<const std::uint8_t> entrada, bool con_rsa) {
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
  const auto firma = con_rsa ? std::optional<std::vector<std::uint8_t>>(firmar_con_rsa(firmado)) : fija->firmar(firmado);
  exigir(firma.has_value());
  const auto& propia = con_rsa ? rsa_como_llave : fija_como_llave;
  const auto& ajena = con_rsa ? fija_como_llave : rsa_como_llave;
  exigir(cifrado::verificar_llave(ajena, guardado, desafio, datos, *firma).resultado != ResultadoLlave::valida);
  const auto resultado = cifrado::verificar_llave(propia, guardado, desafio, datos, *firma);
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
  const auto llave = cifrado::leer_llave(lector);
  const std::uint32_t generacion = lector.leer_u32();
  const std::uint32_t minima = lector.leer_u32();
  const auto firma = lector.leer_bytes(lector.restante());
  if (!lector.ok()) {
    return;
  }
  exigir(llave && (llave->clave.size() == cifrado::kBytesModuloRsa ||
                   llave->clave.size() == cifrado::kBytesClavePublica));
  exigir(!cifrado::verificar_permiso(fija->publica(), *llave, generacion, minima, firma));
  if (llave->algoritmo == AlgoritmoDeLlave::es256) {
    cifrado::ClavePublica otra{};
    std::copy(llave->clave.begin(), llave->clave.end(), otra.begin());
    cifrado::verificar_permiso(otra, fija_como_llave, generacion, minima, firma);
  }
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
  const auto como_llave = cifrado::armar_llave(AlgoritmoDeLlave::es256, fija->publica());
  mbedtls_rsa_init(&rsa);
  std::vector<std::uint8_t> modulo(cifrado::kBytesModuloRsa);
  exigir(prueba::armar_rsa_de_prueba(rsa, 0) &&
         mbedtls_rsa_export_raw(&rsa, modulo.data(), modulo.size(), nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0) ==
             0);
  const auto rsa_publica = cifrado::armar_llave(AlgoritmoDeLlave::rs256, modulo);
  exigir(como_llave.has_value() && rsa_publica.has_value());
  fija_como_llave = *como_llave;
  rsa_como_llave = *rsa_publica;
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
    llave_firmada(entrada.subspan(1), (entrada[0] & 6) == 6);
  } else {
    permiso(entrada.subspan(1));
  }
  return 0;
}
