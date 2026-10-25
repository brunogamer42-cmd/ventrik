#pragma once

#include <atomic>
#include <bitset>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>

#include <windows.h>

#include "client/guardados.h"
#include "client/motor_voz.h"
#include "client/puente_voz.h"

namespace ventrik::cliente {
constexpr int kSegmentosDelMedidor = 10;

using Teclas = std::bitset<256>;

int tecla_nueva(const Teclas& antes, const Teclas& ahora);
bool es_la_tecla(std::uint8_t elegida, const RAWKEYBOARD& tecla);

class Vigia {
 public:
  Vigia(MotorVoz& motor, const PuenteVoz& puente, std::uint8_t tecla, HWND ventana, std::function<void()> avisar);
  ~Vigia();
  Vigia(const Vigia&) = delete;
  Vigia& operator=(const Vigia&) = delete;

  void usar_tecla(std::uint8_t tecla) { tecla_ = tecla; }
  int nivel() const { return nivel_; }
  void por_voz(bool si);
  void medidor_siempre(bool si);
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
  std::atomic<bool> por_voz_{false};
  std::atomic<bool> medidor_siempre_{false};
  std::atomic<int> nivel_{0};
  std::atomic<int> elegida_{-1};
  std::atomic<bool> terminar_{false};
  std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)> despertar_;
  std::thread hilo_;
};

}
