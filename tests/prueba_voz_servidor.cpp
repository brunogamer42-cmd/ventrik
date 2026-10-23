#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "common/cifrado/tls.h"
#include "common/protocol/pedidos.h"
#include "server/voz.h"
#include <doctest/doctest.h>

using namespace std::chrono_literals;
using namespace ventrik::protocolo;
using ventrik::cifrado::ConfigTls;
using ventrik::cifrado::Paso;
using ventrik::cifrado::SesionTls;
using ventrik::red::Direccion;
using ventrik::servidor::Instante;
using ventrik::servidor::OpcionesVoz;
using ventrik::servidor::Voz;

namespace {
using Datagramas = std::vector<std::pair<Direccion, std::vector<std::uint8_t>>>;
const Direccion kServidor{0x0A000064, 7700};

std::array<std::uint8_t, 32> clave_de(gsl::span<const std::uint8_t> identidad) {
  std::array<std::uint8_t, 32> clave{};
  for (std::size_t i = 0; i < clave.size(); ++i) {
    clave[i] = static_cast<std::uint8_t>(identidad[i % identidad.size()] ^ 0x5A);
  }
  return clave;
}

bool derivar(gsl::span<const std::uint8_t> identidad, gsl::span<std::uint8_t> clave) {
  const auto calculada = clave_de(identidad);
  std::copy(calculada.begin(), calculada.end(), clave.begin());
  return true;
}

struct Banco {
  explicit Banco(OpcionesVoz opciones = {})
      : voz(opciones, [this](const Direccion& a, gsl::span<const std::uint8_t> d) {
          hacia_programas.emplace_back(a, std::vector<std::uint8_t>(d.begin(), d.end()));
        }) {}

  Instante ahora = Instante{} + 1000s;
  Datagramas hacia_programas;
  Datagramas hacia_servidor;
  std::vector<std::uint32_t> atadas;
  Voz voz;

  void al_servidor() {
    for (auto& [origen, datos] : std::exchange(hacia_servidor, {})) {
      if (const auto atada = voz.recibir(origen, datos, ahora)) {
        atadas.push_back(*atada);
      }
    }
  }
};

struct Programa {
  Programa(Banco& banco, Direccion propia, const ClaveVoz& clave)
      : banco_(banco),
        propia(propia),
        config_(ConfigTls::para_voz_programa(clave.identidad, clave_de(clave.identidad))),
        enviar_([this](const Direccion&, gsl::span<const std::uint8_t> d) {
          banco_.hacia_servidor.emplace_back(this->propia, std::vector<std::uint8_t>(d.begin(), d.end()));
        }),
        sesion_(std::make_unique<SesionTls>(*config_, enviar_, kServidor)) {}

  void avanzar() {
    if (!listo) {
      listo = sesion_->apreton() == Paso::listo;
    }
  }

  void llegar(gsl::span<const std::uint8_t> datos) {
    sesion_->entregar(datos);
    if (!listo) {
      avanzar();
      return;
    }
    std::array<std::uint8_t, kMaximoDatagrama> leido{};
    std::size_t largo = 0;
    while (sesion_->leer(leido, largo) == Paso::listo) {
      const auto marco = separar_datagrama(gsl::span<const std::uint8_t>(leido).first(largo));
      REQUIRE(marco);
      if (const auto voz = leer_mensaje<VozBajada>(marco->cuerpo)) {
        recibidas.push_back(*voz);
      } else if (const auto pong = leer_mensaje<Pong>(marco->cuerpo)) {
        pongs.push_back(pong->marca);
      }
    }
  }

  template <typename Mensaje>
  void mandar(const Mensaje& mensaje) {
    std::vector<std::uint8_t> datagrama;
    REQUIRE(armar_datagrama(mensaje, datagrama));
    std::size_t escritos = 0;
    REQUIRE(sesion_->escribir(datagrama, escritos) == Paso::listo);
  }

  Banco& banco_;
  Direccion propia;
  std::unique_ptr<ConfigTls> config_;
  ventrik::cifrado::EnviarDatagrama enviar_;
  std::unique_ptr<SesionTls> sesion_;
  bool listo = false;
  std::vector<VozBajada> recibidas;
  std::vector<std::uint64_t> pongs;
};

void bombear(Banco& banco, std::vector<Programa*> programas, int vueltas = 20) {
  for (int vuelta = 0; vuelta < vueltas; ++vuelta) {
    for (auto* programa : programas) {
      programa->avanzar();
    }
    banco.al_servidor();
    for (auto& [destino, datos] : std::exchange(banco.hacia_programas, {})) {
      for (auto* programa : programas) {
        if (programa->propia == destino) {
          programa->llegar(datos);
        }
      }
    }
  }
}

ClaveVoz alta(Banco& banco, std::uint32_t id, std::uint32_t ip, std::uint32_t canal) {
  const auto clave = banco.voz.alta(id, ip, canal, banco.ahora, derivar);
  REQUIRE(clave);
  return *clave;
}

void hasta_pendiente(Banco& banco, Programa& programa) {
  programa.avanzar();
  banco.al_servidor();
  for (auto& [destino, datos] : std::exchange(banco.hacia_programas, {})) {
    programa.llegar(datos);
  }
  banco.al_servidor();
  banco.hacia_programas.clear();
}

const std::vector<std::uint8_t> kOpus = {1, 2, 3};
}

TEST_CASE("El servidor ata la voz a la conexión y la reenvía solo a los demás del canal") {
  Banco banco;
  REQUIRE(banco.voz.ok());
  Programa a(banco, {0x0A000001, 5000}, alta(banco, 1, 0x0A000001, 1));
  Programa b(banco, {0x0A000002, 5000}, alta(banco, 2, 0x0A000002, 1));
  Programa c(banco, {0x0A000003, 5000}, alta(banco, 3, 0x0A000003, 2));
  bombear(banco, {&a, &b, &c});
  std::sort(banco.atadas.begin(), banco.atadas.end());
  REQUIRE(banco.atadas == std::vector<std::uint32_t>{1, 2, 3});
  a.mandar(VozSubida{7, kOpus});
  a.mandar(Ping{55});
  bombear(banco, {&a, &b, &c});
  REQUIRE(b.recibidas.size() == 1);
  CHECK(b.recibidas[0].hablante == 1);
  CHECK(b.recibidas[0].numero == 7);
  CHECK(b.recibidas[0].opus == kOpus);
  CHECK(a.recibidas.empty());
  CHECK(c.recibidas.empty());
  CHECK(a.pongs == std::vector<std::uint64_t>{55});
  banco.voz.mover(3, 1);
  banco.voz.baja(2);
  CHECK_FALSE(banco.voz.tiene_voz(2));
  a.mandar(VozSubida{8, kOpus});
  bombear(banco, {&a, &b, &c});
  CHECK(b.recibidas.size() == 1);
  REQUIRE(c.recibidas.size() == 1);
  CHECK(c.recibidas[0].hablante == 1);
}

TEST_CASE("Una IP sin conexión no recibe respuesta y otra IP no puede usar una clave ajena") {
  Banco banco;
  const auto clave_a = alta(banco, 1, 0x0A000001, 1);
  alta(banco, 4, 0x0A000004, 1);
  Programa extrano(banco, {0x09090909, 5000}, clave_a);
  extrano.avanzar();
  banco.al_servidor();
  CHECK(banco.hacia_programas.empty());
  Programa vecino(banco, {0x0A000004, 5000}, clave_a);
  bombear(banco, {&vecino});
  CHECK(banco.atadas.empty());
  CHECK_FALSE(banco.voz.tiene_voz(1));
  CHECK_FALSE(banco.voz.tiene_voz(4));
}

TEST_CASE("Hay tope de saludos a medio hacer por IP, y vencen") {
  Banco banco;
  const auto clave = alta(banco, 1, 0x0A000001, 1);
  std::vector<std::unique_ptr<Programa>> programas;
  for (std::uint16_t puerto = 1; puerto <= 6; ++puerto) {
    programas.push_back(std::make_unique<Programa>(banco, Direccion{0x0A000001, puerto}, clave));
    hasta_pendiente(banco, *programas.back());
  }
  CHECK(banco.voz.pendientes() == 4);
  banco.ahora += 11s;
  banco.voz.revisar(banco.ahora);
  CHECK(banco.voz.pendientes() == 0);
}

TEST_CASE("Hay tope de paquetes de voz por persona") {
  Banco banco;
  Programa a(banco, {0x0A000001, 5000}, alta(banco, 1, 0x0A000001, 1));
  Programa b(banco, {0x0A000002, 5000}, alta(banco, 2, 0x0A000002, 1));
  bombear(banco, {&a, &b});
  for (std::uint16_t numero = 0; numero < 100; ++numero) {
    a.mandar(VozSubida{numero, kOpus});
  }
  bombear(banco, {&a, &b});
  CHECK(b.recibidas.size() == 25);
}

TEST_CASE("La clave rota, la vieja deja de servir y la sesión quieta se cierra") {
  OpcionesVoz opciones;
  opciones.rotacion = 10s;
  Banco banco(opciones);
  const auto vieja = alta(banco, 1, 0x0A000001, 1);
  Programa a(banco, {0x0A000001, 5000}, vieja);
  bombear(banco, {&a});
  REQUIRE(banco.voz.tiene_voz(1));
  banco.ahora += 12s;
  REQUIRE(banco.voz.revisar(banco.ahora) == std::vector<std::uint32_t>{1});
  const auto nueva = banco.voz.renovar(1, banco.ahora, derivar);
  REQUIRE(nueva);
  CHECK(nueva->identidad != vieja.identidad);
  CHECK(banco.voz.tiene_voz(1));
  banco.atadas.clear();
  Programa con_la_vieja(banco, {0x0A000001, 5001}, vieja);
  bombear(banco, {&con_la_vieja});
  CHECK(banco.atadas.empty());
  Programa con_la_nueva(banco, {0x0A000001, 5002}, *nueva);
  bombear(banco, {&con_la_nueva});
  CHECK(banco.atadas == std::vector<std::uint32_t>{1});
  banco.ahora += 61s;
  banco.voz.revisar(banco.ahora);
  CHECK_FALSE(banco.voz.tiene_voz(1));
}

TEST_CASE("Un saludo nuevo desde la misma dirección reemplaza la sesión vieja") {
  Banco banco;
  const auto clave = alta(banco, 1, 0x0A000001, 1);
  Programa a(banco, {0x0A000001, 5000}, clave);
  bombear(banco, {&a});
  REQUIRE(banco.atadas == std::vector<std::uint32_t>{1});
  Programa reiniciado(banco, {0x0A000001, 5000}, clave);
  const auto hasta = std::chrono::steady_clock::now() + 5s;
  while (banco.atadas.size() < 2 && std::chrono::steady_clock::now() < hasta) {
    bombear(banco, {&reiniciado}, 1);
    std::this_thread::sleep_for(20ms);
  }
  CHECK(banco.atadas == std::vector<std::uint32_t>{1, 1});
  CHECK(banco.voz.tiene_voz(1));
}

TEST_CASE("Un saludo falso desde la dirección de alguien no le corta la voz") {
  Banco banco;
  Programa a(banco, {0x0A000001, 5000}, alta(banco, 1, 0x0A000001, 1));
  Programa b(banco, {0x0A000002, 5000}, alta(banco, 2, 0x0A000002, 1));
  bombear(banco, {&a, &b});
  REQUIRE(banco.voz.tiene_voz(1));
  const std::vector<std::vector<std::uint8_t>> falsos = {
      {0x16, 0xFE, 0xFD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {0x16, 0xFF, 0xFF, 0, 0, 0x13, 0, 0, 5, 0xFF, 0, 0, 0x12, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7,
       0x0A, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0x32},
      {0x16, 0xFF, 0xFF, 0, 0, 0, 0, 0, 3, 0x5D, 0, 0, 0, 0, 0, 0, 0x5D, 0, 0, 0, 0, 1, 0, 0, 0x5D,
       0, 0, 0, 0, 0, 0, 0}};
  for (const auto& falso : falsos) {
    banco.voz.recibir(a.propia, falso, banco.ahora);
    CHECK(banco.voz.tiene_voz(1));
  }
  a.mandar(VozSubida{1, kOpus});
  bombear(banco, {&a, &b});
  CHECK(b.recibidas.size() == 1);
}
