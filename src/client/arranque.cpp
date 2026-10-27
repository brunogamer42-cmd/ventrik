#include "client/arranque.h"

#include <cwchar>
#include <vector>

#include <windows.h>

namespace ventrik::cliente {
namespace {
constexpr wchar_t kNombre[] = L"Ventrik";

std::wstring este_programa() {
  std::vector<wchar_t> ruta(MAX_PATH);
  DWORD largo = 0;
  while ((largo = GetModuleFileNameW(nullptr, ruta.data(), static_cast<DWORD>(ruta.size()))) == ruta.size() &&
         ruta.size() < 32768) {
    ruta.resize(ruta.size() * 2);
  }
  return L"\"" + std::wstring(ruta.data(), largo) + L"\"";
}

}

bool arranca_con_windows(const std::wstring& clave) {
  HKEY abierta = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, clave.c_str(), 0, KEY_QUERY_VALUE, &abierta) != ERROR_SUCCESS) {
    return false;
  }
  std::vector<wchar_t> valor(32768);
  const std::wstring propio = este_programa();
  DWORD tipo = 0;
  DWORD bytes = static_cast<DWORD>(valor.size() * sizeof(wchar_t));
  const bool hay = RegQueryValueExW(abierta, kNombre, nullptr, &tipo, reinterpret_cast<BYTE*>(valor.data()), &bytes) ==
                       ERROR_SUCCESS &&
                   tipo == REG_SZ && bytes / sizeof(wchar_t) >= propio.size() &&
                   _wcsnicmp(valor.data(), propio.c_str(), propio.size()) == 0;
  RegCloseKey(abierta);
  return hay;
}

bool arrancar_con_windows(bool si, const std::wstring& clave) {
  HKEY abierta = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, clave.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &abierta, nullptr) !=
      ERROR_SUCCESS) {
    return false;
  }
  const std::wstring valor = este_programa();
  const LONG resultado =
      si ? RegSetValueExW(abierta, kNombre, 0, REG_SZ, reinterpret_cast<const BYTE*>(valor.c_str()),
                          static_cast<DWORD>((valor.size() + 1) * sizeof(wchar_t)))
         : RegDeleteValueW(abierta, kNombre);
  RegCloseKey(abierta);
  return resultado == ERROR_SUCCESS || (!si && resultado == ERROR_FILE_NOT_FOUND);
}

}
