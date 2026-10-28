#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

#include "client/conexion.h"
#include "cliente_de_prueba.h"
#include "common/protocol/pedidos.h"
#include "common/red/red.h"
#include "proceso_de_prueba.h"
#include "server/base.h"
#include "server/contrasena.h"
#include "servidor_externo.h"
#include <doctest/doctest.h>

using namespace prueba;
using namespace std::chrono_literals;
using namespace ventrik::protocolo;
using ventrik::cliente::ConexionAbierta;
using ventrik::cliente::EstadoConexion;

namespace {

int env_num(const wchar_t* nombre, int por_defecto) {
  const std::wstring valor = variable(nombre);
  return valor.empty() ? por_defecto : _wtoi(valor.c_str());
}

ventrik::red::Direccion origen_n(unsigned i) { return {0x7F000100u + i, 0}; }

class Liviana {
 public:
  Liviana(const ServidorExterno& servidor, unsigned i, std::optional<ventrik::red::Direccion> origen)
      : resultado_(ventrik::cliente::conectar(servidor.direccion(), servidor.huella(),
                                              prueba::presentacion("carga" + std::to_string(i)), opciones(origen))) {}
  bool conectada() const { return resultado_.estado == EstadoConexion::conectado && resultado_.abierta; }

  void drenar() {
    if (!resultado_.abierta) {
      return;
    }
    std::array<std::uint8_t, 4096> buffer{};
    std::size_t leidos = 0;
    for (int i = 0; i < 16 && resultado_.abierta->tls().leer(buffer, leidos) ==
                                  ventrik::cifrado::Paso::listo && leidos > 0; ++i) {
    }
  }

  void mandar(gsl::span<const std::uint8_t> datos) {
    if (!resultado_.abierta) {
      return;
    }
    std::size_t escritos = 0;
    resultado_.abierta->tls().escribir(datos, escritos);
  }

  bool viva() {
    if (!resultado_.abierta) {
      return false;
    }
    std::array<std::uint8_t, 64> buffer{};
    std::size_t leidos = 0;
    return resultado_.abierta->tls().leer(buffer, leidos) != ventrik::cifrado::Paso::cerrado;
  }

 private:
  static ventrik::cliente::OpcionesConexion opciones(std::optional<ventrik::red::Direccion> origen) {
    ventrik::cliente::OpcionesConexion opciones;
    opciones.origen = origen;
    opciones.tiempo_para_conectar = 4s;
    opciones.tiempo_para_saludar = 4s;
    return opciones;
  }
  ventrik::cliente::ResultadoConexion resultado_;
};

class Canario {
 public:
  explicit Canario(const ServidorExterno& servidor)
      : cliente_(servidor.direccion(), servidor.huella(), "Canario", servidor.codigo()) {
    cliente_.entrar();
    cliente_.ordenar(CrearCanal{"canario", TipoCanal::texto, false});
    const auto creado = cliente_.esperar<CanalCreado>();
    REQUIRE(creado);
    canal_ = creado->canal.id;
    cliente_.ordenar(MirarCanal{canal_});
    cliente_.ordenar(PedirHistorial{canal_, 0});
    REQUIRE(cliente_.esperar<Historial>());
  }

  std::optional<long long> ida_y_vuelta(std::chrono::milliseconds limite) {
    const std::string texto = "ping" + std::to_string(++numero_);
    const auto desde = std::chrono::steady_clock::now();
    cliente_.ordenar(EnviarChat{canal_, texto});
    const auto eco = cliente_.esperar<MensajeChat>([&](const MensajeChat& m) { return m.texto == texto; }, limite);
    if (!eco) {
      return std::nullopt;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - desde).count();
  }

 private:
  Cliente cliente_;
  std::uint32_t canal_ = 0;
  int numero_ = 0;
};

}

TEST_CASE("estres: el servidor aguanta carga y ataques, y el canario sigue atendido" * doctest::skip()) {
  const std::wstring servidor_exe = variable(L"VENTRIK_SERVIDOR");
  const std::wstring carpeta_base = variable(L"VENTRIK_CARPETA");
  REQUIRE(!servidor_exe.empty());
  REQUIRE(!carpeta_base.empty());
  const int conexiones = env_num(L"VENTRIK_ESTRES_CONEXIONES", 32);
  const int vueltas_churn = env_num(L"VENTRIK_ESTRES_CHURN", 60);
  static std::atomic<int> corrida{0};
  const std::wstring carpeta = carpeta_base + L"\\r" + std::to_wstring(++corrida);
  REQUIRE(ventrik::crear_carpetas(carpeta));
  ServidorExterno servidor(servidor_exe, carpeta);
  Canario canario(servidor);

  const auto base = canario.ida_y_vuelta(5s);
  REQUIRE(base);
  MESSAGE("canario en reposo: " << *base << " ms; servidor " << servidor.proceso().memoria_kb() / 1024
                                << " MB, " << servidor.proceso().manijas() << " manijas");
  const std::size_t memoria_base = servidor.proceso().memoria_kb();
  const std::uint32_t manijas_base = servidor.proceso().manijas();

  // Cada usuario simulado sale de una IP 127.0.x.y distinta para esquivar el tope por IP. XP deja atar
  // esas IP pero no conectar desde ellas, así que la sonda intenta conectar de verdad al servidor desde
  // una; si no puede (XP), todos salen de 127.0.0.1 y el tope por IP limita cuántos entran.
  const ventrik::red::Direccion sonda = origen_n(1);
  const bool multi_ip = ventrik::red::conectar(servidor.direccion(), 2s, nullptr, &sonda).valido();
  MESSAGE("IP de origen distinta por conexion, 1=si 127.0.x.y 0=todas 127.0.0.1: " << (multi_ip ? 1 : 0));
  const auto origen_de = [&](unsigned i) -> std::optional<ventrik::red::Direccion> {
    return multi_ip ? std::optional<ventrik::red::Direccion>(origen_n(i)) : std::nullopt;
  };

  SUBCASE("carga: muchas conexiones a la vez, el canario sigue chateando") {
    std::vector<std::unique_ptr<Liviana>> lote;
    int conectadas = 0;
    for (int i = 0; i < conexiones; ++i) {
      auto liviana = std::make_unique<Liviana>(servidor, static_cast<unsigned>(i), origen_de(static_cast<unsigned>(i)));
      if (liviana->conectada()) {
        ++conectadas;
      }
      lote.push_back(std::move(liviana));
    }
    for (auto& liviana : lote) {
      liviana->drenar();
    }
    const auto durante = canario.ida_y_vuelta(5s);
    MESSAGE("con " << conectadas << "/" << conexiones << " conexiones: canario " << (durante ? *durante : -1)
                   << " ms; servidor " << servidor.proceso().memoria_kb() / 1024 << " MB, "
                   << servidor.proceso().manijas() << " manijas");
    CHECK(conectadas >= (multi_ip ? conexiones / 2 : 1));
    CHECK(durante);
    CHECK(servidor.proceso().vivo());
  }

  SUBCASE("inundacion: conexiones TCP que no terminan el saludo") {
    std::vector<ventrik::red::Socket> crudas;
    for (int i = 0; i < conexiones; ++i) {
      const auto origen = origen_de(static_cast<unsigned>(1000 + i));
      ventrik::red::Socket cruda =
          ventrik::red::conectar(servidor.direccion(), 2s, nullptr, origen ? &*origen : nullptr);
      if (cruda.valido()) {
        crudas.push_back(std::move(cruda));
      }
    }
    const auto durante = canario.ida_y_vuelta(5s);
    MESSAGE("con " << crudas.size() << " conexiones a medias: canario " << (durante ? *durante : -1) << " ms");
    CHECK(durante);
    CHECK(servidor.proceso().vivo());
  }

  SUBCASE("basura: un cliente conectado manda un marco imposible y lo echan") {
    Liviana atacante(servidor, 5000, origen_de(5000));
    REQUIRE(atacante.conectada());
    const std::vector<std::uint8_t> basura(100000, 0xFF);
    atacante.mandar(basura);
    bool cerrada = false;
    for (int i = 0; i < 50 && !cerrada; ++i) {
      std::this_thread::sleep_for(100ms);
      cerrada = !atacante.viva();
    }
    MESSAGE("al atacante lo echaron: " << (cerrada ? "SI" : "NO"));
    CHECK(cerrada);
    CHECK(canario.ida_y_vuelta(5s));
    CHECK(servidor.proceso().vivo());
  }

  SUBCASE("entrar y salir muchas veces no deja fugas") {
    int reconectadas = 0;
    for (int i = 0; i < vueltas_churn; ++i) {
      const unsigned cual = static_cast<unsigned>(6000 + (i % 200));
      Liviana ida(servidor, cual, origen_de(cual));
      if (ida.conectada()) {
        ++reconectadas;
      }
    }
    std::this_thread::sleep_for(1s);
    const std::size_t memoria_fin = servidor.proceso().memoria_kb();
    const std::uint32_t manijas_fin = servidor.proceso().manijas();
    MESSAGE("despues de " << vueltas_churn << " idas y vueltas (" << reconectadas
                          << " reconectaron): " << memoria_base / 1024 << " -> " << memoria_fin / 1024 << " MB, "
                          << manijas_base << " -> " << manijas_fin << " manijas");
    CHECK(canario.ida_y_vuelta(5s));
    CHECK(manijas_fin < manijas_base + 128);
    CHECK(servidor.proceso().vivo());
  }

  CHECK(servidor.cerrar() == DWORD{0});
}

TEST_CASE("estres: muchas contraseñas equivocadas a la vez no frenan al canario" * doctest::skip()) {
  const std::wstring servidor_exe = variable(L"VENTRIK_SERVIDOR");
  const std::wstring carpeta_base = variable(L"VENTRIK_CARPETA");
  REQUIRE(!servidor_exe.empty());
  REQUIRE(!carpeta_base.empty());
  const std::wstring carpeta = carpeta_base + L"\\contrasenas";
  REQUIRE(ventrik::crear_carpetas(carpeta));
  for (const auto* archivo : {L"ventrik.db", L"ventrik.db-wal", L"ventrik.db-shm", L"codigo_dueno.txt"}) {
    DeleteFileW((carpeta + L"\\" + archivo).c_str());
  }
  {
    const auto contrasena = ventrik::servidor::guardar_contrasena("sésamo");
    REQUIRE(contrasena);
    const auto base = ventrik::servidor::Base::abrir(carpeta + L"\\ventrik.db", "General");
    REQUIRE(base);
    REQUIRE(base->guardar_texto("contrasena", *contrasena));
  }
  ServidorExterno servidor(servidor_exe, carpeta);
  Canario canario(servidor);
  const auto base = canario.ida_y_vuelta(5s);
  REQUIRE(base);

  const ventrik::red::Direccion sonda = origen_n(1);
  const bool multi_ip = ventrik::red::conectar(servidor.direccion(), 2s, nullptr, &sonda).valido();
  std::atomic<bool> seguir{true};
  std::atomic<int> incorrectas{0};
  std::atomic<int> ocupadas{0};
  std::atomic<int> otras{0};
  std::vector<std::thread> atacantes;
  for (unsigned i = 0; i < 50; ++i) {
    atacantes.emplace_back([&, i] {
      auto quien = prueba::presentacion("intruso" + std::to_string(i));
      quien.contrasena = secreto_de("equivocada");
      ventrik::cliente::OpcionesConexion opciones;
      opciones.origen = multi_ip ? std::optional<ventrik::red::Direccion>(origen_n(3000 + i)) : std::nullopt;
      opciones.tiempo_para_conectar = 4s;
      opciones.tiempo_para_saludar = 4s;
      while (seguir) {
        const auto estado =
            ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), quien, opciones).estado;
        ++(estado == EstadoConexion::contrasena_incorrecta ? incorrectas
           : estado == EstadoConexion::ocupado             ? ocupadas
                                                           : otras);
        std::this_thread::sleep_for(100ms);
      }
    });
  }
  long long peor = 0;
  bool siempre = true;
  for (int i = 0; i < 10; ++i) {
    std::this_thread::sleep_for(300ms);
    const auto durante = canario.ida_y_vuelta(5s);
    siempre = siempre && durante.has_value();
    peor = std::max(peor, durante.value_or(0));
  }
  seguir = false;
  for (auto& atacante : atacantes) {
    atacante.join();
  }
  MESSAGE("50 intrusos con contraseña equivocada: canario " << *base << " ms en reposo, " << peor << " ms el peor; "
          << incorrectas.load() << " equivocadas, " << ocupadas.load() << " ocupado, " << otras.load() << " otras");
  CHECK(siempre);
  CHECK(incorrectas.load() + ocupadas.load() > 0);
  CHECK(servidor.proceso().vivo());
  CHECK(servidor.cerrar() == DWORD{0});
}
