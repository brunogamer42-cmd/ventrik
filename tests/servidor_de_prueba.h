#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "common/cifrado/identidad.h"
#include "common/cifrado/inicio.h"
#include "server/servidor.h"
#include <doctest/doctest.h>

namespace prueba {

using namespace std::chrono_literals;

constexpr std::uint32_t kLocal = 0x7F000001;

inline ventrik::cifrado::IdentidadServidor identidad_nueva() {
  auto identidad = ventrik::cifrado::IdentidadServidor::crear_nueva();
  REQUIRE(identidad);
  return std::move(*identidad);
}

inline bool esperar_que(const std::function<bool()>& condicion,
                        std::chrono::milliseconds limite = 5s) {
  const auto hasta = std::chrono::steady_clock::now() + limite;
  while (!condicion()) {
    if (std::chrono::steady_clock::now() >= hasta) {
      return false;
    }
    std::this_thread::sleep_for(10ms);
  }
  return true;
}

class ServidorDePrueba {
 public:
  using Evento = ventrik::servidor::Evento;

  explicit ServidorDePrueba(ventrik::servidor::Opciones opciones = {})
      : identidad_(identidad_nueva()) {
    opciones.escuchar = ventrik::red::Direccion{kLocal, 0};
    opciones.nombre_general = "General";
    servidor_ = std::make_unique<ventrik::servidor::Servidor>(
        identidad_, opciones,
        [this](Evento evento, const ventrik::red::Direccion&, const std::string&) {
          ++eventos_[static_cast<std::size_t>(evento)];
        });
    ventrik::cifrado::Secreto clave(ventrik::cifrado::kBytesClaveSimetrica);
    REQUIRE(ventrik::cifrado::aleatorio(nullptr, clave.bytes().data(), clave.size()) == 0);
    servidor_->clave_del_historial(std::move(clave));
    REQUIRE(servidor_->escuchar());
    hilo_ = std::thread([this] { servidor_->correr(seguir_); });
  }
  ~ServidorDePrueba() {
    seguir_ = false;
    hilo_.join();
  }
  ServidorDePrueba(const ServidorDePrueba&) = delete;
  ServidorDePrueba& operator=(const ServidorDePrueba&) = delete;

  ventrik::red::Direccion direccion() const { return {kLocal, servidor_->puerto()}; }
  const ventrik::cifrado::Huella& huella() const { return identidad_.huella(); }
  int cuantos(Evento evento) const { return eventos_[static_cast<std::size_t>(evento)].load(); }

 private:
  ventrik::cifrado::IdentidadServidor identidad_;
  std::array<std::atomic<int>, 32> eventos_{};
  std::atomic<bool> seguir_{true};
  std::unique_ptr<ventrik::servidor::Servidor> servidor_;
  std::thread hilo_;
};

}
