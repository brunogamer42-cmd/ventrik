#pragma once

#include <cstddef>

namespace ventrik::cifrado {

// Una por proceso, en el hilo principal, antes y después de todo lo demás de Mbed TLS.
class Inicio {
 public:
  Inicio();
  ~Inicio();
  Inicio(const Inicio&) = delete;
  Inicio& operator=(const Inicio&) = delete;

  bool ok() const { return ok_; }

 private:
  bool ok_ = false;
};

// Con la forma que piden las funciones de Mbed TLS; usa el generador de PSA.
int aleatorio(void* contexto, unsigned char* salida, std::size_t largo);

}  // namespace ventrik::cifrado
