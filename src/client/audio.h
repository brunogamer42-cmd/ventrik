#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include <windows.h>

#include <mmsystem.h>

#include <gsl/gsl>

struct ma_device;

namespace ventrik::cliente {
class Audio {
 public:
  using Procesar = void (*)(void* datos, gsl::span<const std::int16_t> entrada,
                            gsl::span<std::int16_t> salida);

  Audio();
  ~Audio();
  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;

  bool abrir(unsigned frecuencia, Procesar procesar, void* datos);
  void cerrar();

 private:
  struct Pedazo {
    WAVEHDR cabecera = {};
    std::vector<std::int16_t> muestras;
  };

  bool abrir_puente(unsigned frecuencia);
  static unsigned __stdcall hilo(void* yo);
  void atender();
  Pedazo* libre_para_salida();
  bool algo_sonando() const;
  void escribir(Pedazo& pedazo);
  void escribir_silencio();
  static void desde_miniaudio(ma_device* dispositivo, void* salida, const void* entrada,
                              std::uint32_t cuadros);

  Procesar procesar_ = nullptr;
  void* datos_ = nullptr;
  std::unique_ptr<ma_device> dispositivo_;
  bool hay_miniaudio_ = false;
  HWAVEIN entrada_ = nullptr;
  HWAVEOUT salida_ = nullptr;
  HANDLE aviso_ = nullptr;
  HANDLE hilo_ = nullptr;
  std::vector<Pedazo> pedazos_entrada_;
  std::vector<Pedazo> pedazos_salida_;
  std::vector<std::int16_t> borrador_;
  std::size_t siguiente_entrada_ = 0;
  std::atomic<bool> seguir_{false};
};

}
