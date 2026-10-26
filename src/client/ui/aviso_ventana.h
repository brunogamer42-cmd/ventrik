#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <windows.h>

#include "client/avisos.h"

namespace ventrik::ui {
constexpr std::size_t kAvisosALaVez = 3;
constexpr UINT kSegundosDeAviso = 6;

class PiezasAviso {
 public:
  explicit PiezasAviso(float escala);
  ~PiezasAviso();
  PiezasAviso(const PiezasAviso&) = delete;
  PiezasAviso& operator=(const PiezasAviso&) = delete;

  float escala() const { return escala_; }
  HFONT titulo() const { return titulo_; }
  HFONT cuerpo() const { return cuerpo_; }
  HICON emblema() const { return emblema_; }
  int px(float medida) const { return static_cast<int>(medida * escala_ + 0.5f); }

 private:
  float escala_;
  std::vector<HANDLE> cargadas_;
  HFONT titulo_ = nullptr;
  HFONT cuerpo_ = nullptr;
  HICON emblema_ = nullptr;
};

SIZE medir_aviso(HDC dc, const cliente::Notificacion& aviso, const PiezasAviso& piezas);
void pintar_aviso(HDC dc, const RECT& area, const cliente::Notificacion& aviso, const PiezasAviso& piezas, bool cruz);

class AvisosEnPantalla {
 public:
  AvisosEnPantalla(HWND principal, float escala);
  ~AvisosEnPantalla();
  AvisosEnPantalla(const AvisosEnPantalla&) = delete;
  AvisosEnPantalla& operator=(const AvisosEnPantalla&) = delete;

  void mostrar(const std::vector<cliente::Notificacion>& avisos);
  void ocultar();
  std::optional<cliente::Notificacion> tocado() { return std::exchange(tocado_, std::nullopt); }
  std::uint32_t mostrados() const { return mostrados_; }
  std::size_t abiertas() const { return abiertas_.size(); }

 private:
  struct Abierta {
    HWND ventana = nullptr;
    cliente::Notificacion aviso;
    SIZE tamano{};
    bool encima = false;
    bool cruz = false;
    bool apretada = false;
  };

  static LRESULT CALLBACK procesar(HWND ventana, UINT mensaje, WPARAM wparam, LPARAM lparam);
  LRESULT atender(HWND ventana, UINT mensaje, WPARAM wparam, LPARAM lparam);
  void abrir(const cliente::Notificacion& aviso);
  void cerrar(HWND ventana);
  void acomodar();
  SIZE medir(const cliente::Notificacion& aviso) const;
  RECT cruz(HWND ventana) const;

  HWND principal_;
  PiezasAviso piezas_;
  std::vector<Abierta> abiertas_;
  std::map<std::uint32_t, std::uint64_t> vistas_;
  std::optional<cliente::Notificacion> tocado_;
  std::uint32_t mostrados_ = 0;
};

}
