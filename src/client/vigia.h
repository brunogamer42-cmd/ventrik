#pragma once

#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
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
constexpr std::size_t kTeclasDeVoz = protocolo::kLugaresDeSusurro + 1;

struct EstadoDeTeclas {
  std::optional<std::chrono::steady_clock::time_point> soltar_en;
  std::uint8_t flujo = 0;
  bool esperar_soltar = false;
};

struct PedidoDeVoz {
  bool transmitir = false;
  std::uint8_t susurro = 0;
};

Teclas teclas_apretadas();
int tecla_nueva(const Teclas& antes, const Teclas& ahora);
bool es_la_tecla(std::uint8_t elegida, const RAWKEYBOARD& tecla);
std::optional<PedidoDeVoz> decidir_teclas(EstadoDeTeclas& estado, const std::array<bool, kTeclasDeVoz>& apretadas,
                                          bool por_voz, bool sale, bool soltar_primero,
                                          std::chrono::steady_clock::time_point ahora, std::chrono::milliseconds seguir);

class Vigia {
 public:
  Vigia(MotorVoz& motor, const PuenteVoz& puente, std::uint8_t tecla, HWND ventana, std::function<void()> avisar);
  ~Vigia();
  Vigia(const Vigia&) = delete;
  Vigia& operator=(const Vigia&) = delete;

  void usar_tecla(std::uint8_t tecla) {
    tecla_ = tecla;
    soltar_primero_ = true;
  }
  void no_elegir() { eligiendo_ = false; }
  void susurros(const std::array<std::uint8_t, protocolo::kLugaresDeSusurro>& teclas);
  void solo_con_foco(bool si) { solo_con_foco_ = si; }
  void seguir_al_soltar(std::chrono::milliseconds cuanto) { seguir_al_soltar_ = cuanto.count(); }
  int nivel() const { return nivel_; }
  void por_voz(bool si);
  void medir(bool si);
  void elegir_tecla();
  std::optional<std::uint8_t> tomar_elegida();

 private:
  void correr();

  MotorVoz& motor_;
  const PuenteVoz& puente_;
  HWND ventana_;
  std::function<void()> avisar_;
  std::atomic<std::uint8_t> tecla_;
  std::array<std::atomic<std::uint8_t>, protocolo::kLugaresDeSusurro> susurros_{};
  std::atomic<bool> eligiendo_{false};
  std::atomic<bool> soltar_primero_{false};
  std::atomic<bool> por_voz_{false};
  std::atomic<bool> medir_{false};
  std::atomic<bool> solo_con_foco_{false};
  std::atomic<long long> seguir_al_soltar_{0};
  std::atomic<int> nivel_{0};
  std::atomic<int> elegida_{-1};
  std::atomic<bool> terminar_{false};
  std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)> despertar_;
  std::thread hilo_;
};

}
