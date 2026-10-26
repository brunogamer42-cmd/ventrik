#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <windows.h>

#include <imgui.h>

namespace ventrik::ui {
constexpr UINT kMensajeNovedades = WM_APP + 1;

struct UsoVentana {
  std::map<UINT, std::uint32_t> mensajes;
  double ocupado_ms = 0;
  double pasada_maxima_ms = 0;
  double imgui_ms = 0;
  double direct3d_ms = 0;
  std::uint32_t presentados = 0;
  int vertices_max = 0;
  std::uint32_t despertares = 0;
  std::uint64_t pixeles = 0;
};

struct Situacion {
  bool con_foco = true;
  bool minimizada = false;
  bool tapada = false;
  bool juego = false;
  bool bloqueada = false;
  bool ahorro = false;
  bool dibuja() const { return !minimizada && !tapada && !juego && !bloqueada; }
};

bool pide_cuadros(UINT mensaje, WPARAM tecla, LPARAM datos, bool escribiendo, std::optional<LPARAM>& ultimo_mouse);
DWORD espera_entre_cuadros(double ultimo_ms);
bool tapa_el_monitor(const RECT& ventana, const RECT& monitor, LONG estilo, LONG extendido);

class Ventana {
 public:
  Ventana(const std::wstring& titulo, int como_mostrar);
  ~Ventana();
  Ventana(const Ventana&) = delete;
  Ventana& operator=(const Ventana&) = delete;

  bool ok() const { return lista_; }
  HWND hwnd() const { return ventana_; }
  float escala() const { return escala_; }
  void traer_al_frente();
  ImTextureID logo() const;
  std::string placa() const;
  std::string grafica() const;
  bool por_procesador() const;
  const Situacion& situacion() const { return situacion_; }
  UsoVentana tomar_uso() { return std::exchange(uso_, {}); }
  std::uint32_t arranque_ms() const { return arranque_ms_; }
  int correr(const std::function<bool()>& atender, const std::function<void()>& dibujar);
  void al_terminar_la_sesion(std::function<void()> terminar) { al_terminar_ = std::move(terminar); }

 private:
  struct Grafica;
  static LRESULT CALLBACK procesar(HWND ventana, UINT mensaje, WPARAM wparam, LPARAM lparam);
  bool preparar_dispositivo();
  void cargar_logo(float escala);
  void escuchar_a_windows();
  void mirar_situacion();
  bool recuperar_dispositivo();
  bool dibujar_cuadro(const std::function<void()>& dibujar);

  std::unique_ptr<Grafica> grafica_;
  HWND ventana_ = nullptr;
  int como_mostrar_;
  float escala_ = 1.0f;
  bool lista_ = false;
  bool perdido_ = false;
  bool presentar_igual_ = true;
  bool bloqueo_ = false;
  bool protector_ = false;
  Situacion situacion_;
  UsoVentana uso_;
  std::uint32_t arranque_ms_ = 0;
  UINT ancho_nuevo_ = 0;
  UINT alto_nuevo_ = 0;
  std::function<void()> al_terminar_;
};

}
