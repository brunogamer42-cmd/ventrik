#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "common/cifrado/secreto.h"
#include "common/red/red.h"
#include "server/base.h"
#include "server/contrasena.h"

namespace ventrik::servidor {

struct Comprobacion {
  std::uint64_t marca = 0;
  std::uint32_t ip = 0;
  ClaveDeUsuario clave{};
  Comprobado resultado = Comprobado::fallo;
  bool cambio = false;
  bool rehacer = false;
  std::optional<std::string> hash;
  std::uint64_t version = 0;
};

class Comprobador {
 public:
  Comprobador(const red::Despertador& despertador, std::size_t maximo);
  ~Comprobador();
  Comprobador(const Comprobador&) = delete;
  Comprobador& operator=(const Comprobador&) = delete;

  bool arrancar();
  bool pedir(const Comprobacion& quien, const ContrasenaGuardada& guardada, cifrado::Secreto contrasena);
  std::vector<Comprobacion> tomar();

 private:
  struct Pedido {
    Comprobacion quien;
    ContrasenaGuardada guardada;
    cifrado::Secreto contrasena;
  };

  void trabajar();

  const red::Despertador& despertador_;
  const std::size_t maximo_;
  std::mutex candado_;
  std::condition_variable hay_pedidos_;
  std::deque<Pedido> pedidos_;
  std::vector<Comprobacion> hechas_;
  std::optional<std::uint32_t> en_curso_;
  std::atomic<bool> hay_hechas_{false};
  bool terminar_ = false;
  std::thread hilo_;
};

}
