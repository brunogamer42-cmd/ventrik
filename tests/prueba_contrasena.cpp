#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <windows.h>

#include "client/conexion.h"
#include "cliente_de_prueba.h"
#include "common/cifrado/base32.h"
#include "common/protocol/bytes.h"
#include "server/contrasena.h"
#include <doctest/doctest.h>

using namespace prueba;
using ventrik::cifrado::Argon2;
using ventrik::cliente::EstadoConexion;
using ventrik::cliente::IntentoDeConexion;
using ventrik::cliente::Presentacion;
using ventrik::servidor::Evento;
using ventrik::servidor::Comprobado;
using ventrik::servidor::comprobar;
using ventrik::servidor::guardar_contrasena;
using ventrik::servidor::leer_contrasena;

namespace {

constexpr Argon2 kRapido{8 * 1024, 1};

Presentacion con_contrasena(Presentacion quien, std::optional<std::string_view> contrasena) {
  if (contrasena) {
    quien.contrasena = secreto_de(*contrasena);
  }
  return quien;
}

Presentacion nueva(std::string nombre) { return {std::move(nombre), {}, identidad_nueva_de_prueba(), equipo_de_prueba()}; }

ventrik::servidor::Opciones con_clave(std::chrono::milliseconds espera = 5s) {
  ventrik::servidor::Opciones opciones;
  opciones.poner_contrasena = guardar_contrasena("sésamo", kRapido);
  REQUIRE(opciones.poner_contrasena);
  opciones.espera_tras_fallo = espera;
  return opciones;
}

ventrik::cliente::ResultadoConexion probar(const ServidorDePrueba& servidor, const Presentacion& quien,
                                           std::optional<std::string_view> contrasena) {
  return ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), con_contrasena(quien, contrasena));
}

std::string armada(std::uint8_t version, std::uint32_t memoria, std::uint8_t vueltas) {
  ventrik::protocolo::Escritor escritor;
  escritor.escribir_u8(version);
  escritor.escribir_u32(memoria);
  escritor.escribir_u8(vueltas);
  const std::array<std::uint8_t, ventrik::cifrado::kBytesSal + ventrik::cifrado::kBytesClaveSimetrica> resto{};
  escritor.escribir_bytes(resto);
  REQUIRE(escritor.ok());
  return ventrik::cifrado::a_base32(escritor.datos());
}

std::wstring base_temporal() {
  std::array<wchar_t, MAX_PATH> carpeta{};
  REQUIRE(GetTempPathW(static_cast<DWORD>(carpeta.size()), carpeta.data()) > 0);
  const std::wstring archivo =
      std::wstring(carpeta.data()) + L"ventrik_contrasena_" + std::to_wstring(GetCurrentProcessId()) + L".db";
  DeleteFileW(archivo.c_str());
  return archivo;
}

}

TEST_CASE("La contraseña guardada coincide solo con la misma, y una alterada o armada no se lee") {
  const auto texto = guardar_contrasena("sésamo", kRapido);
  REQUIRE(texto);
  const auto leida = leer_contrasena(*texto);
  REQUIRE(leida);
  CHECK(comprobar(*leida, "sésamo") == Comprobado::coincide);
  CHECK(comprobar(*leida, "Sésamo") == Comprobado::distinta);
  CHECK(comprobar(*leida, "") == Comprobado::distinta);
  CHECK(comprobar(*leida, std::string(ventrik::protocolo::kMaximoBytesContrasena + 1, 'a')) == Comprobado::distinta);
  auto imposible = *leida;
  imposible.argon2.memoria_kib = 1024 * 1024;
  CHECK(comprobar(imposible, "sésamo") == Comprobado::fallo);
  CHECK_FALSE(guardar_contrasena("con\ttabulador", kRapido));
  CHECK_FALSE(guardar_contrasena(std::string("\xC3", 1), kRapido));
  CHECK_FALSE(guardar_contrasena(std::string("borrar\x7F"), kRapido));
  CHECK(guardar_contrasena("sésamo", kRapido) != texto);
  CHECK_FALSE(guardar_contrasena("", kRapido));
  CHECK_FALSE(guardar_contrasena(std::string(ventrik::protocolo::kMaximoBytesContrasena + 1, 'a'), kRapido));
  CHECK(guardar_contrasena(std::string(ventrik::protocolo::kMaximoBytesContrasena, 'a'), kRapido));
  CHECK_FALSE(leer_contrasena(""));
  CHECK_FALSE(leer_contrasena(texto->substr(0, texto->size() - 1)));
  CHECK_FALSE(leer_contrasena(*texto + "A"));
  CHECK(leer_contrasena(armada(1, 8 * 1024, 1)));
  CHECK_FALSE(leer_contrasena(armada(2, 8 * 1024, 1)));
  CHECK_FALSE(leer_contrasena(armada(1, 1024 * 1024, 1)));
  CHECK_FALSE(leer_contrasena(armada(1, 1024, 1)));
  CHECK_FALSE(leer_contrasena(armada(1, 8 * 1024, 0)));
  CHECK_FALSE(leer_contrasena(armada(1, 8 * 1024, 11)));
}

TEST_CASE("Sin valores fijos, la contraseña se calibra para esta PC y se puede comprobar") {
  const auto texto = guardar_contrasena("sésamo");
  REQUIRE(texto);
  const auto leida = leer_contrasena(*texto);
  REQUIRE(leida);
  CHECK(comprobar(*leida, "sésamo") == Comprobado::coincide);
}

TEST_CASE("Una identidad nueva escribe la contraseña del servidor una sola vez") {
  ServidorDePrueba servidor(con_clave());
  const Presentacion ana = nueva("Ana");
  CHECK(probar(servidor, ana, std::nullopt).estado == EstadoConexion::falta_contrasena);
  CHECK(probar(servidor, ana, "sésamo").estado == EstadoConexion::conectado);
  CHECK(probar(servidor, ana, std::nullopt).estado == EstadoConexion::conectado);
  CHECK(probar(servidor, ana, "otra").estado == EstadoConexion::conectado);
  CHECK(servidor.cuantos(Evento::contrasena_incorrecta) == 0);
}

TEST_CASE("Sin la contraseña no se sabe si un nombre está ocupado") {
  ServidorDePrueba servidor(con_clave());
  CHECK(probar(servidor, nueva("Ana"), "sésamo").estado == EstadoConexion::conectado);
  CHECK(probar(servidor, nueva("Ana"), std::nullopt).estado == EstadoConexion::falta_contrasena);
  CHECK(probar(servidor, nueva("Ana"), "sésamo").estado == EstadoConexion::nombre_en_uso);
}

TEST_CASE("Cada contraseña equivocada hace esperar el doble, y mientras tanto ni se calcula") {
  ServidorDePrueba servidor(con_clave(200ms));
  const Presentacion beto = nueva("Beto");
  std::vector<std::uint16_t> esperas;
  for (const int milisegundos : {200, 400, 800, 1600}) {
    const auto resultado = probar(servidor, beto, "mala");
    CHECK(resultado.estado == EstadoConexion::contrasena_incorrecta);
    esperas.push_back(resultado.espera);
    if (milisegundos < 1600) {
      std::this_thread::sleep_for(std::chrono::milliseconds(milisegundos + 150));
    }
  }
  CHECK(esperas == std::vector<std::uint16_t>{1, 1, 1, 2});
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::contrasena_incorrecta) == 4; }));
  const auto apurada = probar(servidor, beto, "sésamo");
  CHECK(apurada.estado == EstadoConexion::esperar);
  CHECK(apurada.espera >= 1);
  CHECK(servidor.cuantos(Evento::contrasena_incorrecta) == 4);
  CHECK(servidor.cuantos(Evento::bloqueada) == 0);
  std::this_thread::sleep_for(1800ms);
  CHECK(probar(servidor, beto, "sésamo").estado == EstadoConexion::conectado);
}

TEST_CASE("Una sola contraseña por IP a la vez: las demás reciben ocupado sin contar falla") {
  ventrik::servidor::Opciones opciones;
  opciones.poner_contrasena = guardar_contrasena("sésamo", Argon2{32 * 1024, 3});
  REQUIRE(opciones.poner_contrasena);
  ServidorDePrueba servidor(opciones);
  std::vector<std::unique_ptr<Cliente>> clientes;
  for (const char* nombre : {"Ana", "Beto", "Caro"}) {
    clientes.push_back(std::make_unique<Cliente>(servidor, con_contrasena(nueva(nombre), "sésamo")));
  }
  int conectados = 0;
  int ocupados = 0;
  for (auto& cliente : clientes) {
    const auto intento = cliente->esperar<IntentoDeConexion>();
    REQUIRE(intento);
    conectados += intento->estado == EstadoConexion::conectado ? 1 : 0;
    ocupados += intento->estado == EstadoConexion::ocupado ? 1 : 0;
  }
  CHECK(conectados >= 1);
  CHECK(ocupados >= 1);
  CHECK(conectados + ocupados == 3);
  CHECK(servidor.cuantos(Evento::bloqueada) == 0);

  opciones.poner_contrasena = guardar_contrasena("sésamo", kRapido);
  opciones.comprobaciones_en_cola = 0;
  ServidorDePrueba sin_lugar(opciones);
  CHECK(probar(sin_lugar, nueva("Ana"), "sésamo").estado == EstadoConexion::ocupado);
}

TEST_CASE("El que reclama el servidor con el código de dueño no necesita la contraseña") {
  const auto codigo = ventrik::cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  ventrik::servidor::Opciones opciones = con_clave();
  opciones.codigo_dueno = codigo;
  ServidorDePrueba servidor(opciones);
  Presentacion dueno = nueva("Dueña");
  dueno.codigo_dueno = codigo;
  const auto resultado = probar(servidor, dueno, std::nullopt);
  CHECK(resultado.estado == EstadoConexion::conectado);
  REQUIRE(resultado.presentado);
  CHECK(resultado.presentado->rol == ventrik::protocolo::Rol::dueno);
}

TEST_CASE("La contraseña queda guardada en la base del servidor y vacía se quita") {
  const std::wstring archivo = base_temporal();
  ventrik::servidor::Opciones opciones = con_clave();
  opciones.archivo_base = archivo;
  { ServidorDePrueba servidor(opciones); }
  opciones.poner_contrasena.reset();
  {
    ServidorDePrueba servidor(opciones);
    CHECK(probar(servidor, nueva("Ana"), std::nullopt).estado == EstadoConexion::falta_contrasena);
  }
  opciones.poner_contrasena = std::string();
  {
    ServidorDePrueba servidor(opciones);
    CHECK(probar(servidor, nueva("Beto"), std::nullopt).estado == EstadoConexion::conectado);
  }
  opciones.poner_contrasena = "no es una contraseña guardada";
  opciones.escuchar = {kLocal, 0};
  {
    ventrik::cifrado::IdentidadServidor identidad = identidad_nueva();
    ventrik::servidor::Servidor roto(identidad, opciones,
                                     [](Evento, const ventrik::red::Direccion&, const std::string&) {});
    CHECK_FALSE(roto.escuchar());
  }
  for (const auto* final : {L"", L"-wal", L"-shm"}) {
    DeleteFileW((archivo + final).c_str());
  }
}
