#include "client/diagnostico.h"

#include <algorithm>

#include <windows.h>
#include <psapi.h>

namespace ventrik::cliente {
namespace {
std::uint64_t a_entero(const FILETIME& tiempo) {
  return (static_cast<std::uint64_t>(tiempo.dwHighDateTime) << 32) | tiempo.dwLowDateTime;
}

std::uint64_t tiempo_usado(BOOL(WINAPI* pedir)(HANDLE, LPFILETIME, LPFILETIME, LPFILETIME, LPFILETIME),
                           HANDLE quien) {
  FILETIME creado = {};
  FILETIME terminado = {};
  FILETIME sistema = {};
  FILETIME usuario = {};
  if (!pedir(quien, &creado, &terminado, &sistema, &usuario)) {
    return 0;
  }
  return a_entero(sistema) + a_entero(usuario);
}

std::uint64_t tiempo_del_proceso() { return tiempo_usado(GetProcessTimes, GetCurrentProcess()); }

std::uint64_t tiempo_del_hilo() { return tiempo_usado(GetThreadTimes, GetCurrentThread()); }

std::pair<std::uint64_t, std::uint64_t> tiempos_del_sistema() {
  FILETIME ocioso = {};
  FILETIME nucleo = {};
  FILETIME usuario = {};
  if (!GetSystemTimes(&ocioso, &nucleo, &usuario)) {
    return {0, 0};
  }
  return {a_entero(ocioso), a_entero(nucleo) + a_entero(usuario)};
}

PROCESS_MEMORY_COUNTERS_EX memoria_del_proceso() {
  PROCESS_MEMORY_COUNTERS_EX memoria = {};
  memoria.cb = sizeof(memoria);
  GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoria), sizeof(memoria));
  return memoria;
}

std::uint64_t reloj() {
  FILETIME ahora = {};
  GetSystemTimeAsFileTime(&ahora);
  return a_entero(ahora);
}

}

Diagnostico::Diagnostico()
    : proceso_anterior_(tiempo_del_proceso()), ventana_anterior_(tiempo_del_hilo()),
      sistema_anterior_(tiempos_del_sistema()), reloj_anterior_(reloj()),
      fallas_anteriores_(memoria_del_proceso().PageFaultCount) {}

Medicion Diagnostico::medir() {
  Medicion medicion;
  const std::uint64_t proceso = tiempo_del_proceso();
  const std::uint64_t ventana = tiempo_del_hilo();
  const std::uint64_t ahora = reloj();
  if (ahora > reloj_anterior_ && proceso >= proceso_anterior_ && ventana >= ventana_anterior_) {
    const double lapso = static_cast<double>(ahora - reloj_anterior_);
    medicion.procesador = 100.0 * static_cast<double>(proceso - proceso_anterior_) / lapso;
    medicion.ventana = 100.0 * static_cast<double>(ventana - ventana_anterior_) / lapso;
  }
  const PROCESS_MEMORY_COUNTERS_EX memoria = memoria_del_proceso();
  if (ahora > reloj_anterior_ && memoria.PageFaultCount >= fallas_anteriores_) {
    medicion.fallas = 1e7 * static_cast<double>(memoria.PageFaultCount - fallas_anteriores_) /
                      static_cast<double>(ahora - reloj_anterior_);
  }
  fallas_anteriores_ = memoria.PageFaultCount;
  medicion.ram_bytes = memoria.WorkingSetSize;
  medicion.memoria_propia = memoria.PrivateUsage;
  const auto sistema = tiempos_del_sistema();
  if (sistema.second > sistema_anterior_.second && sistema.first >= sistema_anterior_.first) {
    const std::uint64_t total = sistema.second - sistema_anterior_.second;
    const std::uint64_t ocioso = std::min(total, sistema.first - sistema_anterior_.first);
    medicion.sistema = 100.0 * static_cast<double>(total - ocioso) / static_cast<double>(total);
  }
  sistema_anterior_ = sistema;
  proceso_anterior_ = proceso;
  ventana_anterior_ = ventana;
  reloj_anterior_ = ahora;
  return medicion;
}

}
