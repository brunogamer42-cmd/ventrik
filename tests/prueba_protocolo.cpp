#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <gsl/gsl>

#include "common/protocol/bytes.h"
#include "common/protocol/marcos.h"
#include "common/protocol/mensajes.h"
#include "common/protocol/negociacion.h"
#include <doctest/doctest.h>

using namespace ventrik::protocolo;
using Bytes = std::vector<std::uint8_t>;

namespace {

const ventrik::VersionPrograma kVersion012{0, 1, 2};

Hola hola_de_prueba() {
  return Hola{1, 3, kVersion012, 0x0102030405060708u};
}

// Los mismos bytes en las dos versiones: así se prueba que 32 y 64 bits se entienden.
const Bytes kHolaEnBytes = {
    0x01, 0x00, 0x03, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x02, 0x00,
    0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
};

template <typename Mensaje>
Bytes en_bytes(const Mensaje& mensaje) {
  Escritor escritor;
  escribir(escritor, mensaje);
  REQUIRE(escritor.ok());
  return escritor.datos();
}

std::vector<Marco> ensamblar(Ensamblador& ensamblador, const Bytes& datos, std::size_t paso) {
  std::vector<Marco> marcos;
  gsl::span<const std::uint8_t> resto(datos);
  while (!resto.empty()) {
    auto pedazo = resto.first(std::min(paso, resto.size()));
    resto = resto.subspan(pedazo.size());
    while (auto marco = ensamblador.siguiente(pedazo)) {
      marcos.push_back(std::move(*marco));
    }
    CHECK(pedazo.empty());
  }
  return marcos;
}

ConfigServidor servidor_de_prueba(std::uint16_t minimo, std::uint16_t maximo) {
  ConfigServidor servidor;
  servidor.protocolo_minimo = minimo;
  servidor.protocolo_maximo = maximo;
  servidor.programa_minimo = {0, 1, 0};
  servidor.funciones = 0b1100;
  return servidor;
}

}  // namespace

TEST_CASE("El escritor arma los números en little-endian") {
  Escritor escritor;
  escritor.escribir_u8(0xAB);
  escritor.escribir_u16(0x1234);
  escritor.escribir_u32(0x12345678u);
  escritor.escribir_u64(0x0102030405060708u);
  escritor.escribir_booleano(true);
  const Bytes esperado = {0xAB, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0x08, 0x07,
                          0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x01};
  CHECK(escritor.ok());
  CHECK(escritor.datos() == esperado);
}

TEST_CASE("El lector lee lo que arma el escritor") {
  Escritor escritor;
  escritor.escribir_u8(0xFF);
  escritor.escribir_u16(0xFFFF);
  escritor.escribir_u32(0xFFFFFFFFu);
  escritor.escribir_u64(0xFFFFFFFFFFFFFFFFu);
  escritor.escribir_booleano(false);
  escritor.escribir_texto("canci\xC3\xB3n", 32);
  escritor.escribir_texto("", 32);
  const Bytes extra = {9, 8, 7};
  escritor.escribir_bytes(extra);
  REQUIRE(escritor.ok());

  Lector lector(escritor.datos());
  CHECK(lector.leer_u8() == 0xFF);
  CHECK(lector.leer_u16() == 0xFFFF);
  CHECK(lector.leer_u32() == 0xFFFFFFFFu);
  CHECK(lector.leer_u64() == 0xFFFFFFFFFFFFFFFFu);
  CHECK_FALSE(lector.leer_booleano());
  CHECK(lector.leer_texto(32) == "canci\xC3\xB3n");
  CHECK(lector.leer_texto(32).empty());
  const auto bytes = lector.leer_bytes(3);
  CHECK(Bytes(bytes.begin(), bytes.end()) == extra);
  CHECK(lector.terminado());
}

static_assert(std::is_constructible<Lector, const Bytes&>::value, "");
static_assert(!std::is_constructible<Lector, Bytes>::value,
              "Un lector sobre un vector temporal leería memoria liberada");

TEST_CASE("El lector que lee de más falla y sigue fallando") {
  const Bytes datos = {1, 2, 3};
  Lector lector(datos);
  CHECK(lector.leer_u32() == 0);
  CHECK_FALSE(lector.ok());
  CHECK(lector.leer_u8() == 0);
  CHECK(lector.leer_bytes(0).empty());
  CHECK_FALSE(lector.ok());
  CHECK_FALSE(lector.terminado());
  CHECK(lector.restante() == 0);
}

TEST_CASE("El lector rechaza pedir más bytes de los que hay, aunque sea muchísimo") {
  const Bytes datos = {1, 2, 3};
  Lector lector(datos);
  CHECK(lector.leer_u8() == 1);
  CHECK(lector.leer_bytes(static_cast<std::size_t>(-1)).empty());
  CHECK_FALSE(lector.ok());
}

TEST_CASE("Un booleano solo puede ser 0 o 1") {
  const Bytes uno = {1};
  Lector lector_uno(uno);
  CHECK(lector_uno.leer_booleano());
  CHECK(lector_uno.terminado());

  const Bytes dos = {2};
  Lector lector_dos(dos);
  CHECK_FALSE(lector_dos.leer_booleano());
  CHECK_FALSE(lector_dos.ok());
}

TEST_CASE("El lector rechaza textos rotos") {
  SUBCASE("el largo dice más de lo que llegó") {
    const Bytes datos = {5, 0, 'h', 'o', 'l', 'a'};
    Lector lector(datos);
    CHECK(lector.leer_texto(100).empty());
    CHECK_FALSE(lector.ok());
  }
  SUBCASE("más largo que el máximo") {
    const Bytes datos = {4, 0, 'h', 'o', 'l', 'a'};
    Lector lector(datos);
    CHECK(lector.leer_texto(3).empty());
    CHECK_FALSE(lector.ok());
  }
  SUBCASE("UTF-8 inválido") {
    const Bytes datos = {2, 0, 0xC0, 0xAF};
    Lector lector(datos);
    CHECK(lector.leer_texto(100).empty());
    CHECK_FALSE(lector.ok());
  }
  SUBCASE("mitad de par UTF-16") {
    const Bytes datos = {3, 0, 0xED, 0xA0, 0x80};
    Lector lector(datos);
    CHECK(lector.leer_texto(100).empty());
    CHECK_FALSE(lector.ok());
  }
  SUBCASE("con un byte 0 adentro") {
    const Bytes datos = {3, 0, 'a', 0, 'b'};
    Lector lector(datos);
    CHECK(lector.leer_texto(100).empty());
    CHECK_FALSE(lector.ok());
  }
}

TEST_CASE("El escritor no recorta ni manda textos inválidos") {
  SUBCASE("más largo que el máximo") {
    Escritor escritor;
    escritor.escribir_texto("hola", 3);
    CHECK_FALSE(escritor.ok());
    CHECK(escritor.datos().empty());
  }
  SUBCASE("más largo de lo que entra en el largo de 16 bits") {
    Escritor escritor;
    escritor.escribir_texto(std::string(70000, 'a'), 100000);
    CHECK_FALSE(escritor.ok());
    CHECK(escritor.datos().empty());
  }
  SUBCASE("UTF-8 inválido") {
    Escritor escritor;
    escritor.escribir_texto("\xFF", 10);
    CHECK_FALSE(escritor.ok());
  }
  SUBCASE("después de un error no escribe nada más") {
    Escritor escritor;
    escritor.escribir_texto("\xFF", 10);
    escritor.escribir_u32(7);
    CHECK(escritor.datos().empty());
  }
  SUBCASE("un error borra lo que ya estaba escrito, así no sale un mensaje a medias") {
    Escritor escritor;
    escritor.escribir_u32(7);
    escritor.escribir_texto("\xFF", 10);
    CHECK_FALSE(escritor.ok());
    CHECK(escritor.datos().empty());
  }
}

TEST_CASE("El saludo tiene bytes fijos") {
  CHECK(en_bytes(hola_de_prueba()) == kHolaEnBytes);
  CHECK(en_bytes(Bienvenida{2, 0x0Fu}) ==
        Bytes{0x02, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  CHECK(en_bytes(Rechazo{MotivoRechazo::programa_viejo, {1, 2, 3}}) ==
        Bytes{0x01, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00});

  Bytes marco;
  REQUIRE(armar_marco(static_cast<std::uint16_t>(TipoMensaje::hola), kHolaEnBytes, marco));
  Bytes esperado = {0x12, 0x00, 0x00, 0x00, 0x01, 0x00};
  esperado.insert(esperado.end(), kHolaEnBytes.begin(), kHolaEnBytes.end());
  CHECK(marco == esperado);
}

TEST_CASE("Los mensajes del saludo se leen tal como se escribieron") {
  CHECK(leer_hola(kHolaEnBytes) == hola_de_prueba());
  const Bienvenida bienvenida{2, 0x0Fu};
  CHECK(leer_bienvenida(en_bytes(bienvenida)) == bienvenida);
  for (const Rechazo& rechazo : {Rechazo{MotivoRechazo::programa_viejo, {1, 2, 3}},
                                 Rechazo{MotivoRechazo::servidor_viejo, {}},
                                 Rechazo{MotivoRechazo::saludo_invalido, {}}}) {
    CHECK(leer_rechazo(en_bytes(rechazo)) == rechazo);
  }
}

TEST_CASE("Un mensaje cortado o con bytes de sobra no se acepta") {
  const Bytes bienvenida = en_bytes(Bienvenida{1, 0});
  const Bytes rechazo = en_bytes(Rechazo{MotivoRechazo::servidor_viejo, {}});
  for (std::size_t largo = 0; largo < kHolaEnBytes.size(); ++largo) {
    CHECK_FALSE(leer_hola(gsl::span<const std::uint8_t>(kHolaEnBytes).first(largo)));
  }
  for (std::size_t largo = 0; largo < bienvenida.size(); ++largo) {
    CHECK_FALSE(leer_bienvenida(gsl::span<const std::uint8_t>(bienvenida).first(largo)));
  }
  for (std::size_t largo = 0; largo < rechazo.size(); ++largo) {
    CHECK_FALSE(leer_rechazo(gsl::span<const std::uint8_t>(rechazo).first(largo)));
  }

  Bytes hola_de_mas = kHolaEnBytes;
  hola_de_mas.push_back(0);
  Bytes bienvenida_de_mas = bienvenida;
  bienvenida_de_mas.push_back(0);
  Bytes rechazo_de_mas = rechazo;
  rechazo_de_mas.push_back(0);
  CHECK_FALSE(leer_hola(hola_de_mas));
  CHECK_FALSE(leer_bienvenida(bienvenida_de_mas));
  CHECK_FALSE(leer_rechazo(rechazo_de_mas));
}

TEST_CASE("Un rechazo con un motivo que no existe no se acepta") {
  for (const std::uint8_t motivo : {std::uint8_t{0}, std::uint8_t{4}, std::uint8_t{255}}) {
    const Bytes datos = {motivo, 0, 0, 0, 0, 0, 0};
    CHECK_FALSE(leer_rechazo(datos));
  }
}

TEST_CASE("Cada rechazo tiene una sola forma válida") {
  CHECK_FALSE(leer_rechazo(en_bytes(Rechazo{MotivoRechazo::programa_viejo, {}})));
  CHECK_FALSE(leer_rechazo(en_bytes(Rechazo{MotivoRechazo::servidor_viejo, {0, 0, 1}})));
  CHECK_FALSE(leer_rechazo(en_bytes(Rechazo{MotivoRechazo::saludo_invalido, {1, 0, 0}})));
}

TEST_CASE("El ensamblador arma mensajes partidos o juntos") {
  Bytes flujo;
  REQUIRE(armar_marco(1, kHolaEnBytes, flujo));
  REQUIRE(armar_marco(7, Bytes{}, flujo));
  REQUIRE(armar_marco(2, Bytes{42}, flujo));

  for (const std::size_t paso : {std::size_t{1}, std::size_t{5}, std::size_t{7}, flujo.size()}) {
    CAPTURE(paso);
    Ensamblador ensamblador;
    const auto marcos = ensamblar(ensamblador, flujo, paso);
    CHECK_FALSE(ensamblador.error());
    REQUIRE(marcos.size() == 3);
    CHECK(marcos[0].tipo == 1);
    CHECK(marcos[0].cuerpo == kHolaEnBytes);
    CHECK(marcos[1].tipo == 7);
    CHECK(marcos[1].cuerpo.empty());
    CHECK(marcos[2].tipo == 2);
    CHECK(marcos[2].cuerpo == Bytes{42});
  }
}

TEST_CASE("El ensamblador corta apenas ve un largo mayor al tope") {
  Bytes flujo = {0x01, 0x00, 0x01, 0x00, 0x01, 0x00};
  flujo.resize(flujo.size() + 100, 0);
  Ensamblador ensamblador;
  gsl::span<const std::uint8_t> datos(flujo);
  CHECK_FALSE(ensamblador.siguiente(datos));
  CHECK(ensamblador.error());
  CHECK(datos.empty());

  gsl::span<const std::uint8_t> mas(flujo);
  CHECK_FALSE(ensamblador.siguiente(mas));
  CHECK(mas.empty());
}

TEST_CASE("El ensamblador devuelve un marco por vez y deja el resto") {
  Bytes flujo;
  REQUIRE(armar_marco(1, Bytes{1}, flujo));
  REQUIRE(armar_marco(2, Bytes{2}, flujo));
  Ensamblador ensamblador;
  gsl::span<const std::uint8_t> datos(flujo);
  const auto primero = ensamblador.siguiente(datos);
  REQUIRE(primero);
  CHECK(primero->tipo == 1);
  CHECK(datos.size() == kCabeceraMarco + 1);
}

TEST_CASE("Con un tope chico, el ensamblador corta antes") {
  Bytes chico;
  REQUIRE(armar_marco(1, Bytes(64, 1), chico));
  Bytes grande;
  REQUIRE(armar_marco(1, Bytes(65, 1), grande));

  Ensamblador ensamblador(64);
  CHECK(ensamblar(ensamblador, chico, 10).size() == 1);
  CHECK_FALSE(ensamblador.error());
  ensamblador.cambiar_maximo(kMaximoCuerpo);
  CHECK(ensamblar(ensamblador, grande, 10).size() == 1);
  CHECK_FALSE(ensamblador.error());

  Ensamblador en_saludo(64);
  CHECK(ensamblar(en_saludo, grande, 10).empty());
  CHECK(en_saludo.error());
}

TEST_CASE("El ensamblador acepta un mensaje justo del tope") {
  const Bytes cuerpo(kMaximoCuerpo, 0x5A);
  Bytes flujo;
  REQUIRE(armar_marco(3, cuerpo, flujo));
  Ensamblador ensamblador;
  const auto marcos = ensamblar(ensamblador, flujo, 1000);
  REQUIRE(marcos.size() == 1);
  CHECK(marcos[0].cuerpo == cuerpo);
}

TEST_CASE("No se arma un mensaje más grande que el tope") {
  const Bytes cuerpo(kMaximoCuerpo + 1, 0);
  Bytes salida;
  CHECK_FALSE(armar_marco(3, cuerpo, salida));
  CHECK(salida.empty());
}

TEST_CASE("La negociación elige la versión más alta que entienden los dos") {
  const Hola hola{1, 3, kVersion012, 0b1010};
  SUBCASE("misma versión") {
    const auto respuesta = negociar(hola, servidor_de_prueba(1, 3));
    REQUIRE(std::holds_alternative<Bienvenida>(respuesta));
    CHECK(std::get<Bienvenida>(respuesta).protocolo == 3);
  }
  SUBCASE("el servidor es más nuevo") {
    const auto respuesta = negociar(hola, servidor_de_prueba(2, 5));
    REQUIRE(std::holds_alternative<Bienvenida>(respuesta));
    CHECK(std::get<Bienvenida>(respuesta).protocolo == 3);
  }
  SUBCASE("el servidor es más viejo") {
    const auto respuesta = negociar(hola, servidor_de_prueba(1, 2));
    REQUIRE(std::holds_alternative<Bienvenida>(respuesta));
    CHECK(std::get<Bienvenida>(respuesta).protocolo == 2);
  }
  SUBCASE("solo las funciones que tienen los dos") {
    const auto respuesta = negociar(hola, servidor_de_prueba(1, 3));
    REQUIRE(std::holds_alternative<Bienvenida>(respuesta));
    CHECK(std::get<Bienvenida>(respuesta).funciones == 0b1000);
  }
}

TEST_CASE("La negociación rechaza con el motivo correcto") {
  SUBCASE("programa con un protocolo demasiado viejo: le dice a qué versión actualizar") {
    ConfigServidor servidor = servidor_de_prueba(2, 3);
    servidor.programa_minimo = {0, 3, 0};
    const auto respuesta = negociar(Hola{1, 1, kVersion012, 0}, servidor);
    REQUIRE(std::holds_alternative<Rechazo>(respuesta));
    CHECK(std::get<Rechazo>(respuesta) ==
          Rechazo{MotivoRechazo::programa_viejo, ventrik::VersionPrograma{0, 3, 0}});
  }
  SUBCASE("programa más viejo que el mínimo del servidor") {
    ConfigServidor servidor = servidor_de_prueba(1, 3);
    servidor.programa_minimo = {0, 2, 0};
    const auto respuesta = negociar(hola_de_prueba(), servidor);
    REQUIRE(std::holds_alternative<Rechazo>(respuesta));
    CHECK(std::get<Rechazo>(respuesta) ==
          Rechazo{MotivoRechazo::programa_viejo, ventrik::VersionPrograma{0, 2, 0}});
  }
  SUBCASE("servidor demasiado viejo") {
    const auto respuesta = negociar(Hola{4, 6, kVersion012, 0}, servidor_de_prueba(1, 3));
    REQUIRE(std::holds_alternative<Rechazo>(respuesta));
    CHECK(std::get<Rechazo>(respuesta).motivo == MotivoRechazo::servidor_viejo);
  }
  SUBCASE("rango al revés") {
    const auto respuesta = negociar(Hola{3, 1, kVersion012, 0}, servidor_de_prueba(1, 3));
    REQUIRE(std::holds_alternative<Rechazo>(respuesta));
    CHECK(std::get<Rechazo>(respuesta).motivo == MotivoRechazo::saludo_invalido);
  }
  SUBCASE("nunca pide una versión que no hable el protocolo del servidor") {
    const auto respuesta = negociar(Hola{1, 1, {0, 0, 5}, 0}, ConfigServidor{});
    REQUIRE(std::holds_alternative<Rechazo>(respuesta));
    CHECK(std::get<Rechazo>(respuesta) ==
          Rechazo{MotivoRechazo::programa_viejo, ventrik::kPrimeraVersionDelProtocolo[0]});
  }
  SUBCASE("protocolo 0") {
    const auto respuesta = negociar(Hola{0, 3, kVersion012, 0}, servidor_de_prueba(1, 3));
    REQUIRE(std::holds_alternative<Rechazo>(respuesta));
    CHECK(std::get<Rechazo>(respuesta).motivo == MotivoRechazo::saludo_invalido);
  }
}

TEST_CASE("El programa rechaza una bienvenida que no pudo pedir") {
  const Hola enviado{2, 3, kVersion012, 0b0110};
  CHECK(aceptar_bienvenida(enviado, Bienvenida{3, 0b0110}));
  CHECK(aceptar_bienvenida(enviado, Bienvenida{2, 0}));
  CHECK_FALSE(aceptar_bienvenida(enviado, Bienvenida{1, 0}));
  CHECK_FALSE(aceptar_bienvenida(enviado, Bienvenida{4, 0}));
  CHECK_FALSE(aceptar_bienvenida(enviado, Bienvenida{3, 0b1000}));
}

TEST_CASE("El programa rechaza un pedido de actualizar que no tiene sentido") {
  const Hola enviado{1, 1, kVersion012, 0};
  CHECK(aceptar_rechazo(enviado, Rechazo{MotivoRechazo::programa_viejo, {0, 2, 0}}));
  CHECK_FALSE(aceptar_rechazo(enviado, Rechazo{MotivoRechazo::programa_viejo, {0, 1, 2}}));
  CHECK_FALSE(aceptar_rechazo(enviado, Rechazo{MotivoRechazo::programa_viejo, {0, 1, 0}}));
  CHECK(aceptar_rechazo(enviado, Rechazo{MotivoRechazo::servidor_viejo, {}}));
}
