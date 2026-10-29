#include "common/proceso.h"

#include <initializer_list>

#include <malloc.h>
#include <windows.h>

namespace ventrik {

namespace {

FARPROC buscar_en_kernel32(const char* nombre) {
  const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  if (kernel32 == nullptr) {
    return nullptr;
  }
  return GetProcAddress(kernel32, nombre);
}

#if defined(_M_IX86)
constexpr DWORD kDepActivo = 0x00000001;
#endif

constexpr DWORD kSoloCarpetaDeSistema = 0x00000800;

}

#if defined(_M_IX86)

void activar_dep() {
  using FuncionActivar = BOOL(WINAPI*)(DWORD);
  const auto activar =
      reinterpret_cast<FuncionActivar>(buscar_en_kernel32("SetProcessDEPPolicy"));
  if (activar != nullptr) {
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

CargaDeDll endurecer_carga_de_dll() {
  using FuncionDirectorios = BOOL(WINAPI*)(DWORD);
  const auto solo_sistema = reinterpret_cast<FuncionDirectorios>(
      buscar_en_kernel32("SetDefaultDllDirectories"));
  if (solo_sistema != nullptr && solo_sistema(kSoloCarpetaDeSistema)) {
    return CargaDeDll::solo_carpeta_de_sistema;
  }
  using FuncionCarpeta = BOOL(WINAPI*)(LPCWSTR);
  const auto sin_carpeta_actual =
      reinterpret_cast<FuncionCarpeta>(buscar_en_kernel32("SetDllDirectoryW"));
  if (sin_carpeta_actual != nullptr && sin_carpeta_actual(L"")) {
    return CargaDeDll::sin_carpeta_actual;
  }
  return CargaDeDll::sin_cambios;
}

bool windows_es_compatible() {
#if defined(VENTRIK_CLASICA)
  OSVERSIONINFOEXW pedido = {};
  pedido.dwOSVersionInfoSize = sizeof(pedido);
  pedido.dwMajorVersion = 5;
  pedido.dwMinorVersion = 1;
  pedido.wServicePackMajor = 3;
  DWORDLONG condicion = 0;
  condicion = VerSetConditionMask(condicion, VER_MAJORVERSION, VER_GREATER_EQUAL);
  condicion = VerSetConditionMask(condicion, VER_MINORVERSION, VER_GREATER_EQUAL);
  condicion = VerSetConditionMask(condicion, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
  return VerifyVersionInfoW(
             &pedido, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
             condicion) != FALSE;
#else
  return true;
#endif
}

bool es_windows_xp() {
  OSVERSIONINFOEXW pedido = {};
  pedido.dwOSVersionInfoSize = sizeof(pedido);
  pedido.dwMajorVersion = 6;
  return VerifyVersionInfoW(&pedido, VER_MAJORVERSION,
                            VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL)) == FALSE;
}

void proteger_el_monton() {
  HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
  ULONG baja_fragmentacion = 2;
  for (const HANDLE monton : {GetProcessHeap(), reinterpret_cast<HANDLE>(_get_heap_handle())}) {
    HeapSetInformation(monton, HeapCompatibilityInformation, &baja_fragmentacion,
                       sizeof(baja_fragmentacion));
  }
}

bool queda_poca_memoria() {
  static const HANDLE aviso = CreateMemoryResourceNotification(LowMemoryResourceNotification);
  BOOL poca = FALSE;
  return aviso != nullptr && QueryMemoryResourceNotification(aviso, &poca) && poca;
}

}
