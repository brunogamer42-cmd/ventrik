#pragma once

#include <cstdint>
#include <memory>
#include <vector>

struct IUnknown;

#include <windows.h>
#include <audiopolicy.h>

namespace ventrik::cliente {
struct LiberarCom {
  void operator()(IUnknown* objeto) const { objeto->Release(); }
};

bool con_auriculares();
float factor_de_bajada(std::uint8_t cuanto);

class OtrosProgramas {
 public:
  OtrosProgramas() = default;
  ~OtrosProgramas();
  OtrosProgramas(const OtrosProgramas&) = delete;
  OtrosProgramas& operator=(const OtrosProgramas&) = delete;

  void bajar(float factor);
  void devolver();

 private:
  struct Bajado {
    std::unique_ptr<ISimpleAudioVolume, LiberarCom> volumen;
    float antes = 1;
    float puesto = 1;
  };

  bool com_ = false;
  std::vector<Bajado> bajados_;
};

}
