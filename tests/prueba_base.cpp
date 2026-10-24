#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <cstdlib>

#include <windows.h>

#include <gsl/gsl>
#include <sqlite3.h>

#include "common/archivos.h"
#include "common/red/red.h"
#include "common/utf.h"
#include "server/base.h"
#include <doctest/doctest.h>

using namespace ventrik::servidor;
using ventrik::protocolo::TipoCanal;

namespace {
constexpr std::uint64_t kDia = 24ull * 60 * 60 * 1000;

std::unique_ptr<Base> base_en_memoria() {
  auto base = Base::abrir(L"", "General");
  REQUIRE(base);
  return base;
}

void armar_base(const std::wstring& archivo, const char* sql) {
  sqlite3* base = nullptr;
  REQUIRE(sqlite3_open_v2(ventrik::a_utf8(archivo).c_str(), &base,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
  CHECK(sqlite3_exec(base, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(base);
}

int leer_numero(const std::wstring& archivo, const char* sql) {
  sqlite3* base = nullptr;
  int valor = -1;
  if (sqlite3_open_v2(ventrik::a_utf8(archivo).c_str(), &base, SQLITE_OPEN_READONLY, nullptr) ==
      SQLITE_OK) {
    sqlite3_exec(
        base, sql,
        [](void* destino, int, char** valores, char**) {
          *static_cast<int*>(destino) = std::atoi(valores[0]);
          return 0;
        },
        &valor, nullptr);
  }
  sqlite3_close(base);
  return valor;
}

std::wstring archivo_temporal() {
  wchar_t carpeta[MAX_PATH] = {};
  REQUIRE(GetTempPathW(MAX_PATH, carpeta) > 0);
  return std::wstring(carpeta) + L"ventrik_prueba_" + std::to_wstring(GetCurrentProcessId()) +
         L"_ñ.db";
}
}

TEST_CASE("La base arranca con General y administra canales") {
  auto base = base_en_memoria();
  REQUIRE(base->canales().size() == 1);
  CHECK(base->canales()[0].id == kCanalGeneral);
  CHECK(base->canales()[0].nombre == "General");
  const auto juegos = base->crear_canal("Juegos");
  REQUIRE(juegos);
  CHECK_FALSE(base->crear_canal("JUEGOS"));
  CHECK_FALSE(base->crear_canal(" Juegos"));
  CHECK(base->cambiar_canal(*juegos, "Música", false));
  CHECK_FALSE(base->cambiar_canal(*juegos, "general", false));
  CHECK_FALSE(base->cambiar_canal(99, "Otro", false));
  CHECK(base->cambiar_canal(kCanalGeneral, "Charla", false));
  const auto canales = base->canales();
  REQUIRE(canales.size() == 2);
  CHECK(canales[0].nombre == "Charla");
  CHECK(canales[1].nombre == "Música");
  CHECK_FALSE(base->borrar_canal(kCanalGeneral));
  REQUIRE(base->guardar_mensaje(*juegos, 1, "Ana", 1, "hola"));
  CHECK(base->borrar_canal(*juegos));
  CHECK_FALSE(base->borrar_canal(*juegos));
  CHECK(base->historial(*juegos, 0, 65536).mensajes.empty());
  CHECK(base->canales().size() == 1);
}

TEST_CASE("Los canales de texto guardan su marca y los de voz no la aceptan") {
  auto base = base_en_memoria();
  const auto anuncios = base->crear_canal("Anuncios", TipoCanal::texto, true);
  REQUIRE(anuncios);
  CHECK_FALSE(base->crear_canal("Otro", TipoCanal::voz, true));
  CHECK_FALSE(base->crear_canal("Otro", static_cast<TipoCanal>(3), false));
  CHECK_FALSE(base->crear_canal("general", TipoCanal::texto, false));
  CHECK_FALSE(base->cambiar_canal(kCanalGeneral, "General", true));
  CHECK(base->cambiar_canal(*anuncios, "Novedades", false));
  const auto canales = base->canales();
  REQUIRE(canales.size() == 2);
  CHECK(canales[0].tipo == TipoCanal::voz);
  CHECK_FALSE(canales[0].solo_admins);
  CHECK(canales[1].tipo == TipoCanal::texto);
  CHECK(canales[1].nombre == "Novedades");
  CHECK_FALSE(canales[1].solo_admins);
  REQUIRE(base->cambiar_canal(*anuncios, "Novedades", true));
  CHECK(base->canales()[1].solo_admins);
}

TEST_CASE("El contador de cada canal solo sube, aunque no se guarden los mensajes") {
  auto base = base_en_memoria();
  REQUIRE(base->guardar_mensaje(kCanalGeneral, 1, "Ana", 1, "hola"));
  CHECK_FALSE(base->guardar_mensaje(kCanalGeneral, 2, "Ana", 2, "sin guardar", false));
  CHECK_FALSE(base->guardar_mensaje(kCanalGeneral, 0, "Ana", 3, "sin número"));
  REQUIRE(base->guardar_mensaje(kCanalGeneral, 1, "Ana", 3, "atrasado"));
  CHECK(base->canales()[0].contador == 2);
  const auto historial = base->historial(kCanalGeneral, 0, 65536);
  REQUIRE(historial.mensajes.size() == 2);
  CHECK(historial.mensajes[0].texto == "atrasado");
  CHECK(historial.mensajes[1].numero == 1);
}

TEST_CASE("Un canal borrado no deja su número ni recibe mensajes") {
  auto base = base_en_memoria();
  const auto juegos = base->crear_canal("Juegos");
  REQUIRE(juegos);
  REQUIRE(base->borrar_canal(*juegos));
  CHECK_FALSE(base->guardar_mensaje(*juegos, 1, "Ana", 1, "secreto"));
  const auto nuevo = base->crear_canal("Nuevo");
  REQUIRE(nuevo);
  CHECK(*nuevo != *juegos);
  CHECK(base->historial(*juegos, 0, 65536).mensajes.empty());
  CHECK_FALSE(base->guardar_mensaje(99, 1, "Ana", 1, "hola"));
}

TEST_CASE("El historial sale de a partes, del más nuevo al más viejo") {
  auto base = base_en_memoria();
  for (int i = 1; i <= 300; ++i) {
    REQUIRE(base->guardar_mensaje(kCanalGeneral, static_cast<std::uint32_t>(i), "Ana",
                                  static_cast<std::uint64_t>(i),
                                  "mensaje " + std::to_string(i)));
  }
  const auto primera = base->historial(kCanalGeneral, 0, 65536);
  REQUIRE(primera.mensajes.size() == ventrik::protocolo::kMaximoMensajesEnHistorial);
  CHECK(primera.hay_mas);
  CHECK(primera.mensajes.front().texto == "mensaje 300");
  const auto segunda = base->historial(kCanalGeneral, primera.mensajes.back().id, 65536);
  REQUIRE(segunda.mensajes.size() == 44);
  CHECK_FALSE(segunda.hay_mas);
  CHECK(segunda.mensajes.back().texto == "mensaje 1");
  const auto chica = base->historial(kCanalGeneral, 0, 100);
  CHECK(chica.mensajes.size() == 2);
  CHECK(chica.hay_mas);
  CHECK(base->historial(2, 0, 65536).mensajes.empty());
}

TEST_CASE("La retención borra lo viejo y por defecto se guarda siempre") {
  auto base = base_en_memoria();
  const std::uint64_t ahora = 100 * kDia;
  REQUIRE(base->guardar_mensaje(kCanalGeneral, 1, "Ana", ahora - 40 * kDia, "muy viejo"));
  REQUIRE(base->guardar_mensaje(kCanalGeneral, 1, "Ana", ahora - 10 * kDia, "viejo"));
  REQUIRE(base->guardar_mensaje(kCanalGeneral, 1, "Ana", ahora - 1000, "nuevo"));
  const auto cuantos = [&] { return base->historial(kCanalGeneral, 0, 65536).mensajes.size(); };
  CHECK(base->retencion() == Retencion::siempre);
  base->borrar_viejos(ahora);
  CHECK(cuantos() == 3);
  REQUIRE(base->cambiar_retencion(Retencion::dias_30));
  base->borrar_viejos(ahora);
  CHECK(cuantos() == 2);
  REQUIRE(base->cambiar_retencion(Retencion::dias_7));
  base->borrar_viejos(ahora);
  CHECK(cuantos() == 1);
  REQUIRE(base->cambiar_retencion(Retencion::no_guardar));
  CHECK(base->retencion() == Retencion::no_guardar);
  base->borrar_viejos(ahora);
  CHECK(cuantos() == 0);
}

TEST_CASE("Comillas y SQL en los textos se guardan tal cual") {
  auto base = base_en_memoria();
  const std::string canal = "x'); DROP TABLE canales;--";
  const std::string texto = "'; DROP TABLE mensajes; -- \"%_\\";
  const auto id = base->crear_canal(canal);
  REQUIRE(id);
  REQUIRE(base->guardar_mensaje(*id, 1, "O'Brien", 1, texto));
  const auto historial = base->historial(*id, 0, 65536);
  REQUIRE(historial.mensajes.size() == 1);
  CHECK(historial.mensajes[0].texto == texto);
  CHECK(historial.mensajes[0].autor == "O'Brien");
  CHECK(base->canales().size() == 2);
  CHECK(base->canales()[1].nombre == canal);
}

TEST_CASE("La base no se deja usar desde otro hilo") {
  auto base = base_en_memoria();
  std::thread otro([&base] {
    CHECK(base->canales().empty());
    CHECK_FALSE(base->crear_canal("Juegos"));
    CHECK_FALSE(base->guardar_mensaje(kCanalGeneral, 1, "Ana", 1, "hola"));
    CHECK_FALSE(base->cambiar_retencion(Retencion::no_guardar));
  });
  otro.join();
  CHECK(base->canales().size() == 1);
  CHECK(base->retencion() == Retencion::siempre);
}

TEST_CASE("La base guarda en el archivo y se vuelve a abrir igual") {
  const std::wstring archivo = archivo_temporal();
  DeleteFileW(archivo.c_str());
  {
    auto base = Base::abrir(archivo, "General");
    REQUIRE(base);
    REQUIRE(base->crear_canal("Juegos"));
    REQUIRE(base->guardar_mensaje(kCanalGeneral, 1, "Ana", 1, "hola"));
  }
  {
    auto base = Base::abrir(archivo, "Otro nombre");
    REQUIRE(base);
    const auto canales = base->canales();
    REQUIRE(canales.size() == 2);
    CHECK(canales[0].nombre == "General");
    CHECK(base->historial(kCanalGeneral, 0, 65536).mensajes.size() == 1);
  }
  CHECK(DeleteFileW(archivo.c_str()));
  CHECK_FALSE(Base::abrir(L"", " mal"));
  CHECK_FALSE(Base::abrir(L"", ""));
}

TEST_CASE("Una base de la versión 1 se actualiza: canales de voz y mensajes numerados") {
  const std::wstring archivo = archivo_temporal();
  DeleteFileW(archivo.c_str());
  armar_base(archivo,
                       "CREATE TABLE canales(id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       " nombre TEXT NOT NULL, comparable TEXT NOT NULL UNIQUE, orden INTEGER NOT NULL);"
                       "CREATE TABLE mensajes(id INTEGER PRIMARY KEY AUTOINCREMENT, canal INTEGER NOT NULL,"
                       " autor TEXT NOT NULL, hora INTEGER NOT NULL, texto TEXT NOT NULL);"
                       "CREATE TABLE config(clave TEXT PRIMARY KEY, valor INTEGER NOT NULL);"
                       "INSERT INTO canales VALUES (1, 'General', 'GENERAL', 0), (2, 'Juegos', 'JUEGOS', 1),"
                       " (3, 'Charla', 'CHARLA', 2);"
                       "INSERT INTO mensajes(canal, autor, hora, texto) VALUES (1, 'Ana', 1, 'a'),"
                       " (2, 'Ana', 2, 'b'), (1, 'Ana', 3, 'c');"
                       "PRAGMA user_version=1;");
  {
    auto base = Base::abrir(archivo, "General");
    REQUIRE(base);
    const auto canales = base->canales();
    REQUIRE(canales.size() == 3);
    CHECK(canales[0].contador == 2);
    CHECK(canales[1].contador == 1);
    CHECK(canales[2].contador == 0);
    for (const auto& canal : canales) {
      CHECK(canal.tipo == TipoCanal::voz);
      CHECK_FALSE(canal.solo_admins);
    }
    const auto general = base->historial(kCanalGeneral, 0, 65536);
    REQUIRE(general.mensajes.size() == 2);
    CHECK(general.mensajes[0].numero == 2);
    CHECK(general.mensajes[1].numero == 1);
    const auto juegos = base->historial(2, 0, 65536);
    REQUIRE(juegos.mensajes.size() == 1);
    CHECK(juegos.mensajes[0].numero == 1);
    REQUIRE(base->guardar_mensaje(kCanalGeneral, 3, "Ana", 4, "d"));
  }
  {
    auto base = Base::abrir(archivo, "General");
    REQUIRE(base);
    CHECK(base->canales()[0].contador == 3);
    CHECK(base->historial(kCanalGeneral, 0, 65536).mensajes.size() == 3);
  }
  CHECK(DeleteFileW(archivo.c_str()));
}

TEST_CASE("Si la actualización de la versión 1 falla, la base queda como estaba") {
  const std::wstring archivo = archivo_temporal();
  DeleteFileW(archivo.c_str());
  armar_base(archivo,
             "CREATE TABLE canales(id INTEGER PRIMARY KEY AUTOINCREMENT, nombre TEXT NOT NULL,"
             " comparable TEXT NOT NULL UNIQUE, orden INTEGER NOT NULL, contador INTEGER);"
             "PRAGMA user_version=1;");
  CHECK_FALSE(Base::abrir(archivo, "General"));
  CHECK(leer_numero(archivo, "PRAGMA user_version") == 1);
  CHECK(leer_numero(archivo, "SELECT COUNT(*) FROM pragma_table_info('canales')") == 5);
  CHECK(DeleteFileW(archivo.c_str()));
}

TEST_CASE("Una base de una versión futura o que no es una base no se abre") {
  const std::wstring archivo = archivo_temporal();
  const std::string basura = "esto no es una base de SQLite, aunque se llame .db";
  REQUIRE(ventrik::escribir_archivo_completo(
      archivo, gsl::make_span(reinterpret_cast<const std::uint8_t*>(basura.data()), basura.size())));
  CHECK_FALSE(Base::abrir(archivo, "General"));
  REQUIRE(DeleteFileW(archivo.c_str()));
  armar_base(archivo, "PRAGMA user_version=3");
  CHECK_FALSE(Base::abrir(archivo, "General"));
  CHECK(leer_numero(archivo, "SELECT COUNT(*) FROM sqlite_master") == 0);
  CHECK(DeleteFileW(archivo.c_str()));
}

TEST_CASE("Al cerrarse, el servicio de la base termina los pedidos que quedaron en la cola") {
  ventrik::red::Inicio red;
  REQUIRE(red.ok());
  ventrik::red::Despertador despertador;
  REQUIRE(despertador.ok());
  const std::wstring archivo = archivo_temporal();
  DeleteFileW(archivo.c_str());
  {
    ServicioBase servicio;
    REQUIRE(servicio.arrancar(archivo, "General", despertador));
    for (std::uint32_t numero = 1; numero <= 100; ++numero) {
      REQUIRE(servicio.pedir([numero](Base& base) -> std::function<void()> {
        base.guardar_mensaje(kCanalGeneral, numero, "Ana", 1, "hola", false);
        return {};
      }));
    }
  }
  CHECK(leer_numero(archivo, "SELECT contador FROM canales WHERE id = 1") == 100);
  CHECK(DeleteFileW(archivo.c_str()));
}

TEST_CASE("El servicio de la base atiende pedidos en su hilo y despierta al de red") {
  ventrik::red::Inicio red;
  REQUIRE(red.ok());
  ventrik::red::Despertador despertador;
  REQUIRE(despertador.ok());
  ServicioBase servicio;
  REQUIRE(servicio.arrancar(L"", "General", despertador));
  std::atomic<bool> respondio{false};
  std::size_t canales = 0;
  REQUIRE(servicio.pedir([&](Base& base) -> std::function<void()> {
    canales = base.canales().size();
    return [&respondio] { respondio = true; };
  }));
  CHECK(ventrik::red::esperar(despertador.socket(), true, false, std::chrono::seconds(5)) ==
        ventrik::red::Espera::lista);
  despertador.vaciar();
  servicio.atender_respuestas();
  CHECK(respondio.load());
  CHECK(canales == 1);
}
