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
  version[4] = 8;
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
  REQUIRE(nuevo.size() == escritor.datos().size() + 5);
  CHECK(nuevo[4] == 7);
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
  REQUIRE(nuevo.size() == escritor.datos().size() + 7);
  CHECK(nuevo[4] == 7);
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
  sin_armar([](Guardados& g) { g.servidores[0].personas = {{"Sofi", 201, false}}; });
  sin_armar([](Guardados& g) { g.servidores[0].personas = {{" Sofi", 50, false}}; });
  sin_armar([](Guardados& g) { g.servidores[0].personas = {{"Sofi", 50, false}, {"Sofi", 0, true}}; });
  sin_armar([](Guardados& g) {
    for (int i = 0; i < 17; ++i) {
      g.servidores[0].personas.push_back({"P" + std::to_string(i), 50, false});
    }
  });
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
  REQUIRE(nuevo.size() == escritor.datos().size() + 22);
  CHECK(nuevo[4] == 7);
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
    for (std::size_t i = 0; i < ventrik::cliente::kMaximoPersonasPorServidor; ++i) {
      uno.personas.push_back({largo.substr(0, 60) + std::to_string(10 + i), 200, true});
    }
    for (std::uint32_t canal = 1; canal <= ventrik::protocolo::kMaximoCanales; ++canal) {
      ventrik::cliente::cambiar_canal(uno.avisos, canal, {ventrik::cliente::kHastaQueLoActive, false,
                                                          ventrik::cliente::Avisar::nada});
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

TEST_CASE("el volumen de cada persona se guarda por servidor y por nombre, hasta 16") {
  using ventrik::cliente::PersonaGuardada;
  Guardados guardados = ejemplo();
  guardados.servidores[1].personas = {{"Sofi", 0, true}, {"Ñandú", 150, false}};
  const auto bytes = armar_guardados(guardados);
  REQUIRE_FALSE(bytes.empty());
  Guardados leidos;
  REQUIRE(leer_guardados(bytes, leidos) == LecturaGuardados::leidos);
  REQUIRE(leidos.servidores[1].personas.size() == 2);
  CHECK(leidos.servidores[1].personas[0].silenciada);
  CHECK(leidos.servidores[1].personas[1].nombre == "Ñandú");
  CHECK(leidos.servidores[1].personas[1].volumen == 150);
  CHECK(armar_guardados(leidos) == bytes);
  auto silenciada_mal = bytes;
  silenciada_mal[silenciada_mal.size() - 16] = 2;
  CHECK(leer_sin_tocar(silenciada_mal) == LecturaGuardados::danados);

  std::vector<PersonaGuardada> personas;
  ventrik::cliente::ajustar_persona(personas, {"Sofi", 50, false});
  ventrik::cliente::ajustar_persona(personas, {"Beto", 100, true});
  ventrik::cliente::ajustar_persona(personas, {" mal", 50, false});
  ventrik::cliente::ajustar_persona(personas, {"Ana", 201, false});
  REQUIRE(personas.size() == 2);
  CHECK(personas[0].nombre == "Beto");
  ventrik::cliente::ajustar_persona(personas, {"Sofi", 70, false});
  CHECK(personas[0].nombre == "Sofi");
  CHECK(personas[0].volumen == 70);
  ventrik::cliente::ajustar_persona(personas, {"Beto", 100, false});
  REQUIRE(personas.size() == 1);
  for (int i = 0; i < 20; ++i) {
    ventrik::cliente::ajustar_persona(personas, {"P" + std::to_string(i), 10, false});
  }
  CHECK(personas.size() == ventrik::cliente::kMaximoPersonasPorServidor);
  CHECK(personas[0].nombre == "P19");

  const std::map<std::uint32_t, ventrik::protocolo::UsuarioInfo> usuarios = {
      {3, {3, "Sofi", 1, 0}}, {7, {7, "Beto", 1, 0}}, {9, {9, "Ana", 0, 0}}};
  const auto ajustes = ventrik::cliente::ajustes_de_voz({{"Ana", 0, true}, {"Sofi", 30, false}, {"Nadie", 0, true}},
                                                        usuarios);
  CHECK(ajustes.volumenes == std::vector<std::pair<std::uint32_t, std::uint8_t>>{{3, 30}, {9, 0}});
  CHECK(ajustes.silenciados == std::vector<std::uint32_t>{9});
}

TEST_CASE("los avisos de cada servidor se guardan con sus canales, y lo fuera de rango no se lee") {
  using ventrik::cliente::Avisar;
  Guardados guardados = ejemplo();
  auto& avisos = guardados.servidores[1].avisos;
  avisos.servidor = {1761234567890, false, Avisar::todos};
  avisos.contar_todos = true;
  avisos.sonidos_de_voz = false;
  ventrik::cliente::cambiar_canal(avisos, 9, {0, true, Avisar::nada});
  ventrik::cliente::cambiar_canal(avisos, 3, {ventrik::cliente::kHastaQueLoActive, true, Avisar::como_el_servidor});
  const auto bytes = armar_guardados(guardados);
  REQUIRE_FALSE(bytes.empty());
  Guardados leidos;
  REQUIRE(leer_guardados(bytes, leidos) == LecturaGuardados::leidos);
  const auto& vuelta = leidos.servidores[1].avisos;
  CHECK(vuelta.servidor.silenciado_hasta == 1761234567890);
  CHECK_FALSE(vuelta.servidor.aun_con_mencion);
  CHECK(vuelta.servidor.avisar == Avisar::todos);
  CHECK(vuelta.contar_todos);
  CHECK_FALSE(vuelta.sonidos_de_voz);
  REQUIRE(vuelta.canales.size() == 2);
  CHECK(vuelta.canales[0].canal == 3);
  CHECK(vuelta.canales[1].ajuste.avisar == Avisar::nada);
  CHECK(leidos.servidores[0].avisos.servidor.avisar == Avisar::menciones);
  CHECK(armar_guardados(leidos) == bytes);

  const auto sin_armar = [](auto cambiar) {
    Guardados malos = ejemplo();
    cambiar(malos.servidores[0].avisos);
    CHECK(armar_guardados(malos).empty());
  };
  sin_armar([](auto& a) { a.servidor.avisar = Avisar::como_el_servidor; });
  sin_armar([](auto& a) { a.servidor.avisar = static_cast<Avisar>(4); });
  sin_armar([](auto& a) { a.canales = {{5, {}}, {4, {}}}; });
  sin_armar([](auto& a) { a.canales = {{0, {}}}; });
  sin_armar([](auto& a) { a.canales.assign(ventrik::protocolo::kMaximoCanales + 1, {}); });

  const std::size_t fin = bytes.size();
  auto avisar_mal = bytes;
  avisar_mal[fin - 13 - 2 * 14 + 9] = 4;
  CHECK(leer_sin_tocar(avisar_mal) == LecturaGuardados::danados);
  auto aun_mal = bytes;
  aun_mal[fin - 13 - 2 * 14 + 8] = 2;
  CHECK(leer_sin_tocar(aun_mal) == LecturaGuardados::danados);
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

TEST_CASE("los ajustes van por clave: se guardan en orden, se conservan los que no se conocen y se validan los conocidos") {
  Guardados guardados = ejemplo();
  ventrik::cliente::poner_ajuste(guardados, ventrik::cliente::Ajuste::eco, 2);
  ventrik::cliente::poner_ajuste(guardados, ventrik::cliente::Ajuste::ruido, 2);
  guardados.ajustes[900] = {1, 2, 3};
  const auto bytes = armar_guardados(guardados);
  REQUIRE_FALSE(bytes.empty());
  CHECK(bytes[4] == 7);
  Guardados leidos;
  REQUIRE(leer_guardados(bytes, leidos) == LecturaGuardados::leidos);
  CHECK(leidos.ajustes == guardados.ajustes);
  CHECK(armar_guardados(leidos) == bytes);
  CHECK(ventrik::cliente::ajuste_u8(leidos, ventrik::cliente::Ajuste::ruido) == 2);
  CHECK_FALSE(ventrik::cliente::ajuste_u8(leidos, ventrik::cliente::Ajuste::bajar_otros));

  Guardados fuera = guardados;
  fuera.ajustes[static_cast<std::uint16_t>(ventrik::cliente::Ajuste::eco)] = {3};
  CHECK(armar_guardados(fuera).empty());
  fuera = guardados;
  fuera.ajustes[static_cast<std::uint16_t>(ventrik::cliente::Ajuste::volumen_automatico)] = {1, 0};
  CHECK(armar_guardados(fuera).empty());
  fuera = guardados;
  fuera.ajustes[901] = std::vector<std::uint8_t>(ventrik::cliente::kMaximoLargoDeAjuste + 1);
  CHECK(armar_guardados(fuera).empty());
  fuera = guardados;
  fuera.ajustes[901] = std::vector<std::uint8_t>(ventrik::cliente::kMaximoLargoDeAjuste);
  CHECK_FALSE(armar_guardados(fuera).empty());
  for (std::uint16_t clave = 902; clave < 910; ++clave) {
    fuera.ajustes[clave] = std::vector<std::uint8_t>(ventrik::cliente::kMaximoLargoDeAjuste);
  }
  CHECK(armar_guardados(fuera).empty());

  auto desordenado = bytes;
  const std::size_t bloque = bytes.size() - (2 + 3 * 4 + 1 + 1 + 3);
  REQUIRE(desordenado[bloque] == 3);
  std::swap(desordenado[bloque + 2], desordenado[bloque + 2 + 5]);
  CHECK(leer_sin_tocar(desordenado) == LecturaGuardados::danados);
  auto demasiados = bytes;
  demasiados[bloque] = static_cast<std::uint8_t>(ventrik::cliente::kMaximoAjustes + 1);
  CHECK(leer_sin_tocar(demasiados) == LecturaGuardados::danados);
  auto cortado = bytes;
  cortado.pop_back();
  CHECK(leer_sin_tocar(cortado) == LecturaGuardados::danados);
}

TEST_CASE("la limpieza de la voz viene prendida en las PCs comunes y apagada en las de un núcleo, hasta que se cambia") {
  using ventrik::cliente::Bajar;
  using ventrik::cliente::Eco;
  using ventrik::cliente::Ruido;
  Guardados guardados;
  const auto comun = ventrik::cliente::limpieza_de(guardados, false);
  CHECK(comun.ruido == Ruido::media);
  CHECK(comun.eco == Eco::automatico);
  CHECK(comun.volumen);
  CHECK(comun.bajar == Bajar::nada);
  const auto debil = ventrik::cliente::limpieza_de(guardados, true);
  CHECK(debil.ruido == Ruido::apagado);
  CHECK(debil.eco == Eco::apagado);
  ventrik::cliente::poner_ajuste(guardados, ventrik::cliente::Ajuste::ruido, static_cast<std::uint8_t>(Ruido::alta));
  CHECK(ventrik::cliente::limpieza_de(guardados, true).ruido == Ruido::alta);
}

TEST_CASE("los susurros se guardan por nombre con su tecla, se validan y se traducen a los ids del servidor") {
  using ventrik::cliente::Susurro;
  using ventrik::cliente::poner_susurro;
  using ventrik::cliente::susurro_de;
  Guardados guardados = ejemplo();
  CHECK_FALSE(susurro_de(guardados, 0));
  poner_susurro(guardados, 1, Susurro{0x05, "", {"Sofi", "Meli"}});
  poner_susurro(guardados, 3, Susurro{0, "Partida 2", {}});
  const auto bytes = armar_guardados(guardados);
  REQUIRE_FALSE(bytes.empty());
  Guardados leidos;
  REQUIRE(leer_guardados(bytes, leidos) == LecturaGuardados::leidos);
  REQUIRE(susurro_de(leidos, 1));
  CHECK(susurro_de(leidos, 1)->tecla == 0x05);
  CHECK(susurro_de(leidos, 1)->personas == std::vector<std::string>{"Sofi", "Meli"});
  CHECK(susurro_de(leidos, 3)->canal == "Partida 2");
  CHECK_FALSE(susurro_de(leidos, 0));
  CHECK_FALSE(susurro_de(leidos, 4));
  poner_susurro(leidos, 1, std::nullopt);
  CHECK_FALSE(susurro_de(leidos, 1));

  poner_susurro(guardados, 0, Susurro{0x1B, "", {}});
  CHECK_FALSE(susurro_de(guardados, 0));
  poner_susurro(guardados, 0, Susurro{0, "", {"a", "b", "c", "d", "e", "f", "g", "h", "i"}});
  CHECK_FALSE(susurro_de(guardados, 0));
  poner_susurro(guardados, 0, Susurro{0, "", {""}});
  CHECK_FALSE(susurro_de(guardados, 0));
  Guardados mal = guardados;
  mal.ajustes[20] = {0, 0, 0, 9};
  CHECK(armar_guardados(mal).empty());
  mal = guardados;
  mal.ajustes[23] = {0, 0, 0, 0, 1};
  CHECK(armar_guardados(mal).empty());

  const std::vector<ventrik::protocolo::CanalInfo> canales = {
      {4, "Partida 2", ventrik::protocolo::TipoCanal::voz}, {5, "Charla", ventrik::protocolo::TipoCanal::texto}};
  const std::map<std::uint32_t, ventrik::protocolo::UsuarioInfo> usuarios = {
      {3, {3, "Meli", 1}}, {7, {7, "Sofi", 4}}, {9, {9, "Yo", 1}}};
  const auto a_personas = ventrik::cliente::destinos_de(Susurro{0, "", {"Sofi", "Meli", "Nadie"}}, 1, 9, canales, usuarios);
  CHECK(a_personas.lugar == 2);
  CHECK(a_personas.canal == 0);
  CHECK(a_personas.personas == std::vector<std::uint32_t>{3, 7});
  CHECK(ventrik::cliente::destinos_de(Susurro{0, "Charla", {}}, 0, 9, canales, usuarios).canal == 0);
  const auto al_canal = ventrik::cliente::destinos_de(Susurro{0, "Partida 2", {"Yo"}}, 3, 9, canales, usuarios);
  CHECK(al_canal.lugar == 4);
  CHECK(al_canal.canal == 4);
  CHECK(al_canal.personas.empty());
}

TEST_CASE("los ajustes de texto (idioma y dispositivos) se guardan como UTF-8 válido y con tope") {
  using ventrik::cliente::Ajuste;
  Guardados guardados = ejemplo();
  ventrik::cliente::poner_texto(guardados, Ajuste::idioma, "es");
  ventrik::cliente::poner_texto(guardados, Ajuste::microfono, "Micr\xC3\xB3" "fono USB");
  const auto bytes = armar_guardados(guardados);
  REQUIRE_FALSE(bytes.empty());
  Guardados leidos;
  REQUIRE(leer_guardados(bytes, leidos) == LecturaGuardados::leidos);
  CHECK(ventrik::cliente::ajuste_texto(leidos, Ajuste::idioma) == "es");
  CHECK(ventrik::cliente::ajuste_texto(leidos, Ajuste::microfono) == "Micr\xC3\xB3" "fono USB");
  CHECK(ventrik::cliente::ajuste_texto(leidos, Ajuste::salida).empty());
  ventrik::cliente::poner_texto(leidos, Ajuste::microfono, "");
  CHECK(leidos.ajustes.count(static_cast<std::uint16_t>(Ajuste::microfono)) == 0);

  Guardados mal = guardados;
  mal.ajustes[static_cast<std::uint16_t>(Ajuste::idioma)] = {'e', 's', 'p', 'a', 'n', 'o', 'l', '!', '!'};
  CHECK(armar_guardados(mal).empty());
  mal = guardados;
  mal.ajustes[static_cast<std::uint16_t>(Ajuste::salida)] = {0xC3};
  CHECK(armar_guardados(mal).empty());
  mal = guardados;
  mal.ajustes[static_cast<std::uint16_t>(Ajuste::salida)] = {};
  CHECK(armar_guardados(mal).empty());
  mal = guardados;
  mal.ajustes[static_cast<std::uint16_t>(Ajuste::seguir_al_soltar)] = {4};
  CHECK(armar_guardados(mal).empty());
}
