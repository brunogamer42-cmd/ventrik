#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

#include <gsl/gsl>
#include <sqlite3.h>

#include "common/archivos.h"
#include "common/cifrado/sobre.h"
#include "common/protocol/bytes.h"
#include "common/utf.h"
#include "server/base.h"
#include "server/respaldo.h"
#include <doctest/doctest.h>

using namespace ventrik::servidor;
using ventrik::cifrado::AperturaSobre;

namespace {

constexpr ventrik::cifrado::Argon2 kRapido{ventrik::cifrado::kMinimoMemoriaArgon2, 1};
constexpr const char* kContrasena = "una contraseña larga";

using Bytes = std::vector<std::uint8_t>;

Bytes bytes(const std::string& texto) {
  return Bytes(texto.begin(), texto.end());
}

Bytes copia(gsl::span<const std::uint8_t> datos) {
  return Bytes(datos.begin(), datos.end());
}

Bytes contenido(std::uint8_t version, const std::vector<std::pair<int, Bytes>>& partes) {
  ventrik::protocolo::Escritor escritor;
  escritor.escribir_u8(version);
  for (const auto& parte : partes) {
    escritor.escribir_u8(static_cast<std::uint8_t>(parte.first));
    escritor.escribir_u32(static_cast<std::uint32_t>(parte.second.size()));
    escritor.escribir_bytes(parte.second);
  }
  return std::move(escritor).soltar();
}

std::wstring temporal(const std::wstring& nombre) {
  wchar_t carpeta[MAX_PATH] = {};
  REQUIRE(GetTempPathW(MAX_PATH, carpeta) > 0);
  return std::wstring(carpeta) + L"ventrik_respaldo_" + std::to_wstring(GetCurrentProcessId()) + L"_" + nombre +
         L"_ñ.db";
}

bool existe(const std::wstring& archivo) {
  return GetFileAttributesW(archivo.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void ejecutar(const std::wstring& archivo, const char* sql) {
  sqlite3* base = nullptr;
  REQUIRE(sqlite3_open_v2(ventrik::a_utf8(archivo).c_str(), &base, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                          nullptr) == SQLITE_OK);
  CHECK(sqlite3_exec(base, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(base);
}

std::wstring base_nueva(const std::wstring& nombre) {
  const std::wstring archivo = temporal(nombre);
  borrar_base(archivo);
  REQUIRE(Base::abrir(archivo, "General"));
  return archivo;
}

std::size_t mensajes(const std::wstring& archivo) {
  auto base = Base::abrir(archivo, "General");
  REQUIRE(base);
  return base->historial({kCanalGeneral, 0, ventrik::protocolo::Sentido::viejos, 100}, 60000).mensajes.size();
}

struct Ejemplo {
  Bytes clave = bytes("clave del servidor");
  Bytes boveda = bytes("bóveda de prueba");
  Bytes base = Bytes(5000, 0x5A);

  PartesRespaldo partes() const { return {clave, boveda, base}; }
};

}

TEST_CASE("Un respaldo del servidor vuelve igual con su contraseña y no se abre con otra") {
  const Ejemplo ejemplo;
  const auto archivo = armar_respaldo(ejemplo.partes(), kContrasena, kRapido);
  REQUIRE(archivo);
  CHECK(archivo->size() <= kMaximoArchivoRespaldo);
  CHECK(std::search(archivo->begin(), archivo->end(), ejemplo.clave.begin(), ejemplo.clave.end()) == archivo->end());
  const auto abierto = abrir_respaldo(*archivo, kContrasena);
  REQUIRE(abierto.resultado == AperturaSobre::abierto);
  REQUIRE(abierto.contenido);
  CHECK(copia(abierto.partes.clave) == ejemplo.clave);
  CHECK(copia(abierto.partes.boveda) == ejemplo.boveda);
  CHECK(copia(abierto.partes.base) == ejemplo.base);
  const auto equivocada = abrir_respaldo(*archivo, "una contraseña larg");
  CHECK(equivocada.resultado == AperturaSobre::contrasena);
  CHECK_FALSE(equivocada.contenido);
  CHECK(equivocada.partes.clave.empty());
  CHECK(equivocada.partes.base.empty());
}

TEST_CASE("Un respaldo cortado, con un byte cambiado o con uno de más no se abre") {
  const auto archivo = armar_respaldo(Ejemplo().partes(), kContrasena, kRapido);
  REQUIRE(archivo);
  for (const std::size_t lugar : {std::size_t{0}, std::size_t{3}, std::size_t{4}, std::size_t{5}, std::size_t{9},
                                  std::size_t{10}, std::size_t{25}, std::size_t{26}, std::size_t{38}, std::size_t{60},
                                  archivo->size() / 2, archivo->size() - 1}) {
    auto alterado = *archivo;
    alterado[lugar] ^= 0x10;
    const auto abierto = abrir_respaldo(alterado, kContrasena);
    CHECK(abierto.resultado != AperturaSobre::abierto);
    CHECK_FALSE(abierto.contenido);
  }
  for (const std::size_t largo : {std::size_t{0}, std::size_t{20}, std::size_t{60}, archivo->size() - 1}) {
    CHECK(abrir_respaldo(gsl::span<const std::uint8_t>(*archivo).first(largo), kContrasena).resultado !=
          AperturaSobre::abierto);
  }
  auto largo = *archivo;
  largo.push_back(0);
  CHECK(abrir_respaldo(largo, kContrasena).resultado != AperturaSobre::abierto);
}

TEST_CASE("Un respaldo no se confunde con una copia de identidad ni al revés") {
  const Ejemplo ejemplo;
  const auto respaldo = armar_respaldo(ejemplo.partes(), kContrasena, kRapido);
  REQUIRE(respaldo);
  CHECK(ventrik::cifrado::abrir_sobre("VKID", *respaldo, kContrasena).resultado == AperturaSobre::danado);
  const auto identidad = ventrik::cifrado::cerrar_sobre(
      "VKID", contenido(1, {{1, ejemplo.clave}, {2, ejemplo.boveda}, {3, ejemplo.base}}), kContrasena, kRapido);
  REQUIRE(identidad);
  CHECK(abrir_respaldo(*identidad, kContrasena).resultado == AperturaSobre::danado);
}

TEST_CASE("El contenido del respaldo rechaza partes repetidas, desconocidas, vacías, faltantes, en otro orden o de más del tope") {
  const Ejemplo e;
  const auto bueno = contenido(1, {{1, e.clave}, {2, e.boveda}, {3, e.base}});
  const auto partes = leer_contenido(bueno);
  REQUIRE(partes);
  CHECK(copia(partes->clave) == e.clave);
  CHECK(copia(partes->boveda) == e.boveda);
  CHECK(copia(partes->base) == e.base);
  CHECK_FALSE(leer_contenido(contenido(1, {{3, e.base}, {1, e.clave}, {2, e.boveda}})));
  CHECK_FALSE(leer_contenido(contenido(1, {{1, e.clave}, {1, e.clave}, {2, e.boveda}, {3, e.base}})));
  CHECK_FALSE(leer_contenido(contenido(2, {{1, e.clave}, {2, e.boveda}, {3, e.base}})));
  CHECK_FALSE(leer_contenido(contenido(1, {{1, e.clave}, {2, e.boveda}})));
  CHECK_FALSE(leer_contenido(contenido(1, {{1, e.clave}, {2, e.boveda}, {3, e.base}, {1, e.clave}})));
  CHECK_FALSE(leer_contenido(contenido(1, {{1, e.clave}, {2, e.boveda}, {3, e.base}, {4, e.clave}})));
  CHECK_FALSE(leer_contenido(contenido(1, {{1, e.clave}, {2, e.boveda}, {3, e.base}, {4, {}}})));
  CHECK_FALSE(leer_contenido(contenido(1, {{1, e.clave}, {2, e.boveda}, {3, e.base}, {5, e.clave}})));
  CHECK_FALSE(leer_contenido(contenido(1, {{0, e.clave}, {1, e.clave}, {2, e.boveda}, {3, e.base}})));
  CHECK_FALSE(leer_contenido(contenido(1, {{1, {}}, {2, e.boveda}, {3, e.base}})));
  CHECK(leer_contenido(contenido(1, {{1, Bytes(kMaximoClaveRespaldo, 1)}, {2, e.boveda}, {3, e.base}})));
  CHECK_FALSE(leer_contenido(contenido(1, {{1, Bytes(kMaximoClaveRespaldo + 1, 1)}, {2, e.boveda}, {3, e.base}})));
  CHECK_FALSE(leer_contenido(contenido(1, {{1, e.clave}, {2, Bytes(kMaximoBovedaRespaldo + 1, 1)}, {3, e.base}})));
  CHECK_FALSE(leer_contenido(contenido(1, {{1, e.clave}, {2, e.boveda}, {3, Bytes(kMaximoBaseRespaldo + 1, 1)}})));
  CHECK_FALSE(leer_contenido(gsl::span<const std::uint8_t>(bueno).first(bueno.size() - 1)));
  CHECK_FALSE(leer_contenido({}));
  auto con_mas = bueno;
  con_mas.push_back(0);
  CHECK_FALSE(leer_contenido(con_mas));
  auto mas_largo = bueno;
  mas_largo[2] = 0xFF;
  CHECK_FALSE(leer_contenido(mas_largo));
  const auto sobre = ventrik::cifrado::cerrar_sobre(
      kMarcaRespaldo, contenido(1, {{1, e.clave}, {2, e.boveda}, {3, e.base}, {1, e.clave}}), kContrasena, kRapido);
  REQUIRE(sobre);
  const auto abierto = abrir_respaldo(*sobre, kContrasena);
  CHECK(abierto.resultado == AperturaSobre::danado);
  CHECK_FALSE(abierto.contenido);
}

TEST_CASE("Exportar pide una contraseña de 8 letras o más y las tres partes dentro de su tope") {
  const Ejemplo e;
  CHECK_FALSE(armar_respaldo(e.partes(), "", kRapido));
  CHECK_FALSE(armar_respaldo(e.partes(), "corta12", kRapido));
  CHECK_FALSE(armar_respaldo(e.partes(), "ñandú12", kRapido));
  CHECK(armar_respaldo(e.partes(), "ñandú123", kRapido));
  CHECK_FALSE(armar_respaldo({{}, e.boveda, e.base}, kContrasena, kRapido));
  CHECK_FALSE(armar_respaldo({e.clave, {}, e.base}, kContrasena, kRapido));
  CHECK_FALSE(armar_respaldo({e.clave, e.boveda, {}}, kContrasena, kRapido));
  const Bytes grande(kMaximoBaseRespaldo + 1, 1);
  CHECK_FALSE(armar_respaldo({e.clave, e.boveda, grande}, kContrasena, kRapido));
  const Bytes clave_grande(kMaximoClaveRespaldo + 1, 1);
  CHECK_FALSE(armar_respaldo({clave_grande, e.boveda, e.base}, kContrasena, kRapido));
}

TEST_CASE("La copia de una base abierta y en uso sale entera, sin lo que no se confirmó, y no pisa otro archivo") {
  const std::wstring archivo = temporal(L"en_uso");
  const std::wstring destino = temporal(L"copia");
  borrar_base(archivo);
  borrar_base(destino);
  {
    auto base = Base::abrir(archivo, "General");
    REQUIRE(base);
    for (std::uint32_t i = 1; i <= 20; ++i) {
      REQUIRE(base->guardar_mensaje(kCanalGeneral, i, "Ana", i, "hola " + std::to_string(i)));
    }
    CHECK(base->hay_sin_asentar());
    REQUIRE(base->empezar_tanda());
    REQUIRE(base->guardar_mensaje(kCanalGeneral, 21, "Ana", 21, "sin confirmar"));
    REQUIRE(copiar_base(archivo, destino));
    CHECK_FALSE(copiar_base(archivo, destino));
    REQUIRE(base->confirmar_tanda());
    REQUIRE(base->guardar_mensaje(kCanalGeneral, 22, "Ana", 22, "después"));
  }
  CHECK(revisar_base(destino) == RevisionBase::buena);
  CHECK(mensajes(destino) == 20);
  CHECK(mensajes(archivo) == 22);
  const std::wstring sin_origen = temporal(L"sin_origen");
  borrar_base(sin_origen);
  CHECK_FALSE(copiar_base(temporal(L"no_existe"), sin_origen));
  CHECK_FALSE(existe(sin_origen));
  CHECK_FALSE(existe(temporal(L"no_existe")));
  REQUIRE(ventrik::escribir_archivo_completo(destino + L".txt", bytes(std::string(4096, 'x'))));
  CHECK_FALSE(copiar_base(destino + L".txt", sin_origen));
  CHECK_FALSE(existe(sin_origen));
  borrar_base(destino + L".txt");
  CHECK_FALSE(copiar_base(archivo, L""));
  borrar_base(archivo);
  borrar_base(destino);
}

TEST_CASE("La revisión acepta una base de Ventrik, también una que se actualizó desde la versión 1 o la 2") {
  const std::wstring nueva = base_nueva(L"nueva");
  CHECK(revisar_base(nueva) == RevisionBase::buena);
  borrar_base(nueva);
  const std::wstring vieja = temporal(L"v1");
  borrar_base(vieja);
  ejecutar(vieja,
           "CREATE TABLE IF NOT EXISTS canales(id INTEGER PRIMARY KEY AUTOINCREMENT, nombre TEXT NOT NULL,"
           " comparable TEXT NOT NULL UNIQUE, orden INTEGER NOT NULL);"
           "CREATE TABLE IF NOT EXISTS mensajes(id INTEGER PRIMARY KEY AUTOINCREMENT,"
           " canal INTEGER NOT NULL, autor TEXT NOT NULL, hora INTEGER NOT NULL, texto TEXT NOT NULL);"
           "CREATE INDEX IF NOT EXISTS mensajes_por_canal ON mensajes(canal, id);"
           "CREATE INDEX IF NOT EXISTS mensajes_por_hora ON mensajes(hora);"
           "CREATE TABLE IF NOT EXISTS config(clave TEXT PRIMARY KEY, valor INTEGER NOT NULL);"
           "INSERT INTO canales VALUES (1, 'General', 'GENERAL', 0);"
           "INSERT INTO mensajes(canal, autor, hora, texto) VALUES (1, 'Ana', 1, 'a');"
           "PRAGMA user_version=1;");
  REQUIRE(Base::abrir(vieja, "General"));
  CHECK(revisar_base(vieja) == RevisionBase::buena);
  borrar_base(vieja);
  const std::wstring dos = temporal(L"v2");
  borrar_base(dos);
  ejecutar(dos,
           "CREATE TABLE IF NOT EXISTS canales(id INTEGER PRIMARY KEY AUTOINCREMENT, nombre TEXT NOT NULL,"
           " comparable TEXT NOT NULL UNIQUE, orden INTEGER NOT NULL, tipo INTEGER NOT NULL DEFAULT 1,"
           " solo_admins INTEGER NOT NULL DEFAULT 0, contador INTEGER NOT NULL DEFAULT 0);"
           "CREATE TABLE IF NOT EXISTS mensajes(id INTEGER PRIMARY KEY AUTOINCREMENT,"
           " canal INTEGER NOT NULL, autor TEXT NOT NULL, hora INTEGER NOT NULL, texto TEXT NOT NULL,"
           " numero INTEGER NOT NULL DEFAULT 0);"
           "CREATE INDEX IF NOT EXISTS mensajes_por_canal ON mensajes(canal, id);"
           "CREATE INDEX IF NOT EXISTS mensajes_por_hora ON mensajes(hora);"
           "CREATE TABLE IF NOT EXISTS config(clave TEXT PRIMARY KEY, valor INTEGER NOT NULL);"
           "PRAGMA user_version=2;");
  REQUIRE(Base::abrir(dos, "General"));
  CHECK(revisar_base(dos) == RevisionBase::buena);
  borrar_base(dos);
}

TEST_CASE("La revisión rechaza una base con un trigger, una vista, una tabla, un índice o una columna de más") {
  for (const char* cambio : {"CREATE TRIGGER borrar AFTER INSERT ON mensajes BEGIN DELETE FROM canales; END;",
                             "CREATE VIEW todo AS SELECT * FROM canales;", "CREATE TABLE extra(x);",
                             "CREATE INDEX extra ON mensajes(autor);", "DROP INDEX mensajes_por_hora;",
                             "ALTER TABLE canales ADD COLUMN extra INTEGER;"}) {
    const std::wstring archivo = base_nueva(L"esquema");
    ejecutar(archivo, cambio);
    CHECK_MESSAGE(revisar_base(archivo) == RevisionBase::otro_esquema, cambio);
    borrar_base(archivo);
  }
}

TEST_CASE("La revisión rechaza una base dañada, de otra versión, vacía, que no es una base o que no existe") {
  const std::wstring archivo = base_nueva(L"version");
  ejecutar(archivo, "PRAGMA user_version=2;");
  CHECK(revisar_base(archivo) == RevisionBase::version_vieja);
  CHECK_FALSE(Base::abrir(archivo, "General"));
  ejecutar(archivo, "PRAGMA user_version=99;");
  CHECK(revisar_base(archivo) == RevisionBase::otra_version);
  ejecutar(archivo, "PRAGMA user_version=0;");
  CHECK(revisar_base(archivo) == RevisionBase::otra_version);
  borrar_base(archivo);

  const std::wstring danada = temporal(L"danada");
  borrar_base(danada);
  {
    auto base = Base::abrir(danada, "General");
    REQUIRE(base);
    for (std::uint32_t i = 1; i <= 200; ++i) {
      REQUIRE(base->guardar_mensaje(kCanalGeneral, i, "Ana", i, std::string(500, 'a')));
    }
  }
  REQUIRE(revisar_base(danada) == RevisionBase::buena);
  ventrik::cifrado::Secreto leido;
  REQUIRE(ventrik::leer_archivo(danada, kMaximoBaseRespaldo, leido) == ventrik::LecturaArchivo::leido);
  Bytes rota = copia(leido.bytes());
  const std::size_t pagina = rota.size() / 4096 / 2 * 4096;
  std::fill(rota.begin() + static_cast<std::ptrdiff_t>(pagina), rota.begin() + static_cast<std::ptrdiff_t>(pagina + 16),
            std::uint8_t{0xFF});
  REQUIRE(ventrik::escribir_archivo_completo(danada, rota));
  CHECK(revisar_base(danada) == RevisionBase::danada);
  borrar_base(danada);

  const std::wstring solo_lectura = base_nueva(L"solo_lectura");
  REQUIRE(ventrik::leer_archivo(solo_lectura, kMaximoBaseRespaldo, leido) == ventrik::LecturaArchivo::leido);
  Bytes marcada = copia(leido.bytes());
  marcada[18] = 3;
  REQUIRE(ventrik::escribir_archivo_completo(solo_lectura, marcada));
  CHECK(revisar_base(solo_lectura) == RevisionBase::danada);
  CHECK_FALSE(Base::abrir(solo_lectura, "General"));
  borrar_base(solo_lectura);

  const std::wstring vacia = temporal(L"vacia");
  REQUIRE(ventrik::escribir_archivo_completo(vacia, {}));
  CHECK(revisar_base(vacia) == RevisionBase::danada);
  REQUIRE(ventrik::escribir_archivo_completo(vacia, bytes(std::string(4096, 'x'))));
  CHECK(revisar_base(vacia) == RevisionBase::danada);
  borrar_base(vacia);

  CHECK(revisar_base(temporal(L"no_existe")) == RevisionBase::error);
  CHECK(revisar_base(L"") == RevisionBase::error);
}
