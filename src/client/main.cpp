#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>
#include <shellapi.h>

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
  std::wstring registro;
  std::vector<std::wstring_view> resto;
  for (int i = 1; argumentos && i < cantidad; ++i) {
    if (std::wstring_view(argumentos.get()[i]) == L"--registro" && i + 1 < cantidad) {
      registro = argumentos.get()[++i];
    } else {
      resto.emplace_back(argumentos.get()[i]);
    }
  }
  std::vector<std::string> entrar;
  if ((resto.size() == 4 || (resto.size() == 6 && resto[4] == L"--dueno")) && resto[0] == L"--entrar") {
    for (std::size_t i = 1; i < resto.size(); ++i) {
      if (i != 4) {
        entrar.push_back(ventrik::a_utf8(resto[i]));
      }
    }
  }
  return ventrik::ui::abrir_programa(idioma, titulo, como_mostrar, entrar, registro);
}
