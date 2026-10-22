#include <cstddef>
#include <cstdint>
#include <string>

#include "common/cifrado/huella.h"
#include "common/cifrado/identidad.h"
#include <doctest/doctest.h>

using ventrik::cifrado::Huella;
using ventrik::cifrado::leer_huella;

namespace {

Huella de_0_a_31() {
  Huella::Bytes bytes{};
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    bytes[i] = static_cast<std::uint8_t>(i);
  }
  return Huella(bytes);
}

Huella todo_ff() {
  Huella::Bytes bytes{};
  bytes.fill(0xFF);
  return Huella(bytes);
}

}  // namespace

// Los textos esperados salen de base64.b32encode de Python, no de este código.
TEST_CASE("La huella se escribe en base32 de a 4 letras") {
  CHECK(de_0_a_31().a_texto() ==
        "AAAQ-EAYE-AUDA-OCAJ-BIFQ-YDIO-B4IB-CEQT-CQKR-MFYY-DENB-WHA5-DYPQ");
  CHECK(todo_ff().a_texto() ==
        "7777-7777-7777-7777-7777-7777-7777-7777-7777-7777-7777-7777-777Q");
  CHECK(Huella(Huella::Bytes{}).a_texto().size() == 64);
}

TEST_CASE("La huella se lee tal como se escribe, con o sin guiones y en minúsculas") {
  const Huella huella = de_0_a_31();
  CHECK(leer_huella(huella.a_texto()) == huella);
  CHECK(leer_huella("aaaqeayeaudaocajbifqydiob4ibceqtcqkrmfyydenbwha5dypq") == huella);
  CHECK(leer_huella("AAAQ EAYE AUDA OCAJ BIFQ YDIO B4IB CEQT CQKR MFYY DENB WHA5 DYPQ") == huella);
  CHECK(leer_huella(todo_ff().a_texto()) == todo_ff());
}

TEST_CASE("Una huella mal escrita no se acepta") {
  const std::string buena = de_0_a_31().a_texto();
  CHECK_FALSE(leer_huella(""));
  CHECK_FALSE(leer_huella(buena.substr(0, buena.size() - 1)));
  CHECK_FALSE(leer_huella(buena + "A"));
  for (const char letra : {'0', '1', '8', '9', '=', '_', '.'}) {
    std::string mala = buena;
    mala[0] = letra;
    CAPTURE(letra);
    CHECK_FALSE(leer_huella(mala));
  }
  // La última letra lleva 4 bits de relleno que tienen que ser 0: una sola forma por huella.
  std::string relleno = buena;
  relleno.back() = 'R';
  CHECK_FALSE(leer_huella(relleno));
  CHECK_FALSE(leer_huella(std::string(300, ' ') + buena));
}

TEST_CASE("Dos huellas se comparan byte por byte") {
  Huella::Bytes otra = de_0_a_31().bytes();
  otra[31] = 0;
  CHECK(de_0_a_31() == de_0_a_31());
  CHECK(de_0_a_31() != Huella(otra));
}

TEST_CASE("La huella sale de la clave pública") {
  auto identidad = ventrik::cifrado::IdentidadServidor::crear_nueva();
  REQUIRE(identidad);
  const auto de_la_clave = ventrik::cifrado::huella_de_clave(*identidad->clave());
  const auto del_certificado = ventrik::cifrado::huella_de_clave(identidad->certificado()->pk);
  REQUIRE(de_la_clave);
  REQUIRE(del_certificado);
  CHECK(*de_la_clave == identidad->huella());
  CHECK(*del_certificado == identidad->huella());
}
