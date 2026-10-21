/*
 * RNNoise 0.2 incluye "os_support.h" en src/vec.h cuando el compilador no
 * anuncia SSE2 (Visual Studio nunca lo anuncia), pero ese archivo no viene
 * en el paquete ni está en su repositorio.
 *
 * Esa parte de src/vec.h es idéntica a dnn/vec.h de Opus 1.5.2, y lo único
 * que usa de "os_support.h" es OPUS_CLEAR. Acá va con la misma definición
 * que tiene Opus en celt/os_support.h.
 */
#ifndef VENTRIK_RNNOISE_OS_SUPPORT_H
#define VENTRIK_RNNOISE_OS_SUPPORT_H

#include <string.h>

#define OPUS_CLEAR(dst, n) (memset((dst), 0, (n)*sizeof(*(dst))))

#endif
