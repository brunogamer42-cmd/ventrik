#include "common/sistema.h"

#include <string>

namespace ventrik {
HMODULE cargar_de_sistema(const wchar_t* dll) noexcept {
  wchar_t carpeta[MAX_PATH] = {};
  const UINT largo = GetSystemDirectoryW(carpeta, MAX_PATH);
  if (largo == 0 || largo >= MAX_PATH) {
    return nullptr;
  }
  std::wstring ruta(carpeta, largo);
  ruta += L'\\';
  ruta += dll;
  return LoadLibraryExW(ruta.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
}
}
