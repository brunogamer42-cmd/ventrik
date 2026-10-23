# Ventrik — Estado del trabajo

Para retomar sin perder contexto (se carga desde CLAUDE.md). Actualizar al cerrar cada paso.
Última actualización: 2026-10-23, al cerrar el paso 2.4.

## Dónde estamos

- **Etapa 0: cerrada.** Falta solo probar en Windows 11 (no hay PC con Windows 11).
- **Etapa 1: cerrada.** Resultados en docs/mediciones.md; decisiones en docs/diseno.md.
- **Etapa 2, paso 2.1: cerrado.** Lector y escritor único de mensajes, saludo con negociación de versión y funciones, y fuzzing (src/common/protocol, fuzz/). Probado en la HP 2133 con XP.
- **Etapa 2, paso 2.2: cerrado.** Servidor de consola con su clave y su huella, conexión por TLS 1.3 que verifica la huella, protección contra inundaciones (pedida por el dueño, en todos los niveles) y Mbed TLS con control de hilos. Probado en la HP 2133: pruebas automáticas en XP, y conexión real XP → PC con la huella correcta (conecta) y con una falsa (rechaza).
- **Etapa 2, paso 2.3: cerrado.** Presentación con nombre, canales administrados por el dueño desde adentro (código de dueño provisorio), chat por canal guardado en SQLite, permisos verificados en el servidor, ping, diagnóstico y consola de prueba provisoria (`ventrik.exe --consola`). Probado a mano: HP 2133 como invitado y la PC como dueño, contra el servidor de prueba de la PC.
- **Etapa 2, paso 2.4: cerrado.** Voz por DTLS 1.2 a través del servidor: clave derivada de la conexión TLS, reenvío solo al mismo canal, audio con miniaudio o el puente WinMM en XP, `/hablar` en la consola. Probado a mano: voz en los dos sentidos entre la HP 2133 con XP y la PC, y que otro canal no escucha.
- **Lo que sigue: paso 2.5**: ventana principal en modo Rendimiento y pantalla Inicio con Direct3D 9 y Dear ImGui. Proponer el plan primero.

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
- **HP 2133 (netbook de referencia):** Windows XP SP3, VIA C7-M 1,2 GHz, un núcleo. En la red local: 192.168.0.42 (por DHCP). El dueño prueba ahí copiando `build\para_xp` con un pendrive (`H:\para_xp\`), y los archivos que deja (`resultado.txt`, `opus_complejidad.txt`, `registro_eco.csv`) se leen desde `H:\para_xp\`.
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
- **Servidor, archivos de datos:** `codigo_dueno.txt` (se crea al arrancar por primera vez y se muestra en la consola) y `ventrik.db` (SQLite: canales, mensajes y configuración).
- **Programa:** `ventrik.exe --consola <ip[:puerto]> <huella> <nombre> [--dueno <código>]` (provisorio hasta el paso 2.5) abre una consola de texto con canales, chat y voz (`/hablar` empieza y corta); `/ayuda` lista los comandos. Los `.bat` de `build\para_xp` para probar contra la PC no van a git porque llevan la IP.
- **Bibliotecas:** versiones, origen y huellas en third_party/VERSIONS.md. Para actualizar una: bajarla, anotarla, reemplazarla y correr `huellas_terceros.ps1 -Actualizar`. Lo nuestro para integrarlas está en `third_party/integracion/`.
- **Fuzzing:** `tools\fuzzing.ps1 [-Segundos N]` (10 minutos si no se dice otra cosa) corre los fuzzers de fuzz/ en 64 y 32 bits, a la vez, con AddressSanitizer. Antes hay que compilarlos con check.ps1, que además los corre 30 segundos cada vez.
  - Son 8: `fuzz_voz` (BufferVoz y Opus con paquetes al azar), `fuzz_voz_servidor` (datagramas al azar contra `servidor::Voz`, crudos o cifrados por una sesión real), `fuzz_protocolo` (con todos los mensajes), `fuzz_nombres` (reglas de nombres y chat contra otra versión escrita aparte), `fuzz_base` (archivo `ventrik.db` dañado o armado a mano), `fuzz_utf`, `fuzz_textos` (huella, código de dueño y dirección) y `fuzz_identidad` (archivo de la clave).
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
  - sin comentarios en el código y lo más corto posible, reusando funciones que ya existen.

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
  - un envío que quedó a medias (`WANT_WRITE`) se reintenta con **el mismo largo** (`SesionTls::a_medias_`): al reintentar, Mbed TLS manda lo que tenía guardado pero devuelve como enviado el largo nuevo, y si mientras tanto se encolaron más datos se perdían sin aviso. Lo prueba "Un envío TLS a medias..." en prueba_conexion.cpp.
- **Huella:** SHA-256 de la clave pública (SubjectPublicKeyInfo), en base32 de a 4 (`src/common/cifrado/huella.cpp`). Se compara en tiempo constante.
- **Identidad del servidor:** clave ECDSA P-256 en DER; el certificado se rehace en cada arranque y vence en 2099. Al cargar se revisa que la pública y la privada coincidan (`mbedtls_pk_check_pair`). Si el archivo existe pero no sirve, el servidor no arranca. La clave se crea sin reemplazar nunca un archivo existente (si otro servidor la creó a la vez, gana el otro).
- **Red:** Winsock con funciones de XP, solo IPv4. Sockets sin trabarse y sin heredarse. `SO_EXCLUSIVEADDRUSE` y `SetHandleInformation` pueden fallar en XP sin que importe. `aceptar` saltea las conexiones que se cortaron en la cola.
- **Servidor (src/server/):**
  - un hilo de red con `select()` (`FD_SETSIZE` 1024 solo en servidor.cpp), conexiones como máquinas de estados;
  - topes: 10 s para cifrar y saludar, 5 s para cerrar, 4 a medio saludar y 16 en total por IP, 64 a medio saludar en total, 512 conexiones, 16 KiB antes del saludo, mensajes de 64 bytes hasta el Hola;
  - `proteccion.cpp`: ritmo por IP, bloqueo temporal, turno global de saludos y una reserva con techo para las IP conocidas (diseno.md, "Contra inundaciones de conexiones"). Una falla le quita a una IP lo de conocida;
  - `registro.cpp`: la consola se escribe desde otro hilo con una cola de 256 líneas; si se llena, descarta y avisa cuántas.
- **Servidor, paso 2.3:**
  - `base.cpp`: SQLite en su propio hilo (`ServicioBase`) con una cola de 4096 pedidos; cada función de `Base` se niega si la llama otro hilo. Las respuestas vuelven al hilo de red, que se despierta con un socket UDP de 127.0.0.1 (`red::Despertador`, anda en XP);
  - `permisos.cpp`: una función por pedido, sin red, que dice sí o el motivo del no;
  - estado de canales y personas en memoria (copia de la base); el hilo de red nunca espera a la base salvo al arrancar;
  - topes: 10 mensajes de chat cada 10 s por persona y 20 (más 2 por segundo) por IP; 20 pedidos (más 5 por segundo) y 3 historiales (más 1 por segundo) por persona; 60 canales, 5 intentos de nombre, 30 s sin datos, 1 MiB pendiente de enviar por conexión, historial de a 256 mensajes o 64 KiB;
  - la cola de salida borra lo ya enviado cuando pasa de 64 KiB o de la mitad (así no crece sin fin, no pasa de 1 MiB + 64 KiB y no se copia entera en cada envío), y el socket se marca para escribir apenas hay algo pendiente: si no, lo encolado para otra persona esperaba a su próximo ping. Si el envío avanza, la conexión cuenta como viva aunque no mande nada;
  - la base: ids de canal con AUTOINCREMENT (nunca se reusan), un mensaje solo se guarda si su canal existe, `SQLITE_DBCONFIG_DEFENSIVE`, `trusted_schema=OFF`, filas de 64 KiB como máximo, y se mira `user_version` antes de tocar nada. El historial descarta filas fuera de orden (un índice dañado las devuelve así);
  - `Cubeta` (proteccion.h) es la única cuenta de ritmo: la usan la protección por IP y los topes por persona;
  - el código de dueño se compara en tiempo constante y uno equivocado cuenta como falla para la protección.
- **Programa:** `cliente::conectar` saluda, se presenta y devuelve la conexión abierta (`ConexionAbierta`) con lo que sobró de leer. `cliente::Sesion` corre la conexión en su propio hilo con una cola de órdenes y otra de novedades (la va a usar la ventana del 2.5) y manda ping cada 5 s.
- **Textos de la interfaz con datos:** `con_datos(plantilla, {...})` cambia `{1}` a `{9}`; así el orden de las palabras queda en lang/.
- **Voz (paso 2.4):**
  - la clave de DTLS no viaja: el servidor manda `ClaveVoz{identidad}` por TLS y los dos lados la derivan con `SesionTls::exportar` (exportador de TLS 1.3, etiqueta "EXPERIMENTAL ventrik voz", contexto = identidad). DTLS 1.2 solo con PSK y AES-GCM o ChaCha20-Poly1305; el servidor respeta el orden del programa (`MBEDTLS_SSL_SRV_CIPHERSUITE_ORDER_CLIENT`);
  - servidor (`servidor::Voz`, sin sockets): solo atiende UDP de IPs con conexión presentada; una sesión "escucha" para direcciones nuevas que se promueve recién cuando mandó el ServerHello (si solo mandó la cookie, no); `set_client_transport_id` después de **cada** `session_reset` (si no, la cookie nunca coincide); `CLIENT_RECONNECT` sobre una sesión armada la destruye; se ata a la conexión al terminar el saludo y "la última gana". a una sesión armada solo le llegan datagramas cuyos registros son todos datos o avisos de la época 1 (`solo_datos_cifrados`, que solo mira las cabeceras DTLS); el resto va a la escucha, con su cookie y sus topes. Mbed TLS devuelve error ante un ClientHello falso de 14 bytes (`MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE`) o un mensaje de saludo de la época 0 después del saludo, y eso cortaba la voz de cualquiera (lo encontró la revisión y después el fuzzer);
  - topes: saludos por IP (20, después 10 por segundo) y una cubeta global de techo; 4 saludos pendientes por IP y 64 en total, 10 s de plazo; por sesión, 120 datagramas por segundo antes de descifrar y 60 voces por segundo reenviadas, una sola voz por datagrama; 60 s sin datos cierra la sesión; la clave rota cada hora ±10 %;
  - `SesionTls` en modo datagrama: el datagrama entregado se olvida después de cada llamada a Mbed TLS (si no, podía quedar apuntando a memoria vieja); un datagrama vacío no corta la sesión (Mbed TLS lo toma como fin); en datagrama no se recorta lo que se escribe;
  - UDP: `SIO_UDP_CONNRESET` y `SIO_UDP_NETRESET` apagados (constantes a mano, los SDK los ponen en encabezados distintos), buffer de recepción de 256 KiB (XP trae 8 KiB), el servidor atiende UDP antes que TCP y hasta 256 datagramas por vuelta;
  - programa: `CanalVoz` en el hilo de la sesión, un socket UDP nuevo por saludo, ping cada 5 s, reintento con espera creciente de 2 a 60 s y algo de azar; el audio y la red se pasan los paquetes por dos colas fijas de un productor y un consumidor (`PuenteVoz`), sin candados ni memoria nueva en el hilo de audio. Una sola `Sesion` por `PuenteVoz` a la vez;
  - audio: miniaudio desde Vista y el puente WinMM en XP (10 ms, 8 de entrada, 3 adelantados, sin corregir desfase); Opus VOIP de 20 ms; calidad automática: Alto (48 kHz, complejidad 10, 24 kbps) o Bajo (16 kHz, complejidad 0, 20 kbps) en PCs de un núcleo físico (`GetLogicalProcessorInformation` por `GetProcAddress`: los de un núcleo con HyperThreading cuentan como uno); el puente WinMM borra `WHDR_DONE` antes de devolver cada pedazo del micrófono (si no, con el micrófono desenchufado giraba sin pausa en "tiempo crítico"); una sola voz por vez (la primera hasta 1 s de silencio) y espera fija de 40 ms hasta la etapa 3.
- **Reglas de texto (src/common/texto.cpp):** nombres de 1 a 32 caracteres (64 bytes) sin controles, invisibles ni marcas de dirección; chat hasta 4000 bytes con saltos de línea y U+200D; los nombres se comparan con `LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE)`. Los mensajes del protocolo las aplican al leer y al escribir.
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
- **Caracteres que dan vuelta el texto** (U+202A–202E, U+2066–2069) dentro del código frenan /WX (C5255, contra "Trojan Source"): en las pruebas van como bytes, por ejemplo `"\xE2\x80\xAE"`.
- **Una prueba no crea su propio `cifrado::Inicio`:** al destruirse apaga PSA y las pruebas que siguen fallan. Ya lo crea tests/main.cpp.
- **El antivirus de Windows** bloquea unos segundos los `.exe` recién hechos: check.ps1 reintenta la copia.
- **libFuzzer** escribe lo que aprende en la **primera** carpeta que recibe: nunca pasarle primero `fuzz\semillas`.
- **AddressSanitizer** guarda la memoria liberada un rato (para detectar usos tardíos), así que el fuzzer llega a unos 600 MB. No es una pérdida: con `ASAN_OPTIONS=quarantine_size_mb=16` se queda en 135 MB.
- **Start-Process** no pone comillas a los argumentos: con espacios en la ruta del proyecto, ponerlas a mano (fuzzing.ps1 lo hace).

## Pendientes (además de los VENTRIK_PENDIENTE)

- Prueba de 10 minutos (con la primera beta) y prueba larga del puente WinMM en XP.
- Números de la prueba de red entre la PC y la HP 2133, la RAM en la HP 2133, el ancho de banda medido y el arranque.
- Windows 11.
- De las revisiones del 2.1 y 2.2: validar la configuración del servidor cuando se cargue de un archivo (rango del protocolo derecho y sin pasar de `kProtocoloMaximo`; las pruebas usan protocolos 2 a 5).
- Nombres "parecidos" con letras de otro alfabeto: etapa 9.
- Voz, etapa 3: buffer que se ajusta solo (hoy 40 ms fijos, unas 110 ms de punta a punta en la red de casa), mezcla de varias voces, silenciar y ensordecer.
- Voz, etapa 6: la regla "el UDP viene de la misma IP que el TCP" deja sin voz a casas con dos salidas a internet, algunos CGNAT y servidores con varias placas; revisarla con el relay.
- Voz, etapa 8: un hilo por núcleo en el servidor (en la netbook, un saludo TLS en el mismo hilo puede meter un corte corto en la voz), tope de voces, corrección de errores de Opus cuando hay pérdida.
- Voz: al rotar la clave cada hora, el programa corta la sesión vieja antes de tener la nueva (unos milisegundos de voz perdida que Opus disimula); se puede pasar a "armar la nueva antes de soltar la vieja".
- Para la ventana del 2.5: mostrar cada nombre aislado entre U+2068 y U+2069, para que un nombre en hebreo o árabe no desordene la línea del chat.
- `Presentado` va en un solo mensaje: con unas 880 personas no entra en 64 KiB. Partirlo antes de subir `conexiones_maximas` de 512.
- Archivos que este paso no tocó todavía tienen comentarios (bytes.cpp, marcos.h, utf.*, registro.h, inicio.h, proceso.cpp, archivos.cpp): se sacan cuando se toquen.
- Para el paso 2.5 y la reconexión automática (etapa 8): **nunca** reintentar solo después de "la huella no coincide". Es un aviso de seguridad, y 5 intentos en un minuto bloquean 5 minutos la IP de toda la casa.
- Para las etapas 6 y 7: con relay, muchas personas llegan desde la misma IP; los topes por IP y el bloqueo necesitan otra regla ahí. Las pruebas de puerto del directorio no tienen que contar como fallas. Con IPv6, los topes por IP van por bloque /64.
- Ajustar los números de la protección contra inundaciones después de medir en la HP 2133 como servidor (modo servidor limitado).
