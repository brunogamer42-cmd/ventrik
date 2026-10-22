#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ventrik::servidor {

// La consola se traba si alguien selecciona texto o aprieta Pausa: el hilo de red nunca la espera.
class Registro {
 public:
  using Escribir = std::function<void(const std::string&)>;

  Registro(Escribir escribir, std::string aviso_de_perdidas, std::size_t maximo_en_cola = 256);
  ~Registro();
  Registro(const Registro&) = delete;
  Registro& operator=(const Registro&) = delete;

  void agregar(std::string linea);

 private:
  void escribir_en_orden();

  const Escribir escribir_;
  const std::string aviso_de_perdidas_;
  const std::size_t maximo_en_cola_;
  std::mutex candado_;
  std::condition_variable hay_lineas_;
  std::deque<std::string> lineas_;
  std::size_t perdidas_ = 0;
  bool terminar_ = false;
  std::thread hilo_;
};

}  // namespace ventrik::servidor
