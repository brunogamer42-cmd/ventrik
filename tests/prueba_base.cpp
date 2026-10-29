#include <atomic>
#include <chrono>
#include <future>
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

std::string leer_texto(const std::wstring& archivo, const char* sql) {
  sqlite3* base = nullptr;
  std::string valor;
  if (sqlite3_open_v2(ventrik::a_utf8(archivo).c_str(), &base, SQLITE_OPEN_READWRITE, nullptr) ==
      SQLITE_OK) {
    sqlite3_exec(
        base, sql,
        [](void* destino, int, char** valores, char**) {
          *static_cast<std::string*>(destino) = valores[0] != nullptr ? valores[0] : "";
          return 0;
        },
        &valor, nullptr);
  }
  sqlite3_close(base);
  return valor;
}

int leer_numero(const std::wstring& archivo, const char* sql) {
  const std::string texto = leer_texto(archivo, sql);
  return texto.empty() ? -1 : std::atoi(texto.c_str());
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
  CHECK(base->historial({*juegos, 0}, 65536).mensajes.empty());
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
  const auto historial = base->historial({kCanalGeneral, 0}, 65536);
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
  CHECK(base->historial({*juegos, 0}, 65536).mensajes.empty());
  CHECK_FALSE(base->guardar_mensaje(99, 1, "Ana", 1, "hola"));
}

TEST_CASE("El historial sale de a partes, del más nuevo al más viejo") {
  auto base = base_en_memoria();
  for (int i = 1; i <= 300; ++i) {
    REQUIRE(base->guardar_mensaje(kCanalGeneral, static_cast<std::uint32_t>(i), "Ana",
                                  static_cast<std::uint64_t>(i),
                                  "mensaje " + std::to_string(i)));
  }
  const auto primera = base->historial({kCanalGeneral, 0}, 65536);
  REQUIRE(primera.mensajes.size() == ventrik::protocolo::kMaximoMensajesEnHistorial);
  CHECK(primera.hay_mas);
  CHECK(primera.mensajes.front().texto == "mensaje 300");
  const auto segunda = base->historial({kCanalGeneral, primera.mensajes.back().id}, 65536);
  REQUIRE(segunda.mensajes.size() == 44);
  CHECK_FALSE(segunda.hay_mas);
  CHECK(segunda.mensajes.back().texto == "mensaje 1");
  const auto chica = base->historial({kCanalGeneral, 0}, 100);
  CHECK(chica.mensajes.size() == 2);
  CHECK(chica.hay_mas);
  CHECK(base->historial({2, 0}, 65536).mensajes.empty());
}

TEST_CASE("El historial también sale hacia lo más nuevo, y de a tantos mensajes como se pidan") {
  using ventrik::protocolo::Sentido;
  auto base = base_en_memoria();
  for (int i = 1; i <= 300; ++i) {
    REQUIRE(base->guardar_mensaje(kCanalGeneral, static_cast<std::uint32_t>(i), "Ana", 1, "m " + std::to_string(i)));
  }
  const auto primeros = base->historial({kCanalGeneral, 0, Sentido::nuevos, 10}, 65536);
  CHECK(primeros.sentido == Sentido::nuevos);
  REQUIRE(primeros.mensajes.size() == 10);
  CHECK(primeros.hay_mas);
  CHECK(primeros.mensajes.front().texto == "m 1");
  CHECK(primeros.mensajes.back().texto == "m 10");
  const auto desde = primeros.mensajes.back().id;
  const auto siguientes = base->historial({kCanalGeneral, desde, Sentido::nuevos, 256}, 65536);
  CHECK(siguientes.desde == desde);
  REQUIRE(siguientes.mensajes.size() == 256);
  CHECK(siguientes.mensajes.front().texto == "m 11");
  CHECK(siguientes.hay_mas);
  const auto resto = base->historial({kCanalGeneral, siguientes.mensajes.back().id, Sentido::nuevos, 256}, 65536);
  CHECK(resto.mensajes.size() == 34);
  CHECK_FALSE(resto.hay_mas);
  CHECK(base->historial({kCanalGeneral, resto.mensajes.back().id, Sentido::nuevos, 5}, 65536).mensajes.empty());
  const auto ultimos = base->historial({kCanalGeneral, 0, Sentido::viejos, 64}, 65536);
  REQUIRE(ultimos.mensajes.size() == 64);
  CHECK(ultimos.hay_mas);
  CHECK(ultimos.mensajes.front().texto == "m 300");
  const auto anteriores = base->historial({kCanalGeneral, ultimos.mensajes.back().id, Sentido::viejos, 1}, 65536);
  REQUIRE(anteriores.mensajes.size() == 1);
  CHECK(anteriores.mensajes.front().texto == "m 236");
  std::vector<std::uint8_t> marco;
  for (const auto& armado : {primeros, siguientes, resto, ultimos, anteriores}) {
    CHECK(ventrik::protocolo::armar_mensaje(armado, marco));
  }
  CHECK(base->historial({kCanalGeneral, 0, static_cast<Sentido>(3), 5}, 65536).mensajes.empty());
}

TEST_CASE("En una tanda, un mensaje que falla no deshace a los demás") {
  auto base = base_en_memoria();
  REQUIRE(base->empezar_tanda());
  CHECK(base->guardar_mensaje(kCanalGeneral, 1, "Ana", 1, "uno"));
  CHECK_FALSE(base->guardar_mensaje(99, 1, "Ana", 1, "canal que no existe"));
  CHECK_FALSE(base->guardar_mensaje(kCanalGeneral, 2, "Ana", 1, ""));
  CHECK(base->guardar_mensaje(kCanalGeneral, 3, "Ana", 1, "tres"));
  const auto juegos = base->crear_canal("Juegos");
  REQUIRE(juegos);
  CHECK(base->borrar_canal(*juegos));
  CHECK_FALSE(base->borrar_canal(*juegos));
  REQUIRE(base->confirmar_tanda());
  const auto historial = base->historial({kCanalGeneral, 0}, 65536);
  REQUIRE(historial.mensajes.size() == 2);
  CHECK(historial.mensajes[0].texto == "tres");
  CHECK(base->canales().size() == 1);
  CHECK(base->canales()[0].contador == 3);
}

TEST_CASE("En un archivo la base usa WAL, se asienta cuando se pide y al cerrarse no deja archivos de más") {
  const std::wstring archivo = archivo_temporal();
  DeleteFileW(archivo.c_str());
  const auto existe = [&](const wchar_t* sufijo) {
    return GetFileAttributesW((archivo + sufijo).c_str()) != INVALID_FILE_ATTRIBUTES;
  };
  {
    auto base = Base::abrir(archivo, "General");
    REQUIRE(base);
    CHECK_FALSE(base->hay_sin_asentar());
    REQUIRE(base->guardar_mensaje(kCanalGeneral, 1, "Ana", 1, "hola"));
    CHECK(base->hay_sin_asentar());
    CHECK(existe(L"-wal"));
    CHECK(existe(L"-shm"));
    REQUIRE(base->asentar());
    CHECK_FALSE(base->hay_sin_asentar());
    WIN32_FILE_ATTRIBUTE_DATA datos{};
    REQUIRE(GetFileAttributesExW((archivo + L"-wal").c_str(), GetFileExInfoStandard, &datos));
    CHECK(datos.nFileSizeLow == 0);
    CHECK(leer_numero(archivo, "SELECT COUNT(*) FROM mensajes") == 1);
  }
  CHECK_FALSE(existe(L"-wal"));
  CHECK_FALSE(existe(L"-shm"));
  CHECK(leer_texto(archivo, "PRAGMA journal_mode") == "wal");
  CHECK(DeleteFileW(archivo.c_str()));
}

TEST_CASE("La retención borra lo viejo y por defecto se guarda siempre") {
  auto base = base_en_memoria();
  const std::uint64_t ahora = 100 * kDia;
  REQUIRE(base->guardar_mensaje(kCanalGeneral, 1, "Ana", ahora - 40 * kDia, "muy viejo"));
  REQUIRE(base->guardar_mensaje(kCanalGeneral, 1, "Ana", ahora - 10 * kDia, "viejo"));
  REQUIRE(base->guardar_mensaje(kCanalGeneral, 1, "Ana", ahora - 1000, "nuevo"));
  const auto cuantos = [&] { return base->historial({kCanalGeneral, 0}, 65536).mensajes.size(); };
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
  const auto historial = base->historial({*id, 0}, 65536);
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
    CHECK(base->historial({kCanalGeneral, 0}, 65536).mensajes.size() == 1);
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
    const auto general = base->historial({kCanalGeneral, 0}, 65536);
    REQUIRE(general.mensajes.size() == 2);
    CHECK(general.mensajes[0].numero == 2);
    CHECK(general.mensajes[1].numero == 1);
    const auto juegos = base->historial({2, 0}, 65536);
    REQUIRE(juegos.mensajes.size() == 1);
    CHECK(juegos.mensajes[0].numero == 1);
    REQUIRE(base->guardar_mensaje(kCanalGeneral, 3, "Ana", 4, "d"));
  }
  {
    auto base = Base::abrir(archivo, "General");
    REQUIRE(base);
    CHECK(base->canales()[0].contador == 3);
    CHECK(base->historial({kCanalGeneral, 0}, 65536).mensajes.size() == 3);
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
  armar_base(archivo, "PRAGMA user_version=5");
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

TEST_CASE("Si SQLite deshace la tanda en el medio, ningún trabajo queda hecho dos veces") {
  ventrik::red::Inicio red;
  REQUIRE(red.ok());
  ventrik::red::Despertador despertador;
  REQUIRE(despertador.ok());
  const std::wstring archivo = archivo_temporal();
  DeleteFileW(archivo.c_str());
  {
    ServicioBase servicio;
    REQUIRE(servicio.arrancar(archivo, "General", despertador));
    std::promise<void> seguir;
    const std::shared_future<void> esperar = seguir.get_future().share();
    REQUIRE(servicio.pedir([esperar](Base&) -> std::function<void()> {
      esperar.wait();
      return {};
    }));
    const auto guardar = [](std::uint32_t numero) {
      return [numero](Base& base) -> std::function<void()> {
        base.guardar_mensaje(kCanalGeneral, numero, "Ana", 1, "hola");
        return {};
      };
    };
    REQUIRE(servicio.pedir(guardar(1)));
    REQUIRE(servicio.pedir([](Base& base) -> std::function<void()> {
      base.deshacer_lo_abierto();
      return {};
    }));
    REQUIRE(servicio.pedir(guardar(2)));
    REQUIRE(servicio.pedir(guardar(3)));
    seguir.set_value();
  }
  CHECK(leer_numero(archivo, "SELECT COUNT(*) FROM mensajes") == 3);
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

TEST_CASE("Las identidades se guardan, se renombran y las que no vuelven en 180 días se borran") {
  const std::wstring archivo = archivo_temporal();
  DeleteFileW(archivo.c_str());
  const std::uint64_t ahora = 1800000000000ull;
  {
    auto base = Base::abrir(archivo, "General");
    REQUIRE(base);
    UsuarioGuardado ana;
    ana.clave.fill(1);
    ana.clave[0] = 4;
    ana.nombre = "Ana";
    ana.visto_ms = ahora;
    UsuarioGuardado vieja = ana;
    vieja.clave[1] = 2;
    vieja.nombre = "Vieja";
    vieja.visto_ms = ahora - 181 * kDia;
    UsuarioGuardado duena = vieja;
    duena.clave[1] = 3;
    duena.nombre = "Dueña";
    duena.dueno = true;
    REQUIRE(base->guardar_usuario(ana));
    REQUIRE(base->guardar_usuario(vieja));
    REQUIRE(base->guardar_usuario(duena));
    ana.nombre = "Anita";
    REQUIRE(base->guardar_usuario(ana));
    UsuarioGuardado mismo_nombre = ana;
    mismo_nombre.clave[1] = 9;
    mismo_nombre.nombre = "ANITA";
    CHECK_FALSE(base->guardar_usuario(mismo_nombre));
    UsuarioGuardado sin_nombre = ana;
    sin_nombre.clave[1] = 8;
    sin_nombre.nombre.clear();
    CHECK_FALSE(base->guardar_usuario(sin_nombre));
    Equipo equipo{};
    for (std::uint8_t i = 0; i < 20; ++i) {
      equipo[0] = i;
      REQUIRE(base->anotar_equipo(ana.clave, equipo, ahora + i));
    }
    REQUIRE(base->anotar_equipo(vieja.clave, equipo, ahora));
    CHECK(base->texto_guardado("nombre").empty());
    CHECK(base->guardar_texto("nombre", "Taberna"));
    CHECK(base->texto_guardado("nombre") == "Taberna");
    UsuarioGuardado anterior = ana;
    anterior.visto_ms = ahora - kDia;
    REQUIRE(base->guardar_usuario(anterior));
    const auto atrasado = base->usuarios(ahora - 200 * kDia);
    REQUIRE(atrasado);
    CHECK(atrasado->size() == 3);
    const auto lista = base->usuarios(ahora);
    REQUIRE(lista);
    REQUIRE(lista->size() == 2);
    CHECK((*lista)[0].nombre == "Dueña");
    CHECK((*lista)[0].dueno);
    CHECK((*lista)[1].nombre == "Anita");
    CHECK((*lista)[1].clave == ana.clave);
    CHECK((*lista)[1].visto_ms == ahora);
    CHECK_FALSE((*lista)[1].dueno);
    CHECK(base->quitar_dueno());
    const auto sin_duena = base->usuarios(ahora);
    REQUIRE(sin_duena);
    REQUIRE(sin_duena->size() == 1);
    CHECK((*sin_duena)[0].nombre == "Anita");
    CHECK(base->liberar_nombre("ANITA"));
    CHECK(base->usuarios(ahora)->empty());
  }
  CHECK(leer_numero(archivo, "SELECT COUNT(*) FROM equipos") == 0);
  CHECK(leer_numero(archivo, "SELECT COUNT(*) FROM usuarios") == 0);
  CHECK(leer_numero(archivo, "PRAGMA user_version") == 4);
  for (const auto* final : {L"", L"-wal", L"-shm"}) {
    DeleteFileW((archivo + final).c_str());
  }
}

TEST_CASE("Las identidades que entraron una sola vez vencen antes, y se pueden desalojar a la semana") {
  const std::uint64_t ahora = 1800000000000ull;
  UsuarioGuardado una_vez;
  una_vez.entradas = 1;
  una_vez.creado_ms = ahora - 9 * kDia;
  una_vez.visto_ms = ahora - 8 * kDia;
  CHECK(se_puede_desalojar(una_vez, ahora));
  CHECK_FALSE(esta_vencido(una_vez, ahora));
  una_vez.visto_ms = ahora - 31 * kDia;
  CHECK(esta_vencido(una_vez, ahora));
  UsuarioGuardado volvio = una_vez;
  volvio.entradas = 2;
  CHECK_FALSE(esta_vencido(volvio, ahora));
  CHECK_FALSE(se_puede_desalojar(volvio, ahora));
  volvio.visto_ms = ahora - 181 * kDia;
  volvio.creado_ms = volvio.visto_ms - kDia;
  CHECK(esta_vencido(volvio, ahora));
  UsuarioGuardado reciente;
  reciente.entradas = 1;
  reciente.creado_ms = reciente.visto_ms = ahora - kDia;
  CHECK_FALSE(se_puede_desalojar(reciente, ahora));
  una_vez.dueno = true;
  CHECK_FALSE(esta_vencido(una_vez, ahora));
  CHECK_FALSE(se_puede_desalojar(una_vez, ahora));

  auto base = base_en_memoria();
  UsuarioGuardado guardada;
  guardada.clave.fill(5);
  guardada.clave[0] = 4;
  guardada.nombre = "Una vez";
  guardada.entradas = 1;
  guardada.creado_ms = guardada.visto_ms = ahora - 31 * kDia;
  REQUIRE(base->guardar_usuario(guardada));
  UsuarioGuardado al_reves = guardada;
  al_reves.clave[1] = 6;
  al_reves.nombre = "Al revés";
  al_reves.creado_ms = ahora;
  al_reves.visto_ms = ahora - kDia;
  CHECK_FALSE(base->guardar_usuario(al_reves));
  REQUIRE(base->usuarios(ahora));
  CHECK(base->usuarios(ahora)->empty());
  REQUIRE(base->guardar_usuario(guardada));
  CHECK(base->borrar_usuarios({guardada.clave}));
  CHECK(base->usuarios(ahora - 40 * kDia)->empty());
}

TEST_CASE("Si la tabla de identidades no se puede leer, no se la toma por vacía") {
  const std::wstring archivo = archivo_temporal();
  DeleteFileW(archivo.c_str());
  armar_base(archivo,
             "CREATE TABLE usuarios(clave BLOB PRIMARY KEY, nombre TEXT NOT NULL);"
             "INSERT INTO usuarios VALUES (x'04', 'Ana');");
  {
    auto base = Base::abrir(archivo, "General");
    REQUIRE(base);
    CHECK_FALSE(base->usuarios(1800000000000ull));
  }
  for (const auto* final : {L"", L"-wal", L"-shm"}) {
    DeleteFileW((archivo + final).c_str());
  }
}

TEST_CASE("Los avisos pendientes son de cada identidad, con tope, no vuelven al equipo que los causó y se van con ella") {
  auto base = base_en_memoria();
  UsuarioGuardado ana;
  ana.clave.fill(4);
  ana.nombre = "Ana";
  ana.creado_ms = ana.visto_ms = 1000;
  REQUIRE(base->guardar_usuario(ana));
  Equipo casa{};
  casa.fill(1);
  Equipo otra{};
  otra.fill(2);
  ClaveDeUsuario nadie{};
  nadie.fill(9);
  using ventrik::protocolo::AvisoDeSeguridad;
  using ventrik::protocolo::Seguridad;
  const AvisoDeSeguridad borrado{Seguridad::canal_borrado, 5, "Tano", "partidas"};
  CHECK(base->guardar_aviso(nadie, std::nullopt, borrado));
  CHECK(base->tomar_avisos(nadie, casa).empty());
  CHECK_FALSE(base->guardar_aviso(ana.clave, std::nullopt, {Seguridad::canal_borrado, 5, " Tano", ""}));
  for (int i = 0; i < 120; ++i) {
    REQUIRE(base->guardar_aviso(ana.clave, std::nullopt, borrado));
  }
  REQUIRE(base->guardar_aviso(ana.clave, otra, {Seguridad::equipo_nuevo, 6, "Ana", ""}));
  CHECK(base->tomar_avisos(ana.clave, otra).size() == kMaximoAvisosPorUsuario - 1);
  CHECK(base->tomar_avisos(ana.clave, otra).empty());
  const auto del_otro = base->tomar_avisos(ana.clave, casa);
  REQUIRE(del_otro.size() == 1);
  CHECK(del_otro[0].tipo == Seguridad::equipo_nuevo);
  CHECK(del_otro[0].hora_ms == 6);
  CHECK(base->tomar_avisos(ana.clave, casa).empty());
  REQUIRE(base->guardar_aviso(ana.clave, std::nullopt, borrado));
  CHECK(base->liberar_nombre("Ana"));
  REQUIRE(base->guardar_usuario(ana));
  CHECK(base->tomar_avisos(ana.clave, casa).empty());
}

TEST_CASE("El registro de seguridad guarda lo último de cada tipo, así muchas fallas no borran lo demás") {
  auto base = base_en_memoria();
  using ventrik::protocolo::Seguridad;
  REQUIRE(base->anotar({Seguridad::canal_borrado, 1, "Tano", "partidas"}));
  REQUIRE(base->anotar({Seguridad::admin_nombrado, 2, "Ana", "Tano"}));
  for (std::size_t i = 0; i < kMaximoRegistroPorTipo + 50; ++i) {
    REQUIRE(base->anotar({Seguridad::ip_bloqueada, 3 + i, "", "203.0.113.9:4000"}));
  }
  CHECK_FALSE(base->anotar({static_cast<Seguridad>(9), 1, "", ""}));
  const auto registro = base->registro(100000);
  REQUIRE(registro.size() == kMaximoRegistroPorTipo + 2);
  CHECK(registro.front().hora_ms == 3 + kMaximoRegistroPorTipo + 49);
  CHECK(registro[registro.size() - 2].tipo == Seguridad::admin_nombrado);
  CHECK(registro.back().tipo == Seguridad::canal_borrado);
  CHECK(registro.back().detalle == "partidas");
  CHECK(base->registro(5).size() == 5);
}

TEST_CASE("Una base de la versión 2 pasa sola a la 4 y suma las tablas de identidades y de seguridad") {
  const std::wstring archivo = archivo_temporal();
  DeleteFileW(archivo.c_str());
  armar_base(archivo,
             "CREATE TABLE canales(id INTEGER PRIMARY KEY AUTOINCREMENT, nombre TEXT NOT NULL,"
             " comparable TEXT NOT NULL UNIQUE, orden INTEGER NOT NULL, tipo INTEGER NOT NULL DEFAULT 1,"
             " solo_admins INTEGER NOT NULL DEFAULT 0, contador INTEGER NOT NULL DEFAULT 0);"
             "CREATE TABLE mensajes(id INTEGER PRIMARY KEY AUTOINCREMENT, canal INTEGER NOT NULL,"
             " autor TEXT NOT NULL, hora INTEGER NOT NULL, texto TEXT NOT NULL, numero INTEGER NOT NULL DEFAULT 0);"
             "CREATE TABLE config(clave TEXT PRIMARY KEY, valor INTEGER NOT NULL);"
             "INSERT INTO canales(id, nombre, comparable, orden) VALUES (1, 'General', 'GENERAL', 0);"
             "INSERT INTO canales(nombre, comparable, orden) VALUES ('Juegos', 'JUEGOS', 1);"
             "PRAGMA user_version=2;");
  {
    auto base = Base::abrir(archivo, "General");
    REQUIRE(base);
    CHECK(base->canales().size() == 2);
    CHECK(base->usuarios(1800000000000ull)->empty());
  }
  CHECK(leer_numero(archivo, "PRAGMA user_version") == 4);
  CHECK(leer_numero(archivo, "SELECT COUNT(*) FROM sqlite_master WHERE name IN ('usuarios', 'equipos', 'textos', 'avisos', 'seguridad')") == 5);
  for (const auto* final : {L"", L"-wal", L"-shm"}) {
    DeleteFileW((archivo + final).c_str());
  }
}
