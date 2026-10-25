# Ventrik — Plan de optimización

Ventrik tiene que consumir poco en cualquier PC:
- en una común, para no molestar a lo demás (un juego, el navegador, la batería de una notebook);
- en una débil, porque ahí es la diferencia entre andar bien y no andar.

Este plan vale para **las dos versiones** (la moderna, para Windows 10 y 11, y la clásica, para XP a 8.1) y para **los dos modos** (Rendimiento y Visual). **El modo Visual se ve y se comporta igual:** lo que cambia es cuánto le cuesta a la PC.

El plan tiene dos partes:
- **Lo que ya existe** se optimiza **de un solo tirón**, con todas las pruebas al final (sección 17).
- **Lo que todavía no existe** recibe reglas que valen desde el día en que se programa (sección 15).

Lo que cambia algo que se ve o se oye, o que toca el protocolo o la seguridad, está marcado **(decide el dueño)** y se resuelve antes del tirón (sección 18).

## En simple

**Lo que ya se ganó** (medido en la HP 2133 con XP, con `--registro`):

| Situación | Antes | Ahora |
| --- | --- | --- |
| Quieta | 4 % | cerca de 1 % |
| Escuchando a otro | 6 % | cerca de 2 % |
| Hablando (tecla apretada) | 96 % | 24 % (17 % la ventana, 7 % la voz) |
| Moviendo el mouse encima | 65 % (picos de 100 %) | cerca de 14 % |
| Microcortes de la PC hacia la HP | 1 cada 1,6 s | ninguno del programa (quedan los saltos del WiFi) |
| Memoria | — | 8 MB en la HP; 53 MB en la PC con la moderna, en buena parte del controlador de video |

**Lo más importante que falta:**
1. **Cada dibujo más barato.** En la HP, cada imagen cuesta 15 ms: 4,6 en armar la ventana y 10,5 en Direct3D. Gran parte de lo de Direct3D se puede sacar.
2. **Escalones de ahorro.** Con un juego en pantalla completa, la PC bloqueada o la ventana tapada, el programa casi no gasta. La voz y la tecla para hablar siguen igual.
3. **Menos despertares.** Hoy el programa se despierta unas 150 veces por segundo aunque nadie hable. Cada despertar gasta, sobre todo la batería.
4. **Que la PC pueda dormir.** Un sonido abierto, aunque esté en silencio, no deja que Windows ahorre energía.
5. **La voz y la red:**
   - si estás solo en el canal, no comprimir ni mandar voz;
   - en los silencios con la tecla apretada, mandar casi nada;
   - pedir el historial del chat a la medida de la pantalla.
6. **La memoria:**
   - el chat guarda solo lo cercano a lo que estás viendo (tu ejemplo);
   - el programa aprovecha la memoria virtual de Windows sin arriesgar la voz (sección 9).
7. **El modo Visual más liviano, igual de lindo:**
   - animaciones solo mientras se mueven;
   - fondos dibujados una vez y reutilizados;
   - solo la zona que cambia.
8. **El código:** nada de memoria pedida en cada dibujo ni en cada paquete de voz, y nada recalculado si no cambió.
9. **Reglas para todo lo que viene:** notificaciones, mezcla de voces, archivos, antivirus, red, lista pública, modo Visual completo, compartir pantalla, amigos y llamadas, copias en Google Drive.

## 1. Cómo se trabaja y se mide

### 1.1 Método

- **Primero el dato, después el arreglo** (regla del proyecto):
  - se mide antes y después, en la PC con la moderna y en la HP con la clásica, y en Windows 11 cuando haya una PC;
  - en el tirón (sección 17), el "antes" son los números de hoy y el "después" se mide al final.
- **Se mide en la PC más débil.** Lo que en una PC común es 1 ms, en la HP son 15 o 20.
- **Lo que no gana nada medible se deshace** antes del commit.
- **Presupuestos:** cada pantalla tiene un tope de costo por cuadro, medido con el banco sin ventana, y cada situación tiene metas (1.4).
- **Lo que ahorra nunca saca seguridad** (sección 16).

### 1.2 Herramientas propias

**Lo que ya hay:**
- **`ventrik.exe --registro <archivo>` (o `registro.bat`):** una línea por segundo. Anota:
  - procesador del programa, de la ventana y de toda la PC;
  - tiempo ocupado de la ventana;
  - cuadros, imágenes a la pantalla, vértices y lo que tarda cada parte del dibujo;
  - colchón, recortes, voz tirada hacia los parlantes, pedazos que faltaron;
  - memoria.
- **El diagnóstico** de la barra de estado.

**Lo que se suma en el tirón:**
- **Despertares por segundo de cada hilo** (ventana, audio, red, vigía).
- **Memoria propia (`PrivateUsage`, con `PROCESS_MEMORY_COUNTERS_EX`).** El "working set" de hoy también cuenta las bibliotecas del controlador de video, que se comparten con otros programas.
- **Fallas de página por segundo,** del mismo contador: cuántas veces el programa necesitó algo que Windows había sacado de la RAM.
- **Bytes por segundo de red,** de subida y de bajada, separados en voz y el resto.
- **Tiempo de arranque.** La meta del diseño es menos de 1 segundo.
- **Banco sin ventana permanente:** una prueba que no corre sola (`doctest::skip`) y mide el costo por cuadro de cada pantalla con un servidor chico y con el más grande (60 canales y 512 personas). `check.ps1` muestra sus números sin fallar por ellos.

### 1.3 Herramientas de Windows

| Para | Moderna (Windows 10 y 11) | Clásica (XP en adelante) |
| --- | --- | --- |
| Dónde se va el procesador | Windows Performance Recorder y Analyzer (WPR/WPA), con "CPU Usage (Precise)" | Very Sleepy (muestrea el programa sin modificarlo) |
| Despertares | WPA: cambios de contexto por hilo | Process Explorer: columna "Context Switch Delta" |
| Por qué no ahorra energía | `powercfg /energy` (avisa si alguien cambió el reloj de Windows) y `powercfg /requests` (avisa si un sonido abierto no deja dormir) | — |
| Memoria propia y fallas de página | Administrador de tareas (memoria privada) | Monitor de rendimiento ("Private Bytes", "Page Faults/sec") |
| Red por programa | Monitor de recursos | Nuestro registro |
| Placa de video | GPUView o el Administrador de tareas | Nuestro registro (`direct3d_ms`) |

### 1.4 Metas

- **PC común:** valen las del diseño ("Rendimiento"):
  - RAM hasta 50 MB en Rendimiento y 150 MB en Visual;
  - casi 0 % de procesador sin nadie hablando;
  - menos de 1 kbps en silencio;
  - arranque en menos de 1 segundo.
- **Netbook (decide el dueño, para agregar al diseño):**
  - quieta, hasta 1 %;
  - escuchando, hasta 3 % más la voz;
  - con el mouse, hasta 10 %;
  - hablando, la voz más 5 %;
  - memoria, hasta 20 MB.
- **Energía (moderna):**
  - sin nadie hablando y con la ventana quieta, menos de 20 despertares por segundo;
  - nada que impida dormir a la PC si no hay una llamada en curso.

## 2. Principios (para todo el código, el que hay y el que viene)

1. **Nada se recalcula si no cambió lo que lo produce.**
2. **Nada se dibuja ni se manda a la pantalla si la imagen no cambió** (ya rige).
3. **Nada se despierta sin tener trabajo:**
   - un evento antes que una consulta periódica;
   - cada espera dura hasta lo próximo que hay que hacer;
   - en la moderna, las esperas que no son críticas aceptan tolerancia.
4. **Lo que no se ve no se calcula ni se guarda entero.**
5. **Todo lo que crece tiene techo y regla de descarte.**
6. **Por la red va solo lo necesario:** cambios en vez de listas, nada en silencio, nada si nadie escucha.
7. **La voz siempre primero:**
   - el hilo de audio nunca espera a otro ni toma un candado que otro tenga (ya: la cola de la voz no usa candados);
   - la memoria de la voz nunca se va al disco (sección 9);
   - lo caro va en hilos o procesos de baja prioridad.
8. **Escalones según la situación** (sección 7).
9. **En lo que corre en cada cuadro o en cada paquete:**
   - ningún recorrido que multiplique dos listas;
   - ninguna memoria pedida y soltada en cada vuelta.
10. **Lo moderno se aprovecha en la moderna sin romper la clásica:** con `GetProcAddress` y una alternativa (CLAUDE.md).
11. **El modo Visual se ve igual;** solo cambia cuánto cuesta (sección 6).
12. **Lo que ahorra nunca saca seguridad** (sección 16).

## 3. Código (cómo se escribe)

- **3.1 Datos seguidos en la memoria y búsquedas directas.**
  - El procesador lee mucho más rápido lo que está junto (regla Per.19 de las C++ Core Guidelines).
  - Para lo que se recorre seguido: `std::vector` ordenado o índices, como el de personas por canal, en vez de `std::map` o listas enlazadas.
- **3.2 Nada de memoria pedida en cada dibujo ni en cada paquete.**
  - **Hoy:** cada paquete de voz que sale arma dos `std::vector` (`MedioSubida` y el datagrama de `CanalVoz::enviar`), y el servidor otro por cada paquete reenviado (`Voz::reenviar`).
  - **Arreglo:** búferes que viven en la clase y se reutilizan (`clear()` no suelta la capacidad).
  - **Dibujo:** los textos de un solo cuadro van a una memoria por cuadro (`std::pmr::monotonic_buffer_resource`, que se vacía al terminar el cuadro), o se guardan (3.3). Hay que comprobar que `<memory_resource>` anda con el compilador de la clásica.
- **3.3 Caché por entrada.** Se guarda el resultado junto con lo que lo produjo, y se rehace solo si eso cambió.
  - Ya está en alturas, horas e iniciales de los mensajes, en el índice por canal y en la huella del dibujo.
  - Falta en los textos de la barra de arriba, la lista y la barra de estado (5.6).
- **3.4 Mover en vez de copiar** (regla Per.7):
  - `std::string_view` y `gsl::span` para mirar sin copiar;
  - `reserve` cuando se sabe el tamaño;
  - nada de excepciones ni de `iostream` en lo que corre seguido.
- **3.5 Toda pantalla nueva pasa por el banco** con un servidor chico y uno grande.
- **3.6 Compilación:**
  - probar `/GL` y `/LTCG`, que optimizan todo el programa junto;
  - en la clásica, probar `/O1` (código más chico), que puede rendir más con la memoria rápida chica del C7;
  - queda solo lo que gane en el banco y en la HP.
  
  Las protecciones (`/guard:cf`, `/GS`, SafeSEH, DEP, ASLR) no se tocan.

## 4. Procesador, hilos y despertares

**Despertares de hoy, sin nadie hablando:**

| Hilo | Hoy | Plan |
| --- | --- | --- |
| Audio | 100 por segundo (pedazos de 10 ms, las dos versiones) | 4.4 |
| Vigía (tecla para hablar) | 50 por segundo (consulta cada 20 ms) | 4.3 |
| Ventana | 1 por segundo (aviso de la vigía) | 4.5 |
| Red | Solo cuando hay algo; cada 100 ms solo durante el saludo cifrado (ya hecho) | — |

- **4.1 Hilos fijos, como en el diseño:** audio, red, interfaz y tareas pesadas.
- **4.2 Prioridades.**
  - **Audio:** tiempo crítico. En la moderna, además, MMCSS ("Pro Audio", por `GetProcAddress`).
  - **Red:** alta.
  - **Ventana:** normal, y baja con un juego en pantalla completa (7.1).
  - **Tareas pesadas:** baja. En Windows 11, EcoQoS (`SetThreadInformation` con `ThreadPowerThrottling`), que Microsoft aconseja solo para trabajo que no se ve.
  - **En Windows 10 y 11:** que Windows no frene la voz cuando la ventana está minimizada. Se pide con el mismo `SetThreadInformation`, apagando el freno para los hilos de audio y red. Microsoft dice que los programas que reproducen sonido ya tienen prioridad, pero se comprueba en Windows 11.
- **4.3 Tecla para hablar por eventos.**
  - Teclado: Raw Input con `RIDEV_INPUTSINK`. Windows avisa cada tecla apretada o soltada aunque la ventana no tenga el foco, sin ganchos globales (que los antivirus miran mal).
  - Respaldo: una consulta cada 250 ms, para no quedar transmitiendo si se pierde un aviso de "soltada" (lo mismo hace otro programa de voz que usa este método).
  - Botones del mouse: se siguen consultando, porque el Raw Input del mouse avisa cada movimiento.
  - `WM_INPUT` no pide cuadros.
  - Pasa de 50 despertares por segundo a 4.
- **4.4 Audio (decide el dueño).**
  - Moderna: miniaudio usa pedazos de 10 ms con el perfil de baja demora. En Rendimiento: 20 ms y perfil normal.
  - Clásica: el puente WinMM usa pedazos de 10 ms. En Rendimiento: 20 ms.
  - En las dos, la mitad de despertares, con 10 ms más de demora.
- **4.5 El aviso de cada segundo,** solo con la ventana visible o con el registro prendido.
- **4.6 Temporizadores** (moderna).
  - Las esperas que no son críticas (pings, reintentos, el aviso de cada segundo) aceptan de 32 a 50 ms de tolerancia (`SetWaitableTimerEx`), para que Windows junte despertares. Es la recomendación de Microsoft, después de sacar los despertares que no hacen falta.
  - Nunca `timeBeginPeriod`: cambia el reloj de toda la PC y gasta batería.
- **4.7 La voz.**
  - Opus ya usa SSE, SSE2, SSE4.1 y AVX2, eligiendo según el procesador (comprobado en la clásica), y la complejidad ya depende de los núcleos.
  - Falta el nivel "Mínimo" (8 kHz) que pide el diseño.
  - **Solo en el canal, no se comprime:** la `Sala` sabe quién está en tu canal (`en_canal`). Si sos el único, la tecla no gasta procesador ni red (7 % en la HP). No cambia el protocolo.
- **4.8 Cifrado.**
  - X25519 primero, que es lo más rápido sin ayuda especial del procesador (ya).
  - AES si el procesador lo acelera (AES-NI); si no, ChaCha20 (ya).
  - La ayuda para AES del C7 (VIA PadLock) solo existe en Mbed TLS con el compilador GCC. Usarla obligaría a tocar Mbed TLS: descartada.

## 5. Placa de video y dibujo

**Ya hecho:**
- dibujar solo si algo cambia, y no mandar una imagen igual;
- el tope de 20 % del procesador;
- las teclas repetidas y el mouse quieto no piden dibujos.

**Por qué Direct3D gasta procesador si el trabajo es de la placa.** La placa dibuja, pero el procesador le prepara todo. Por cada imagen, en la HP:
- **Armar la ventana: 4,6 ms.**
- **Direct3D: 10,5 ms.**
  - Guarda y vuelve a poner todos los ajustes de la placa (un "bloque de estado" completo en cada imagen). Microsoft avisa que sacar ese seguimiento da una mejora "significativa".
  - Copia los vértices convirtiendo el color uno por uno.
  - Borra y copia la imagen entera (4 MB).
  - Pasa cada orden por el controlador y por el núcleo de Windows, que en XP es más caro.
- **La placa integrada usa la misma RAM que el procesador,** así que cada copia grande compite con todo lo demás.

En la PC, lo mismo cuesta menos de 1 ms. **Dibujar con GDI queda descartado por ahora:**
- obliga a rehacer todas las pantallas;
- GDI no dibuja letras con textura de forma eficiente;
- lo medido muestra que el problema era cuántas veces se dibuja.

Se reevalúa con los números de 5.1 y 5.2.

- **5.1 Renderizador propio** (código nuestro en `src/client/ui`; el de `third_party/imgui/backends` no se modifica).
  - Los ajustes de la placa se ponen una vez, al crear o recuperar el dispositivo.
  - Búferes que se agrandan pocas veces.
  - El color llega ya en el orden de la placa (`IMGUI_USE_BGRA_PACKED_COLOR` en un `imconfig` propio).
  - Se prueba `D3DCREATE_PUREDEVICE`, con vuelta al modo normal si falla.
  - No se borra la imagen si la interfaz siempre cubre toda la ventana.
  - Se mantiene `D3DCREATE_FPU_PRESERVE`.
- **5.2 Direct3D 9Ex con modo "flip"** (moderna, y la clásica desde Vista).
  - `Direct3DCreate9Ex` por `GetProcAddress`.
  - `D3DSWAPEFFECT_FLIPEX` desde Windows 7: Windows usa la imagen sin copiarla.
  - `PresentEx` avisa cuando la ventana está tapada (`S_PRESENT_OCCLUDED`), y ahí se deja de dibujar.
- **5.3 Mandar solo lo que cambió** (clásica sin composición del escritorio).
  - `D3DSWAPEFFECT_COPY` y `Present` con el rectángulo que cambió, calculado con la huella por zonas.
  - El rectángulo es solo una sugerencia para Windows: queda si la medición en la HP muestra que gana.
- **5.4 Medidor de nivel (decide el dueño):** 4 imágenes por segundo en Rendimiento y sin foco, y 10 en Visual. Hoy son 10, unos 17 % en la HP.
- **5.5 Rendimiento sin suavizado de bordes (decide el dueño):** `AntiAliasedLines` y `AntiAliasedFill` apagados y curvas con menos segmentos. En Visual queda igual.
- **5.6 Textos guardados:** conectados, ping, voz perdida, nombre de la tecla y subtítulos se rehacen solo cuando cambia el dato o el idioma.
- **5.7 Ritmo con el mouse:** en Rendimiento, como mucho 15 vueltas por segundo (hoy 28 en la HP, el 14 %).
- **5.8 Atlas de letras:**
  - ya se arma a medida que se usan las letras;
  - se mide su tamaño;
  - con 5.1 se evalúa una textura de un solo canal (`D3DFMT_A8`): 4 veces menos memoria.
- **5.9 Notebooks con dos placas:** no pedir la potente. Windows usa la integrada para un programa que no la pide. Regla: no agregar `NvOptimusEnablement` ni `AmdPowerXpressRequestHighPerformance`.

## 6. Modo Visual: se ve igual, cuesta menos

El diseño ya pide que las animaciones corran solo a la vista, a 30 cuadros por segundo como máximo, y quietas si Windows las tiene apagadas. Esto lo completa:

- **6.1 Animaciones solo mientras se mueven.** Cada una se calcula por tiempo y pide imágenes solo mientras dura. Al terminar, la ventana vuelve a quedar quieta.
- **6.2 Al ritmo de la pantalla.** Mientras hay una animación, la imagen se sincroniza con el monitor (`D3DPRESENT_INTERVAL_ONE`), así nunca se dibujan más imágenes de las que la pantalla muestra.
- **6.3 Solo la zona que se anima** (5.3, o región sucia con 9Ex).
- **6.4 Capas quietas guardadas en una textura.**
  - Fondos y paneles decorados (degradados, triángulos, rayado, bordes, sombras) se dibujan una vez en una textura y después se usa esa textura.
  - Se rehacen al cambiar el tamaño, el tema o al recuperar el dispositivo, que es lo delicado en Direct3D 9.
  - Así lo hacen CEGUI, Flax y el motor C4. Encima va lo que cambia.
- **6.5 Efectos caros precalculados.** Un resplandor o una sombra grande se calcula una vez y queda en textura.
- **6.6 En pausa cuando no se ve:** minimizada, sin foco, con un juego en pantalla completa o con la PC bloqueada (diseño).
- **6.7 Respeta a Windows.** Si la persona apagó las animaciones (`SPI_GETCLIENTAREAANIMATION`, desde Vista, releído con `WM_SETTINGCHANGE`), se muestra el estado final (diseño).
- **6.8 Ahorro de batería (decide el dueño).** Con el ahorro de batería de Windows (`GUID_POWER_SAVING_STATUS`), las animaciones del Visual a la mitad del ritmo.
- **6.9 Imágenes, emojis y GIF:**
  - se decodifican al tamaño mostrado;
  - íconos y emojis en un atlas;
  - la caché tiene un techo en bytes y se puede soltar si falta memoria (sección 9);
  - un GIF se anima solo si se ve y la ventana tiene foco, y guarda solo el cuadro actual.

## 7. Escalones de ahorro (las dos versiones)

- **7.1** Qué hace el programa en cada situación:

| Situación | Qué hace | Cómo se detecta |
| --- | --- | --- |
| Con foco | Normal | — |
| Sin foco | Sin carteles; medidor a 4 por segundo en Rendimiento; animaciones del Visual en pausa | `WM_ACTIVATEAPP` |
| Minimizada | No dibuja (ya); sin el aviso de cada segundo; pings cada 20 s | `IsIconic` |
| Tapada por completo | No dibuja | Moderna: `S_PRESENT_OCCLUDED` (5.2) |
| Un juego u otro programa en pantalla completa | No dibuja; suelta las texturas y búferes de Direct3D; baja la prioridad de la ventana; pings cada 20 s. **La voz y la tecla siguen igual.** También es lo que necesita el modo Automático del diseño para pasar a Rendimiento | Cada 1 a 2 s. Moderna: `SHQueryUserNotificationState` (`QUNS_RUNNING_D3D_FULL_SCREEN`, `QUNS_BUSY`, `QUNS_PRESENTATION_MODE`). Las dos: la comprobación de Chromium (la ventana de adelante cubre todo el monitor, sin bordes ni barra de título) |
| PC bloqueada, sesión desconectada o protector de pantalla | No dibuja | `WTSRegisterSessionNotification` por `GetProcAddress`. En XP, el dispositivo perdido ya frena el dibujo |
| Ahorro de batería de Windows | Animaciones más lentas y medidor como sin foco **(decide el dueño)** | `RegisterPowerSettingNotification` con `GUID_POWER_SAVING_STATUS` (Windows 10 y 11) |
| Poca memoria en la PC | Suelta las cachés (sección 9) | `CreateMemoryResourceNotification` (desde XP) |

## 8. Memoria

- **8.1 El chat guarda solo lo cercano (decide el dueño: cambia el protocolo).**
  - Hoy cada chat guarda hasta 10 000 mensajes o 2 MiB, y hay dos a la vez.
  - Propuesta: unas 3 páginas alrededor de lo que estás viendo (unos 300 mensajes). Al alejarte se sueltan los del otro extremo, y al volver se piden de nuevo.
  - Pedir los más viejos ya existe. Pedir "los más nuevos desde tal número" es un pedido nuevo: revisión de subagente, fuzzing y las dos versiones.
- **8.2 Cada nombre una sola vez:** una tabla de nombres por chat en vez de una copia del autor por mensaje.
- **8.3 Montón de memoria de XP** (clásica). Se activa el de baja fragmentación (`HeapSetInformation` con valor 2). En XP viene apagado y desde Vista se usa solo.
- **8.4 Búferes de Mbed TLS:** 16 KB de entrada y 16 de salida por conexión, unos 16 MB con 512 conexiones en el servidor. La salida se puede achicar (`MBEDTLS_SSL_OUT_CONTENT_LEN`) si se sabe lo más grande que se manda. Toca el cifrado: revisión de subagente, a evaluar.
- **8.5 Texturas sueltas** al minimizar y con un juego en pantalla completa (7.1).
- **8.6 Todo con techo.** Se anota en docs/estado.md el techo de cada colección: novedades (4096), colchón, chat, personas (1024), canales (256), registro, imágenes. Lo nuevo no entra sin techo.

## 9. Memoria virtual y archivo de paginación

Windows ya usa la memoria virtual con todos los programas:
- **Lo que hace solo:** cuando la RAM no alcanza, saca de la RAM lo que hace rato no se usa. Lo puede tirar, si se puede volver a leer de un archivo, o guardarlo en el archivo de paginación. Cuando se vuelve a necesitar, lo trae.
- **El problema:** traerlo del disco es lento, y en la HP más todavía.

Ventrik la aprovecha así:

- **9.1 Lo que se puede volver a armar se marca como descartable** (moderna, desde Windows 8.1). Imágenes decodificadas, letras de tamaños que no se ven, mensajes fuera de la ventana:
  - se ofrecen a Windows con `OfferVirtualMemory`, y Windows puede tirarlos si le falta RAM, **sin escribirlos en el archivo de paginación**;
  - para volver a usarlos, `ReclaimVirtualMemory` dice si siguen ahí o hay que rearmarlos;
  - en la clásica, el aviso de poca memoria (9.3) hace lo mismo a mano.
- **9.2 Lo grande y de solo lectura, en archivos mapeados** (las dos, desde XP).
  - Lo que se lee de un archivo y no cambia (las letras dentro del .exe, que ya se usan así; la caché de imágenes en disco; más adelante la base de ClamAV y los archivos que se envían) se usa con `CreateFileMapping` y `MapViewOfFile`.
  - Windows trae a la RAM solo las partes que se usan, y si falta memoria las suelta sin escribir nada, porque ya están en el archivo.
- **9.3 Cuando Windows avisa que falta memoria, se sueltan las cachés** (las dos, desde XP). `CreateMemoryResourceNotification` da un aviso cuando queda poca memoria física. El programa suelta imágenes, textos guardados y mensajes fuera de la ventana. Firefox hace lo mismo.
- **9.4 Las cachés se ajustan a la PC** (las dos). Con `GlobalMemoryStatusEx` se sabe cuánta RAM hay y cuánta queda libre de verdad, y los techos de las cachés salen de ahí: una PC con 1 GB tiene cachés más chicas que una con 16.
- **9.5 Lo de fondo, con prioridad de memoria baja** (moderna, desde Windows 8). Las tareas pesadas (decodificar imágenes, copias, archivos) bajan su prioridad de memoria (`ThreadMemoryPriority`, `MEMORY_PRIORITY_LOW`). Así Windows saca primero sus páginas y no las de la voz o la ventana.
- **9.6 La voz nunca va al disco** (las dos).
  - La memoria de la voz (búferes de audio, el estado de Opus y del colchón) es poca y se usa cada 10 ms. Aun así, con la ventana minimizada y un juego comiéndose la RAM, Windows la puede sacar, y traerla del disco corta la voz.
  - Por eso solo esa parte se fija en la RAM (`VirtualLock`), subiendo un poco el mínimo de memoria del programa (`SetProcessWorkingSetSize`).
  - Microsoft avisa que fijar mucho perjudica a toda la PC, así que se fija lo justo: menos de 1 MB.
- **9.7 No pelear con Windows:**
  - no vaciar la memoria a la fuerza (Windows ya recorta la de un programa minimizado, y forzarlo hace que todo vuelva más lento);
  - no apagar el archivo de paginación ni pedir que lo cambien;
  - no fijar nada fuera de 9.6.
- **9.8 Se mide.** Con fallas de página por segundo y memoria propia en el registro (1.2), y una prueba en la HP con un programa pesado abierto y Ventrik minimizado.

## 10. Red

- **10.1 Solo en el canal, no se manda voz** (4.7).
- **10.2 Silencio con la tecla apretada (DTX).**
  - En los silencios, Opus manda más o menos un paquete cada 400 ms en lugar de 50 por segundo. Así lo informan en la lista de Opus; se comprueba con la 1.5.2.
  - El que escucha distingue silencio de pérdida: no lo cuenta como perdido y no recorta el colchón.
  - Lleva pruebas y fuzzing del colchón.
- **10.3 Historial a la medida de la pantalla:** unos 64 mensajes al entrar a un canal en vez de 256, y más al subir.
- **10.4 Paquetes más largos.**
  - El diseño ya los usa en la calidad dinámica (Media 40 ms, Baja y Mínima 60 ms), que llega en la etapa 8.
  - Cada paquete de voz lleva unos 65 bytes de cabeceras. Cuenta propia: a 20 ms son unos 26 kbps solo de cabeceras; a 40 ms, unos 13.
  - Un modo liviano propio del programa, con 40 ms y 20 ms más de demora, **decide el dueño**.
- **10.5 Corrección de errores de Opus solo cuando hay pérdida** (ya en el diseño). Necesita que los programas informen la pérdida, cosa que llega con la calidad dinámica (etapa 8).
- **10.6 Pings según la situación.**
  - Visible: TCP y voz cada 5 s.
  - Minimizada o con juego en pantalla completa: cada 20 s. El servidor corta a los 30 s sin datos, y los routers suelen guardar la conexión de UDP 2 minutos o más (lo habitual es mandar algo cada 20 a 60 s).
  - Con la voz conectada, el gráfico usa el ping de la voz y se ahorra el otro.
- **10.7 Juntar lo que sale junto.** Con `TCP_NODELAY` cada mensaje sale enseguida. Lo que sale en la misma vuelta del bucle va en un solo registro de TLS, porque cada registro suma su cabecera y su sello.
- **10.8 Prioridad de la voz en la red** (desde Vista). Los paquetes de voz se marcan como voz (DSCP 46, con qWAVE `QOSAddSocketToFlow`; en Windows moderno `setsockopt` ya no sirve, según Chromium). No ahorra: mejora la voz cuando la red está cargada.
- **10.9 Tamaño variable de la voz (decide el dueño, porque es seguridad).**
  - Opus usa por defecto bitrate variable (VBR), que ahorra datos.
  - La RFC 7587 y estudios publicados avisan que, cifrada, el tamaño de cada paquete puede delatar algo de lo que se dice.
  - Opciones: VBR restringido (`OPUS_SET_VBR_CONSTRAINT`) o bitrate fijo.
- **10.10 El servidor** ya manda cambios, avisa los mensajes nuevos como mucho una vez por segundo y reenvía la voz sin descomprimirla.
- **10.11 Sin compresión extra:** el protocolo ya es compacto, y agregar una biblioteca va contra las reglas.

## 11. Disco

- **11.1 programa.dat:** se escribe solo cuando cambia algo, de forma segura (temporal más `FlushFileBuffers`), y se juntan varios cambios seguidos en una escritura.
- **11.2 Registro del programa** (diseño: las últimas 200 líneas): un anillo fijo en memoria, que se escribe al disco solo para un reporte o un cierre con error.
- **11.3 Servidor:**
  - SQLite en modo WAL con `synchronous=NORMAL`, que sincroniza mucho menos. En un caso medido, de 37 minutos bajó a menos de 3.
  - Sentencias preparadas reutilizadas, escrituras juntas en una transacción, `cache_size` con techo y `wal_checkpoint` periódico, en cada conexión.
- **11.4 Todo lo grande va por pedazos y con techo:** archivos, caché de imágenes en disco y copias.

## 12. Energía y batería (sobre todo la moderna)

- **12.1 Despertares:** sección 4.
- **12.2 El sonido abierto no deja dormir a la PC.**
  - Aunque sea silencio, aparece en `powercfg /requests` ("An audio stream is currently in use"). Se vio con otro programa de voz, y Chrome lo cierra a los 10 o 15 s de silencio, según un informe en Mozilla.
  - Propuesta **(decide el dueño)**: cerrar los parlantes después de un rato sin voz y abrirlos cuando llega. Antes se mide cuánto tardan en volver.
- **12.3 Ahorro de batería de Windows** (`GUID_POWER_SAVING_STATUS`): el programa se entera y aplica 6.8 y 7.1. Microsoft pide eso a los programas.
- **12.4 EcoQoS** para tareas pesadas, y que Windows no frene la voz (4.2).
- **12.5 La placa integrada** en notebooks con dos (5.9).
- **12.6 No impedir que la PC duerma** (`SetThreadExecutionState`), salvo, si hace falta, durante una llamada.

## 13. Arranque y tamaño

- **13.1 Medir** el arranque en frío y en caliente. Metas del diseño: 1 s en Rendimiento y 2 en Visual.
- **13.2 Cargar solo lo que hace falta:**
  - el audio se abre al entrar a un servidor y las letras se arman al usarlas (ya);
  - lo de uso ocasional se carga recién al usarlo: `wtsapi32`, `shell32` (pantalla completa), `qwave`, Direct3D 9Ex, `avrt` (MMCSS), las notificaciones de Windows 10 y 11;
  - todo con `GetProcAddress` y `LoadLibrary` desde la carpeta del sistema, como ya exige la seguridad del programa.
- **13.3 Tamaño:**
  - probar `/O1` en la clásica (3.6);
  - medir cuánto pesan las letras incluidas.
  
  Las letras de los nombres del modo Visual (Bungee, Pacifico y las demás) se cargan recién cuando alguien las usa.

## 14. Servidor

- **14.1 IOCP** en vez de `select` (desde XP), con "un hilo por núcleo" de la etapa 8. En la moderna, para servidores de cientos de personas, se evalúa además RIO (Windows 8 en adelante): búferes registrados una sola vez, que bajan el costo de cada paquete.
- **14.2 SQLite:** 11.3.
- **14.3 Ninguna memoria pedida por paquete reenviado:** 3.2.
- **14.4 Búferes de TLS por conexión:** 8.4.
- **14.5 Medir todo en la prueba de estrés** (propuesta como 2.7): procesador, memoria y red por persona conectada, con 1, 64 y 512 personas.

## 15. Lo que viene: reglas por etapa

Estas reglas no son del tirón: se cumplen cuando se programa cada parte. Lo que el diseño ya pide se repite para tenerlo a mano.

**Etapa 3 — Voz completa:**
- **Notificaciones:**
  - en la moderna, las de Windows se cargan recién con la primera notificación;
  - en la clásica, ventana propia sin Direct3D (diseño), sin hilo extra, que se destruye al cerrarse;
  - el texto se pide al servidor solo cuando se va a mostrar (diseño).
- **Varias voces y mezcla:**
  - se descomprime solo a quien habla y no está silenciado, y se escuchan como mucho las 4 más fuertes (diseño);
  - el volumen por persona es una multiplicación en la mezcla.
- **Colchón que se ajusta solo:** se acelera o se estira la voz de a poquito sin cambiar el tono, como hace WebRTC (NetEQ), en vez de tirar pedazos.
- **Modo por voz:** antes de Opus, una medición barata de volumen decide si hay voz. En silencio no se comprime nada.
- **RNNoise y cancelación de eco:**
  - apagados por defecto en PCs de un núcleo (diseño);
  - se mide su costo en la HP antes de ofrecerlos ahí;
  - la cancelación de eco, solo sin auriculares (diseño);
  - si hay que cambiar la frecuencia del sonido, con la calidad más baja que suene bien, medida.
- **Bajar otros programas cuando alguien habla:** con la API de sesiones de audio de Windows 7, solo mientras alguien habla.
- **Ajustes:** al abrir y cambiar, nunca en cada cuadro.

**Etapa 4 — Identidad y seguridad:**
- Argon2id con parámetros que la netbook aguante (por ejemplo, la segunda opción de la RFC 9106: 64 MiB y 3 pasadas), medidos en la HP. Solo al entrar o al cambiar la contraseña.
- El historial cifrado se descifra a medida que se muestra, no entero.

**Etapa 5 — Archivos y antivirus:**
- **Archivos:**
  - por pedazos, directo al disco, con prioridad más baja que la voz y un tope de velocidad (diseño);
  - la huella se calcula por partes;
  - se leen con archivos mapeados (9.2).
- **ClamAV:**
  - se abre solo para analizar y se cierra al terminar (diseño);
  - antes de abrirlo se revisa la memoria libre (usa más de 1,2 GB);
  - corre en modo de fondo (`PROCESS_MODE_BACKGROUND_BEGIN` desde Vista: baja el procesador, el disco y la memoria a la vez);
  - nunca durante una llamada en una PC de un núcleo.
- **Actualización de la base de virus:** en momentos sin uso, con tope de velocidad. En XP se pide al servidor (diseño).

**Etapa 6 — Red:**
- **UPnP, búsqueda en la red local y prueba desde internet:** solo cuando se piden, nunca en segundo plano. La renovación de UPnP lo más espaciada que acepte el módem.
- **Relay:** reenvía sin descifrar; los mismos topes y prioridades de voz que el servidor.
- **Invitaciones:** sin consultas periódicas.

**Etapa 7 — Lista pública:**
- **La lista:** se baja al abrir Explorar y se guarda con su fecha (diseño); se actualiza solo a pedido o cada varios minutos con la pantalla abierta.
- **Ping de los servidores:** solo de los que se ven en pantalla, uno por vez y con pausas.
- **Reportes:** la captura se recodifica en el proceso de medios (diseño), y los pendientes esperan sin reintentar seguido.

**Etapa 8 — Servidores grandes:**
- Lo del diseño (tope de voces, calidad dinámica, reconexiones escalonadas), más:
  - IOCP o RIO;
  - búferes reutilizados;
  - la subida repartida en un solo lugar, con la voz primero;
  - TLS por conexión con techo.
- **El simulador de 512 personas** mide todo eso.

**Etapa 9 — Moderación:** permisos calculados al cambiar los roles, no en cada mensaje; búsquedas por índice.

**Etapa 10 — Modo Visual completo (sección 6, más):**
- **Fondos de tarjeta del catálogo** (Topográfico, Radar, Lluvia, Estática): los dibuja el programa (diseño):
  - a 30 cuadros por segundo como máximo, solo con la tarjeta abierta y a la vista (diseño);
  - la parte que no se mueve, en textura (6.4).
- **Efectos del editor:**
  - hasta 4 capas y 24 partículas (diseño);
  - las partículas se calculan una vez por cuadro para todas las tarjetas visibles, no por tarjeta.
- **Marcos de avatar animados** en la lista de canales: solo los visibles, y quietos con 30 o más en pantalla.
- **Fondo animado del chat:** se decodifica al tamaño de la ventana y se anima lento (diseño); quieto sin foco.
- **GIFs y stickers:** solo los que se ven, con un máximo de animaciones a la vez.
- **Proceso de medios:**
  - se abre cuando hace falta y se cierra después de un rato sin trabajo;
  - corre en modo de fondo (`PROCESS_MODE_BACKGROUND_BEGIN`).
- **En Rendimiento no se descarga nada de esto** (diseño): se comprueba en la prueba a mano de la etapa.

**Etapa 11 — Cierre:**
- **Actualizaciones:** se bajan en segundo plano, con prioridad baja, tope de velocidad y nunca durante una llamada; se verifican y se aplican al cerrar.
- **Pruebas de 8 horas:** con el registro prendido, para ver si la memoria crece, si suben las fallas de página o si se pierde rendimiento.

**Etapa 12 — Compartir pantalla:**
- Lo del diseño (solo lo que cambió, nada si nadie mira, copia chica, la voz manda), más:
  - no se descomprime lo que no se ve;
  - paquetes de menos de 1200 bytes;
  - búferes de cuadros reutilizados y fijados solo mientras se usan;
  - la captura con GDI en XP, al ritmo elegido, comparando por bloques en un hilo de baja prioridad.

**Etapa 13 — Varias PCs, y "Prestar esta PC":**
- La PC que presta corre con prioridad baja y topes de procesador, red y memoria.
- Deja de prestar sola con un juego en pantalla completa, con batería o con ahorro de batería.

**Etapa 14 — Amigos y llamadas:**
- **Red repartida (DHT):**
  - lo mínimo para seguir en la red: refrescos cada hora, como Kademlia, y los contactos que llevan mucho tiempo se revisan menos seguido;
  - con ahorro de batería o con la PC suspendida, nada.
- **Saber si un amigo está conectado:** ping espaciado y que se adapta (más seguido solo con la lista de amigos abierta).
- **Conexión directa:** el agujero en el módem se mantiene abierto solo con un chat o una llamada activos.
- **Llamadas grupales:**
  - de PC a PC, cada uno sube su voz a todos, así que la subida crece con cada persona;
  - el diseño ya prevé que reenvíe quien tenga buena subida y que una netbook con XP no reenvíe;
  - se escuchan como mucho 4 voces (diseño), y se descomprimen solo esas.
- **Archivos en partes, como torrent:** con tope de velocidad, prioridad baja y nunca por encima de la voz.
- **MLS:** el costo grande es al entrar o salir alguien del grupo, no por mensaje; se mide en la HP con un grupo grande.
- **Base local:** SQLite con WAL (11.3).
- **Copias en Google Drive (servicio de Windows):**
  - el servicio duerme hasta la hora elegida (un temporizador de Windows, sin consultar), o arranca solo cuando hace falta;
  - la copia se toma con la copia en caliente de SQLite (`sqlite3_backup_step`), de a poco y con pausas, en modo de fondo;
  - sube solo los cambios (diseño), con tope de velocidad y nunca durante una llamada.
- **Inicio de sesión en Google:** se pregunta con el intervalo que da Google (5 s o más, más espaciado si Google lo pide, según la RFC 8628), solo con esa pantalla abierta.

**Cualquier cosa nueva:**
- pasa por el banco sin ventana;
- todo hilo nuevo se justifica en docs/estado.md, con su prioridad y sus despertares;
- toda caché tiene techo y se suelta con poca memoria (sección 9).

## 16. Lo que no se toca para ahorrar

- **Las protecciones** (DEP, ASLR, `/guard:cf`, `/GS`, SafeSEH) quedan. Un estudio de 2017 midió para estos controles un costo promedio de 2 %.
- **Todo lo que llega de la red se sigue validando** con el lector único.
- **El cifrado no se debilita:** ni claves más cortas ni rotaciones más espaciadas.
- **El fuzzing y las revisiones** siguen para todo lo que lea datos de afuera.

## 17. Cómo se ejecuta: de un solo tirón

Pedido del dueño: **todo lo de este plan que se puede hacer con lo que ya existe se aplica de una vez.** Recién al final van las pruebas, el fuzzing, la revisión y una sola prueba a mano.

**Antes de empezar:**
- las decisiones de la sección 18 (si el dueño no dice otra cosa, las recomendaciones);
- el commit del paso anterior, "Consumo de la ventana y microcortes en XP", ya probado en la HP.

**Qué entra en el tirón** (en este orden de trabajo, sin pruebas a mano en el medio):
1. **Medición:** despertares, memoria propia, fallas de página, red, arranque y banco permanente (1.2).
2. **Código:**
   - búferes reutilizados en la voz y el servidor (3.2);
   - textos guardados (5.6);
   - nombres una sola vez (8.2).
3. **Dibujo:**
   - renderizador propio (5.1);
   - Direct3D 9Ex con flip (5.2);
   - imagen parcial en la clásica (5.3, queda solo si gana);
   - atlas (5.8).
4. **Escalones de ahorro y pantalla completa:** 7.1, 8.5, 10.6 y 4.5.
5. **Memoria virtual:** 9.1 a 9.6, más el montón de XP (8.3) y los techos (8.6).
6. **Energía:**
   - temporizadores con tolerancia y que Windows no frene la voz (4.2, 4.6);
   - MMCSS;
   - ahorro de batería (12.3).
7. **Tecla para hablar por eventos** (4.3).
8. **Voz y red:**
   - solo en el canal (4.7);
   - DTX (10.2);
   - historial a medida (10.3);
   - juntar registros (10.7);
   - prioridad de la voz (10.8).
9. **Modo Visual:** 6.1 a 6.7, sobre lo que hoy existe del Visual: el selector, la etiqueta que late y el medidor. El resto llega en la etapa 10 con estas reglas.
10. **Con las decisiones del dueño:**
    - medidor, suavizado y ritmo del mouse (5.4, 5.5, 5.7);
    - chat por ventana (8.1);
    - pedazos de audio (4.4);
    - modo de red liviano (10.4);
    - VBR (10.9);
    - parlantes en silencio (12.2);
    - ahorro de batería (6.8).
11. **Compilación** (3.6) y **Mbed TLS** (8.4), solo si ganan.
12. **Servidor:** SQLite (11.3) y búferes (3.2). IOCP queda para la etapa 8, con la prueba de estrés.

**Al final, todas las pruebas:**
1. **`check.ps1` completo:** las dos versiones, todas las pruebas y los 20 fuzzers.
2. **Pruebas nuevas para cada cambio.** Entre otras:
   - escalones;
   - pantalla completa;
   - memoria virtual (ofrecer y recuperar, aviso de poca memoria);
   - DTX en el colchón;
   - solo en el canal;
   - tecla por eventos y su respaldo;
   - renderizador (huella igual, sin estado por cuadro).
3. **Fuzzing de 10 minutos** de lo que lee datos de afuera y cambió: la sala, la voz y el colchón, y el protocolo si entra el pedido de mensajes más nuevos.
4. **Revisión con subagente** de lo que toca protocolo, cifrado o archivos.
5. **Medición en la PC** con el banco y `--registro`, comparada con los números de hoy.
6. **Una sola prueba a mano en la HP** con `registro.bat`, de unos 5 minutos:
   - quieta, escuchando, hablando y con el mouse;
   - minimizada;
   - con un juego o un video en pantalla completa;
   - con un programa pesado abierto, para la memoria.
7. **Un commit** con todo, y GitHub.

## 18. Decisiones del dueño

| # | Pregunta | Recomendación |
| --- | --- | --- |
| 1 | ¿Medidor de nivel a 4 veces por segundo en Rendimiento y sin foco (hoy 10)? | Sí |
| 2 | ¿Rendimiento sin suavizado de bordes (un poco más dentado; Visual igual)? | Sí, en PCs débiles |
| 3 | Con un juego en pantalla completa: ¿no dibujar, soltar texturas y bajar la prioridad de la ventana? La voz y la tecla siguen igual | Sí |
| 4 | ¿Agregar al protocolo el pedido de "mensajes más nuevos", para que el chat guarde solo lo cercano? | Sí |
| 5 | ¿Pedazos de audio de 20 ms en Rendimiento (10 ms más de demora)? | Sí en la clásica; medir en la moderna |
| 6 | ¿Un modo de red liviano con paquetes de 40 ms (20 ms más de demora)? | Opcional, no por defecto; lo demás llega con la calidad dinámica |
| 7 | ¿VBR restringido o bitrate fijo, para que el tamaño de los paquetes no delate lo que se dice? | VBR restringido |
| 8 | ¿Cerrar los parlantes después de un rato sin voz, para que la PC pueda ahorrar y dormir? | Sí, si la medición muestra que no se pierde el principio de lo que dicen |
| 9 | Con el ahorro de batería de Windows: ¿animaciones del Visual a la mitad y medidor más lento? | Sí |
| 10 | ¿Agregar a docs/diseno.md las metas de la netbook (1.4)? | Sí |
| 11 | ¿Dibujar con GDI en vez de la placa? | No por ahora; se reevalúa con los números del tirón |

## 19. Fuentes consultadas

**Microsoft (Direct3D y ventanas):**
- [Pure devices](https://learn.microsoft.com/windows-hardware/drivers/display/pure-devices), [D3DCREATE](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dcreate) y [Present](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-present)
- [Mejoras de Direct3D 9Ex](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/direct3d-9ex-improvements), [XPDM y WDDM](https://learn.microsoft.com/en-us/windows/win32/direct3d9/xpdm-vs-wddm) y [Direct3D 9 en XP](https://learn.microsoft.com/en-us/dotnet/desktop/wpf/advanced/performance-considerations-for-direct3d9-and-wpf-interoperability)
- [SHQueryUserNotificationState](https://msdn.microsoft.com/en-us/library/bb762242(v=vs.85)), [QUERY_USER_NOTIFICATION_STATE](https://learn.microsoft.com/tr-tr/windows/win32/api/shellapi/ne-shellapi-query_user_notification_state) y [WTSRegisterSessionNotification](https://learn.microsoft.com/en-us/windows/desktop/api/wtsapi32/nf-wtsapi32-wtsregistersessionnotification)
- [Animación del área de la ventana](https://msdn2.microsoft.com/library/aa965255(v=vs.85)) y [RAWINPUTDEVICE](https://learn.microsoft.com/windows/win32/api/winuser/ns-winuser-rawinputdevice)

**Microsoft (procesador, energía y memoria):**
- [timeBeginPeriod](https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod) y [SetCoalescableTimer](https://msdn.microsoft.com/en-us/library/hh405404.aspx)
- [SetThreadInformation](https://learn.microsoft.com/en-us/windows/desktop/api/processthreadsapi/nf-processthreadsapi-setthreadinformation), [THREAD_POWER_THROTTLING_STATE](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-thread_power_throttling_state), [Quality of Service](https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/ProcThread/quality-of-service.md) e [Introducing EcoQoS](https://devblogs.microsoft.com/performance-diagnostics/introducing-ecoqos/)
- [MMCSS](https://learn.microsoft.com/en-us/windows/win32/procthread/multimedia-class-scheduler-service) y [Power Setting GUIDs](https://msdn.microsoft.com/en-us/library/windows/desktop/hh448380(v=vs.85).aspx)
- [OfferVirtualMemory](https://msdn.microsoft.com/en-us/library/dn781436), [ReclaimVirtualMemory](https://learn.microsoft.com/cs-cz/windows/win32/api/memoryapi/nf-memoryapi-reclaimvirtualmemory), [CreateMemoryResourceNotification](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-creatememoryresourcenotification), [VirtualLock](https://learn.microsoft.com/en-us/windows/desktop/api/memoryapi/nf-memoryapi-virtuallock) y [SetProcessWorkingSetSizeEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-setprocessworkingsetsizeex)
- [SetProcessInformation y prioridad de memoria](https://learn.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessinformation), [MEMORYSTATUSEX](https://msdn.microsoft.com/en-us/aa366770), [PROCESS_MEMORY_COUNTERS_EX](https://msdn.microsoft.com/en-us/library/ms684874), [HeapSetInformation](https://msdn.microsoft.com/es-es/library/Aa366705) y [Low-fragmentation heap](https://learn.microsoft.com/pl-pl/windows/win32/Memory/low-fragmentation-heap)
- [QOS_TRAFFIC_TYPE](https://learn.microsoft.com/en-us/windows/desktop/api/qos2/ne-qos2-qos_traffic_type), [QOSAddSocketToFlow](https://learn.microsoft.com/en-au/windows/win32/api/qos2/nf-qos2-qosaddsockettoflow) y [Registered I/O](https://technet.microsoft.com/en-us/hh997032(v=ws.11))
- [Análisis de procesador con WPA](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/cpu-analysis), [cambios de contexto en Process Explorer](https://learn.microsoft.com/en-us/archive/blogs/sysinternals/volume-6-number-1) y [servicios con inicio diferido o a pedido](https://learn.microsoft.com/en-us/archive/msdn-magazine/2008/launch/windows-with-c-windows-services-enhancements)

**Chromium y Firefox:**
- [Pantalla completa](https://chromium.googlesource.com/chromium/src/+/main/ui/base/fullscreen_win.cc)
- [Memoria propia y compartida](https://chromium.googlesource.com/chromium/src/+/2b7c19d3/base/process/process_metrics_win.cc)
- [Animaciones de Windows](https://chromium.googlesource.com/chromium/src.git/+/HEAD/ui/gfx/animation/animation_win.cc)
- [Prioridad de la red](https://chromium.googlesource.com/chromium/src/+/main/net/socket/udp_socket_win.h)
- [Aviso de poca memoria en Firefox](https://searchfox.org/firefox-main/source/xpcom/base/AvailableMemoryWatcherWin.cpp)

**Voz y red:**
- [Controles de Opus](https://opus-codec.org/docs/opus_api-1.5/opus__defines_8h.html), [tamaño de cuadro](https://lists.xiph.org/pipermail/opus/2014-November/002731.html) y [DTX cada 400 ms](https://lists.xiph.org/pipermail/opus/2015-March/002908.html)
- [RFC 7587](https://www.rfc-editor.org/rfc/rfc7587) y el [estudio sobre VBR cifrado](https://par.nsf.gov/servlets/purl/10178638)
- [NetEQ de WebRTC](https://webrtc.googlesource.com/src/+/HEAD/modules/audio_coding/neteq/g3doc/index.md)
- [Tiempos de los routers para UDP](https://support.goto.com/connect/help/what-are-the-recommended-nat-keep-alive-settings) y [Nagle y TCP_NODELAY](https://en.wikipedia.org/wiki/Nagle%27s_algorithm)
- [Llamadas en malla o con reenvío](https://getstream.io/blog/low-latency-group-calls/)
- [Mantenimiento de Kademlia](https://arxiv.org/pdf/1408.3079)
- [Inicio de sesión para dispositivos (RFC 8628, implementación)](https://pkg.go.dev/github.com/int128/oauth2dev)

**Bibliotecas:**
- [Ahorro en Dear ImGui](https://github.com/ocornut/imgui/wiki/Implementing-Power-Save%2C-aka-Idling-outside-of-ImGui)
- [Mbed TLS: tamaño](https://mbed-tls.readthedocs.io/en/latest/_sources/kb/how-to/controlling_package_size.md.txt) y [memoria](https://mbed-tls.readthedocs.io/en/latest/_sources/kb/how-to/reduce-polarssl-memory-and-storage-footprint.md.txt)
- [SQLite WAL](https://sqlite.org/forum/forumpost/90ef18d88d?t=c), la [medición de Trac](https://trac.edgewall.org/ticket/11967) y la [copia en caliente](https://www3.sqlite.org/backup.html)

**Código:**
- [C++ Core Guidelines: rendimiento](https://www.modernescpp.com/index.php/c-core-guidelines-the-remaining-rules-to-performance/)
- [monotonic_buffer_resource](https://cppreference.com/cpp/memory/monotonic_buffer_resource/monotonic_buffer_resource)
- [Costo de los controles de saltos](https://github.com/yvt/Stella2/issues/16)

**Interfaces que guardan capas en textura:**
- [CEGUI con Direct3D 9](https://crayzedsgui.sourceforge.net/api_reference/classCEGUI_1_1Direct3D9TextureTarget.html), [Flax](https://docs.flaxengine.com/api/FlaxEngine.GUI.RenderToTextureControl.html) y [C4](https://c4engine.com/docs/InterfaceMgr/ViewportWidget_EnableViewportTexture.html)
- [Capas quietas y animadas](https://handmade.network/forums/t/6838/p/20727)

**Energía:**
- [Sonido abierto que no deja dormir (TeamSpeak)](https://community.teamspeak.com/t/teamspeak-preventing-standby-mode/49030), [informe de Mozilla](https://bugzilla.mozilla.org/show_bug.cgi?id=1165677) y [Sound Keeper](https://veg.by/en/projects/soundkeeper/comment-page-11/)
- [Resolución del reloj (Intel VTune)](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-0/window-timer-resolution-platform-power-analysis.html)

**Memoria virtual:**
- [Páginas de archivo contra páginas anónimas](https://lists.volatilityfoundation.org/mailman3/hyperkitty/list/vol-users@lists.volatilityfoundation.org/thread/O2JA3STUOXBKTVPV6Q3FFLPYJ6WPBMOV/)
- [Recorte al minimizar (Hasso Plattner Institut)](https://osm.hpi.de/bsArch/2006/Slides/Unit05/5.4_WinPhysMemManagement.pdf)
- [Ofrecer y recuperar memoria](https://dblohm7.ca/blog/2015/07/28/interesting-win32-apis/)

**Otros programas y herramientas:**
- [Tecla para hablar por Raw Input](https://code.iamtalon.me/Talon/voice-cat/src/commit/2baefddbe4b7512bdf534ca9d61a874ae221ea55)
- [Lista de mensajes virtual](https://getstream.io/chat/docs/sdk/react/components/core-components/virtualized_list/)
- [Consumo de Discord](https://www.guidingtech.com/fix-discord-high-cpu-usage)
- [Very Sleepy](https://github.com/CyberShadow/verysleepy)
- [select contra IOCP](https://forums.codeguru.com/showthread.php?p=2035359) (informal; se confirma midiendo)
- [Parámetros de Argon2id](https://openjdk.org/jeps/8377081)
