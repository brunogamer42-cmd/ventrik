#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "client/guardados.h"

using ventrik::cliente::armar_guardados;
using ventrik::cliente::Guardados;
using ventrik::cliente::leer_guardados;
using ventrik::cliente::LecturaGuardados;
using ventrik::cliente::ServidorGuardado;

namespace {
ServidorGuardado servidor(std::uint32_t ip, std::uint16_t puerto = 7700, std::string nombre = "Tano") {
  ServidorGuardado guardado;
  guardado.direccion = {ip, puerto};
  guardado.huella.fill(0xAB);
  guardado.nombre = std::move(nombre);
  guardado.canal = 3;
  guardado.nombre_canal = "General";
  guardado.ultima_vez_ms = 1761234567890;
  return guardado;
}

Guardados ejemplo() {
  Guardados guardados;
  guardados.tecla_hablar = 0xA4;
  guardados.nombre = "Tano";
  guardados.servidores = {servidor(0xC0A80099), servidor(0x0A000001, 9000, "Ñandú")};
  guardados.servidores[1].nombre_canal.clear();
  return guardados;
}

LecturaGuardados leer_sin_tocar(const std::vector<std::uint8_t>& bytes) {
  Guardados guardados;
  guardados.nombre = "sin tocar";
  const LecturaGuardados lectura = leer_guardados(bytes, guardados);
  CHECK(guardados.nombre == "sin tocar");
  return lectura;
}

}

TEST_CASE("los servidores guardados van y vuelven iguales") {
  const auto bytes = armar_guardados(ejemplo());
  REQUIRE(!bytes.empty());
  Guardados leidos;
  REQUIRE(leer_guardados(bytes, leidos) == LecturaGuardados::leidos);
  CHECK(armar_guardados(leidos) == bytes);
  CHECK(leidos.tecla_hablar == 0xA4);
  CHECK(leidos.nombre == "Tano");
  REQUIRE(leidos.servidores.size() == 2);
  CHECK(leidos.servidores[0].direccion.ipv4 == 0xC0A80099);
  CHECK(leidos.servidores[0].huella[31] == 0xAB);
  CHECK(leidos.servidores[0].nombre_canal == "General");
  CHECK(leidos.servidores[0].ultima_vez_ms == 1761234567890);
  CHECK(leidos.servidores[1].direccion.puerto == 9000);
  CHECK(leidos.servidores[1].nombre == "Ñandú");
  CHECK(leidos.servidores[1].nombre_canal.empty());

  Guardados vacio;
  Guardados leido_vacio;
  CHECK(leer_guardados(armar_guardados(vacio), leido_vacio) == LecturaGuardados::leidos);
}

TEST_CASE("el archivo de servidores rechaza lo que no armó Ventrik") {
  const auto bien = armar_guardados(ejemplo());
  CHECK(leer_sin_tocar({}) == LecturaGuardados::danados);
  for (std::size_t largo = 0; largo < bien.size(); ++largo) {
    CHECK(leer_sin_tocar(std::vector<std::uint8_t>(bien.begin(), bien.begin() + largo)) ==
          LecturaGuardados::danados);
  }
  auto de_mas = bien;
  de_mas.push_back(0);
  CHECK(leer_sin_tocar(de_mas) == LecturaGuardados::danados);
  auto marca = bien;
  marca[0] ^= 1;
  CHECK(leer_sin_tocar(marca) == LecturaGuardados::danados);
  auto version = bien;
  version[4] = 2;
  CHECK(leer_sin_tocar(version) == LecturaGuardados::version_nueva);
  version[4] = 0;
  CHECK(leer_sin_tocar(version) == LecturaGuardados::danados);
  auto puerto = bien;
  puerto[18] = puerto[19] = 0;
  CHECK(leer_sin_tocar(puerto) == LecturaGuardados::danados);
  auto cantidad = bien;
  cantidad[13] = 3;
  CHECK(leer_sin_tocar(cantidad) == LecturaGuardados::danados);
  auto nombre = bien;
  nombre[9] = ' ';
  CHECK(leer_sin_tocar(nombre) == LecturaGuardados::danados);
}

TEST_CASE("no se arma un archivo de servidores con datos inválidos") {
  const auto sin_armar = [](auto cambiar) {
    Guardados guardados = ejemplo();
    cambiar(guardados);
    CHECK(armar_guardados(guardados).empty());
  };
  sin_armar([](Guardados& g) { g.nombre = " Tano"; });
  sin_armar([](Guardados& g) { g.servidores[0].direccion.puerto = 0; });
  sin_armar([](Guardados& g) { g.servidores[0].direccion.ipv4 = 0; });
  sin_armar([](Guardados& g) { g.servidores[0].nombre.clear(); });
  sin_armar([](Guardados& g) { g.servidores[0].nombre_canal = "a\nb"; });
  sin_armar([](Guardados& g) { g.servidores[1].direccion = g.servidores[0].direccion; });
  sin_armar([](Guardados& g) {
    for (std::uint32_t ip = 3; ip <= 33; ++ip) {
      g.servidores.push_back(servidor(ip));
    }
  });
}

TEST_CASE("el archivo de servidores no se lee con más de 32") {
  Guardados guardados;
  guardados.nombre = "Tano";
  for (std::uint32_t ip = 1; ip <= 33; ++ip) {
    guardados.servidores.push_back(servidor(ip));
  }
  const auto ultimo = armar_guardados(Guardados{0, "", {guardados.servidores.back()}});
  guardados.servidores.pop_back();
  auto bytes = armar_guardados(guardados);
  REQUIRE(bytes.size() > 13);
  bytes[13] = 33;
  bytes.insert(bytes.end(), ultimo.begin() + 10, ultimo.end());
  CHECK(leer_sin_tocar(bytes) == LecturaGuardados::danados);
  bytes[13] = 32;
  bytes.resize(bytes.size() - (ultimo.size() - 10));
  Guardados leidos;
  CHECK(leer_guardados(bytes, leidos) == LecturaGuardados::leidos);
}

TEST_CASE("recordar pone el servidor adelante, lo actualiza y guarda hasta 32") {
  Guardados guardados;
  CHECK(!ventrik::cliente::recordar(guardados, servidor(0)));
  CHECK(!ventrik::cliente::recordar(guardados, servidor(1, 7700, " mal")));
  CHECK(guardados.servidores.empty());
  for (std::uint32_t ip = 1; ip <= 40; ++ip) {
    ventrik::cliente::recordar(guardados, servidor(ip));
  }
  REQUIRE(guardados.servidores.size() == 32);
  CHECK(guardados.servidores.front().direccion.ipv4 == 40);
  CHECK(guardados.servidores.back().direccion.ipv4 == 9);
  ventrik::cliente::recordar(guardados, servidor(20, 7700, "Nuevo"));
  CHECK(guardados.servidores.size() == 32);
  CHECK(guardados.servidores.front().nombre == "Nuevo");
  CHECK(std::count_if(guardados.servidores.begin(), guardados.servidores.end(),
                      [](const ServidorGuardado& s) { return s.direccion.ipv4 == 20; }) == 1);
  CHECK(!armar_guardados(guardados).empty());
}
