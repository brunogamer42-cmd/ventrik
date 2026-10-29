#include "common/cifrado/llave.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include <mbedtls/base64.h>

#include "common/protocol/bytes.h"

namespace ventrik::cifrado {
namespace {

using protocolo::Lector;

constexpr char kEtiquetaPermiso[] = "ventrik permiso de llave v1";
constexpr std::array<std::uint8_t, 10> kCoseAntesDeX = {0xA5, 0x01, 0x02, 0x03, 0x26, 0x20, 0x01, 0x21, 0x58, 0x20};
constexpr std::array<std::uint8_t, 3> kCoseAntesDeY = {0x22, 0x58, 0x20};
constexpr std::size_t kBytesCoordenada = 32;
constexpr std::size_t kBytesAaguid = 16;

gsl::span<const std::uint8_t> bytes_de(std::string_view texto) {
  return gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(texto.data()), texto.size());
}

bool leer_igual(Lector& lector, gsl::span<const std::uint8_t> esperado) {
  const auto leido = lector.leer_bytes(esperado.size());
  return lector.ok() && std::equal(leido.begin(), leido.end(), esperado.begin(), esperado.end());
}

std::uint32_t leer_grande(Lector& lector, std::size_t bytes) {
  std::uint32_t valor = 0;
  for (const std::uint8_t byte : lector.leer_bytes(bytes)) {
    valor = (valor << 8) | byte;
  }
  return valor;
}

struct Cabecera {
  std::uint8_t marcas = 0;
  std::uint32_t contador = 0;
};

std::optional<Cabecera> leer_cabecera(Lector& lector) {
  const auto aplicacion = resumir(bytes_de(kAplicacionDeLlave));
  const bool nuestra = aplicacion && leer_igual(lector, *aplicacion);
  Cabecera cabecera;
  cabecera.marcas = lector.leer_u8();
  cabecera.contador = leer_grande(lector, 4);
  if (!nuestra || !lector.ok() || (cabecera.marcas & kLlavePresente) == 0) {
    return std::nullopt;
  }
  return cabecera;
}

std::optional<ClavePublica> leer_cose(Lector& lector) {
  const bool forma_x = leer_igual(lector, kCoseAntesDeX);
  const auto x = lector.leer_bytes(kBytesCoordenada);
  const bool forma_y = leer_igual(lector, kCoseAntesDeY);
  const auto y = lector.leer_bytes(kBytesCoordenada);
  if (!forma_x || !forma_y || !lector.ok()) {
    return std::nullopt;
  }
  ClavePublica publica{0x04};
  std::copy(y.begin(), y.end(), std::copy(x.begin(), x.end(), publica.begin() + 1));
  return huella_de_publica(publica) ? std::optional<ClavePublica>(publica) : std::nullopt;
}

}

std::string texto_de_cliente(Ceremonia ceremonia, gsl::span<const std::uint8_t> desafio) {
  std::string base64(4 * ((desafio.size() + 2) / 3) + 1, '\0');
  std::size_t largo = 0;
  if (mbedtls_base64_encode(reinterpret_cast<unsigned char*>(base64.data()), base64.size(), &largo, desafio.data(),
                            desafio.size()) != 0) {
    return {};
  }
  base64.resize(largo);
  std::replace(base64.begin(), base64.end(), '+', '-');
  std::replace(base64.begin(), base64.end(), '/', '_');
  base64.erase(std::remove(base64.begin(), base64.end(), '='), base64.end());
  return std::string(R"({"type":"webauthn.)") + (ceremonia == Ceremonia::crear ? "create" : "get") +
         R"(","challenge":")" + base64 + R"(","origin":")" + kAplicacionDeLlave + R"("})";
}

LlaveVerificada verificar_llave(const ClavePublica& llave, std::uint32_t contador_guardado,
                                gsl::span<const std::uint8_t> desafio, gsl::span<const std::uint8_t> datos,
                                gsl::span<const std::uint8_t> firma) {
  LlaveVerificada verificada;
  Lector lector(datos);
  const auto cabecera = leer_cabecera(lector);
  const std::string texto = texto_de_cliente(Ceremonia::firmar, desafio);
  const auto resumen = resumir(bytes_de(texto));
  if (!cabecera || !lector.terminado() || (cabecera->marcas & (kLlaveConCredencial | kLlaveConExtensiones)) != 0 ||
      texto.empty() || !resumen) {
    return verificada;
  }
  std::vector<std::uint8_t> firmado(datos.begin(), datos.end());
  firmado.insert(firmado.end(), resumen->begin(), resumen->end());
  if (!verificar_firma(llave, firmado, firma)) {
    verificada.resultado = ResultadoLlave::firma_invalida;
    return verificada;
  }
  if ((cabecera->contador != 0 || contador_guardado != 0) && cabecera->contador <= contador_guardado) {
    verificada.resultado = ResultadoLlave::posible_llave_clonada;
    return verificada;
  }
  return {ResultadoLlave::valida, (cabecera->marcas & kLlaveVerificada) != 0, cabecera->contador};
}

std::vector<std::uint8_t> lo_que_firma_el_permiso(const ClavePublica& identidad, const ClavePublica& llave,
                                                  std::uint32_t generacion) {
  protocolo::Escritor escritor;
  escritor.escribir_bytes(bytes_de(kEtiquetaPermiso));
  escritor.escribir_bytes(identidad);
  escritor.escribir_bytes(llave);
  escritor.escribir_u32(generacion);
  return std::move(escritor).soltar();
}

bool verificar_permiso(const ClavePublica& identidad, const ClavePublica& llave, std::uint32_t generacion,
                       std::uint32_t generacion_minima, gsl::span<const std::uint8_t> firma) {
  return generacion >= generacion_minima &&
         verificar_firma(identidad, lo_que_firma_el_permiso(identidad, llave, generacion), firma);
}

std::optional<ClavePublica> leer_clave_cose(gsl::span<const std::uint8_t> cose) {
  Lector lector(cose);
  const auto publica = leer_cose(lector);
  return lector.terminado() ? publica : std::nullopt;
}

std::optional<LlaveCreada> leer_llave_creada(gsl::span<const std::uint8_t> datos) {
  Lector lector(datos);
  const auto cabecera = leer_cabecera(lector);
  lector.leer_bytes(kBytesAaguid);
  const std::size_t largo = leer_grande(lector, 2);
  const auto id = lector.leer_bytes(largo);
  const auto publica = leer_cose(lector);
  if (!cabecera || (cabecera->marcas & kLlaveConCredencial) == 0 || largo == 0 || largo > kMaximoIdDeLlave ||
      !publica || ((cabecera->marcas & kLlaveConExtensiones) == 0 && !lector.terminado())) {
    return std::nullopt;
  }
  return LlaveCreada{std::vector<std::uint8_t>(id.begin(), id.end()), *publica};
}

}
