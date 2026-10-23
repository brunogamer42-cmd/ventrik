#include <chrono>
#include <cstdint>

#include "server/proteccion.h"
#include <doctest/doctest.h>

using namespace std::chrono_literals;
using ventrik::servidor::Decision;
using ventrik::servidor::OpcionesProteccion;
using ventrik::servidor::Proteccion;

namespace {

constexpr std::uint32_t kUna = 0x0A000001;
constexpr std::uint32_t kOtra = 0x0A000002;

const ventrik::servidor::Instante kInicio = ventrik::servidor::Instante{} + 1000s;

OpcionesProteccion chicas() {
  OpcionesProteccion opciones;
  opciones.rafaga_por_ip = 3;
  opciones.conexiones_por_segundo_por_ip = 1;
  opciones.rafaga_total = 100;
  opciones.saludos_por_segundo_total = 100;
  opciones.fallas_para_bloquear = 3;
  opciones.ventana_de_fallas = 60s;
  opciones.tiempo_de_bloqueo = 300s;
  opciones.silencio_entre_avisos = 30s;
  return opciones;
}

}  // namespace

TEST_CASE("Cada IP tiene una ráfaga de conexiones y después un ritmo") {
  Proteccion proteccion(chicas());
  for (int i = 0; i < 3; ++i) {
    CHECK(proteccion.nueva_conexion(kUna, kInicio).decision == Decision::aceptar);
  }
  CHECK(proteccion.nueva_conexion(kUna, kInicio).decision == Decision::demasiado_rapido);
  CHECK(proteccion.nueva_conexion(kOtra, kInicio).decision == Decision::aceptar);
  CHECK(proteccion.nueva_conexion(kUna, kInicio + 1s).decision == Decision::aceptar);
  CHECK(proteccion.nueva_conexion(kUna, kInicio + 1s).decision == Decision::demasiado_rapido);
}

TEST_CASE("Una IP que falla seguido queda bloqueada un rato") {
  Proteccion proteccion(chicas());
  CHECK_FALSE(proteccion.fallo(kUna, kInicio));
  CHECK_FALSE(proteccion.fallo(kUna, kInicio + 1s));
  CHECK(proteccion.fallo(kUna, kInicio + 2s));
  CHECK(proteccion.nueva_conexion(kUna, kInicio + 10s).decision == Decision::bloqueada);
  CHECK(proteccion.nueva_conexion(kOtra, kInicio + 10s).decision == Decision::aceptar);
  CHECK(proteccion.nueva_conexion(kUna, kInicio + 2s + 301s).decision == Decision::aceptar);
}

TEST_CASE("Las fallas espaciadas no bloquean") {
  Proteccion proteccion(chicas());
  CHECK_FALSE(proteccion.fallo(kUna, kInicio));
  CHECK_FALSE(proteccion.fallo(kUna, kInicio + 1s));
  CHECK_FALSE(proteccion.fallo(kUna, kInicio + 120s));
  CHECK_FALSE(proteccion.fallo(kUna, kInicio + 121s));
  CHECK(proteccion.nueva_conexion(kUna, kInicio + 122s).decision == Decision::aceptar);
}

TEST_CASE("Un saludo bueno entre medio no borra las fallas") {
  Proteccion proteccion(chicas());
  CHECK_FALSE(proteccion.fallo(kUna, kInicio));
  CHECK_FALSE(proteccion.fallo(kUna, kInicio + 1s));
  proteccion.saludo_completo(kUna, kInicio + 2s);
  CHECK(proteccion.fallo(kUna, kInicio + 3s));
}

TEST_CASE("Hay un tope de saludos por segundo para todas las IP juntas") {
  OpcionesProteccion opciones = chicas();
  opciones.rafaga_total = 2;
  opciones.saludos_por_segundo_total = 1;
  Proteccion proteccion(opciones);
  CHECK(proteccion.empezar_saludo(1, kInicio).decision == Decision::aceptar);
  CHECK(proteccion.empezar_saludo(2, kInicio).decision == Decision::aceptar);
  CHECK(proteccion.empezar_saludo(3, kInicio).decision == Decision::ocupado);
  CHECK(proteccion.empezar_saludo(4, kInicio + 1s).decision == Decision::aceptar);
}

OpcionesProteccion casi_sin_turnos() {
  OpcionesProteccion opciones = chicas();
  opciones.rafaga_total = 1;
  opciones.saludos_por_segundo_total = 0.001;
  opciones.rafaga_reserva = 1;
  opciones.reserva_por_segundo = 0.001;
  return opciones;
}

TEST_CASE("Las IP que ya saludaron bien tienen una reserva, pero con techo") {
  Proteccion proteccion(casi_sin_turnos());
  proteccion.saludo_completo(kUna, kInicio);
  CHECK(proteccion.empezar_saludo(kOtra, kInicio).decision == Decision::aceptar);
  CHECK(proteccion.empezar_saludo(kOtra, kInicio).decision == Decision::ocupado);
  CHECK(proteccion.empezar_saludo(kUna, kInicio).decision == Decision::aceptar);
  CHECK(proteccion.empezar_saludo(kUna, kInicio).decision == Decision::ocupado);
}

TEST_CASE("Ser conocida dura un día y se pierde con una falla") {
  Proteccion proteccion(casi_sin_turnos());
  proteccion.saludo_completo(kUna, kInicio);
  proteccion.saludo_completo(kOtra, kInicio);
  CHECK(proteccion.empezar_saludo(3, kInicio).decision == Decision::aceptar);
  proteccion.fallo(kOtra, kInicio);
  CHECK(proteccion.empezar_saludo(kOtra, kInicio).decision == Decision::ocupado);
  CHECK(proteccion.empezar_saludo(4, kInicio + 25h).decision == Decision::aceptar);
  CHECK(proteccion.empezar_saludo(kUna, kInicio + 25h).decision == Decision::ocupado);
}

TEST_CASE("Con la memoria llena, se olvidan primero las IP desconocidas") {
  OpcionesProteccion opciones = casi_sin_turnos();
  opciones.ips_recordadas = 3;
  Proteccion proteccion(opciones);
  proteccion.saludo_completo(kUna, kInicio);
  proteccion.nueva_conexion(2, kInicio);
  proteccion.nueva_conexion(3, kInicio);
  proteccion.nueva_conexion(4, kInicio + 1s);
  CHECK(proteccion.empezar_saludo(5, kInicio + 1s).decision == Decision::aceptar);
  CHECK(proteccion.empezar_saludo(kUna, kInicio + 1s).decision == Decision::aceptar);
}

TEST_CASE("Durante un ataque, cada IP se avisa una vez cada tanto") {
  Proteccion proteccion(chicas());
  for (int i = 0; i < 3; ++i) {
    proteccion.nueva_conexion(kUna, kInicio);
  }
  CHECK(proteccion.nueva_conexion(kUna, kInicio).avisar);
  CHECK_FALSE(proteccion.nueva_conexion(kUna, kInicio).avisar);
  CHECK_FALSE(proteccion.nueva_conexion(kUna, kInicio + 500ms).avisar);
  for (int i = 0; i < 40; ++i) {
    proteccion.nueva_conexion(kUna, kInicio + 31s);
  }
  CHECK_FALSE(proteccion.nueva_conexion(kUna, kInicio + 31s).avisar);
}

TEST_CASE("La memoria de IPs tiene techo, aunque ataquen miles") {
  OpcionesProteccion opciones = chicas();
  opciones.ips_recordadas = 100;
  Proteccion proteccion(opciones);
  for (std::uint32_t ip = 1; ip <= 10000; ++ip) {
    proteccion.nueva_conexion(ip, kInicio);
    CHECK(proteccion.ips_recordadas() <= 100);
  }
}

TEST_CASE("Al olvidar IPs, primero se olvidan las quietas y no las bloqueadas") {
  OpcionesProteccion opciones = chicas();
  opciones.ips_recordadas = 3;
  Proteccion proteccion(opciones);
  for (int i = 0; i < 3; ++i) {
    proteccion.fallo(kUna, kInicio);
  }
  proteccion.nueva_conexion(2, kInicio);
  proteccion.nueva_conexion(3, kInicio);
  proteccion.nueva_conexion(4, kInicio + 120s);
  CHECK(proteccion.nueva_conexion(kUna, kInicio + 121s).decision == Decision::bloqueada);
}

TEST_CASE("El chat tiene un tope por IP, sumando todas sus conexiones") {
  OpcionesProteccion opciones = chicas();
  opciones.rafaga_chat_por_ip = 4;
  opciones.chat_por_segundo_por_ip = 1;
  Proteccion proteccion(opciones);
  for (int i = 0; i < 4; ++i) {
    CHECK(proteccion.puede_chatear(kUna, kInicio));
  }
  CHECK_FALSE(proteccion.puede_chatear(kUna, kInicio));
  CHECK(proteccion.puede_chatear(kOtra, kInicio));
  CHECK(proteccion.puede_chatear(kUna, kInicio + 1s));
  CHECK_FALSE(proteccion.puede_chatear(kUna, kInicio + 1s));
}
