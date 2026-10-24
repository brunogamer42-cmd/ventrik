#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>
#include <shellapi.h>

#include "client/consola.h"
#include "client/ui/programa.h"
#include "common/proceso.h"
#include "common/textos.h"
#include "common/utf.h"

namespace {

struct LiberarArgumentos {
  void operator()(LPWSTR* argumentos) const { LocalFree(argumentos); }
};

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int como_mostrar) {
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
  if (argumentos && cantidad >= 2 && std::wstring_view(argumentos.get()[1]) == L"--consola") {
    return ventrik::cliente::abrir_consola(idioma, cantidad, argumentos.get());
  }

  std::vector<std::string> entrar;
  if (argumentos && (cantidad == 5 || (cantidad == 7 && std::wstring_view(argumentos.get()[5]) == L"--dueno")) &&
      std::wstring_view(argumentos.get()[1]) == L"--entrar") {
    for (int i = 2; i < cantidad; ++i) {
      if (i != 5) {
        entrar.push_back(ventrik::a_utf8(argumentos.get()[i]));
      }
    }
  }
  return ventrik::ui::abrir_programa(idioma, titulo, como_mostrar, entrar);
}
