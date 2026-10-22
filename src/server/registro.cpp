#include "server/registro.h"

#include <utility>

namespace ventrik::servidor {

Registro::Registro(Escribir escribir, std::string aviso_de_perdidas, std::size_t maximo_en_cola)
    : escribir_(std::move(escribir)),
      aviso_de_perdidas_(std::move(aviso_de_perdidas)),
      maximo_en_cola_(maximo_en_cola),
      hilo_([this] { escribir_en_orden(); }) {}

Registro::~Registro() {
  {
    const std::lock_guard<std::mutex> candado(candado_);
    terminar_ = true;
  }
  hay_lineas_.notify_one();
  hilo_.join();
}

void Registro::agregar(std::string linea) {
  {
    const std::lock_guard<std::mutex> candado(candado_);
    if (lineas_.size() >= maximo_en_cola_) {
      ++perdidas_;
      return;
    }
    lineas_.push_back(std::move(linea));
  }
  hay_lineas_.notify_one();
}

void Registro::escribir_en_orden() {
  std::unique_lock<std::mutex> candado(candado_);
  for (;;) {
    hay_lineas_.wait(candado, [this] { return terminar_ || !lineas_.empty(); });
    const std::size_t perdidas = std::exchange(perdidas_, 0);
    const bool hay_linea = !lineas_.empty();
    std::string linea;
    if (hay_linea) {
      linea = std::move(lineas_.front());
      lineas_.pop_front();
    }
    candado.unlock();
    if (perdidas > 0) {
      escribir_(aviso_de_perdidas_ + " " + std::to_string(perdidas));
    }
    if (!hay_linea) {
      return;
    }
    escribir_(linea);
    candado.lock();
  }
}

}  // namespace ventrik::servidor
