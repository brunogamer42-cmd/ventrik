#include "common/cifrado/inicio.h"

#include <windows.h>

#include <mbedtls/entropy.h>
#include <mbedtls/threading.h>
#include <psa/crypto.h>

#if !defined(MBEDTLS_THREADING_C) || !defined(MBEDTLS_THREADING_ALT)
#error "Mbed TLS tiene que tener control de hilos (ver ventrik_mbedtls_config.h)"
#endif

namespace ventrik::cifrado {

namespace {

void crear_candado(mbedtls_threading_mutex_t* candado) noexcept {
  // En XP, el bit alto reserva todo al crearlo: después, cerrar el candado no puede fallar por memoria.
  candado->lista = InitializeCriticalSectionAndSpinCount(&candado->seccion, 0x80000000) ? 1 : 0;
}

void liberar_candado(mbedtls_threading_mutex_t* candado) noexcept {
  if (candado->lista != 0) {
    DeleteCriticalSection(&candado->seccion);
    candado->lista = 0;
  }
}

int cerrar_candado(mbedtls_threading_mutex_t* candado) noexcept {
  if (candado->lista == 0) {
    return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
  }
  EnterCriticalSection(&candado->seccion);
  return 0;
}

int abrir_candado(mbedtls_threading_mutex_t* candado) noexcept {
  if (candado->lista == 0) {
    return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
  }
  LeaveCriticalSection(&candado->seccion);
  return 0;
}

}  // namespace

Inicio::Inicio() {
  mbedtls_threading_set_alt(crear_candado, liberar_candado, cerrar_candado, abrir_candado);
  ok_ = psa_crypto_init() == PSA_SUCCESS;
}

Inicio::~Inicio() {
  mbedtls_psa_crypto_free();
  mbedtls_threading_free_alt();
}

int aleatorio(void*, unsigned char* salida, std::size_t largo) {
  return psa_generate_random(salida, largo) == PSA_SUCCESS ? 0 : MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

}  // namespace ventrik::cifrado
