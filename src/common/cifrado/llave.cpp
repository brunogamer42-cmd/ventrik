#include "common/cifrado/llave.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include <mbedtls/base64.h>
#include <mbedtls/rsa.h>

namespace ventrik::cifrado {
namespace {

using protocolo::Lector;

constexpr char kEtiquetaPermiso[] = "ventrik permiso de llave v2";
constexpr std::array<std::uint8_t, 10> kCoseAntesDeX = {0xA5, 0x01, 0x02, 0x03, 0x26, 0x20, 0x01, 0x21, 0x58, 0x20};
constexpr std::array<std::uint8_t, 3> kCoseAntesDeY = {0x22, 0x58, 0x20};
constexpr std::array<std::uint8_t, 11> kCoseAntesDeN = {0xA4, 0x01, 0x03, 0x03, 0x39, 0x01,
                                                        0x00, 0x20, 0x59, 0x01, 0x00};
constexpr std::array<std::uint8_t, 2> kCoseAntesDeE = {0x21, 0x43};
constexpr std::array<std::uint8_t, 3> kExponenteRsa = {0x01, 0x00, 0x01};
constexpr std::size_t kBytesCoordenada = 32;
constexpr std::size_t kBytesAaguid = 16;

gsl::span<const std::uint8_t> bytes_de(std::string_view texto) {
  return gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(texto.data()), texto.size());
}

bool forma_valida(AlgoritmoDeLlave algoritmo, gsl::span<const std::uint8_t> clave) {
  switch (algoritmo) {
    case AlgoritmoDeLlave::es256:
      return clave.size() == kBytesClavePublica && clave.front() == 0x04;
    case AlgoritmoDeLlave::rs256:
      return clave.size() == kBytesModuloRsa && (clave.front() & 0x80) != 0 && (clave.back() & 0x01) != 0;
  }
  return false;
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

std::optional<LlavePublica> leer_cose(Lector& lector) {
  Lector rsa = lector;
  if (leer_igual(rsa, kCoseAntesDeN)) {
    const auto modulo = rsa.leer_bytes(kBytesModuloRsa);
    const bool exponente = leer_igual(rsa, kCoseAntesDeE) && leer_igual(rsa, kExponenteRsa);
    lector = rsa;
    return exponente ? armar_llave(AlgoritmoDeLlave::rs256, modulo) : std::nullopt;
  }
  const bool forma_x = leer_igual(lector, kCoseAntesDeX);
  const auto x = lector.leer_bytes(kBytesCoordenada);
  const bool forma_y = leer_igual(lector, kCoseAntesDeY);
  const auto y = lector.leer_bytes(kBytesCoordenada);
  if (!forma_x || !forma_y || !lector.ok()) {
    return std::nullopt;
  }
  ClavePublica publica{0x04};
  std::copy(y.begin(), y.end(), std::copy(x.begin(), x.end(), publica.begin() + 1));
  return huella_de_publica(publica) ? armar_llave(AlgoritmoDeLlave::es256, publica) : std::nullopt;
}

bool verificar_rsa(gsl::span<const std::uint8_t> modulo, gsl::span<const std::uint8_t> datos,
                   gsl::span<const std::uint8_t> firma) {
  const auto resumen = resumir(datos);
  mbedtls_rsa_context rsa;
  mbedtls_rsa_init(&rsa);
  const auto liberar = gsl::finally([&rsa] { mbedtls_rsa_free(&rsa); });
  return resumen && firma.size() == kBytesModuloRsa &&
         mbedtls_rsa_import_raw(&rsa, modulo.data(), modulo.size(), nullptr, 0, nullptr, 0, nullptr, 0,
                                kExponenteRsa.data(), kExponenteRsa.size()) == 0 &&
         mbedtls_rsa_complete(&rsa) == 0 && mbedtls_rsa_check_pubkey(&rsa) == 0 &&
         mbedtls_rsa_get_len(&rsa) == kBytesModuloRsa &&
         mbedtls_rsa_rsassa_pkcs1_v15_verify(&rsa, MBEDTLS_MD_SHA256, static_cast<unsigned int>(resumen->size()),
                                             resumen->data(), firma.data()) == 0;
}

bool verificar_con_llave(const LlavePublica& llave, gsl::span<const std::uint8_t> datos,
                         gsl::span<const std::uint8_t> firma) {
  if (!forma_valida(llave.algoritmo, llave.clave) || !firma_de_llave_valida(llave.algoritmo, firma.size())) {
    return false;
  }
  if (llave.algoritmo == AlgoritmoDeLlave::rs256) {
    return verificar_rsa(llave.clave, datos, firma);
  }
  ClavePublica publica{};
  std::copy(llave.clave.begin(), llave.clave.end(), publica.begin());
  return verificar_firma(publica, datos, firma);
}

}

std::optional<LlavePublica> armar_llave(AlgoritmoDeLlave algoritmo, gsl::span<const std::uint8_t> clave) {
  if (!forma_valida(algoritmo, clave)) {
    return std::nullopt;
  }
  return LlavePublica{algoritmo, std::vector<std::uint8_t>(clave.begin(), clave.end())};
}

bool firma_de_llave_valida(AlgoritmoDeLlave algoritmo, std::size_t largo) {
  return algoritmo == AlgoritmoDeLlave::rs256   ? largo == kBytesModuloRsa
         : algoritmo == AlgoritmoDeLlave::es256 ? largo >= 1 && largo <= kMaximoFirma
                                                : false;
}

void escribir_llave(protocolo::Escritor& escritor, const LlavePublica& llave) {
  if (!forma_valida(llave.algoritmo, llave.clave)) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_u8(static_cast<std::uint8_t>(llave.algoritmo));
  escritor.escribir_u16(static_cast<std::uint16_t>(llave.clave.size()));
  escritor.escribir_bytes(llave.clave);
}

std::optional<LlavePublica> leer_llave(protocolo::Lector& lector) {
  const auto algoritmo = static_cast<AlgoritmoDeLlave>(lector.leer_u8());
  const auto clave = lector.leer_bytes(lector.leer_u16());
  auto llave = lector.ok() ? armar_llave(algoritmo, clave) : std::nullopt;
  if (!llave) {
    lector.marcar_error();
  }
  return llave;
}

std::uint32_t generacion_de_la_hora(std::uint64_t ms_desde_1970) {
  constexpr std::uint64_t kDesde2026 = 1767225600000ull;
  constexpr std::uint64_t kMsPorMinuto = 60000;
  const std::uint64_t minutos = ms_desde_1970 > kDesde2026 ? (ms_desde_1970 - kDesde2026) / kMsPorMinuto : 0;
  return static_cast<std::uint32_t>(std::min<std::uint64_t>(minutos, UINT32_MAX - kAdelantoDeGeneracion));
}

std::uint32_t generacion_tope(std::uint64_t ms_desde_1970) {
  return generacion_de_la_hora(ms_desde_1970) + kAdelantoDeGeneracion;
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

LlaveVerificada verificar_llave(const LlavePublica& llave, std::uint32_t contador_guardado,
                                gsl::span<const std::uint8_t> desafio, gsl::span<const std::uint8_t> datos,
                                gsl::span<const std::uint8_t> firma) {
  LlaveVerificada verificada;
  Lector lector(datos);
  const auto cabecera = leer_cabecera(lector);
  const std::string texto = texto_de_cliente(Ceremonia::firmar, desafio);
  const auto resumen = resumir(bytes_de(texto));
  if (!cabecera || !lector.terminado() || (cabecera->marcas & (kLlaveConCredencial | kLlaveConExtensiones)) != 0 ||
      desafio.size() != kBytesDesafioDeLlave || texto.empty() || !resumen) {
    return verificada;
  }
  std::vector<std::uint8_t> firmado(datos.begin(), datos.end());
  firmado.insert(firmado.end(), resumen->begin(), resumen->end());
  if (!verificar_con_llave(llave, firmado, firma)) {
    verificada.resultado = ResultadoLlave::firma_invalida;
    return verificada;
  }
  if ((cabecera->contador != 0 || contador_guardado != 0) && cabecera->contador <= contador_guardado) {
    verificada.resultado = ResultadoLlave::posible_llave_clonada;
    return verificada;
  }
  return {ResultadoLlave::valida, (cabecera->marcas & kLlaveVerificada) != 0, cabecera->contador};
}

std::vector<std::uint8_t> lo_que_firma_el_permiso(const ClavePublica& identidad, const LlavePublica& llave,
                                                  std::uint32_t generacion) {
  protocolo::Escritor escritor;
  escritor.escribir_bytes(bytes_de(kEtiquetaPermiso));
  escritor.escribir_bytes(identidad);
  escribir_llave(escritor, llave);
  escritor.escribir_u32(generacion);
  return escritor.ok() ? std::move(escritor).soltar() : std::vector<std::uint8_t>();
}

bool verificar_permiso(const ClavePublica& identidad, const LlavePublica& llave, std::uint32_t generacion,
                       std::uint32_t generacion_minima, gsl::span<const std::uint8_t> firma) {
  const auto firmado = lo_que_firma_el_permiso(identidad, llave, generacion);
  return generacion >= generacion_minima && !firmado.empty() && verificar_firma(identidad, firmado, firma);
}

std::optional<LlavePublica> leer_clave_cose(gsl::span<const std::uint8_t> cose) {
  Lector lector(cose);
  auto publica = leer_cose(lector);
  return lector.terminado() ? std::move(publica) : std::nullopt;
}

std::optional<LlaveCreada> leer_llave_creada(gsl::span<const std::uint8_t> datos) {
  Lector lector(datos);
  const auto cabecera = leer_cabecera(lector);
  lector.leer_bytes(kBytesAaguid);
  const std::size_t largo = leer_grande(lector, 2);
  const auto id = lector.leer_bytes(largo);
  auto publica = leer_cose(lector);
  if (!cabecera || (cabecera->marcas & kLlaveConCredencial) == 0 || largo == 0 || largo > kMaximoIdDeLlave ||
      !publica || ((cabecera->marcas & kLlaveConExtensiones) == 0 && !lector.terminado())) {
    return std::nullopt;
  }
  return LlaveCreada{std::vector<std::uint8_t>(id.begin(), id.end()), std::move(*publica)};
}

}
