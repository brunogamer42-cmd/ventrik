// Números aleatorios para Mbed TLS en la versión clásica.
//
// Mbed TLS los saca de BCryptGenRandom, que no existe en Windows XP. En la
// versión clásica se le apaga esa fuente
// (third_party/integracion/mbedtls/ventrik_mbedtls_config.h) y Mbed TLS los
// pide a mbedtls_hardware_poll, que está acá y usa RtlGenRandom: existe
// desde XP y es la misma que usa la biblioteca de C de Microsoft para
// rand_s. Este archivo solo se compila en la versión clásica.

#include "common/entropia_xp.h"

#include <atomic>
#include <initializer_list>
#include <limits>
#include <type_traits>

#include <windows.h>

#include <gsl/gsl>
#include <mbedtls/entropy.h>

#include "common/sistema.h"

#if !defined(MBEDTLS_NO_PLATFORM_ENTROPY) || !defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
#error "La versión clásica necesita MBEDTLS_NO_PLATFORM_ENTROPY y MBEDTLS_ENTROPY_HARDWARE_ALT (ver ventrik_mbedtls_config.h)"
#endif

// La firma tiene que ser la que Mbed TLS espera de una fuente de números
// aleatorios. Su declaración está en un encabezado privado de Mbed TLS y,
// por ser extern "C", el enlazador no avisaría si no coincide.
static_assert(std::is_convertible<decltype(&mbedtls_hardware_poll),
                                  mbedtls_entropy_f_source_ptr>::value,
              "mbedtls_hardware_poll no tiene la firma que espera Mbed TLS");

namespace {

// RtlGenRandom se exporta con el nombre SystemFunction036.
using FuncionRtlGenRandom = BOOLEAN(WINAPI*)(PVOID, ULONG);

// Desde Windows 7, advapi32.dll no tiene RtlGenRandom adentro: la reenvía a
// cryptbase.dll, que no es una de las DLL protegidas de Windows y podría
// buscarse al lado del .exe. Por eso se carga primero cryptbase.dll con su
// ruta completa y la función se toma de ahí. En XP cryptbase.dll no existe
// y la función está en advapi32.dll.
FuncionRtlGenRandom buscar_rtlgenrandom() noexcept {
  for (const wchar_t* dll : {L"cryptbase.dll", L"advapi32.dll"}) {
    const HMODULE modulo = ventrik::cargar_de_sistema(dll);
    if (modulo == nullptr) {
      continue;
    }
    const FARPROC funcion = GetProcAddress(modulo, "SystemFunction036");
    if (funcion != nullptr) {
      return reinterpret_cast<FuncionRtlGenRandom>(funcion);
    }
  }
  return nullptr;
}

// Se busca una sola vez. Si dos hilos la buscan a la vez, encuentran la
// misma, así que no importa cuál la guarde.
std::atomic<FuncionRtlGenRandom> g_rtlgenrandom{nullptr};

FuncionRtlGenRandom rtlgenrandom() noexcept {
  FuncionRtlGenRandom funcion = g_rtlgenrandom.load(std::memory_order_acquire);
  if (funcion == nullptr) {
    funcion = buscar_rtlgenrandom();
    if (funcion != nullptr) {
      g_rtlgenrandom.store(funcion, std::memory_order_release);
    }
  }
  return funcion;
}

}  // namespace

extern "C" int mbedtls_hardware_poll(void* /*datos*/, unsigned char* salida,
                                     std::size_t largo,
                                     std::size_t* obtenidos) noexcept {
  if (obtenidos == nullptr) {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  *obtenidos = 0;
  if (largo == 0) {
    return 0;
  }
  if (salida == nullptr) {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  const FuncionRtlGenRandom generar = rtlgenrandom();
  if (generar == nullptr) {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }

  // RtlGenRandom recibe el largo como ULONG: se pide de a partes.
  const gsl::span<unsigned char> destino(salida, largo);
  constexpr std::size_t kMaximoPorVez = std::numeric_limits<ULONG>::max();
  std::size_t hecho = 0;
  while (hecho < largo) {
    const std::size_t resta = largo - hecho;
    const std::size_t parte = resta < kMaximoPorVez ? resta : kMaximoPorVez;
    const gsl::span<unsigned char> tramo = destino.subspan(hecho, parte);
    if (!generar(tramo.data(), static_cast<ULONG>(tramo.size()))) {
      return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
    hecho += parte;
  }
  *obtenidos = largo;
  return 0;
}
