# Ventrik — Estado del trabajo

Para retomar sin perder contexto (se carga desde CLAUDE.md). Actualizar al cerrar cada paso.
Última actualización: 2026-10-22, al cerrar el paso 2.1.

## Dónde estamos

- **Etapa 0: cerrada.** Falta solo probar en Windows 11 (no hay PC con Windows 11).
- **Etapa 1: cerrada.** Resultados en docs/mediciones.md; decisiones en docs/diseno.md.
- **Etapa 2, paso 2.1: cerrado.** Lector y escritor único de mensajes, saludo con negociación de versión y funciones, y fuzzing (src/common/protocol, fuzz/).
- **Lo que sigue: paso 2.2**: certificado del servidor con su huella y conexión por TLS 1.3 con Mbed TLS, con pruebas de huella correcta, incorrecta e intento de bajar el cifrado. Antes, encender el control de hilos de Mbed TLS (ver Pendientes). Proponer el plan primero.

## Ramas y repositorio

- **main:** pública en GitHub (brunogamer42-cmd/ventrik). La acción `.github/workflows/compilar.yml` corre `tools\check.ps1 -SoloModerna` en `windows-2022`; windows-2025 ya trae solo VS 2026 y no sirve.
- **prueba-de-voz:** solo local, **no subir nunca** (el plan dice que la prueba de voz no se publica). Tiene:
  - `ventrik_eco.exe`: eco local, con `--registro`, `--banco`, `--sistema` y `--complejidad`;
  - `ventrik_red.exe`: voz por UDP en la red local;
  - el puente WinMM para XP, el motor de voz, el buffer de red y el paquete provisorio, en `src/prueba_voz/`.

  En las etapas 2 y 3 se porta lo que sirva a main, con sus marcas VENTRIK_PENDIENTE.
- Identidad de Git solo para este proyecto: Bruno Serenelli <225036956+brunogamer42-cmd@users.noreply.github.com>.

## Equipos y herramientas

- **PC de trabajo:** Windows 10 Pro 64 bits, Intel Xeon E5-2690 v4. Sin parlantes, con auriculares.
- **HP 2133 (netbook de referencia):** Windows XP SP3, VIA C7-M 1,2 GHz, un núcleo. El dueño prueba ahí copiando `build\para_xp` con un pendrive (`H:\para_xp\`), y los archivos que deja (`resultado.txt`, `opus_complejidad.txt`, `registro_eco.csv`) se leen desde `H:\para_xp\`.
- **Visual Studio:** las Build Tools 2022 que instaló Applio, en `C:\ApplioV3.0.5\bin\build_tools`, con v143, v141, v141_xp, el SDK 7.1A y CMake 3.31 adentro (no está en el PATH: check.ps1 lo encuentra con vswhere).
- **GitHub CLI:** `C:\Program Files\GitHub CLI\gh.exe`, con sesión iniciada. El modo automático de permisos bloqueó crear el repositorio público; lo creó el dueño.
- **VirtualBox** está instalado, sin máquinas.

## Cómo se trabaja

- `tools\check.ps1`, con la opción `-SoloModerna` para GitHub:
  - compila las dos versiones y corre las pruebas;
  - revisa cada `.exe` (/MT, DEP, ASLR, CFG, SafeSEH y XP 5.01 en la clásica, DLL que XP tiene, funciones que XP no tiene, la marca 0x800 en la moderna);
  - abre y cierra la ventana;
  - revisa que third_party no cambió (`tools\huellas_terceros.ps1`, `third_party\HUELLAS.txt`);
  - deja `build\para_xp` para la XP;
  - lista los VENTRIK_PENDIENTE.
- **Bibliotecas:** versiones, origen y huellas en third_party/VERSIONS.md. Para actualizar una: bajarla, anotarla, reemplazarla y correr `huellas_terceros.ps1 -Actualizar`. Lo nuestro para integrarlas está en `third_party/integracion/`.
- **Fuzzing:** `tools\fuzzing.ps1 [-Segundos N]` (10 minutos si no se dice otra cosa) corre los fuzzers de fuzz/ en 64 y 32 bits, a la vez, con AddressSanitizer. Antes hay que compilarlos con check.ps1, que además los corre 30 segundos cada vez.
  - Las compilaciones de fuzzing son los presets `fuzzing64` y `fuzzing32` (v143, nunca se publican). Usan el mismo `_WIN32_WINNT` que la clásica.
  - Lo aprendido queda en `build\fuzzing\corpus`; cada caso que falla, en `build\fuzzing\fallas`. Las semillas de arranque están en `fuzz\semillas`.
  - Código nuevo que lea datos de red o archivos: sumar su fuzzer en fuzz/ y en CMakeLists.txt y fuzzing.ps1.
- **Textos:** van en `lang/es.txt` y `lang/en.txt` (`clave = texto`). `tools/generar_textos.cmake` los mete en el `.exe`, y una prueba exige que estén en los dos idiomas.
- **Pendientes:** lo provisorio se marca con `VENTRIK_PENDIENTE("texto sin acentos")` (src/common/pendiente.h). Avisa al compilar sin frenar /WX, y check.ps1 lo lista.
- **Tus preferencias** también están en las memorias:
  - plan antes de cada paso;
  - medir antes de cambiar;
  - no pedir pruebas largas;
  - comprobar uno mismo todo lo posible;
  - comentarios cortos, solo para explicar por qué.

## Decisiones técnicas que no están a la vista

- **GSL 3.1.0,** no la última: desde la 4.0 exige VS 2019, y la clásica usa VS 2017.
- **SQLite:**
  - se compila con `SQLITE_THREADSAFE=0`: su control de hilos usa funciones de Windows 7, y el diseño ya lo usa desde un solo hilo;
  - en la clásica, `GetTickCount64` sale de `third_party/integracion/sqlite/`.
- **RNNoise 0.2:**
  - le falta `os_support.h`; el reemplazo está en integracion, con la misma definición de Opus;
  - en la clásica se compila con `restrict=`, porque el compilador de C de VS 2017 no conoce esa palabra.
- **Opus:** se compila con `/MT` (OPUS_STATIC_RUNTIME) y con coma, no en enteros: en enteros es más lento hasta en el C7-M.
- **Mbed TLS 3.6.7:**
  - en la clásica, los números aleatorios salen de RtlGenRandom (src/common/entropia_xp.cpp), cargado desde cryptbase.dll y advapi32.dll con su ruta completa;
  - `MBEDTLS_TEST_SW_INET_PTON` en la clásica, porque XP no tiene `inet_pton`;
  - **no tiene control de hilos:** hay que encenderlo antes de usarlo desde varios hilos (ver la memoria de pendientes de la etapa 2).
- **doctest:** en la clásica va sin su informe de cierres inesperados, que usa SetThreadStackGuarantee.
- **Al arrancar:**
  - `endurecer_carga_de_dll()` (SetDefaultDllDirectories, o si no SetDllDirectoryW);
  - DEP en la clásica;
  - la clásica se niega a abrir si no es XP SP3 o posterior.
- **UTF-8:** Ventrik lo valida por su cuenta (`es_utf8_valido`), porque el de XP acepta mitades de par UTF-16.
- **Audio en XP:** puente propio con WinMM.
  - Pedazos de 10 ms, 64 lugares hacia los parlantes y 8 para el micrófono, **sin ninguna corrección de desfase**: XP avisa mal la posición de los parlantes y devuelve tarde los pedazos.
  - miniaudio tiene mínimos fijos (30 ms con DirectSound, 40 ms con WinMM) y en XP, con DirectSound, consulta cada 1 ms.
- **Consumo de la voz:** niveles Alto, Normal, Bajo, Mínimo y Automático, todos con Opus (docs/diseno.md). No sumar otros códecs.

- **Protocolo (src/common/protocol):**
  - todo lo que llega de la red pasa por `Lector`: error pegajoso, cada largo comparado con lo que queda, texto con largo u16, UTF-8 validado y sin bytes 0;
  - en TLS, cada mensaje va en un marco: largo del cuerpo (u32) + tipo (u16), cuerpo de 64 KiB como máximo; el `Ensamblador` guarda un solo marco por vez;
  - el saludo (`Hola`, `Bienvenida`, `Rechazo`) no cambia nunca de formato; lo nuevo va en mensajes posteriores, cuando los dos ya saben qué versión hablan;
  - las funciones son bits de un u64; todavía no hay ninguna asignada;
  - la versión de Ventrik y el rango del protocolo están en src/common/version.h. Al subir `kProtocoloMaximo` hay que sumar en `kPrimeraVersionDelProtocolo` la primera versión que lo habla (no compila si falta): es la que se le pide al programa que no llega;
  - el rechazo tiene una sola forma válida: la versión va solo en `programa_viejo`. El programa revisa con `aceptar_bienvenida` y `aceptar_rechazo` lo que contesta el servidor;
  - `Ensamblador::siguiente` entrega un marco por vez; con error consume todo, y `Lector::restante()` da 0, para que ningún bucle quede trabado.

## Trampas de las herramientas

- **PowerShell 5.1:**
  - un `.ps1` con acentos tiene que guardarse en UTF-8 **con BOM**, y check.ps1 tiene fin de línea CRLF: al reemplazar texto adentro, usar `` `r`n ``;
  - `git commit -m` con comillas dobles falla: usar `git commit -F` con un archivo UTF-8 **sin BOM**;
  - si se redirige la salida de error de un programa con `$ErrorActionPreference='Stop'`, PowerShell corta: usar `Continue`.
- **doctest:** `MESSAGE(a + b)` no compila; armar el texto en una variable antes.
- **inet_ntoa e inet_addr** están obsoletas y frenan /WX, y sus reemplazos no existen en XP: usar `ip_a_texto` y `leer_ip`, que están en la rama de prueba.
- **Encabezados de terceros:** se incluyen con `<...>` (`/external:anglebrackets /external:W0`); en la clásica hace falta además `/experimental:external`.
- **dumpbin muestra en hexadecimal** la cantidad de manejadores de SafeSEH.
- **El antivirus de Windows** bloquea unos segundos los `.exe` recién hechos: check.ps1 reintenta la copia.
- **libFuzzer** escribe lo que aprende en la **primera** carpeta que recibe: nunca pasarle primero `fuzz\semillas`.
- **AddressSanitizer** guarda la memoria liberada un rato (para detectar usos tardíos), así que el fuzzer llega a unos 600 MB. No es una pérdida: con `ASAN_OPTIONS=quarantine_size_mb=16` se queda en 135 MB.
- **Start-Process** no pone comillas a los argumentos: con espacios en la ruta del proyecto, ponerlas a mano (fuzzing.ps1 lo hace).

## Pendientes (además de los VENTRIK_PENDIENTE)

- Prueba de 10 minutos (con la primera beta) y prueba larga del puente WinMM en XP.
- Números de la prueba de red entre la PC y la HP 2133, la RAM en la HP 2133, el ancho de banda medido y el arranque.
- Windows 11.
- Etapa 2:
  - control de hilos de Mbed TLS;
  - envoltorio de un solo hilo para SQLite;
  - `/DEPENDENTLOADFLAG` en la clásica, a evaluar.
- De la revisión del paso 2.1, para el 2.2 (conexión):
  - tiempo máximo para el saludo y para que avance cada marco (diseno.md, "Conexiones que nunca terminan de saludar");
  - durante el saludo, `Ensamblador` con tope chico (por ejemplo 64 bytes) y `cambiar_maximo` al terminar; sumar ese cambio de tope al fuzzer;
  - después de cada `while (auto marco = ensamblador.siguiente(datos))`, si `error()`, cerrar la conexión: "faltan datos" y "error" se ven igual en el bucle;
  - para aceptar un mensaje, `terminado()`, nunca `restante() == 0` (con error también da 0);
  - validar la configuración del servidor al cargarla: `negociar` corta el programa si el rango del protocolo está al revés, y el máximo no puede pasar de `kProtocoloMaximo` (hoy un protocolo desconocido se toma como el último conocido; ajustar las pruebas que usan 2 a 5 y, en `primera_version_del_protocolo`, usar `gsl::at`);
  - un `Rechazo` que no se puede leer se muestra como "el servidor no aceptó la conexión", no como error.
- De la misma revisión, para el 2.3 (nombres y chat): qué caracteres acepta `leer_texto`. Hoy rechaza solo UTF-8 inválido y el byte 0. Faltan controles, saltos de línea en nombres y caracteres invisibles o de dirección (diseno.md, "Nombres disfrazados con Unicode"); ojo, en el chat el U+200D arma emojis.
