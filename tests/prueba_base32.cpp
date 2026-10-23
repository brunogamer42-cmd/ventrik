#include <array>
#include <cstdint>
#include <string>

#include "common/cifrado/base32.h"
#include "common/cifrado/codigo_dueno.h"
#include <doctest/doctest.h>

using namespace ventrik::cifrado;

namespace {
const std::array<std::uint8_t, 6> kFoobar = {'f', 'o', 'o', 'b', 'a', 'r'};
}

TEST_CASE("base32 escribe como el RFC 4648, de a 4 letras") {
  CHECK(a_base32(kFoobar) == "MZXW-6YTB-OI");
  CHECK(a_base32(gsl::span<const std::uint8_t>(kFoobar).first(1)) == "MY");
  CHECK(a_base32(gsl::span<const std::uint8_t>()) == "");
}

TEST_CASE("base32 lee con o sin guiones o espacios y en minúsculas") {
  for (const char* texto : {"MZXW-6YTB-OI", "MZXW6YTBOI", "mzxw 6ytb oi", "-MZXW6YTBOI-"}) {
    std::array<std::uint8_t, 6> leido{};
    CAPTURE(texto);
    CHECK(de_base32(texto, leido));
    CHECK(leido == kFoobar);
  }
}

TEST_CASE("base32 rechaza textos que no son exactamente los bytes pedidos") {
  std::array<std::uint8_t, 6> leido{};
  for (const char* texto : {"", "MZXW6YTBO", "MZXW6YTBOIA", "MZXW6YTBOJ", "MZXW6YTB0I",
                            "MZXW6YTB=I", "MZXW_6YTBOI"}) {
    CAPTURE(texto);
    CHECK_FALSE(de_base32(texto, leido));
  }
  CHECK_FALSE(de_base32(std::string(300, '-') + "MZXW6YTBOI", leido));
}

TEST_CASE("El código de dueño se genera al azar y se lee tal como se escribe") {
  const auto uno = CodigoDueno::generar();
  const auto otro = CodigoDueno::generar();
  REQUIRE(uno);
  REQUIRE(otro);
  CHECK_FALSE(*uno == *otro);
  const auto leido = leer_codigo_dueno(uno->a_texto());
  REQUIRE(leido);
  CHECK(*leido == *uno);
  CHECK(uno->a_texto().size() == 32);
  CHECK_FALSE(leer_codigo_dueno(uno->a_texto().substr(1)));
  CHECK_FALSE(leer_codigo_dueno("no es un código"));
}
