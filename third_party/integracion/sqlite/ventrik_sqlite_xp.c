/*
 * GetTickCount64 para SQLite en la versión clásica (ver ventrik_sqlite_xp.h).
 */
#include <windows.h>

typedef ULONGLONG(WINAPI *FuncionTick64)(void);

ULONGLONG WINAPI ventrik_GetTickCount64(void) {
  const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  if (kernel32 != NULL) {
    const FuncionTick64 tick64 =
        (FuncionTick64)GetProcAddress(kernel32, "GetTickCount64");
    if (tick64 != NULL) {
      return tick64();
    }
  }
  return GetTickCount();
}
