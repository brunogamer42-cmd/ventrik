#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "common/protocol/pedidos.h"
#include "common/red/red.h"

struct sqlite3;
struct sqlite3_stmt;

namespace ventrik::servidor {
enum class Retencion : std::uint8_t { siempre = 0, dias_30 = 1, dias_7 = 2, no_guardar = 3 };

using protocolo::kCanalGeneral;

class Base {
 public:
  static std::unique_ptr<Base> abrir(const std::wstring& archivo, const std::string& nombre_general);
  ~Base();
  Base(const Base&) = delete;
  Base& operator=(const Base&) = delete;

  std::vector<protocolo::CanalInfo> canales();
  std::optional<std::uint32_t> crear_canal(const std::string& nombre,
                                           protocolo::TipoCanal tipo = protocolo::TipoCanal::voz,
                                           bool solo_admins = false);
  bool cambiar_canal(std::uint32_t id, const std::string& nombre, bool solo_admins);
  bool borrar_canal(std::uint32_t id);

  std::optional<std::uint64_t> guardar_mensaje(std::uint32_t canal, std::uint32_t numero,
                                               const std::string& autor, std::uint64_t hora_ms,
                                               const std::string& texto, bool guardar = true);
  protocolo::Historial historial(const protocolo::PedirHistorial& pedido, std::size_t maximo_bytes);

  Retencion retencion();
  bool cambiar_retencion(Retencion retencion);
  void borrar_viejos(std::uint64_t ahora_ms);

  bool empezar_tanda();
  bool en_tanda() const;
  bool confirmar_tanda();
  void deshacer_lo_abierto();
  bool asentar();
  bool hay_sin_asentar() const;

 private:
  struct Llave {
    explicit Llave() = default;
  };

 public:
  Base(Llave, sqlite3* base);

 private:
  enum class Sentencia : std::size_t {
    empezar,
    confirmar,
    deshacer,
    marcar,
    soltar,
    volver,
    contador,
    insertar,
    viejos,
    nuevos,
    total
  };

  bool es_su_hilo() const;
  sqlite3_stmt* preparada(Sentencia cual);
  bool correr(Sentencia cual);

  sqlite3* base_;
  unsigned long hilo_;
  std::array<sqlite3_stmt*, static_cast<std::size_t>(Sentencia::total)> preparadas_{};
  std::int64_t cambios_asentados_ = 0;
};

class ServicioBase {
 public:
  using Trabajo = std::function<std::function<void()>(Base&)>;

  ServicioBase() = default;
  ~ServicioBase();
  ServicioBase(const ServicioBase&) = delete;
  ServicioBase& operator=(const ServicioBase&) = delete;

  bool arrancar(std::wstring archivo, std::string nombre_general,
                const red::Despertador& despertador);
  bool pedir(Trabajo trabajo);
  void atender_respuestas();

 private:
  void trabajar(std::wstring archivo, std::string nombre_general);

  const red::Despertador* despertador_ = nullptr;
  std::mutex candado_;
  std::condition_variable hay_pedidos_;
  std::condition_variable abierta_;
  std::deque<Trabajo> pedidos_;
  std::deque<std::function<void()>> respuestas_;
  std::optional<bool> pudo_abrir_;
  bool terminar_ = false;
  std::thread hilo_;
};

}
