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

ventrik::cliente::OpcionesConexion desde(unsigned i, const ventrik::protocolo::Hola& hola) {
  ventrik::cliente::OpcionesConexion opciones;
  opciones.hola = hola;
  opciones.origen = origen_n(i);
  return opciones;
}

class Liviana {
 public:
  Liviana(const ServidorExterno& servidor, unsigned i)
      : resultado_(ventrik::cliente::conectar(servidor.direccion(), servidor.huella(),
                                              {"carga" + std::to_string(i), {}},
                                              desde(i, hola_))) {}
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
  static const ventrik::protocolo::Hola hola_;
  ventrik::cliente::ResultadoConexion resultado_;
};
const ventrik::protocolo::Hola Liviana::hola_{ventrik::kProtocoloMinimo, ventrik::kProtocoloMaximo,
                                              ventrik::kVersionPrograma, 0};

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

  Cliente canario(servidor.direccion(), servidor.huella(), "Canario", servidor.codigo());
  canario.entrar();
  canario.ordenar(CrearCanal{"canario", TipoCanal::texto, false});
  const auto creado = canario.esperar<CanalCreado>();
  REQUIRE(creado);
  const std::uint32_t canal = creado->canal.id;
  canario.ordenar(MirarCanal{canal});
  canario.ordenar(PedirHistorial{canal, 0});
  REQUIRE(canario.esperar<Historial>());

  static std::atomic<int> numero{0};
  const auto ida_y_vuelta = [&](std::chrono::milliseconds limite) -> std::optional<long long> {
    const std::string texto = "ping" + std::to_string(++numero);
    const auto desde = std::chrono::steady_clock::now();
    canario.ordenar(EnviarChat{canal, texto});
    const auto eco = canario.esperar<MensajeChat>([&](const MensajeChat& m) { return m.texto == texto; }, limite);
    if (!eco) {
      return std::nullopt;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - desde).count();
  };

  const auto base = ida_y_vuelta(5s);
  REQUIRE(base);
  MESSAGE("canario en reposo: " << *base << " ms; servidor " << servidor.proceso().memoria_kb() / 1024
                                << " MB, " << servidor.proceso().manijas() << " manijas");
  const std::size_t memoria_base = servidor.proceso().memoria_kb();
  const std::uint32_t manijas_base = servidor.proceso().manijas();

  SUBCASE("carga: muchas conexiones a la vez, el canario sigue chateando") {
    std::vector<std::unique_ptr<Liviana>> lote;
    int conectadas = 0;
    for (int i = 0; i < conexiones; ++i) {
      auto liviana = std::make_unique<Liviana>(servidor, static_cast<unsigned>(i));
      if (liviana->conectada()) {
        ++conectadas;
      }
      lote.push_back(std::move(liviana));
    }
    for (auto& liviana : lote) {
      liviana->drenar();
    }
    const auto durante = ida_y_vuelta(5s);
    MESSAGE("con " << conectadas << "/" << conexiones << " conexiones: canario " << (durante ? *durante : -1)
                   << " ms; servidor " << servidor.proceso().memoria_kb() / 1024 << " MB, "
                   << servidor.proceso().manijas() << " manijas");
    CHECK(conectadas >= conexiones / 2);
    CHECK(durante);
    CHECK(servidor.proceso().vivo());
  }

  SUBCASE("inundacion: conexiones TCP que no terminan el saludo") {
    std::vector<ventrik::red::Socket> crudas;
    for (int i = 0; i < conexiones; ++i) {
      const ventrik::red::Direccion origen = origen_n(static_cast<unsigned>(1000 + i));
      ventrik::red::Socket cruda = ventrik::red::conectar(servidor.direccion(), 2s, nullptr, &origen);
      if (cruda.valido()) {
        crudas.push_back(std::move(cruda));
      }
    }
    const auto durante = ida_y_vuelta(5s);
    MESSAGE("con " << crudas.size() << " conexiones a medias: canario " << (durante ? *durante : -1) << " ms");
    CHECK(durante);
    CHECK(servidor.proceso().vivo());
  }

  SUBCASE("basura: un cliente conectado manda un marco imposible y lo echan") {
    Liviana atacante(servidor, 5000);
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
    CHECK(ida_y_vuelta(5s));
    CHECK(servidor.proceso().vivo());
  }

  SUBCASE("entrar y salir muchas veces no deja fugas") {
    int reconectadas = 0;
    for (int i = 0; i < vueltas_churn; ++i) {
      Liviana ida(servidor, static_cast<unsigned>(6000 + (i % 200)));
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
    CHECK(ida_y_vuelta(5s));
    CHECK(manijas_fin < manijas_base + 128);
    CHECK(servidor.proceso().vivo());
  }

  CHECK(servidor.cerrar() == DWORD{0});
}
