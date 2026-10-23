#include "client/diagnostico.h"

#include <windows.h>
#include <psapi.h>

namespace ventrik::cliente {
namespace {
std::uint64_t a_entero(const FILETIME& tiempo) {
  return (static_cast<std::uint64_t>(tiempo.dwHighDateTime) << 32) | tiempo.dwLowDateTime;
}

std::uint64_t tiempo_del_proceso() {
  FILETIME creado = {};
  FILETIME terminado = {};
  FILETIME sistema = {};
  FILETIME usuario = {};
  if (!GetProcessTimes(GetCurrentProcess(), &creado, &terminado, &sistema, &usuario)) {
    return 0;
  }
  return a_entero(sistema) + a_entero(usuario);
}

std::uint64_t reloj() {
  FILETIME ahora = {};
  GetSystemTimeAsFileTime(&ahora);
  return a_entero(ahora);
}

}

Diagnostico::Diagnostico() : proceso_anterior_(tiempo_del_proceso()), reloj_anterior_(reloj()) {}

Medicion Diagnostico::medir() {
  Medicion medicion;
  const std::uint64_t proceso = tiempo_del_proceso();
  const std::uint64_t ahora = reloj();
  if (ahora > reloj_anterior_ && proceso >= proceso_anterior_) {
    medicion.procesador = 100.0 * static_cast<double>(proceso - proceso_anterior_) /
                          static_cast<double>(ahora - reloj_anterior_);
  }
  proceso_anterior_ = proceso;
  reloj_anterior_ = ahora;
  PROCESS_MEMORY_COUNTERS memoria = {};
  if (GetProcessMemoryInfo(GetCurrentProcess(), &memoria, sizeof(memoria))) {
    medicion.ram_bytes = memoria.WorkingSetSize;
  }
  return medicion;
}

}
