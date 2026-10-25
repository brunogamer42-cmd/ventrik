#include "client/voz_primero.h"

#include <cstdint>

#include <windows.h>

namespace ventrik::cliente {
namespace {
struct FrenoDeHilo {
  ULONG version;
  ULONG control;
  ULONG estado;
};

constexpr int kFrenoDeEnergia = 3;
constexpr ULONG kVersionDelFreno = 1;
constexpr ULONG kFrenoDeVelocidad = 1;

std::size_t en_paginas(const Tramo& tramo) {
  SYSTEM_INFO sistema = {};
  GetSystemInfo(&sistema);
  const std::uintptr_t pagina = sistema.dwPageSize;
  const auto desde = reinterpret_cast<std::uintptr_t>(tramo.desde);
  return static_cast<std::size_t>(((desde + tramo.largo + pagina - 1) / pagina - desde / pagina) * pagina);
}

bool mover_minimo(SIZE_T bytes, bool subir) {
  SIZE_T minimo = 0;
  SIZE_T maximo = 0;
  return GetProcessWorkingSetSize(GetCurrentProcess(), &minimo, &maximo) && (subir || minimo > bytes) &&
         SetProcessWorkingSetSize(GetCurrentProcess(), subir ? minimo + bytes : minimo - bytes,
                                  subir ? maximo + bytes : maximo - bytes);
}

}

bool no_frenar_este_hilo() {
  using Ajustar = BOOL(WINAPI*)(HANDLE, int, LPVOID, DWORD);
  const auto ajustar =
      reinterpret_cast<Ajustar>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadInformation"));
  FrenoDeHilo sin_freno = {kVersionDelFreno, kFrenoDeVelocidad, 0};
  return ajustar != nullptr && ajustar(GetCurrentThread(), kFrenoDeEnergia, &sin_freno, sizeof(sin_freno));
}

void MemoriaFija::fijar(std::initializer_list<Tramo> tramos) {
  soltar();
  std::size_t total = 0;
  for (const Tramo& tramo : tramos) {
    total += en_paginas(tramo);
  }
  subida_ = mover_minimo(total, true) ? total : 0;
  for (const Tramo& tramo : tramos) {
    if (tramo.largo > 0 && VirtualLock(tramo.desde, tramo.largo)) {
      tramos_.push_back(tramo);
      fijada_ += en_paginas(tramo);
    }
  }
}

void MemoriaFija::soltar() {
  for (const Tramo& tramo : tramos_) {
    VirtualUnlock(tramo.desde, tramo.largo);
  }
  if (subida_ > 0) {
    mover_minimo(subida_, false);
  }
  tramos_.clear();
  fijada_ = 0;
  subida_ = 0;
}

}
