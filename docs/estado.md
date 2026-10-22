# Ventrik — Estado del trabajo

Para retomar sin perder contexto (se carga desde CLAUDE.md). Actualizar al cerrar cada paso.
Última actualización: 2026-10-22, al cerrar el paso 2.2.

## Dónde estamos

- **Etapa 0: cerrada.** Falta solo probar en Windows 11 (no hay PC con Windows 11).
- **Etapa 1: cerrada.** Resultados en docs/mediciones.md; decisiones en docs/diseno.md.
- **Etapa 2, paso 2.1: cerrado.** Lector y escritor único de mensajes, saludo con negociación de versión y funciones, y fuzzing (src/common/protocol, fuzz/). Probado en la HP 2133 con XP.
- **Etapa 2, paso 2.2: cerrado.** Servidor de consola con su clave y su huella, conexión por TLS 1.3 que verifica la huella, protección contra inundaciones (pedida por el dueño, en todos los niveles) y Mbed TLS con control de hilos.
- **Lo que sigue: paso 2.3**: canales, entrar y salir, chat por canal en SQLite con consultas preparadas, permisos básicos verificados en el servidor en cada acción y panel de diagnóstico. Proponer el plan primero. Ver "Pendientes" para lo que la revisión del 2.2 dejó para el 2.3.

## Ramas y repositorio

- **main:** pública en GitHub (brunogamer42-cmd/ventrik). La acción `.github/workflows/compilar.yml` corre `tools\check.ps1 -SoloModerna` en `windows-2022`; windows-2025 ya trae solo VS 2026 y no sirve. windows-2022 trae el componente de AddressSanitizer.
- **prueba-de-voz:** solo local, **no subir nunca** (el plan dice que la prueba de voz no se publica). Tiene:
  - `ventrik_eco.exe`: eco local, con `--registro`, `--banco`, `--sistema` y `--complejidad`;
  - `ventrik_red.exe`: voz por UDP en la red local;
  - el puente WinMM para XP, el motor de voz, el buffer de red y el paquete provisorio, en `src/prueba_voz/`.

  En las etapas 2 y 3 se porta lo que sirva a main, con sus marcas VENTRIK_PENDIENTE.
- Identidad de Git solo para este proyecto: Bruno Serenelli <225036956+brunogamer42-cmd@users.noreply.github.com>.

## Equipos y herramientas

- **PC de trabajo:** Windows 10 Pro 64 bits, Intel Xeon E5-2690 v4. Sin parlantes, con auriculares. IP en la red local: 192.168.0.153 (por DHCP, puede cambiar). La red es "Privada".
- **HP 2133 (netbook de referencia):** Windows XP SP3, VIA C7-M 1,2 GHz, un núcleo. El dueño prueba ahí copiando `build\para_xp` con un pendrive (`H:\para_xp\`), y los archivos que deja (`resultado.txt`, `opus_complejidad.txt`, `registro_eco.csv`) se leen desde `H:\para_xp\`.
- **Visual Studio:** las Build Tools 2022 que instaló Applio, en `C:\ApplioV3.0.5\bin\build_tools`, con v143, v141, v141_xp, el SDK 7.1A, AddressSanitizer y CMake 3.31 adentro (no está en el PATH: check.ps1 lo encuentra con vswhere).
- **GitHub CLI:** `C:\Program Files\GitHub CLI\gh.exe`, con sesión iniciada. El modo automático de permisos bloqueó crear el repositorio público; lo creó el dueño.
- **VirtualBox** está instalado, sin máquinas.

## Cómo se trabaja

- `tools\check.ps1`, con la opción `-SoloModerna` para GitHub:
  - compila las dos versiones y corre las pruebas;
  - revisa cada `.exe` (`ventrik`, `ventrik_servidor`, `ventrik_pruebas`): /MT, DEP, ASLR, CFG, la marca 0x800 de DLL (también en la clásica), y en la clásica SafeSEH, XP 5.01, DLL que XP tiene y funciones que XP no tiene;
  - abre y cierra la ventana;
  - revisa que third_party no cambió (`tools\huellas_terceros.ps1`, `third_party\HUELLAS.txt`);
  - compila los fuzzers y los corre 30 segundos;
  - deja `build\para_xp` para la XP (con `ventrik_servidor.exe`);
  - lista los VENTRIK_PENDIENTE.
- **Servidor:** `ventrik_servidor.exe [--carpeta <carpeta>] [--puerto <número>] [--huella]`.
  - La clave queda en `%APPDATA%\Ventrik\Servidor\clave_servidor.der` si no se dice otra carpeta.
  - `--huella` muestra la huella y termina sin abrir el puerto, así no salta el aviso del firewall.
  - Abrir el puerto en todas las placas (0.0.0.0) hace que el firewall de Windows pregunte la primera vez: eso lo tiene que contestar el dueño. Las pruebas escuchan solo en 127.0.0.1 y no preguntan.
  - Servidor de prueba para el dueño: `build\servidor_de_prueba` (huella `JAUB-ASLN-E5PH-PVU4-5WJN-F457-46BH-IJMD-DFBK-PKGW-Q5YJ-6LF7-63EA`).
- **Programa:** `ventrik.exe --conectar <ip[:puerto]> <huella>` (provisorio hasta el paso 2.5) muestra en un cuadro si se conectó o por qué no. `build\para_xp\conectar_a_la_pc.bat` y `conectar_con_huella_falsa.bat` lo usan contra el servidor de prueba de la PC; no van a git porque llevan la IP.
- **Bibliotecas:** versiones, origen y huellas en third_party/VERSIONS.md. Para actualizar una: bajarla, anotarla, reemplazarla y correr `huellas_terceros.ps1 -Actualizar`. Lo nuestro para integrarlas está en `third_party/integracion/`.
- **Fuzzing:** `tools\fuzzing.ps1 [-Segundos N]` (10 minutos si no se dice otra cosa) corre los fuzzers de fuzz/ en 64 y 32 bits, a la vez, con AddressSanitizer. Antes hay que compilarlos con check.ps1, que además los corre 30 segundos cada vez.
  - Son 4: `fuzz_protocolo`, `fuzz_utf`, `fuzz_textos` (huella y dirección) y `fuzz_identidad` (archivo de la clave).
  - Las compilaciones de fuzzing son los presets `fuzzing64` y `fuzzing32` (v143, nunca se publican). Usan el mismo `_WIN32_WINNT` que la clásica y RtlGenRandom.
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
  - en la clásica y en el fuzzing, los números aleatorios salen de RtlGenRandom (src/common/entropia_xp.cpp), cargado desde cryptbase.dll y advapi32.dll con su ruta completa;
  - `MBEDTLS_TEST_SW_INET_PTON` en la clásica, porque XP no tiene `inet_pton`;
  - **control de hilos encendido** (`MBEDTLS_THREADING_ALT` con CRITICAL_SECTION, en `third_party/integracion/mbedtls/threading_alt.h`). Todo `.exe` crea primero un `ventrik::cifrado::Inicio` (candados + `psa_crypto_init`) y lo destruye al final; sin eso, todo Mbed TLS falla;
  - números aleatorios para Mbed TLS: `ventrik::cifrado::aleatorio` (`psa_generate_random`).
- **TLS (src/common/cifrado/tls.cpp):**
  - TLS 1.3 solo; AES-128-GCM, AES-256-GCM y ChaCha20-Poly1305; X25519 y P-256; firma ECDSA P-256; solo intercambio efímero, sin tickets. El programa ordena los cifrados según AES-NI;
  - la huella se verifica con `mbedtls_ssl_set_verify` en modo OPTIONAL (REQUIRED exige una autoridad certificante): si no coincide devuelve error fatal, y antes de mandar el Hola se vuelve a confirmar. `mbedtls_ssl_set_hostname(NULL)` es obligatorio desde Mbed TLS 3.6.3;
  - cada llamada a TLS lee como mucho 64 veces y 32 KiB: Mbed TLS descarta sin límite registros ChangeCipherSpec y alertas sueltas, y sin tope trababa el hilo de red.
- **Huella:** SHA-256 de la clave pública (SubjectPublicKeyInfo), en base32 de a 4 (`src/common/cifrado/huella.cpp`). Se compara en tiempo constante.
- **Identidad del servidor:** clave ECDSA P-256 en DER; el certificado se rehace en cada arranque y vence en 2099. Al cargar se revisa que la pública y la privada coincidan (`mbedtls_pk_check_pair`). Si el archivo existe pero no sirve, el servidor no arranca. La clave se crea sin reemplazar nunca un archivo existente (si otro servidor la creó a la vez, gana el otro).
- **Red:** Winsock con funciones de XP, solo IPv4. Sockets sin trabarse y sin heredarse. `SO_EXCLUSIVEADDRUSE` y `SetHandleInformation` pueden fallar en XP sin que importe. `aceptar` saltea las conexiones que se cortaron en la cola.
- **Servidor (src/server/):**
  - un hilo de red con `select()` (`FD_SETSIZE` 1024 solo en servidor.cpp), conexiones como máquinas de estados;
  - topes: 10 s para cifrar y saludar, 5 s para cerrar, 4 a medio saludar y 16 en total por IP, 64 a medio saludar en total, 512 conexiones, 16 KiB antes del saludo, mensajes de 64 bytes hasta el Hola;
  - `proteccion.cpp`: ritmo por IP, bloqueo temporal, turno global de saludos y una reserva con techo para las IP conocidas (diseno.md, "Contra inundaciones de conexiones"). Una falla le quita a una IP lo de conocida;
  - `registro.cpp`: la consola se escribe desde otro hilo con una cola de 256 líneas; si se llena, descarta y avisa cuántas.
- **Programa:** `cliente::conectar` espera con tiempos máximos y devuelve la conexión abierta (`ConexionAbierta`), que avisa el cierre al destruirse.
- **doctest:** en la clásica va sin su informe de cierres inesperados, que usa SetThreadStackGuarantee.
- **Al arrancar:**
  - `endurecer_carga_de_dll()` (SetDefaultDllDirectories, o si no SetDllDirectoryW);
  - DEP en la clásica;
  - la clásica se niega a abrir si no es XP SP3 o posterior.
- **UTF-8:** Ventrik lo valida por su cuenta (`es_utf8_valido`), porque el de XP acepta mitades de par UTF-16. `a_utf8` cambia las mitades sueltas por U+FFFD, porque XP no lo hace.
- **Audio en XP:** puente propio con WinMM.
  - Pedazos de 10 ms, 64 lugares hacia los parlantes y 8 para el micrófono, **sin ninguna corrección de desfase**: XP avisa mal la posición de los parlantes y devuelve tarde los pedazos.
  - miniaudio tiene mínimos fijos (30 ms con DirectSound, 40 ms con WinMM) y en XP, con DirectSound, consulta cada 1 ms.
- **Consumo de la voz:** niveles Alto, Normal, Bajo, Mínimo y Automático, todos con Opus (docs/diseno.md). No sumar otros códecs.
- **Protocolo (src/common/protocol):**
  - todo lo que llega de la red pasa por `Lector`: error pegajoso, cada largo comparado con lo que queda, texto con largo u16, UTF-8 validado y sin bytes 0;
  - en TLS, cada mensaje va en un marco: largo del cuerpo (u32) + tipo (u16), cuerpo de 64 KiB como máximo (64 bytes hasta el saludo); el `Ensamblador` guarda un solo marco por vez;
  - el saludo (`Hola`, `Bienvenida`, `Rechazo`) no cambia nunca de formato; lo nuevo va en mensajes posteriores, cuando los dos ya saben qué versión hablan;
  - las funciones son bits de un u64; todavía no hay ninguna asignada;
  - la versión de Ventrik y el rango del protocolo están en src/common/version.h. Al subir `kProtocoloMaximo` hay que sumar en `kPrimeraVersionDelProtocolo` la primera versión que lo habla (no compila si falta): es la que se le pide al programa que no llega;
  - el rechazo tiene una sola forma válida: la versión va solo en `programa_viejo`. El programa revisa con `aceptar_bienvenida` y `aceptar_rechazo` lo que contesta el servidor;
  - `Ensamblador::siguiente` entrega un marco por vez; con error consume todo, y `Lector::restante()` da 0, para que ningún bucle quede trabado. Para aceptar un mensaje, `terminado()`, nunca `restante() == 0`.

## Trampas de las herramientas

- **PowerShell 5.1:**
  - un `.ps1` con acentos tiene que guardarse en UTF-8 **con BOM**, y check.ps1 tiene fin de línea CRLF: al reemplazar texto adentro, usar `` `r`n ``;
  - `git commit -m` con comillas dobles falla: usar `git commit -F` con un archivo UTF-8 **sin BOM**;
  - si se redirige la salida de error de un programa con `$ErrorActionPreference='Stop'`, PowerShell corta: usar `Continue`.
- **doctest:** `MESSAGE(a + b)` no compila; armar el texto en una variable antes.
- **inet_ntoa e inet_addr** están obsoletas y frenan /WX, y sus reemplazos no existen en XP: Ventrik usa `red::leer_direccion` y `red::a_texto`.
- **Encabezados de terceros:** se incluyen con `<...>` (`/external:anglebrackets /external:W0`); en la clásica hace falta además `/experimental:external`.
- **Mbed TLS 3.6:** `mbedtls/constant_time.h` y `mbedtls/psa_util.h` no avisan que son de C: incluirlos dentro de `extern "C" { }` o no enlaza.
- **SDK 7.1A (el de XP):** su `objbase.h` no compila con `/permissive-`; antes de `windows.h` y `shlobj.h` va `struct IUnknown;` (src/common/archivos.cpp).
- **Nombres de parámetros** como `socket` tapan la función global de Winsock y frenan /W4 (C4459).
- **dumpbin muestra en hexadecimal** la cantidad de manejadores de SafeSEH.
- **El antivirus de Windows** bloquea unos segundos los `.exe` recién hechos: check.ps1 reintenta la copia.
- **libFuzzer** escribe lo que aprende en la **primera** carpeta que recibe: nunca pasarle primero `fuzz\semillas`.
- **AddressSanitizer** guarda la memoria liberada un rato (para detectar usos tardíos), así que el fuzzer llega a unos 600 MB. No es una pérdida: con `ASAN_OPTIONS=quarantine_size_mb=16` se queda en 135 MB.
- **Start-Process** no pone comillas a los argumentos: con espacios en la ruta del proyecto, ponerlas a mano (fuzzing.ps1 lo hace).

## Pendientes (además de los VENTRIK_PENDIENTE)

- Prueba de 10 minutos (con la primera beta) y prueba larga del puente WinMM en XP.
- Números de la prueba de red entre la PC y la HP 2133, la RAM en la HP 2133, el ancho de banda medido y el arranque.
- Windows 11.
- Etapa 2: envoltorio de un solo hilo para SQLite (paso 2.3).
- De las revisiones del 2.1 y 2.2, para el 2.3:
  - tiempo máximo y "sigue ahí" (keepalive) para las conexiones ya saludadas: hoy esperan para siempre, y un programa que no lee lo que recibe se queda conectado;
  - el bucle de lectura de `Servidor::avanzar` renueva el tope en cada `leer`: con mensajes después del saludo necesita un tope por vuelta y mirar lo que Mbed TLS ya tiene guardado (`mbedtls_ssl_get_bytes_avail`) si corta antes;
  - colas de salida con tope (diseño: "Un programa que no lee lo que recibe");
  - validar la configuración del servidor cuando se cargue de un archivo: rango del protocolo derecho y sin pasar de `kProtocoloMaximo` (las pruebas usan protocolos 2 a 5);
  - qué caracteres acepta `leer_texto`: hoy rechaza solo UTF-8 inválido y el byte 0. Faltan controles, saltos de línea en nombres y caracteres invisibles o de dirección (diseno.md, "Nombres disfrazados con Unicode"); ojo, en el chat el U+200D arma emojis.
- Para el paso 2.5 y la reconexión automática (etapa 8): **nunca** reintentar solo después de "la huella no coincide". Es un aviso de seguridad, y 5 intentos en un minuto bloquean 5 minutos la IP de toda la casa.
- Para las etapas 6 y 7: con relay, muchas personas llegan desde la misma IP; los topes por IP y el bloqueo necesitan otra regla ahí. Las pruebas de puerto del directorio no tienen que contar como fallas. Con IPv6, los topes por IP van por bloque /64.
- Ajustar los números de la protección contra inundaciones después de medir en la HP 2133 como servidor (modo servidor limitado).
