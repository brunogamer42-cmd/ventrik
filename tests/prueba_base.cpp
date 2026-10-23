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

namespace {
constexpr std::uint64_t kDia = 24ull * 60 * 60 * 1000;

std::unique_ptr<Base> base_en_memoria() {
  auto base = Base::abrir(L"", "General");
  REQUIRE(base);
  return base;
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
  CHECK(base->renombrar_canal(*juegos, "Música"));
  CHECK_FALSE(base->renombrar_canal(*juegos, "general"));
  CHECK_FALSE(base->renombrar_canal(99, "Otro"));
  CHECK(base->renombrar_canal(kCanalGeneral, "Charla"));
  const auto canales = base->canales();
  REQUIRE(canales.size() == 2);
  CHECK(canales[0].nombre == "Charla");
  CHECK(canales[1].nombre == "Música");
  CHECK_FALSE(base->borrar_canal(kCanalGeneral));
  REQUIRE(base->guardar_mensaje(*juegos, "Ana", 1, "hola"));
  CHECK(base->borrar_canal(*juegos));
  CHECK_FALSE(base->borrar_canal(*juegos));
  CHECK(base->historial(*juegos, 0, 65536).mensajes.empty());
  CHECK(base->canales().size() == 1);
}

TEST_CASE("Un canal borrado no deja su número ni recibe mensajes") {
  auto base = base_en_memoria();
  const auto juegos = base->crear_canal("Juegos");
  REQUIRE(juegos);
  REQUIRE(base->borrar_canal(*juegos));
  CHECK_FALSE(base->guardar_mensaje(*juegos, "Ana", 1, "secreto"));
  const auto nuevo = base->crear_canal("Nuevo");
  REQUIRE(nuevo);
  CHECK(*nuevo != *juegos);
  CHECK(base->historial(*juegos, 0, 65536).mensajes.empty());
  CHECK_FALSE(base->guardar_mensaje(99, "Ana", 1, "hola"));
}

TEST_CASE("El historial sale de a partes, del más nuevo al más viejo") {
  auto base = base_en_memoria();
  for (int i = 1; i <= 300; ++i) {
    REQUIRE(base->guardar_mensaje(kCanalGeneral, "Ana", static_cast<std::uint64_t>(i),
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
  REQUIRE(base->guardar_mensaje(kCanalGeneral, "Ana", ahora - 40 * kDia, "muy viejo"));
  REQUIRE(base->guardar_mensaje(kCanalGeneral, "Ana", ahora - 10 * kDia, "viejo"));
  REQUIRE(base->guardar_mensaje(kCanalGeneral, "Ana", ahora - 1000, "nuevo"));
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
  REQUIRE(base->guardar_mensaje(*id, "O'Brien", 1, texto));
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
    CHECK_FALSE(base->guardar_mensaje(kCanalGeneral, "Ana", 1, "hola"));
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
    REQUIRE(base->guardar_mensaje(kCanalGeneral, "Ana", 1, "hola"));
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

TEST_CASE("Una base de una versión futura o que no es una base no se abre") {
  const std::wstring archivo = archivo_temporal();
  const std::string basura = "esto no es una base de SQLite, aunque se llame .db";
  REQUIRE(ventrik::escribir_archivo_completo(
      archivo, gsl::make_span(reinterpret_cast<const std::uint8_t*>(basura.data()), basura.size())));
  CHECK_FALSE(Base::abrir(archivo, "General"));
  REQUIRE(DeleteFileW(archivo.c_str()));
  sqlite3* futura = nullptr;
  REQUIRE(sqlite3_open_v2(ventrik::a_utf8(archivo).c_str(), &futura,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
  REQUIRE(sqlite3_exec(futura, "PRAGMA user_version=2", nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(futura);
  CHECK_FALSE(Base::abrir(archivo, "General"));
  sqlite3* revisada = nullptr;
  REQUIRE(sqlite3_open_v2(ventrik::a_utf8(archivo).c_str(), &revisada, SQLITE_OPEN_READONLY,
                          nullptr) == SQLITE_OK);
  int tablas = -1;
  sqlite3_exec(
      revisada, "SELECT COUNT(*) FROM sqlite_master",
      [](void* cuantas, int, char** valores, char**) {
        *static_cast<int*>(cuantas) = std::atoi(valores[0]);
        return 0;
      },
      &tablas, nullptr);
  sqlite3_close(revisada);
  CHECK(tablas == 0);
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
