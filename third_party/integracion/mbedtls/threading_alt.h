/* Candados sobre CRITICAL_SECTION, que existe en XP (los SRW locks, desde Vista). */
#ifndef VENTRIK_THREADING_ALT_H
#define VENTRIK_THREADING_ALT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef struct mbedtls_threading_mutex_t {
    CRITICAL_SECTION seccion;
    /* 0 si no se pudo crear: entonces cerrarlo devuelve error. */
    char lista;
} mbedtls_threading_mutex_t;

#endif
