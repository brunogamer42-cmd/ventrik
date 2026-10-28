#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "common/cifrado/dpapi.h"
#include "common/cifrado/sobre.h"
#include <doctest/doctest.h>

using ventrik::cifrado::AperturaSobre;
using ventrik::cifrado::Argon2;

namespace {

constexpr Argon2 kRapido{ventrik::cifrado::kMinimoMemoriaArgon2, 1};

std::vector<std::uint8_t> bytes(const std::string& texto) {
  return std::vector<std::uint8_t>(texto.begin(), texto.end());
}

}  // namespace

TEST_CASE("Lo sellado con AES-GCM se abre solo con la misma clave y los mismos datos asociados") {
  std::array<std::uint8_t, 32> clave{};
  clave[0] = 7;
  const auto asociados = bytes("cabecera");
  const auto datos = bytes("un secreto de prueba");
  const auto sellado = ventrik::cifrado::sellar(clave, asociados, datos);
  REQUIRE(sellado);
  CHECK(sellado->size() == datos.size() + ventrik::cifrado::kBytesNonce + ventrik::cifrado::kBytesEtiqueta);
  const auto abierto = ventrik::cifrado::abrir_sellado(clave, asociados, *sellado);
  REQUIRE(abierto);
  CHECK(std::vector<std::uint8_t>(abierto->bytes().begin(), abierto->bytes().end()) == datos);
  for (std::size_t i = 0; i < sellado->size(); ++i) {
    auto alterado = *sellado;
    alterado[i] ^= 0x01;
    CHECK_FALSE(ventrik::cifrado::abrir_sellado(clave, asociados, alterado));
  }
  CHECK_FALSE(ventrik::cifrado::abrir_sellado(clave, bytes("otra cabecera"), *sellado));
  auto otra_clave = clave;
  otra_clave[31] = 1;
  CHECK_FALSE(ventrik::cifrado::abrir_sellado(otra_clave, asociados, *sellado));
  CHECK_FALSE(ventrik::cifrado::abrir_sellado(clave, asociados, gsl::span<const std::uint8_t>(*sellado).first(27)));
  CHECK_FALSE(ventrik::cifrado::sellar(gsl::span<const std::uint8_t>(clave).first(16), asociados, datos));
}

TEST_CASE("Dos sellados del mismo texto no se parecen") {
  std::array<std::uint8_t, 32> clave{};
  const auto datos = bytes("lo mismo");
  const auto primero = ventrik::cifrado::sellar(clave, {}, datos);
  const auto segundo = ventrik::cifrado::sellar(clave, {}, datos);
  REQUIRE(primero);
  REQUIRE(segundo);
  CHECK(*primero != *segundo);
}

TEST_CASE("Un sobre se abre con su contraseña y no con otra") {
  const auto contenido = bytes("clave privada de mentira");
  const auto sobre = ventrik::cifrado::cerrar_sobre("VKID", contenido, "una contraseña larga", kRapido);
  REQUIRE(sobre);
  const auto abierto = ventrik::cifrado::abrir_sobre("VKID", *sobre, "una contraseña larga");
  REQUIRE(abierto.resultado == AperturaSobre::abierto);
  REQUIRE(abierto.contenido);
  CHECK(std::vector<std::uint8_t>(abierto.contenido->bytes().begin(), abierto.contenido->bytes().end()) == contenido);
  CHECK(ventrik::cifrado::abrir_sobre("VKID", *sobre, "una contraseña larg").resultado == AperturaSobre::contrasena);
  CHECK(ventrik::cifrado::abrir_sobre("VKSV", *sobre, "una contraseña larga").resultado == AperturaSobre::danado);
}

TEST_CASE("Un sobre alterado no se abre") {
  const auto sobre = ventrik::cifrado::cerrar_sobre("VKID", bytes("contenido"), "contraseña", kRapido);
  REQUIRE(sobre);
  for (const std::size_t i : {std::size_t{0}, std::size_t{3}, std::size_t{4}, std::size_t{5}, std::size_t{9},
                              std::size_t{10}, std::size_t{25}, std::size_t{26}, std::size_t{38}, sobre->size() - 1}) {
    auto alterado = *sobre;
    alterado[i] ^= 0x10;
    CHECK(ventrik::cifrado::abrir_sobre("VKID", alterado, "contraseña").resultado != AperturaSobre::abierto);
  }
  CHECK(ventrik::cifrado::abrir_sobre("VKID", gsl::span<const std::uint8_t>(*sobre).first(30), "contraseña").resultado ==
        AperturaSobre::danado);
}

TEST_CASE("Un sobre que pide valores de Argon2 fuera de rango se rechaza sin calcular") {
  const auto sobre = ventrik::cifrado::cerrar_sobre("VKID", bytes("contenido"), "contraseña", kRapido);
  REQUIRE(sobre);
  const auto con = [&sobre](std::uint32_t memoria, std::uint8_t vueltas) {
    auto armado = *sobre;
    armado[5] = static_cast<std::uint8_t>(memoria & 0xFF);
    armado[6] = static_cast<std::uint8_t>((memoria >> 8) & 0xFF);
    armado[7] = static_cast<std::uint8_t>((memoria >> 16) & 0xFF);
    armado[8] = static_cast<std::uint8_t>(memoria >> 24);
    armado[9] = vueltas;
    return armado;
  };
  CHECK(ventrik::cifrado::leer_cabecera_sobre("VKID", con(kRapido.memoria_kib, 1)));
  CHECK_FALSE(ventrik::cifrado::leer_cabecera_sobre("VKID", con(0xFFFFFFFF, 1)));
  CHECK_FALSE(ventrik::cifrado::leer_cabecera_sobre("VKID", con(ventrik::cifrado::kMaximoMemoriaArgon2 + 1, 1)));
  CHECK_FALSE(ventrik::cifrado::leer_cabecera_sobre("VKID", con(ventrik::cifrado::kMinimoMemoriaArgon2 - 1, 1)));
  CHECK_FALSE(ventrik::cifrado::leer_cabecera_sobre("VKID", con(kRapido.memoria_kib, 0)));
  CHECK_FALSE(ventrik::cifrado::leer_cabecera_sobre("VKID", con(kRapido.memoria_kib, 11)));
  CHECK(ventrik::cifrado::abrir_sobre("VKID", con(0xFFFFFFFF, 1), "contraseña").resultado == AperturaSobre::danado);
}

TEST_CASE("Sin valores fijos, Argon2 se ajusta a esta PC y el sobre se abre") {
  const auto sobre = ventrik::cifrado::cerrar_sobre("VKID", bytes("contenido"), "contraseña de prueba");
  REQUIRE(sobre);
  const auto cabecera = ventrik::cifrado::leer_cabecera_sobre("VKID", *sobre);
  REQUIRE(cabecera);
  CHECK(cabecera->argon2.memoria_kib == ventrik::cifrado::memoria_para_esta_pc().memoria_kib);
  CHECK(cabecera->argon2.vueltas >= 1);
  CHECK(cabecera->argon2.vueltas <= ventrik::cifrado::kMaximoVueltasArgon2);
  CHECK(ventrik::cifrado::abrir_sobre("VKID", *sobre, "contraseña de prueba").resultado == AperturaSobre::abierto);
}

TEST_CASE("Argon2 da siempre lo mismo para la misma contraseña y sal, y otra cosa con otra sal") {
  const auto sal = bytes("sal de dieciseis");
  std::array<std::uint8_t, 32> primero{};
  std::array<std::uint8_t, 32> segundo{};
  std::array<std::uint8_t, 32> otra{};
  REQUIRE(ventrik::cifrado::derivar_argon2("clave", sal, kRapido, primero));
  REQUIRE(ventrik::cifrado::derivar_argon2("clave", sal, kRapido, segundo));
  REQUIRE(ventrik::cifrado::derivar_argon2("clave", bytes("otra sal de 16 b"), kRapido, otra));
  CHECK(primero == segundo);
  CHECK(primero != otra);
  CHECK_FALSE(ventrik::cifrado::derivar_argon2("clave", sal, Argon2{256, 1}, primero));
}

TEST_CASE("DPAPI devuelve lo protegido solo con el mismo propósito") {
  const auto datos = bytes("algo para proteger");
  const auto protegido = ventrik::cifrado::proteger_con_windows(datos, "ventrik prueba");
  REQUIRE(protegido);
  CHECK(*protegido != datos);
  const auto abierto = ventrik::cifrado::abrir_con_windows(*protegido, "ventrik prueba");
  REQUIRE(abierto);
  CHECK(std::vector<std::uint8_t>(abierto->bytes().begin(), abierto->bytes().end()) == datos);
  CHECK_FALSE(ventrik::cifrado::abrir_con_windows(*protegido, "otro propósito"));
  auto alterado = *protegido;
  alterado[alterado.size() / 2] ^= 0x01;
  CHECK_FALSE(ventrik::cifrado::abrir_con_windows(alterado, "ventrik prueba"));
  CHECK_FALSE(ventrik::cifrado::proteger_con_windows({}, "ventrik prueba"));
}
