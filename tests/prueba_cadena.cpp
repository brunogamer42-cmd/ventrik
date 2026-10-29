#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

#include "common/cifrado/cadena.h"
#include "common/cifrado/clave.h"
#include <doctest/doctest.h>

using ventrik::cifrado::agregar_eslabon;
using ventrik::cifrado::Cadena;
using ventrik::cifrado::Clave;
using ventrik::cifrado::escribir_cadena;
using ventrik::cifrado::Eslabon;
using ventrik::cifrado::firmar_eslabon;
using ventrik::cifrado::Huella;
using ventrik::cifrado::kMaximoEslabones;
using ventrik::cifrado::leer_cadena;
using ventrik::cifrado::seguir_cadena;

namespace {

std::vector<Clave> claves_nuevas(std::size_t cantidad) {
  std::vector<Clave> claves;
  for (std::size_t i = 0; i < cantidad; ++i) {
    auto clave = Clave::crear_nueva();
    if (!clave) {
      std::abort();
    }
    claves.push_back(std::move(*clave));
  }
  return claves;
}

Eslabon eslabon(const Clave& vieja, const Clave& nueva, std::uint32_t numero) {
  auto firmado = firmar_eslabon(vieja, nueva.publica(), numero);
  if (!firmado) {
    std::abort();
  }
  return std::move(*firmado);
}

Cadena encadenar(const std::vector<Clave>& claves) {
  Cadena cadena;
  for (std::size_t i = 0; i + 1 < claves.size(); ++i) {
    if (!agregar_eslabon(cadena, eslabon(claves[i], claves[i + 1], static_cast<std::uint32_t>(i + 1)))) {
      std::abort();
    }
  }
  return cadena;
}

std::vector<Huella> huellas(const std::vector<Clave>& claves, std::size_t desde, std::size_t hasta) {
  std::vector<Huella> resultado;
  for (std::size_t i = desde; i < hasta; ++i) {
    resultado.push_back(claves[i].huella());
  }
  return resultado;
}

bool iguales(const Eslabon& a, const Eslabon& b) {
  return a.vieja == b.vieja && a.nueva == b.nueva && a.numero == b.numero && a.firma == b.firma;
}

}  // namespace

TEST_CASE("Una cadena de un eslabón lleva de la clave vieja a la nueva") {
  const auto claves = claves_nuevas(2);
  const auto seguida = seguir_cadena(encadenar(claves), claves[0].huella(), claves[1].huella(), {});
  REQUIRE(seguida);
  CHECK(seguida->nueva == claves[1].huella());
  CHECK(seguida->atras == huellas(claves, 0, 1));
}

TEST_CASE("Una cadena de tres eslabones se sigue desde cualquier clave que el programa conocía") {
  const auto claves = claves_nuevas(4);
  const Cadena cadena = encadenar(claves);
  for (std::size_t desde = 0; desde < 3; ++desde) {
    CAPTURE(desde);
    const auto seguida = seguir_cadena(cadena, claves[desde].huella(), claves[3].huella(), {});
    REQUIRE(seguida);
    CHECK(seguida->nueva == claves[3].huella());
    CHECK(seguida->atras == huellas(claves, desde, 3));
  }
  CHECK(seguir_cadena(cadena, claves[1].huella(), claves[3].huella(), huellas(claves, 0, 1)));
}

TEST_CASE("Una cadena con una firma o un dato alterado no se acepta") {
  const auto claves = claves_nuevas(5);
  const Cadena buena = encadenar(claves);
  for (std::size_t i = 0; i < buena.size(); ++i) {
    CAPTURE(i);
    Cadena firma = buena;
    firma[i].firma.back() ^= 1;
    CHECK_FALSE(seguir_cadena(firma, claves[0].huella(), claves[4].huella(), {}));
    Cadena numero = buena;
    numero[i].numero += 100;
    CHECK_FALSE(seguir_cadena(numero, claves[0].huella(), claves[4].huella(), {}));
  }
  const auto falsa = claves_nuevas(1);
  Cadena otra_nueva = buena;
  otra_nueva.back().nueva = falsa[0].publica();
  CHECK_FALSE(seguir_cadena(otra_nueva, claves[0].huella(), falsa[0].huella(), {}));
  Cadena firmada_por_otra = buena;
  firmada_por_otra[1].firma = eslabon(claves[0], claves[2], 2).firma;
  CHECK_FALSE(seguir_cadena(firmada_por_otra, claves[0].huella(), claves[4].huella(), {}));
}

TEST_CASE("Una cadena cortada no se acepta") {
  const auto claves = claves_nuevas(4);
  Cadena cortada = encadenar(claves);
  cortada.erase(cortada.begin() + 1);
  CHECK_FALSE(seguir_cadena(cortada, claves[0].huella(), claves[3].huella(), {}));
  CHECK(seguir_cadena(cortada, claves[2].huella(), claves[3].huella(), {}));
}

TEST_CASE("Una cadena que termina en otra clave no se acepta") {
  const auto claves = claves_nuevas(4);
  const Cadena cadena = encadenar(claves);
  CHECK_FALSE(seguir_cadena(cadena, claves[0].huella(), claves[2].huella(), {}));
  const auto falsas = claves_nuevas(2);
  CHECK_FALSE(seguir_cadena(cadena, claves[0].huella(), falsas[0].huella(), {}));
  Cadena con_falso = cadena;
  con_falso.push_back(eslabon(falsas[0], falsas[1], 4));
  CHECK_FALSE(seguir_cadena(con_falso, claves[0].huella(), falsas[1].huella(), {}));
  CHECK_FALSE(seguir_cadena(Cadena{eslabon(falsas[0], falsas[1], 1)}, claves[0].huella(), falsas[1].huella(), {}));
}

TEST_CASE("Los números de la cadena tienen que subir") {
  const auto claves = claves_nuevas(3);
  for (const std::uint32_t segundo : {1u, 0u}) {
    CAPTURE(segundo);
    const Cadena cadena{eslabon(claves[0], claves[1], 1), eslabon(claves[1], claves[2], segundo)};
    CHECK_FALSE(seguir_cadena(cadena, claves[0].huella(), claves[2].huella(), {}));
    Cadena agregada{cadena[0]};
    CHECK_FALSE(agregar_eslabon(agregada, cadena[1]));
    CHECK(agregada.size() == 1);
  }
  const Cadena saltando{eslabon(claves[0], claves[1], 5), eslabon(claves[1], claves[2], 90)};
  CHECK(seguir_cadena(saltando, claves[0].huella(), claves[2].huella(), {}));
}

TEST_CASE("La cadena no hace volver a una clave que ya quedó atrás") {
  const auto claves = claves_nuevas(3);
  Cadena cadena = encadenar(claves);
  cadena.push_back(eslabon(claves[2], claves[1], 3));
  CHECK_FALSE(seguir_cadena(cadena, claves[2].huella(), claves[1].huella(), huellas(claves, 0, 2)));
  CHECK_FALSE(seguir_cadena(cadena, claves[0].huella(), claves[1].huella(), {}));
  const Cadena a_la_primera{eslabon(claves[2], claves[0], 4)};
  CHECK_FALSE(seguir_cadena(a_la_primera, claves[2].huella(), claves[0].huella(), huellas(claves, 0, 2)));
  CHECK(seguir_cadena(a_la_primera, claves[2].huella(), claves[0].huella(), huellas(claves, 1, 2)));
}

TEST_CASE("Una cadena de más de 8 eslabones no se acepta") {
  const auto claves = claves_nuevas(kMaximoEslabones + 2);
  Cadena larga;
  for (std::size_t i = 0; i + 1 < claves.size(); ++i) {
    larga.push_back(eslabon(claves[i], claves[i + 1], static_cast<std::uint32_t>(i + 1)));
  }
  REQUIRE(larga.size() == kMaximoEslabones + 1);
  CHECK_FALSE(seguir_cadena(larga, claves[1].huella(), claves.back().huella(), {}));
  CHECK(escribir_cadena(larga).empty());
  const Cadena ocho(larga.begin() + 1, larga.end());
  CHECK(seguir_cadena(ocho, claves[1].huella(), claves.back().huella(), {}));
  auto bytes = escribir_cadena(ocho);
  const auto uno = escribir_cadena(Cadena{larga.front()});
  REQUIRE(!bytes.empty());
  REQUIRE(!uno.empty());
  bytes[0] = static_cast<std::uint8_t>(kMaximoEslabones + 1);
  bytes.insert(bytes.end(), uno.begin() + 1, uno.end());
  CHECK_FALSE(leer_cadena(bytes));
}

TEST_CASE("Una cadena donde no aparece la huella guardada no se acepta") {
  const auto claves = claves_nuevas(4);
  Cadena cadena = encadenar(claves);
  cadena.erase(cadena.begin());
  CHECK_FALSE(seguir_cadena(cadena, claves[0].huella(), claves[3].huella(), {}));
  CHECK_FALSE(seguir_cadena(Cadena{}, claves[0].huella(), claves[3].huella(), {}));
  CHECK_FALSE(seguir_cadena(cadena, claves[3].huella(), claves[3].huella(), {}));
}

TEST_CASE("La cadena se lee tal como se escribe; cortada, vacía o con bytes de más, no") {
  const auto claves = claves_nuevas(4);
  const Cadena cadena = encadenar(claves);
  const auto bytes = escribir_cadena(cadena);
  REQUIRE(!bytes.empty());
  const auto leida = leer_cadena(bytes);
  REQUIRE(leida);
  REQUIRE(leida->size() == cadena.size());
  for (std::size_t i = 0; i < cadena.size(); ++i) {
    CHECK(iguales((*leida)[i], cadena[i]));
  }
  CHECK(seguir_cadena(*leida, claves[0].huella(), claves[3].huella(), {}));
  for (std::size_t largo = 0; largo < bytes.size(); ++largo) {
    CAPTURE(largo);
    CHECK_FALSE(leer_cadena(gsl::span<const std::uint8_t>(bytes).first(largo)));
  }
  auto de_mas = bytes;
  de_mas.push_back(0);
  CHECK_FALSE(leer_cadena(de_mas));
  CHECK(escribir_cadena(Cadena{}).empty());
  CHECK_FALSE(leer_cadena(std::vector<std::uint8_t>{0}));
  const std::size_t donde_va_el_largo = 1 + 2 * ventrik::cifrado::kBytesClavePublica + 4;
  for (const std::uint8_t malo : {std::uint8_t{0}, static_cast<std::uint8_t>(ventrik::cifrado::kMaximoFirma + 1)}) {
    auto firma_mala = escribir_cadena(Cadena{cadena[0]});
    firma_mala[donde_va_el_largo] = malo;
    firma_mala.resize(donde_va_el_largo + 1 + malo);
    CHECK_FALSE(leer_cadena(firma_mala));
  }
  Cadena sin_firma = cadena;
  sin_firma[1].firma.clear();
  CHECK(escribir_cadena(sin_firma).empty());
}

TEST_CASE("Agregar a una cadena de 8 descarta el eslabón más viejo") {
  const auto claves = claves_nuevas(kMaximoEslabones + 2);
  const Cadena cadena = encadenar(claves);
  REQUIRE(cadena.size() == kMaximoEslabones);
  CHECK(cadena.front().vieja == claves[1].publica());
  CHECK(cadena.front().numero == 2);
  CHECK(cadena.back().nueva == claves.back().publica());
  CHECK(seguir_cadena(cadena, claves[1].huella(), claves.back().huella(), {}));
  CHECK_FALSE(seguir_cadena(cadena, claves[0].huella(), claves.back().huella(), {}));
  CHECK(!escribir_cadena(cadena).empty());
}

TEST_CASE("Agregar rechaza un eslabón que no sigue a la cadena o que no está bien firmado") {
  const auto claves = claves_nuevas(4);
  Cadena cadena = encadenar(claves);
  const Cadena antes = cadena;
  CHECK_FALSE(agregar_eslabon(cadena, eslabon(claves[1], claves[2], 10)));
  Eslabon mal_firmado = eslabon(claves[3], claves[0], 10);
  mal_firmado.firma.back() ^= 1;
  CHECK_FALSE(agregar_eslabon(cadena, mal_firmado));
  REQUIRE(cadena.size() == antes.size());
  for (std::size_t i = 0; i < cadena.size(); ++i) {
    CHECK(iguales(cadena[i], antes[i]));
  }
  Cadena vacia;
  CHECK(agregar_eslabon(vacia, eslabon(claves[2], claves[3], 7)));
  CHECK(vacia.size() == 1);
}
