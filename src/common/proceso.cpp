#include "common/proceso.h"

#include <windows.h>

namespace ventrik {

#if defined(_M_IX86)

namespace {

constexpr DWORD kDepActivo = 0x00000001;  // PROCESS_DEP_ENABLE

FARPROC buscar_en_kernel32(const char* nombre) {
  const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  if (kernel32 == nullptr) {
    return nullptr;
  }
  return GetProcAddress(kernel32, nombre);
}

}  // namespace

void activar_dep() {
  using FuncionActivar = BOOL(WINAPI*)(DWORD);
  const auto activar =
      reinterpret_cast<FuncionActivar>(buscar_en_kernel32("SetProcessDEPPolicy"));
  if (activar != nullptr) {
    // Si Windows ya lo fuerza por su cuenta, esto falla sin cambiar nada.
    activar(kDepActivo);
  }
}

bool dep_activo() {
  using FuncionConsultar = BOOL(WINAPI*)(HANDLE, LPDWORD, PBOOL);
  const auto consultar =
      reinterpret_cast<FuncionConsultar>(buscar_en_kernel32("GetProcessDEPPolicy"));
  if (consultar == nullptr) {
    return false;
  }
  DWORD opciones = 0;
  BOOL permanente = FALSE;
  if (!consultar(GetCurrentProcess(), &opciones, &permanente)) {
    return false;
  }
  return (opciones & kDepActivo) != 0;
}

#else

void activar_dep() {}

bool dep_activo() { return true; }

#endif

}  // namespace ventrik
