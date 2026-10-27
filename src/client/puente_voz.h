#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include <windows.h>

#include "common/protocol/pedidos.h"
#include "common/red/red.h"

namespace ventrik::cliente {
constexpr std::size_t kVoces = 4;
using Hablando = std::array<std::uint32_t, kVoces>;

struct PaqueteVoz {
  std::uint32_t hablante = 0;
  std::uint16_t numero = 0;
  std::uint16_t tiempo = 0;
  std::uint8_t nivel = 0;
  std::uint16_t largo = 0;
  std::array<std::uint8_t, protocolo::kMaximoOpus> opus{};
};

template <std::size_t N>
class ColaVoz {
 public:
  bool poner(const PaqueteVoz& paquete) {
    const std::size_t fin = fin_.load(std::memory_order_relaxed);
    const std::size_t siguiente = (fin + 1) % N;
    if (siguiente == inicio_.load(std::memory_order_acquire)) {
      return false;
    }
    lugares_[fin] = paquete;
    fin_.store(siguiente, std::memory_order_release);
    return true;
  }

  bool sacar(PaqueteVoz& paquete) {
    const std::size_t inicio = inicio_.load(std::memory_order_relaxed);
    if (inicio == fin_.load(std::memory_order_acquire)) {
      return false;
    }
    paquete = lugares_[inicio];
    inicio_.store((inicio + 1) % N, std::memory_order_release);
    return true;
  }

 private:
  std::array<PaqueteVoz, N> lugares_{};
  std::atomic<std::size_t> inicio_{0};
  std::atomic<std::size_t> fin_{0};
};

enum class EstadoVoz : std::uint8_t { sin_clave, conectando, lista, sin_conexion };

class Aviso {
 public:
  Aviso() : evento_(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {}
  ~Aviso() {
    if (evento_ != nullptr) {
      CloseHandle(evento_);
    }
  }
  Aviso(const Aviso&) = delete;
  Aviso& operator=(const Aviso&) = delete;

  void avisar() const { SetEvent(evento_); }
  HANDLE evento() const { return evento_; }

 private:
  HANDLE evento_;
};

struct PuenteVoz {
  template <typename T>
  void cambiar(std::atomic<T>& valor, T nuevo) {
    if (valor.exchange(nuevo) != nuevo) {
      cambio.avisar();
    }
  }

  Hablando quienes() const {
    Hablando ids{};
    std::transform(hablando.begin(), hablando.end(), ids.begin(),
                   [](const std::atomic<std::uint32_t>& id) { return id.load(); });
    return ids;
  }

  ColaVoz<8> subida;
  ColaVoz<64> bajada;
  red::Despertador despertador;
  Aviso cambio;
  std::atomic<EstadoVoz> estado{EstadoVoz::sin_clave};
  std::array<std::atomic<std::uint32_t>, kVoces> hablando{};
  std::atomic<std::uint32_t> recibidos{0};
  std::atomic<std::uint32_t> perdidos{0};
  std::atomic<std::uint32_t> ping_ms{0};
};

}
