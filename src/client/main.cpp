// Ventrik: por ahora, una ventana vacía.

#include <string>

#include <windows.h>

#include "common/proceso.h"
#include "common/textos.h"
#include "common/utf.h"

namespace {

constexpr wchar_t kClaseVentana[] = L"VentrikVentanaPrincipal";
constexpr int kAnchoInicial = 960;
constexpr int kAltoInicial = 600;

LRESULT CALLBACK procesar_mensaje(HWND ventana, UINT mensaje, WPARAM wparam,
                                  LPARAM lparam) {
  switch (mensaje) {
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(ventana, mensaje, wparam, lparam);
  }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instancia, HINSTANCE, PWSTR, int como_mostrar) {
  ventrik::activar_dep();

  const ventrik::Idioma idioma = ventrik::idioma_de_windows();
  const std::wstring titulo =
      ventrik::a_utf16(ventrik::texto(idioma, ventrik::Clave::titulo_ventana));

  WNDCLASSEXW clase = {};
  clase.cbSize = sizeof(clase);
  clase.lpfnWndProc = procesar_mensaje;
  clase.hInstance = instancia;
  clase.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  clase.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
  clase.lpszClassName = kClaseVentana;
  if (RegisterClassExW(&clase) == 0) {
    return 1;
  }

  const HWND ventana = CreateWindowExW(
      0, kClaseVentana, titulo.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
      CW_USEDEFAULT, kAnchoInicial, kAltoInicial, nullptr, nullptr, instancia,
      nullptr);
  if (ventana == nullptr) {
    return 1;
  }
  ShowWindow(ventana, como_mostrar);
  UpdateWindow(ventana);

  MSG mensaje = {};
  BOOL resultado = 0;
  while ((resultado = GetMessageW(&mensaje, nullptr, 0, 0)) > 0) {
    TranslateMessage(&mensaje);
    DispatchMessageW(&mensaje);
  }
  return resultado < 0 ? 1 : static_cast<int>(mensaje.wParam);
}
