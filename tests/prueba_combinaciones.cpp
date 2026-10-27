#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

#include "client/motor_voz.h"
#include "cliente_de_prueba.h"
#include "common/utf.h"
#include "servidor_externo.h"
#include "sonido_de_prueba.h"
#include <doctest/doctest.h>

using namespace prueba;
using namespace ventrik::protocolo;
using ventrik::cliente::EstadoVoz;
using ventrik::cliente::MotorVoz;
using ventrik::cliente::PuenteVoz;
using ventrik::servidor::kCanalGeneral;

namespace {

struct Voz {
  std::shared_ptr<PuenteVoz> puente = std::make_shared<PuenteVoz>();
  MotorVoz motor{*puente, {}};
  std::vector<std::int16_t> escuchado;
  std::set<std::uint32_t> hablaron;
  std::ptrdiff_t a_la_vez = 0;
};

void hablar_un_rato(const std::vector<Voz*>& voces, std::chrono::milliseconds cuanto) {
  std::vector<std::int16_t> sonido(480);
  std::vector<std::int16_t> salida(480);
  std::size_t muestra = 0;
  const auto hasta = std::chrono::steady_clock::now() + cuanto;
  while (std::chrono::steady_clock::now() < hasta) {
    tono(sonido, muestra, 48000);
    for (Voz* voz : voces) {
      voz->motor.procesar(sonido, salida);
      voz->escuchado.insert(voz->escuchado.end(), salida.begin(), salida.end());
      const auto ids = voz->puente->quienes();
      voz->hablaron.insert(ids.begin(), ids.end());
      voz->hablaron.erase(0);
      voz->a_la_vez = std::max(voz->a_la_vez, std::count_if(ids.begin(), ids.end(), [](std::uint32_t id) {
                                 return id != 0;
                               }));
    }
    std::this_thread::sleep_for(10ms);
  }
}

void ver_en(Cliente& cliente, std::uint32_t id, std::uint32_t canal) {
  REQUIRE(cliente.esperar<UsuarioCambioCanal>(
      [&](const UsuarioCambioCanal& cambio) { return cambio.id == id && cambio.canal == canal; }));
}

void mirar(Cliente& cliente, std::uint32_t canal) {
  cliente.ordenar(MirarCanal{canal});
  cliente.ordenar(PedirHistorial{canal, 0});
  REQUIRE(cliente.esperar<Historial>());
}

}

TEST_CASE("combinaciones: servidor y programa de verdad, de la misma versión o de la otra" * doctest::skip()) {
  const std::wstring servidor_exe = variable(L"VENTRIK_SERVIDOR");
  const std::wstring programa_exe = variable(L"VENTRIK_PROGRAMA");
  const std::wstring carpeta = variable(L"VENTRIK_CARPETA");
  REQUIRE(!servidor_exe.empty());
  REQUIRE(!programa_exe.empty());
  REQUIRE(!carpeta.empty());
  REQUIRE(ventrik::crear_carpetas(carpeta + L"\\servidor"));
  REQUIRE(ventrik::crear_carpetas(carpeta + L"\\programa"));
  ServidorExterno servidor(servidor_exe, carpeta + L"\\servidor");

  Cliente ana(servidor.direccion(), servidor.huella(), "Ana", servidor.codigo());
  Cliente beto(servidor.direccion(), servidor.huella(), "Beto");
  Cliente carla(servidor.direccion(), servidor.huella(), "Carla");
  const auto ana_id = ana.entrar().tu_id;
  const auto beto_id = beto.entrar().tu_id;
  const auto carla_id = carla.entrar().tu_id;

  ana.ordenar(CrearCanal{"charla", TipoCanal::texto, false});
  const auto charla = beto.esperar<CanalCreado>();
  REQUIRE(charla);
  const std::uint32_t canal = charla->canal.id;
  mirar(ana, canal);
  mirar(beto, canal);
  const std::string saludo = "hola \xF0\x9F\x98\x80";
  ana.ordenar(EnviarChat{canal, saludo});
  const auto en_beto = beto.esperar<MensajeChat>([&](const MensajeChat& m) { return m.autor_id == ana_id; });
  REQUIRE(en_beto);
  CHECK(en_beto->texto == saludo);
  CHECK(en_beto->canal == canal);
  beto.ordenar(EnviarChat{canal, "chau"});
  CHECK(ana.esperar<MensajeChat>([&](const MensajeChat& m) { return m.autor_id == beto_id && m.texto == "chau"; }));
  ana.ordenar(PedirHistorial{canal, 0});
  const auto historial = ana.esperar<Historial>();
  REQUIRE(historial);
  CHECK(historial->mensajes.size() == 2);

  for (Cliente* cliente : {&ana, &beto, &carla}) {
    cliente->ordenar(EntrarACanal{kCanalGeneral});
  }
  for (Cliente* cliente : {&ana, &beto, &carla}) {
    ver_en(*cliente, ana_id, kCanalGeneral);
    ver_en(*cliente, beto_id, kCanalGeneral);
    ver_en(*cliente, carla_id, kCanalGeneral);
  }
  {
    Voz de_ana;
    Voz de_beto;
    Voz de_carla;
    REQUIRE(de_ana.motor.ok());
    REQUIRE(de_beto.motor.ok());
    REQUIRE(de_carla.motor.ok());
    ana.voz(de_ana.puente);
    beto.voz(de_beto.puente);
    carla.voz(de_carla.puente);
    REQUIRE(esperar_que(
        [&] {
          return de_ana.puente->estado == EstadoVoz::lista && de_beto.puente->estado == EstadoVoz::lista &&
                 de_carla.puente->estado == EstadoVoz::lista;
        },
        10s));
    de_ana.motor.transmitir(true);
    de_beto.motor.transmitir(true);
    de_carla.motor.transmitir(true);
    hablar_un_rato({&de_ana, &de_beto, &de_carla}, 2s);
    CHECK(energia(de_ana.escuchado, de_ana.escuchado.size() / 2) > 1e6);
    CHECK(energia(de_beto.escuchado, de_beto.escuchado.size() / 2) > 1e6);
    CHECK(de_ana.hablaron == std::set<std::uint32_t>{beto_id, carla_id});
    CHECK(de_beto.hablaron == std::set<std::uint32_t>{ana_id, carla_id});
    CHECK(de_ana.a_la_vez == 2);
    de_carla.motor.transmitir(false);
    carla.ordenar(EntrarACanal{0});
    carla.voz(nullptr);
    ver_en(ana, carla_id, 0);

    beto.ordenar(Silenciar{1});
    REQUIRE(ana.esperar<UsuarioSilencio>([&](const UsuarioSilencio& s) { return s.id == beto_id && s.silencio == 1; }));
    hablar_un_rato({&de_ana, &de_beto}, 300ms);
    const std::uint32_t llegados_a_ana = de_ana.puente->recibidos;
    const std::uint32_t llegados_a_beto = de_beto.puente->recibidos;
    hablar_un_rato({&de_ana, &de_beto}, 1s);
    CHECK(de_ana.puente->recibidos == llegados_a_ana);
    CHECK(de_beto.puente->recibidos > llegados_a_beto);
    beto.ordenar(Silenciar{0});
    REQUIRE(ana.esperar<UsuarioSilencio>([&](const UsuarioSilencio& s) { return s.id == beto_id && s.silencio == 0; }));

    for (Cliente* cliente : {&ana, &beto}) {
      cliente->ordenar(EntrarACanal{0});
      cliente->voz(nullptr);
    }
    ver_en(ana, beto_id, 0);
    ver_en(beto, ana_id, 0);
  }
  for (Cliente* cliente : {&ana, &beto}) {
    cliente->ordenar(EntrarACanal{kCanalGeneral});
  }
  ver_en(ana, beto_id, kCanalGeneral);
  ver_en(beto, ana_id, kCanalGeneral);
  const auto otra_de_ana = std::make_shared<PuenteVoz>();
  const auto otra_de_beto = std::make_shared<PuenteVoz>();
  ana.voz(otra_de_ana);
  beto.voz(otra_de_beto);
  REQUIRE(esperar_que(
      [&] { return otra_de_ana->estado == EstadoVoz::lista && otra_de_beto->estado == EstadoVoz::lista; }, 10s));
  CHECK(llega_alguno(*otra_de_ana, *otra_de_beto));

  const std::wstring programa = carpeta + L"\\programa";
  const std::wstring registro = programa + L"\\registro.csv";
  Proceso ventana(programa_exe, {L"--carpeta", programa, L"--entrar",
                                 ventrik::a_utf16(ventrik::red::a_texto(servidor.direccion())),
                                 ventrik::a_utf16(servidor.huella().a_texto()), L"Ventana", L"--registro", registro});
  const auto entro = ana.esperar<UsuarioEntro>([](const UsuarioEntro& u) { return u.usuario.nombre == "Ventana"; }, 30s);
  REQUIRE(entro);
  const HWND hwnd = ventana.ventana(10s);
  REQUIRE(hwnd);
  REQUIRE(esperar_que(
      [&] {
        const std::string anotado = leer_todo(registro);
        return std::count(anotado.begin(), anotado.end(), '\n') >= 3;
      },
      10s));
  escribir(hwnd, L"hola desde la ventana");
  enter(hwnd);
  CHECK(ana.esperar<MensajeChat>(
      [&](const MensajeChat& m) { return m.autor_id == entro->usuario.id && m.texto == "hola desde la ventana"; },
      10s));

  const auto notificaciones = [&] {
    std::string anotado = leer_todo(registro);
    while (!anotado.empty() && (anotado.back() == '\n' || anotado.back() == '\r')) {
      anotado.pop_back();
    }
    const std::string linea = anotado.substr(anotado.find_last_of('\n') + 1);
    return std::strtoul(linea.c_str() + linea.find_last_of(',') + 1, nullptr, 10);
  };
  PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
  REQUIRE(esperar_que([&] { return IsIconic(hwnd) != FALSE; }));
  ana.ordenar(EnviarChat{canal, "aviso para la ventana"});
  const HWND aviso = ventana.ventana(10s, L"VentrikAviso");
  REQUIRE(aviso != nullptr);
  CHECK(esperar_que([&] { return notificaciones() >= 1; }));
  PostMessageW(aviso, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(20, 20));
  PostMessageW(aviso, WM_LBUTTONUP, 0, MAKELPARAM(20, 20));
  CHECK(esperar_que([&] { return IsIconic(hwnd) == FALSE; }));
  PostMessageW(hwnd, WM_CLOSE, 0, 0);
  CHECK(ventana.esperar_salida(15s) == DWORD{0});
  CHECK(ana.esperar<UsuarioSalio>([&](const UsuarioSalio& u) { return u.id == entro->usuario.id; }));
  CHECK(servidor.cerrar() == DWORD{0});
}
