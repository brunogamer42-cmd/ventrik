#pragma once

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
  protocolo::Historial historial(std::uint32_t canal, std::uint64_t antes_de,
                                 std::size_t maximo_bytes);

  Retencion retencion();
  bool cambiar_retencion(Retencion retencion);
  void borrar_viejos(std::uint64_t ahora_ms);

 private:
  struct Llave {
    explicit Llave() = default;
  };

 public:
  Base(Llave, sqlite3* base);

 private:
  bool es_su_hilo() const;

  sqlite3* base_;
  unsigned long hilo_;
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
