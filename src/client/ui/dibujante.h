#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <windows.h>

#include <imgui.h>

struct IUnknown;
struct IDirect3DDevice9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;

namespace ventrik::ui {
constexpr std::size_t kZonasMaximas = 4;

struct Soltar {
  void operator()(IUnknown* objeto) const;
};

struct Trazo {
  std::size_t huella = 0;
  RECT caja{};
};

class Zonas {
 public:
  const std::vector<RECT>& cambiadas(const ImDrawData& dibujo);
  void olvidar() { anteriores_.clear(); }

 private:
  void sumar(const RECT& caja);

  std::vector<Trazo> anteriores_;
  std::vector<Trazo> actuales_;
  std::vector<std::size_t> huellas_antes_;
  std::vector<std::size_t> huellas_ahora_;
  std::vector<RECT> zonas_;
};

class Dibujante {
 public:
  explicit Dibujante(IDirect3DDevice9& dispositivo, int textura_maxima);
  ~Dibujante();
  Dibujante(const Dibujante&) = delete;
  Dibujante& operator=(const Dibujante&) = delete;

  void preparar();
  void soltar();
  void dibujar(const ImDrawData& dibujo, const std::vector<RECT>& zonas);

 private:
  void actualizar(ImTextureData& textura);
  bool llenar(const ImDrawData& dibujo);
  void ordenes(const ImDrawData& dibujo, const RECT* zona);

  IDirect3DDevice9& dispositivo_;
  std::unique_ptr<IDirect3DVertexBuffer9, Soltar> vertices_;
  std::unique_ptr<IDirect3DIndexBuffer9, Soltar> indices_;
  int lugar_vertices_ = 0;
  int lugar_indices_ = 0;
  std::vector<std::pair<UINT, UINT>> rangos_;
};

}
