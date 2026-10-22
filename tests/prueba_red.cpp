#include <chrono>
#include <string>

#include "common/red/red.h"
#include <doctest/doctest.h>

using ventrik::red::Direccion;
using ventrik::red::leer_direccion;
using namespace std::chrono_literals;

namespace {

constexpr std::uint32_t kLocal = 0x7F000001;

}  // namespace

TEST_CASE("Se leen direcciones IPv4 con y sin puerto") {
  CHECK(leer_direccion("192.168.0.10") == Direccion{0xC0A8000A, 7700});
  CHECK(leer_direccion("10.0.0.1:8000") == Direccion{0x0A000001, 8000});
  CHECK(leer_direccion("0.0.0.0:1") == Direccion{0, 1});
  CHECK(leer_direccion("255.255.255.255:65535") == Direccion{0xFFFFFFFF, 65535});
}

TEST_CASE("Las direcciones mal escritas no se aceptan") {
  for (const char* mala :
       {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "01.2.3.4", "1.2.3.4:", "1.2.3.4:0",
        "1.2.3.4:65536", "1.2.3.4:080", " 1.2.3.4", "1.2.3.4 ", "a.b.c.d", "1..2.3",
        "1.2.3.4:123456", "-1.2.3.4", "1.2.3.4:+80", "1.2.3.4::80"}) {
    CAPTURE(mala);
    CHECK_FALSE(leer_direccion(mala));
  }
}

TEST_CASE("Una dirección se escribe y se vuelve a leer igual") {
  const Direccion direccion{0xC0A8000A, 7700};
  CHECK(ventrik::red::a_texto(direccion) == "192.168.0.10:7700");
  CHECK(leer_direccion(ventrik::red::a_texto(direccion)) == direccion);
}

TEST_CASE("Se escucha en un puerto libre y se acepta una conexión") {
  const auto escucha = ventrik::red::escuchar(Direccion{kLocal, 0});
  REQUIRE(escucha.valido());
  const auto local = ventrik::red::direccion_local(escucha);
  REQUIRE(local);
  CHECK(local->puerto != 0);

  const auto programa = ventrik::red::conectar(*local, 2s);
  REQUIRE(programa.valido());
  REQUIRE(ventrik::red::esperar(escucha, true, false, 2s) == ventrik::red::Espera::lista);
  bool fallo = false;
  const auto aceptada = ventrik::red::aceptar(escucha, fallo);
  REQUIRE(aceptada);
  CHECK_FALSE(fallo);
  CHECK(aceptada->second.ipv4 == kLocal);
}

TEST_CASE("Conectar a un puerto cerrado falla sin quedarse esperando") {
  Direccion cerrado{kLocal, 0};
  {
    const auto escucha = ventrik::red::escuchar(Direccion{kLocal, 0});
    REQUIRE(escucha.valido());
    cerrado = *ventrik::red::direccion_local(escucha);
  }
  const auto inicio = std::chrono::steady_clock::now();
  CHECK_FALSE(ventrik::red::conectar(cerrado, 5s).valido());
  CHECK(std::chrono::steady_clock::now() - inicio < 4s);
}
