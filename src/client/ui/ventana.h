#pragma once

#include <functional>
#include <memory>
#include <string>

#include <windows.h>

#include <imgui.h>

namespace ventrik::ui {
constexpr UINT kMensajeNovedades = WM_APP + 1;

class Ventana {
 public:
  Ventana(const std::wstring& titulo, int como_mostrar);
  ~Ventana();
  Ventana(const Ventana&) = delete;
  Ventana& operator=(const Ventana&) = delete;

  bool ok() const { return lista_; }
  HWND hwnd() const { return ventana_; }
  ImTextureID logo() const;
  int correr(const std::function<void()>& atender, const std::function<void()>& dibujar);
  void al_terminar_la_sesion(std::function<void()> terminar) { al_terminar_ = std::move(terminar); }

 private:
  struct Grafica;
  static LRESULT CALLBACK procesar(HWND ventana, UINT mensaje, WPARAM wparam, LPARAM lparam);
  bool preparar_dispositivo();
  void cargar_logo(float escala);
  bool recuperar_dispositivo();
  void dibujar_cuadro(const std::function<void()>& dibujar);

  std::unique_ptr<Grafica> grafica_;
  HWND ventana_ = nullptr;
  int como_mostrar_;
  float escala_ = 1.0f;
  bool lista_ = false;
  bool perdido_ = false;
  UINT ancho_nuevo_ = 0;
  UINT alto_nuevo_ = 0;
  std::function<void()> al_terminar_;
};

}
