#include "client/vigia.h"

#include <chrono>

#include <windows.h>

namespace ventrik::cliente {
namespace {
using Reloj = std::chrono::steady_clock;
using namespace std::chrono_literals;

bool apretada(int tecla) {
  return (GetAsyncKeyState(tecla) & 0x8000) != 0;
}

Teclas teclas_apretadas() {
  Teclas teclas;
  for (int tecla = 1; tecla < 0xFF; ++tecla) {
    teclas[static_cast<std::size_t>(tecla)] = apretada(tecla);
  }
  return teclas;
}

}

int tecla_nueva(const Teclas& antes, const Teclas& ahora) {
  if (ahora[VK_ESCAPE] && !antes[VK_ESCAPE]) {
    return 0;
  }
  for (int tecla = VK_MBUTTON; tecla < 0xFF; ++tecla) {
    const auto lugar = static_cast<std::size_t>(tecla);
    if (ahora[lugar] && !antes[lugar] && es_tecla_valida(static_cast<std::uint8_t>(tecla))) {
      return tecla;
    }
  }
  return -1;
}

Vigia::Vigia(MotorVoz& motor, const PuenteVoz& puente, std::uint8_t tecla, HWND ventana,
             std::function<void()> avisar)
    : motor_(motor), puente_(puente), ventana_(ventana), avisar_(std::move(avisar)), tecla_(tecla),
      hilo_([this] { correr(); }) {}

Vigia::~Vigia() {
  {
    const std::lock_guard<std::mutex> candado(candado_);
    terminar_ = true;
  }
  despertar_.notify_all();
  hilo_.join();
  motor_.transmitir(false);
}

void Vigia::elegir_tecla() {
  elegida_ = -1;
  eligiendo_ = true;
}

std::optional<std::uint8_t> Vigia::tomar_elegida() {
  const int elegida = elegida_.exchange(-1);
  return elegida < 0 ? std::nullopt : std::optional<std::uint8_t>(static_cast<std::uint8_t>(elegida));
}

void Vigia::correr() {
  auto proximo_tic = Reloj::now() + 1s;
  auto proximo_nivel = Reloj::now();
  std::uint32_t hablando = puente_.hablando;
  EstadoVoz estado = puente_.estado;
  int nivel = 0;
  Teclas antes;
  bool empezando = true;
  bool esperar_soltar = false;
  std::unique_lock<std::mutex> candado(candado_);
  while (!despertar_.wait_for(candado, 20ms, [this] { return terminar_; })) {
    bool cambio = false;
    if (eligiendo_) {
      if (motor_.transmitiendo()) {
        motor_.transmitir(false);
        cambio = true;
      }
      const Teclas ahora = teclas_apretadas();
      const int elegida = GetForegroundWindow() != ventana_ ? 0 : empezando ? -1 : tecla_nueva(antes, ahora);
      antes = ahora;
      empezando = false;
      if (elegida >= 0) {
        elegida_ = elegida;
        eligiendo_ = false;
        esperar_soltar = elegida != 0;
        cambio = true;
      }
    } else {
      empezando = true;
      const bool hablar = apretada(tecla_);
      esperar_soltar = esperar_soltar && hablar;
      if (!esperar_soltar && hablar != motor_.transmitiendo()) {
        motor_.transmitir(hablar);
        cambio = true;
      }
    }
    if (puente_.hablando != hablando || puente_.estado != estado) {
      hablando = puente_.hablando;
      estado = puente_.estado;
      cambio = true;
    }
    const auto ahora = Reloj::now();
    if (motor_.transmitiendo() && ahora >= proximo_nivel) {
      proximo_nivel = ahora + 100ms;
      const int nuevo = motor_.nivel(kSegmentosDelMedidor);
      cambio = cambio || nuevo != nivel;
      nivel = nuevo;
    }
    if (ahora >= proximo_tic) {
      proximo_tic = ahora + 1s;
      cambio = true;
    }
    if (cambio) {
      candado.unlock();
      avisar_();
      candado.lock();
    }
  }
}

}
