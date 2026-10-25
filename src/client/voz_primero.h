#pragma once

#include <cstddef>
#include <initializer_list>
#include <vector>

namespace ventrik::cliente {
bool no_frenar_este_hilo();

struct Tramo {
  void* desde = nullptr;
  std::size_t largo = 0;
};

class MemoriaFija {
 public:
  MemoriaFija() = default;
  ~MemoriaFija() { soltar(); }
  MemoriaFija(const MemoriaFija&) = delete;
  MemoriaFija& operator=(const MemoriaFija&) = delete;

  void fijar(std::initializer_list<Tramo> tramos);
  void soltar();
  std::size_t fijada() const { return fijada_; }

 private:
  std::vector<Tramo> tramos_;
  std::size_t fijada_ = 0;
  std::size_t subida_ = 0;
};

}
