# Ventrik — Mediciones de la etapa 1

Prueba de voz descartable, sin cifrado (rama local `prueba-de-voz`, no se publica). Fechas: 21 y 22 de octubre de 2026.

## Equipos

| Equipo | Procesador | Windows | Versión de Ventrik |
| --- | --- | --- | --- |
| PC | Intel Xeon E5-2690 v4, 2,6 GHz | 10 Pro (19045), 64 bits | moderna y clásica |
| HP 2133, la netbook de referencia | VIA C7-M, 1,2 GHz, un núcleo | XP SP3 | clásica |

## Demora (eco local: micrófono → Opus → parlantes en la misma PC)

"Calculada" suma las esperas del sistema de audio, el pedazo de 20 ms y Opus (6,5 ms). "Medida" es un pitido que sale por los parlantes y vuelve por el micrófono.

| Equipo y sistema de audio | Calculada | Medida | Procesador según Windows | Sonido |
| --- | --- | --- | --- | --- |
| PC, WASAPI (miniaudio) | 67 ms | sin parlantes para medir | ~0,5 % en silencio | bien |
| HP 2133, DirectSound (miniaudio) | — | 141 ms | 60-65 % | bien |
| HP 2133, WinMM con 3 esperas (miniaudio) | 187 ms | 172 ms | 0-8 % | bien |
| HP 2133, WinMM con 2 esperas (miniaudio) | 147 ms | 147 ms | ~20 % | entrecortado |
| HP 2133, puente propio con WinMM | 67 ms | **77-81 ms** | 20-35 % (casi todo Opus) | bien en la última prueba |

- **Por qué el puente propio en XP:**
  - miniaudio con DirectSound consulta cada 1 ms si hay audio: en la HP 2133 eso solo se come unos 50 puntos de procesador;
  - con WinMM no deja bajar las esperas de 40 ms;
  - el puente espera el aviso de Windows y usa esperas de 10 ms.
- **Lo que costó el puente:**
  - XP devuelve tarde los pedazos ya entregados y avisa mal la posición de los parlantes;
  - las correcciones de desfase basadas en esos datos tiraban audio que hacía falta;
  - la versión final no corrige nada y tiene lugar de sobra (64 pedazos hacia los parlantes, 8 para el micrófono);
  - micrófono y parlantes miden 48.000 muestras por segundo: no hay desfase real.
- **Pendiente:** el puente no tuvo una prueba larga (está marcado con VENTRIK_PENDIENTE). Para investigar cortes, `ventrik_eco.exe --registro` anota cada pedazo y los MHz del procesador.

## Voz por la red local (UDP, puerto 7700, buffer de 40 ms)

| Prueba | Resultado |
| --- | --- |
| PC con dos copias (moderna con miniaudio ↔ clásica con el puente) | 50 paquetes por segundo en cada sentido, 0 perdidos, 0 tarde, 0 desordenados, ping 0 ms, demora estimada de punta a punta 107 ms |
| PC (moderna) ↔ HP 2133 (clásica) en la red local | anda bien en los dos sentidos (sin números anotados) |

La demora estimada incluye los 40 ms que espera el buffer para ordenar paquetes; un buffer que se ajuste solo (etapas 2 y 3) puede bajarla en una red sin pérdidas.

## Procesador de Opus (por cada pedazo de 20 ms, en % de un núcleo)

| Comprimir | PC: compl. 10 | 5 | 0 | HP 2133: compl. 10 | 5 | 0 |
| --- | --- | --- | --- | --- | --- | --- |
| Banda completa, 48 kHz, 24 kbps | 2,2 % | 1,3 % | 0,8 % | 31,2 % | 17,6 % | 10,0 % |
| Banda ancha, 16 kHz, 20 kbps | 1,9 % | 1,1 % | 0,5 % | 26,3 % | 14,5 % | 7,3 % |
| Banda angosta, 8 kHz, 12 kbps | 0,9 % | 0,5 % | 0,3 % | 12,3 % | 7,7 % | 3,8 % |

| Descomprimir voz en banda completa | PC | HP 2133 |
| --- | --- | --- |
| Saliendo a 48 kHz | 0,3 % | 3,7 % |
| Saliendo a 16 kHz | 0,2 % | 2,4 % |
| Saliendo a 8 kHz | 0,2 % | 2,7 % |

- **Opus en enteros** (en lugar de con coma) es más lento en los dos equipos: en la HP 2133, 7,73 contra 6,75 ms por pedazo en complejidad 10. Se queda con coma.
- **Decisión:** niveles de "Consumo de la voz", todos con Opus (ver docs/diseno.md, "Consumo de la voz"). En la HP 2133, el nivel Bajo deja unos 7 % hablando y 2,4 % por voz escuchada.

## Comparación con las metas de docs/diseno.md ("Rendimiento", modo Rendimiento)

| Métrica | Meta | PC | HP 2133 |
| --- | --- | --- | --- |
| RAM del programa | Hasta 50 MB | 9-11 MB | sin anotar |
| Procesador sin nadie hablando | Casi 0 % | 0 % del audio, ~0,5 % en total | bajo con el puente |
| Procesador con 3 personas hablando | Hasta 2 % | ~1 % (3 voces que se descomprimen) | ~7 % en nivel Bajo (no cumple: meta ajustada para la netbook) |
| Demora de la voz en red local | Hasta 80 ms | eco 67 ms; por la red, 107 ms estimados | eco 77-81 ms medidos |
| Conexión por persona hablando | Unos 50 kbps | unos 41 kbps calculados (Opus 24 kbps + cabeceras, sin cifrado) | igual |
| Arranque | Menos de 1 segundo | sin medir | sin medir |

**Criterio de la etapa 1** ("la demora en red local está cerca de las 80 ms de la meta y la netbook aguanta"): se cumple con el puente propio en XP y el nivel de consumo Bajo en la netbook. La red queda en 107 ms estimados por la espera fija del buffer.

## Pendiente

- Prueba de 10 minutos en la PC y en la HP 2133 por la red local, con registro cada segundo: se hace con la primera beta, con la interfaz completa.
- Números de la prueba PC ↔ HP 2133 por la red (ping, perdidos, demora).
- RAM en la HP 2133, ancho de banda medido (no calculado) y tiempo de arranque.
- Prueba en Windows 11 (de la etapa 0).
