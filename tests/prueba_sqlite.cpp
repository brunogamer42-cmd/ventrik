#include <cstddef>
#include <string>

#include <doctest/doctest.h>
#include <sqlite3.h>

TEST_CASE("SQLite es la versión 3.54.1") {
  CHECK(std::string(sqlite3_libversion()) == "3.54.1");
}

TEST_CASE("SQLite no puede cargar código de afuera") {
  CHECK(sqlite3_compileoption_used("OMIT_LOAD_EXTENSION") == 1);
}

TEST_CASE("SQLite está compilado para usarse desde un solo hilo") {
  CHECK(sqlite3_threadsafe() == 0);
}

TEST_CASE("SQLite guarda y lee un texto con consultas preparadas") {
  sqlite3* base = nullptr;
  REQUIRE(sqlite3_open(":memory:", &base) == SQLITE_OK);
  CHECK(sqlite3_exec(base, "CREATE TABLE mensajes(texto TEXT NOT NULL)", nullptr,
                     nullptr, nullptr) == SQLITE_OK);

  // Un texto con acento y con un intento de inyección SQL: con la consulta
  // preparada se guarda tal cual, como texto.
  const std::string texto = "canci\xC3\xB3n'); DROP TABLE mensajes; --";
  sqlite3_stmt* insertar = nullptr;
  REQUIRE(sqlite3_prepare_v2(base, "INSERT INTO mensajes(texto) VALUES (?1)", -1,
                             &insertar, nullptr) == SQLITE_OK);
  CHECK(sqlite3_bind_text(insertar, 1, texto.data(), static_cast<int>(texto.size()),
                          SQLITE_STATIC) == SQLITE_OK);
  CHECK(sqlite3_step(insertar) == SQLITE_DONE);
  sqlite3_finalize(insertar);

  sqlite3_stmt* leer = nullptr;
  REQUIRE(sqlite3_prepare_v2(base, "SELECT texto FROM mensajes", -1, &leer, nullptr) ==
          SQLITE_OK);
  REQUIRE(sqlite3_step(leer) == SQLITE_ROW);
  const auto* leido = reinterpret_cast<const char*>(sqlite3_column_text(leer, 0));
  const auto largo = static_cast<std::size_t>(sqlite3_column_bytes(leer, 0));
  REQUIRE(leido != nullptr);
  CHECK(std::string(leido, largo) == texto);
  CHECK(sqlite3_step(leer) == SQLITE_DONE);
  sqlite3_finalize(leer);

  CHECK(sqlite3_close(base) == SQLITE_OK);
}
