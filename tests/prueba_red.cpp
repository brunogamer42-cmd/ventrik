#include <array>
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

TEST_CASE("UDP: ida y vuelta, y un puerto cerrado no rompe el socket") {
  using namespace ventrik::red;
  const auto a = abrir_udp({kLocal, 0});
  const auto b = abrir_udp({kLocal, 0});
  REQUIRE(a.valido());
  REQUIRE(b.valido());
  const Direccion dir_a = *direccion_local(a);
  const Direccion dir_b = *direccion_local(b);
  Direccion cerrado;
  {
    const auto c = abrir_udp({kLocal, 0});
    cerrado = *direccion_local(c);
  }
  const std::array<std::uint8_t, 3> datos = {1, 2, 3};
  std::array<std::uint8_t, 16> recibido{};
  CHECK(enviar_a(a, cerrado, datos));
  REQUIRE(enviar_a(a, dir_b, datos));
  REQUIRE(esperar(b, true, false, 2s) == Espera::lista);
  const auto en_b = recibir_de(b, recibido);
  REQUIRE(en_b);
  CHECK(en_b->largo == 3);
  CHECK(en_b->origen == dir_a);
  REQUIRE(enviar_a(b, dir_a, datos));
  REQUIRE(esperar(a, true, false, 2s) == Espera::lista);
  const auto en_a = recibir_de(a, recibido);
  REQUIRE(en_a);
  CHECK(en_a->largo == 3);
  CHECK_FALSE(recibir_de(a, recibido));
  const auto conectado = conectar_udp(dir_b);
  REQUIRE(conectado.valido());
  REQUIRE(enviar_a(conectado, dir_b, datos));
  REQUIRE(esperar(b, true, false, 2s) == Espera::lista);
  CHECK(recibir_de(b, recibido)->largo == 3);
  std::array<std::uint8_t, 2> chico{};
  REQUIRE(enviar_a(a, dir_b, datos));
  REQUIRE(esperar(b, true, false, 2s) == Espera::lista);
  CHECK(recibir_de(b, chico)->largo == 0);
}
