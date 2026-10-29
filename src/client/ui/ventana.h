#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <windows.h>

#include <imgui.h>

namespace ventrik::ui {
constexpr UINT kMensajeNovedades = WM_APP + 1;
constexpr UINT kMensajeBandeja = WM_APP + 2;
constexpr UINT_PTR kOrdenAbrir = 1001;
constexpr UINT_PTR kOrdenSalir = 1002;

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
  std::uint32_t reinicios_fallidos = 0;
  std::uint32_t error_direct3d = 0;
  std::uint32_t aviso_de_sesion = 0;
};

struct Situacion {
  bool con_foco = true;
  bool minimizada = false;
  bool tapada = false;
  bool juego = false;
  bool bloqueada = false;
  bool ahorro = false;
  bool oculta = false;
  std::uint32_t bloqueos = 0;
  bool dibuja() const { return !minimizada && !oculta && !tapada && !juego && !bloqueada; }
};

enum class Arreglo { ninguno, esperar, reiniciar, recrear };

Arreglo como_recuperar(HRESULT estado, bool otro_tamano, DWORD perdido_ms);
bool pide_cuadros(UINT mensaje, WPARAM tecla, LPARAM datos, bool escribiendo, std::optional<LPARAM>& ultimo_mouse);
DWORD espera_entre_cuadros(double ultimo_ms);
bool tapa_el_monitor(const RECT& ventana, const RECT& monitor, LONG estilo, LONG extendido);
std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)> ser_el_unico();
void mostrar_el_otro();

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
  std::uint32_t tomar_sin_usar_ms();
  std::uint32_t arranque_ms() const { return arranque_ms_; }
  int correr(const std::function<bool()>& atender, const std::function<void()>& dibujar);
  void al_terminar_la_sesion(std::function<void()> terminar) { al_terminar_ = std::move(terminar); }
  void bandeja(bool al_cerrar, bool al_minimizar, std::wstring abrir, std::wstring salir);
  void ocultar();

 private:
  struct Grafica;
  static LRESULT CALLBACK procesar(HWND ventana, UINT mensaje, WPARAM wparam, LPARAM lparam);
  bool preparar_dispositivo();
  void cargar_logo(float escala);
  void escuchar_a_windows();
  void mirar_situacion();
  bool recuperar_dispositivo();
  bool recrear_dispositivo();
  bool dibujar_cuadro(const std::function<void()>& dibujar);
  void poner_en_bandeja(bool poner);
  void menu_de_bandeja();

  std::unique_ptr<Grafica> grafica_;
  HWND ventana_ = nullptr;
  int como_mostrar_;
  float escala_ = 1.0f;
  bool lista_ = false;
  bool perdido_ = false;
  std::optional<DWORD> perdido_desde_;
  bool presentar_igual_ = true;
  bool bloqueo_ = false;
  bool protector_ = false;
  std::uint32_t mas_sin_usar_ms_ = 0;
  Situacion situacion_;
  UsoVentana uso_;
  std::uint32_t arranque_ms_ = 0;
  UINT ancho_nuevo_ = 0;
  UINT alto_nuevo_ = 0;
  std::function<void()> al_terminar_;
  bool cerrar_a_bandeja_ = false;
  bool minimizar_a_bandeja_ = false;
  std::wstring abrir_;
  std::wstring salir_;
  UINT barra_creada_ = RegisterWindowMessageW(L"TaskbarCreated");
};

}
