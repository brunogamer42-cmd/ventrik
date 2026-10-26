#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "client/guardados.h"
#include "common/protocol/bytes.h"

using ventrik::cliente::armar_guardados;
using ventrik::cliente::Guardados;
using ventrik::cliente::leer_guardados;
using ventrik::cliente::LecturaGuardados;
using ventrik::cliente::Leido;
using ventrik::cliente::ServidorGuardado;

namespace {
ServidorGuardado servidor(std::uint32_t ip, std::uint16_t puerto = 7700, std::string nombre = "Tano") {
  ServidorGuardado guardado;
  guardado.direccion = {ip, puerto};
  guardado.huella.fill(0xAB);
  guardado.nombre = std::move(nombre);
  guardado.texto = 3;
  guardado.nombre_canal = "General";
  guardado.ultima_vez_ms = 1761234567890;
  return guardado;
}

Guardados ejemplo() {
  Guardados guardados;
  guardados.tecla_hablar = 0xA4;
  guardados.por_voz = true;
  guardados.sensibilidad = 55;
  guardados.silencio = 1;
  guardados.volumen_microfono = 70;
  guardados.volumen_salida = 185;
  guardados.nombre = "Tano";
  guardados.servidores = {servidor(0xC0A80099), servidor(0x0A000001, 9000, "Ñandú")};
  guardados.servidores[1].nombre_canal.clear();
  guardados.servidores[0].leidos = {{1, 14}, {3, 0}, {9, 4000000000u}};
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
  CHECK(leidos.por_voz);
  CHECK(leidos.sensibilidad == 55);
  CHECK(leidos.silencio == 1);
  CHECK(leidos.volumen_microfono == 70);
  CHECK(leidos.volumen_salida == 185);
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
  version[4] = 5;
  CHECK(leer_sin_tocar(version) == LecturaGuardados::version_nueva);
  version[4] = 0;
  CHECK(leer_sin_tocar(version) == LecturaGuardados::danados);
  auto puerto = bien;
  puerto[23] = puerto[24] = 0;
  CHECK(leer_sin_tocar(puerto) == LecturaGuardados::danados);
  auto cantidad = bien;
  cantidad[18] = 3;
  CHECK(leer_sin_tocar(cantidad) == LecturaGuardados::danados);
  auto nombre = bien;
  nombre[14] = ' ';
  CHECK(leer_sin_tocar(nombre) == LecturaGuardados::danados);
  for (const std::uint8_t tecla : std::initializer_list<std::uint8_t>{0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x1B, 0xFF}) {
    auto con_tecla = bien;
    con_tecla[6] = tecla;
    CHECK(leer_sin_tocar(con_tecla) == LecturaGuardados::danados);
  }
  auto modo = bien;
  modo[7] = 2;
  CHECK(leer_sin_tocar(modo) == LecturaGuardados::danados);
  auto sensibilidad = bien;
  sensibilidad[8] = 101;
  CHECK(leer_sin_tocar(sensibilidad) == LecturaGuardados::danados);
  sensibilidad[8] = 100;
  Guardados al_tope;
  CHECK(leer_guardados(sensibilidad, al_tope) == LecturaGuardados::leidos);
  for (const std::uint8_t silencio : std::initializer_list<std::uint8_t>{2, 4, 255}) {
    auto con_silencio = bien;
    con_silencio[9] = silencio;
    CHECK(leer_sin_tocar(con_silencio) == LecturaGuardados::danados);
  }
  for (const std::size_t cual : std::initializer_list<std::size_t>{10, 11}) {
    auto volumen = bien;
    volumen[cual] = 201;
    CHECK(leer_sin_tocar(volumen) == LecturaGuardados::danados);
    volumen[cual] = 200;
    Guardados al_maximo;
    CHECK(leer_guardados(volumen, al_maximo) == LecturaGuardados::leidos);
  }
}

TEST_CASE("una lista de la versión 3 se lee sin silencio y con los volúmenes al 100 %") {
  ventrik::protocolo::Escritor escritor;
  escritor.escribir_u32(0x52504B56);
  escritor.escribir_u16(3);
  escritor.escribir_u8(0xA4);
  escritor.escribir_u8(1);
  escritor.escribir_u8(40);
  escritor.escribir_texto("Tano", 64);
  escritor.escribir_u8(0);
  Guardados leidos;
  REQUIRE(leer_guardados(escritor.datos(), leidos) == LecturaGuardados::leidos);
  CHECK(leidos.por_voz);
  CHECK(leidos.sensibilidad == 40);
  CHECK(leidos.silencio == 0);
  CHECK(leidos.volumen_microfono == ventrik::cliente::kVolumenPorDefecto);
  CHECK(leidos.volumen_salida == ventrik::cliente::kVolumenPorDefecto);
  const auto nuevo = armar_guardados(leidos);
  REQUIRE(nuevo.size() == escritor.datos().size() + 3);
  CHECK(nuevo[4] == 4);
  CHECK(nuevo[9] == 0);
  CHECK(nuevo[10] == 100);
  CHECK(nuevo[11] == 100);
}

TEST_CASE("una lista de la versión 2 se lee con la tecla para hablar y la sensibilidad por defecto") {
  ventrik::protocolo::Escritor escritor;
  escritor.escribir_u32(0x52504B56);
  escritor.escribir_u16(2);
  escritor.escribir_u8(0xA5);
  escritor.escribir_texto("Tano", 64);
  escritor.escribir_u8(0);
  Guardados leidos;
  REQUIRE(leer_guardados(escritor.datos(), leidos) == LecturaGuardados::leidos);
  CHECK(leidos.tecla_hablar == 0xA5);
  CHECK_FALSE(leidos.por_voz);
  CHECK(leidos.sensibilidad == ventrik::cliente::kSensibilidadPorDefecto);
  const auto nuevo = armar_guardados(leidos);
  REQUIRE(nuevo.size() == escritor.datos().size() + 5);
  CHECK(nuevo[4] == 4);
  CHECK(nuevo[7] == 0);
  CHECK(nuevo[8] == ventrik::cliente::kSensibilidadPorDefecto);
}

TEST_CASE("la tecla para hablar es Alt izquierdo si no se eligió otra, y no puede ser un clic ni Esc") {
  for (const std::uint8_t tecla : std::initializer_list<std::uint8_t>{0x00, 0x04, 0x05, 0x06, 0x41, 0xA0, 0xA4, 0xA5, 0xFE}) {
    CHECK(ventrik::cliente::es_tecla_valida(tecla));
  }
  for (const std::uint8_t tecla : std::initializer_list<std::uint8_t>{0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x1B, 0xFF}) {
    CHECK_FALSE(ventrik::cliente::es_tecla_valida(tecla));
  }
  CHECK(ventrik::cliente::tecla_para_hablar(0) == 0xA4);
  CHECK(ventrik::cliente::tecla_para_hablar(0x05) == 0x05);
}

TEST_CASE("no se arma un archivo de servidores con datos inválidos") {
  const auto sin_armar = [](auto cambiar) {
    Guardados guardados = ejemplo();
    cambiar(guardados);
    CHECK(armar_guardados(guardados).empty());
  };
  sin_armar([](Guardados& g) { g.nombre = " Tano"; });
  sin_armar([](Guardados& g) { g.servidores[0].leidos = {{3, 1}, {1, 1}}; });
  sin_armar([](Guardados& g) { g.servidores[0].leidos = {{1, 1}, {1, 2}}; });
  sin_armar([](Guardados& g) { g.servidores[0].leidos = {{0, 1}}; });
  sin_armar([](Guardados& g) {
    g.servidores[0].leidos.clear();
    for (std::uint32_t canal = 1; canal <= 65; ++canal) {
      g.servidores[0].leidos.push_back({canal, 0});
    }
  });
  sin_armar([](Guardados& g) { g.tecla_hablar = 0x1B; });
  sin_armar([](Guardados& g) { g.sensibilidad = 101; });
  sin_armar([](Guardados& g) { g.silencio = 2; });
  sin_armar([](Guardados& g) { g.volumen_microfono = 201; });
  sin_armar([](Guardados& g) { g.volumen_salida = 201; });
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
  Guardados solo;
  solo.servidores = {guardados.servidores.back()};
  const auto ultimo = armar_guardados(solo);
  guardados.servidores.pop_back();
  auto bytes = armar_guardados(guardados);
  REQUIRE(bytes.size() > 18);
  bytes[18] = 33;
  bytes.insert(bytes.end(), ultimo.begin() + 15, ultimo.end());
  CHECK(leer_sin_tocar(bytes) == LecturaGuardados::danados);
  bytes[18] = 32;
  bytes.resize(bytes.size() - (ultimo.size() - 15));
  Guardados leidos;
  CHECK(leer_guardados(bytes, leidos) == LecturaGuardados::leidos);
}

TEST_CASE("una lista de la versión 1 se lee con sus servidores y sin lo leído") {
  ventrik::protocolo::Escritor escritor;
  escritor.escribir_u32(0x52504B56);
  escritor.escribir_u16(1);
  escritor.escribir_u8(0xA4);
  escritor.escribir_texto("Tano", 64);
  escritor.escribir_u8(1);
  const ServidorGuardado viejo = servidor(0xC0A80099);
  escritor.escribir_u32(viejo.direccion.ipv4);
  escritor.escribir_u16(viejo.direccion.puerto);
  escritor.escribir_bytes(viejo.huella);
  escritor.escribir_texto(viejo.nombre, 64);
  escritor.escribir_u32(viejo.texto);
  escritor.escribir_texto(viejo.nombre_canal, 64);
  escritor.escribir_u64(viejo.ultima_vez_ms);
  Guardados leidos;
  REQUIRE(leer_guardados(escritor.datos(), leidos) == LecturaGuardados::leidos);
  REQUIRE(leidos.servidores.size() == 1);
  CHECK(leidos.servidores[0].nombre == "Tano");
  CHECK(leidos.servidores[0].leidos.empty());
  const auto nuevo = armar_guardados(leidos);
  REQUIRE(nuevo.size() == escritor.datos().size() + 6);
  CHECK(nuevo[4] == 4);
  auto con_leidos = escritor.datos();
  con_leidos.push_back(0);
  CHECK(leer_sin_tocar(con_leidos) == LecturaGuardados::danados);
}

TEST_CASE("lo leído se guarda por servidor, con canal distinto de 0, en orden y hasta 64") {
  const auto bien = armar_guardados(ejemplo());
  Guardados leidos;
  REQUIRE(leer_guardados(bien, leidos) == LecturaGuardados::leidos);
  REQUIRE(leidos.servidores[0].leidos.size() == 3);
  CHECK(leidos.servidores[0].leidos[2].numero == 4000000000u);
  CHECK(leidos.servidores[1].leidos.empty());
  const std::vector<std::uint8_t> patron = {3, 1, 0, 0, 0};
  const auto primero = std::search(bien.begin(), bien.end(), patron.begin(), patron.end());
  REQUIRE(primero != bien.end());
  auto en_cero = bien;
  en_cero[static_cast<std::size_t>(primero - bien.begin()) + 1] = 0;
  CHECK(leer_sin_tocar(en_cero) == LecturaGuardados::danados);
}

TEST_CASE("se aceptan 64 leídos por servidor y la lista más grande entra en el archivo") {
  std::string largo;
  for (int i = 0; i < 32; ++i) {
    largo += "ñ";
  }
  Guardados grande;
  grande.nombre = largo;
  for (std::uint32_t ip = 1; ip <= 32; ++ip) {
    auto uno = servidor(ip, 7700, largo);
    uno.nombre_canal = largo;
    for (std::uint32_t canal = 1; canal <= ventrik::cliente::kMaximoLeidosPorServidor; ++canal) {
      uno.leidos.push_back({canal, canal * 1000});
    }
    grande.servidores.push_back(uno);
  }
  const auto bytes = armar_guardados(grande);
  REQUIRE_FALSE(bytes.empty());
  CHECK(bytes.size() <= ventrik::cliente::kMaximoArchivoGuardados);
  Guardados leidos;
  REQUIRE(leer_guardados(bytes, leidos) == LecturaGuardados::leidos);
  CHECK(armar_guardados(leidos) == bytes);
}

TEST_CASE("recordar conserva lo leído del mismo servidor, salvo que cambie la huella") {
  Guardados guardados = ejemplo();
  ventrik::cliente::recordar(guardados, servidor(0xC0A80099, 7700, "Otro"));
  REQUIRE(guardados.servidores.front().nombre == "Otro");
  CHECK(guardados.servidores.front().leidos.size() == 3);
  auto con_leidos = servidor(0xC0A80099);
  con_leidos.leidos = {{2, 5}};
  ventrik::cliente::recordar(guardados, con_leidos);
  REQUIRE(guardados.servidores.front().leidos.size() == 1);
  CHECK(guardados.servidores.front().leidos[0].canal == 2);
  auto reinstalado = servidor(0xC0A80099);
  reinstalado.huella.fill(0xCD);
  ventrik::cliente::recordar(guardados, reinstalado);
  CHECK(guardados.servidores.front().leidos.empty());
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
