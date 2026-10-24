#pragma once

#include <atomic>
#include <bitset>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

#include <windows.h>

#include "client/guardados.h"
#include "client/motor_voz.h"
#include "client/puente_voz.h"

namespace ventrik::cliente {
constexpr int kSegmentosDelMedidor = 10;

using Teclas = std::bitset<256>;

int tecla_nueva(const Teclas& antes, const Teclas& ahora);

class Vigia {
 public:
  Vigia(MotorVoz& motor, const PuenteVoz& puente, std::uint8_t tecla, HWND ventana, std::function<void()> avisar);
  ~Vigia();
  Vigia(const Vigia&) = delete;
  Vigia& operator=(const Vigia&) = delete;

  void usar_tecla(std::uint8_t tecla) { tecla_ = tecla; }
  void elegir_tecla();
  std::optional<std::uint8_t> tomar_elegida();

 private:
  void correr();

  MotorVoz& motor_;
  const PuenteVoz& puente_;
  HWND ventana_;
  std::function<void()> avisar_;
  std::atomic<std::uint8_t> tecla_;
  std::atomic<bool> eligiendo_{false};
  std::atomic<int> elegida_{-1};
  std::mutex candado_;
  std::condition_variable despertar_;
  bool terminar_ = false;
  std::thread hilo_;
};

}
