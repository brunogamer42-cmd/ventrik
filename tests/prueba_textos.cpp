#include <cstddef>
#include <initializer_list>
#include <string>

#include "common/textos.h"
#include "common/utf.h"
#include <doctest/doctest.h>

namespace {

std::string codigo(ventrik::Idioma idioma) {
  return std::string(ventrik::codigo_de(idioma));
}

}  // namespace

TEST_CASE("están el español y el inglés") {
  CHECK(codigo(ventrik::elegir_idioma("es")) == "es");
  CHECK(codigo(ventrik::elegir_idioma("en")) == "en");
}

TEST_CASE("un idioma que Ventrik no tiene usa el inglés") {
  for (const char* pedido : {"fr", "", "e", "esp"}) {
    CAPTURE(pedido);
    CHECK(codigo(ventrik::elegir_idioma(pedido)) == "en");
  }
}

TEST_CASE("el idioma de Windows siempre es uno de los de lang/") {
  CHECK(ventrik::idioma_de_windows().indice < ventrik::cantidad_idiomas());
}

TEST_CASE("cada texto está traducido en todos los idiomas y es UTF-8 válido") {
  REQUIRE(ventrik::cantidad_claves() > 0);
  for (std::size_t i = 0; i < ventrik::cantidad_idiomas(); ++i) {
    const ventrik::Idioma idioma{i};
    for (std::size_t c = 0; c < ventrik::cantidad_claves(); ++c) {
      const auto clave = static_cast<ventrik::Clave>(c);
      const std::string idioma_txt = codigo(idioma) + ".txt";
      const std::string nombre(ventrik::nombre_de_clave(clave));
      CAPTURE(idioma_txt);
      CAPTURE(nombre);
      CHECK_FALSE(ventrik::falta_traduccion(idioma, clave));
      CHECK_FALSE(ventrik::a_utf16(ventrik::texto(idioma, clave)).empty());
    }
  }
}
