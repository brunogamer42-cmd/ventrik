#pragma once

// Números aleatorios para Mbed TLS en la versión clásica (ver entropia_xp.cpp).

#include <cstddef>

// La llama Mbed TLS cada vez que necesita números aleatorios del sistema.
// Llena `largo` bytes de `salida`, deja en `*obtenidos` cuántos llenó y
// devuelve 0, o un error de Mbed TLS si no pudo (y entonces `*obtenidos`
// queda en 0). Solo existe en la versión clásica.
extern "C" int mbedtls_hardware_poll(void* datos, unsigned char* salida,
                                     std::size_t largo,
                                     std::size_t* obtenidos) noexcept;
