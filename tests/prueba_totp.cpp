#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include <gsl/gsl>

#include "common/cifrado/totp.h"
#include <doctest/doctest.h>

using namespace ventrik::cifrado;

namespace {
constexpr std::string_view kSecretoRfc = "12345678901234567890";
constexpr std::uint64_t kAhora = 1111111111;
constexpr std::uint64_t kPaso = kAhora / kSegundosPasoTotp;

gsl::span<const std::uint8_t> secreto_rfc() {
  return gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(kSecretoRfc.data()), kSecretoRfc.size());
}

std::uint32_t codigo(std::uint64_t paso) {
  const auto calculado = codigo_totp(secreto_rfc(), paso);
  REQUIRE(calculado);
  return *calculado;
}

}

TEST_CASE("TOTP da los códigos de los vectores oficiales del RFC 6238 con SHA1") {
  const struct {
    std::uint64_t segundos;
    std::uint32_t codigo;
  } vectores[] = {{59, 287082},         {1111111109, 81804}, {1111111111, 50471},
                  {1234567890, 5924},   {2000000000, 279037}, {20000000000, 353130}};
  for (const auto& vector : vectores) {
    CAPTURE(vector.segundos);
    CHECK(codigo(vector.segundos / kSegundosPasoTotp) == vector.codigo);
    CHECK(verificar_totp(secreto_rfc(), vector.codigo, vector.segundos, 0) == vector.segundos / kSegundosPasoTotp);
  }
}

TEST_CASE("TOTP acepta un paso antes y uno después, pero no dos") {
  for (const std::uint64_t paso : {kPaso - 2, kPaso - 1, kPaso + 1, kPaso + 2}) {
    CHECK(codigo(paso) != codigo(kPaso));
  }
  CHECK(verificar_totp(secreto_rfc(), codigo(kPaso), kAhora, 0) == kPaso);
  CHECK(verificar_totp(secreto_rfc(), codigo(kPaso - 1), kAhora, 0) == kPaso - 1);
  CHECK(verificar_totp(secreto_rfc(), codigo(kPaso + 1), kAhora, 0) == kPaso + 1);
  CHECK_FALSE(verificar_totp(secreto_rfc(), codigo(kPaso - 2), kAhora, 0));
  CHECK_FALSE(verificar_totp(secreto_rfc(), codigo(kPaso + 2), kAhora, 0));
  CHECK_FALSE(verificar_totp(secreto_rfc(), codigo(0), 0, 0));
  CHECK(verificar_totp(secreto_rfc(), codigo(1), 0, 0) == 1u);
}

TEST_CASE("TOTP no acepta otra vez un código ya usado ni uno más viejo") {
  CHECK(verificar_totp(secreto_rfc(), codigo(kPaso), kAhora, kPaso - 1) == kPaso);
  CHECK_FALSE(verificar_totp(secreto_rfc(), codigo(kPaso), kAhora, kPaso));
  CHECK_FALSE(verificar_totp(secreto_rfc(), codigo(kPaso - 1), kAhora, kPaso - 1));
  CHECK_FALSE(verificar_totp(secreto_rfc(), codigo(kPaso), kAhora, kPaso + 1));
  CHECK(verificar_totp(secreto_rfc(), codigo(kPaso + 1), kAhora, kPaso) == kPaso + 1);
  CHECK_FALSE(verificar_totp(secreto_rfc(), codigo(kPaso), kAhora, std::numeric_limits<std::uint64_t>::max()));
}

TEST_CASE("TOTP rechaza códigos imposibles y secretos de otro largo") {
  CHECK_FALSE(verificar_totp(secreto_rfc(), codigo(kPaso) + kTopeCodigoTotp, kAhora, 0));
  CHECK_FALSE(verificar_totp(secreto_rfc(), std::numeric_limits<std::uint32_t>::max(), kAhora, 0));
  CHECK_FALSE(codigo_totp(secreto_rfc().first(19), kPaso));
  CHECK_FALSE(codigo_totp(gsl::span<const std::uint8_t>(), kPaso));
  CHECK_FALSE(verificar_totp(secreto_rfc().first(19), codigo(kPaso), kAhora, 0));
  CHECK(codigos_totp_iguales(123456, 123456));
  CHECK_FALSE(codigos_totp_iguales(123456, 123457));
}

TEST_CASE("TOTP genera secretos de 20 bytes al azar") {
  const auto uno = secreto_totp_nuevo();
  const auto otro = secreto_totp_nuevo();
  REQUIRE(uno);
  REQUIRE(otro);
  CHECK(uno->size() == kBytesSecretoTotp);
  CHECK_FALSE(std::equal(uno->bytes().begin(), uno->bytes().end(), otro->bytes().begin()));
  CHECK(codigo_totp(uno->bytes(), kPaso));
}

TEST_CASE("TOTP lee seis cifras, con o sin un espacio en el medio") {
  CHECK(leer_codigo_totp("123456") == 123456u);
  CHECK(leer_codigo_totp("000000") == 0u);
  CHECK(leer_codigo_totp("081804") == 81804u);
  CHECK(leer_codigo_totp("999999") == 999999u);
  CHECK(leer_codigo_totp("123 456") == 123456u);
}

TEST_CASE("TOTP rechaza todo lo que no son exactamente seis cifras") {
  for (const char* texto : {"", "12345", "1234567", "12 3456", "1234 56", " 123456", "123456 ", "12345a",
                            "123-456", "123  456", "+12345", "-12345", "123\t456", "1 2 3 4", "abcdef",
                            "\xEF\xBC\x91\xEF\xBC\x92\xEF\xBC\x93\xEF\xBC\x94\xEF\xBC\x95\xEF\xBC\x96"}) {
    CAPTURE(texto);
    CHECK_FALSE(leer_codigo_totp(texto));
  }
  CHECK_FALSE(leer_codigo_totp(std::string_view("12345\0", 6)));
  CHECK_FALSE(leer_codigo_totp(std::string_view("123\0" "456", 7)));
}

TEST_CASE("TOTP arma el texto otpauth con el nombre codificado y el secreto en base32") {
  CHECK(texto_otpauth("José Pérez", secreto_rfc()) ==
        "otpauth://totp/Ventrik:Jos%C3%A9%20P%C3%A9rez?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"
        "&issuer=Ventrik&digits=6&period=30");
  CHECK(texto_otpauth("a:b/c?d&e=f%g~h-i_j.k#", secreto_rfc()) ==
        "otpauth://totp/Ventrik:a%3Ab%2Fc%3Fd%26e%3Df%25g~h-i_j.k%23?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"
        "&issuer=Ventrik&digits=6&period=30");
}
