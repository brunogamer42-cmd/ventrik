// Ventrik: por ahora, una ventana vacía.

#include <memory>
#include <string>
#include <string_view>

#include <windows.h>
#include <shellapi.h>

#include "client/conexion.h"
#include "common/cifrado/huella.h"
#include "common/cifrado/inicio.h"
#include "common/pendiente.h"
#include "common/proceso.h"
#include "common/red/red.h"
#include "common/textos.h"
#include "common/utf.h"
#include "common/version.h"

namespace {

constexpr wchar_t kClaseVentana[] = L"VentrikVentanaPrincipal";

struct LiberarArgumentos {
  void operator()(LPWSTR* argumentos) const { LocalFree(argumentos); }
};

std::string mensaje_de(const ventrik::Idioma& idioma, const ventrik::cliente::ResultadoConexion& resultado) {
  const auto texto = [&idioma](ventrik::Clave clave) {
    return std::string(ventrik::texto(idioma, clave));
  };
  switch (resultado.estado) {
    case ventrik::cliente::EstadoConexion::conectado:
      return texto(ventrik::Clave::conectar_ok) + "\n\n" + texto(ventrik::Clave::conectar_cifrado) +
             " " + resultado.cifrado + "\n" + texto(ventrik::Clave::conectar_protocolo) + " " +
             std::to_string(resultado.protocolo);
    case ventrik::cliente::EstadoConexion::sin_respuesta:
      return texto(ventrik::Clave::conectar_sin_respuesta);
    case ventrik::cliente::EstadoConexion::cifrado_rechazado:
      return texto(ventrik::Clave::conectar_cifrado_rechazado);
    case ventrik::cliente::EstadoConexion::huella_distinta:
      return texto(ventrik::Clave::conectar_huella_distinta);
    case ventrik::cliente::EstadoConexion::programa_viejo:
      return texto(ventrik::Clave::conectar_programa_viejo) + " " +
             ventrik::a_texto(resultado.version_pedida) + ".";
    case ventrik::cliente::EstadoConexion::servidor_viejo:
      return texto(ventrik::Clave::conectar_servidor_viejo);
    case ventrik::cliente::EstadoConexion::rechazo_ilegible:
      return texto(ventrik::Clave::conectar_rechazo_ilegible);
    case ventrik::cliente::EstadoConexion::cortada:
      return texto(ventrik::Clave::conectar_cortada);
    case ventrik::cliente::EstadoConexion::error_interno:
      break;
  }
  return texto(ventrik::Clave::conectar_error_interno);
}

int probar_conexion(const ventrik::Idioma& idioma, const std::wstring& titulo, int cantidad,
                    LPWSTR* argumentos) {
  VENTRIK_PENDIENTE("ventrik --conectar es provisorio hasta la pantalla de conexion (paso 2.5)");
  const auto avisar = [&](ventrik::Clave clave, UINT icono) {
    const std::wstring aviso = ventrik::a_utf16(ventrik::texto(idioma, clave));
    MessageBoxW(nullptr, aviso.c_str(), titulo.c_str(), MB_OK | icono);
  };
  if (cantidad != 4) {
    avisar(ventrik::Clave::conectar_uso, MB_ICONINFORMATION);
    return 2;
  }
  const auto direccion = ventrik::red::leer_direccion(ventrik::a_utf8(argumentos[2]));
  if (!direccion) {
    avisar(ventrik::Clave::conectar_direccion_invalida, MB_ICONERROR);
    return 2;
  }
  const auto huella = ventrik::cifrado::leer_huella(ventrik::a_utf8(argumentos[3]));
  if (!huella) {
    avisar(ventrik::Clave::conectar_huella_invalida, MB_ICONERROR);
    return 2;
  }
  ventrik::cifrado::Inicio cifrado;
  ventrik::red::Inicio red;
  if (!cifrado.ok() || !red.ok()) {
    avisar(ventrik::Clave::conectar_error_interno, MB_ICONERROR);
    return 1;
  }
  const auto resultado = ventrik::cliente::conectar(*direccion, *huella);
  const bool conectado = resultado.estado == ventrik::cliente::EstadoConexion::conectado;
  const std::wstring mensaje = ventrik::a_utf16(mensaje_de(idioma, resultado));
  MessageBoxW(nullptr, mensaje.c_str(), titulo.c_str(),
              MB_OK | (conectado ? MB_ICONINFORMATION : MB_ICONERROR));
  return conectado ? 0 : 1;
}
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
  ventrik::endurecer_carga_de_dll();
  ventrik::activar_dep();

  const ventrik::Idioma idioma = ventrik::idioma_de_windows();
  const std::wstring titulo =
      ventrik::a_utf16(ventrik::texto(idioma, ventrik::Clave::titulo_ventana));

  if (!ventrik::windows_es_compatible()) {
    const std::wstring aviso =
        ventrik::a_utf16(ventrik::texto(idioma, ventrik::Clave::aviso_windows_viejo));
    MessageBoxW(nullptr, aviso.c_str(), titulo.c_str(), MB_OK | MB_ICONERROR);
    return 1;
  }

  int cantidad = 0;
  const std::unique_ptr<LPWSTR, LiberarArgumentos> argumentos(
      CommandLineToArgvW(GetCommandLineW(), &cantidad));
  if (argumentos && cantidad >= 2 && std::wstring_view(argumentos.get()[1]) == L"--conectar") {
    return probar_conexion(idioma, titulo, cantidad, argumentos.get());
  }

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
