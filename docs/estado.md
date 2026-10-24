# Ventrik — Estado del trabajo

Para retomar sin perder contexto (se carga desde CLAUDE.md). Actualizar al cerrar cada paso.
Última actualización: 2026-10-24, al terminar la parte 3b del paso 2.5.

## Dónde estamos

- **Etapa 0: cerrada.** Falta solo probar en Windows 11 (no hay PC con Windows 11).
- **Etapa 1: cerrada.** Resultados en docs/mediciones.md; decisiones en docs/diseno.md.
- **Etapa 2, paso 2.1: cerrado.** Lector y escritor único de mensajes, saludo con negociación de versión y funciones, y fuzzing (src/common/protocol, fuzz/). Probado en la HP 2133 con XP.
- **Etapa 2, paso 2.2: cerrado.** Servidor de consola con su clave y su huella, conexión por TLS 1.3 que verifica la huella, protección contra inundaciones (pedida por el dueño, en todos los niveles) y Mbed TLS con control de hilos. Probado en la HP 2133: pruebas automáticas en XP, y conexión real XP → PC con la huella correcta (conecta) y con una falsa (rechaza).
- **Etapa 2, paso 2.3: cerrado.** Presentación con nombre, canales administrados por el dueño desde adentro (código de dueño provisorio), chat por canal guardado en SQLite, permisos verificados en el servidor, ping, diagnóstico y consola de prueba provisoria (`ventrik.exe --consola`). Probado a mano: HP 2133 como invitado y la PC como dueño, contra el servidor de prueba de la PC.
- **Diseño actualizado fuera de Claude Code (2026-10-23):** aspecto nuevo de la interfaz (grafito con acento naranja, Barlow) en docs/aspecto.md, con capturas de cada pantalla en docs/pantallas/; ya no vale el aspecto "Equipo de campo". En docs/diseno.md se sumaron: gráfico de ping y pérdida en la barra de estado (los dos modos) y, para la etapa 10, color del tema en los detalles, fondo del chat del servidor, icono del servidor animado, marcos de avatar y el editor de efectos. Nada de eso cambia los pasos 2.4 y 2.5 salvo el aspecto y el gráfico de ping.
- **Etapa 2, paso 2.4: cerrado.** Voz por DTLS 1.2 a través del servidor: clave derivada de la conexión TLS, reenvío solo al mismo canal, audio con miniaudio o el puente WinMM en XP, `/hablar` en la consola. Probado a mano: voz en los dos sentidos entre la HP 2133 con XP y la PC, y que otro canal no escucha.
- **Etapa 2, paso 2.4, segunda parte: cerrada.** El paquete de medios por DTLS lleva el tipo de flujo (1 = voz) y el número de flujo (0), el servidor descarta los tipos y flujos que no conoce sin cortar la sesión, y el bit 0 de las funciones queda reservado para "pantalla". Prepara compartir pantalla (etapa 12). En la misma prueba apareció un retraso de 1 a 2 segundos de la PC a la HP: la espera hacia los parlantes del puente de XP crecía sola (medido con `/diag`: 360, 530 y 580 ms) y ahora se recorta en 120 ms. Probado a mano: voz en los dos sentidos entre la HP 2133 con XP y la PC, sin el retraso.
- **Etapa 2, paso 2.5, parte 1: cerrada.** La ventana con Direct3D 9 y Dear ImGui, el aspecto de docs/aspecto.md (fuentes Barlow dentro del .exe, piezas e íconos dibujados por código) y la pantalla Inicio: servidores guardados en esta PC, entrar con dirección, huella, nombre y código de dueño, y lo de etapas futuras apagado con un cartel que dice en qué etapa llega. Ícono del programa y emblema sacados de docs/logo.png (el dueño pidió solo la cabeza con los auriculares). Comparada con docs/pantallas/Inicio.png. Probada a mano en la PC y en la HP 2133 (entrar, guardado, dueño, huella falsa, minimizar y bloquear).
- **Diseño actualizado (2026-10-23): canales de texto y notificaciones.** Dos tipos de canal (de voz y de texto, en dos grupos en la lista, con contador de nuevos y la marca "solo los admins escriben") y notificaciones (las de Windows 10 y 11 en la moderna, una ventana propia en la clásica). Está en docs/diseno.md, docs/aspecto.md y docs/plan.md; capturas nuevas en docs/pantallas/ (Rendimiento-canal-texto.png y Visual-canal-texto.png, y Rendimiento.png y Visual.png actualizadas).
- **Diseño actualizado (2026-10-24): compartir pantalla, en desarrollo.** Llega en la etapa 12 (VP8 para todos, incluido XP; H.264 por placa de video como copia opcional). Desde antes se prepara sin código muerto: el paquete de medios (paso 2.4, segunda parte), la subida por prioridades (etapa 8), el permiso apagado (etapa 9), el proceso de medios (etapa 10) y la interfaz ("Pronto" en el 2.5, paso 6 de Hostear apagado). Está en docs/diseno.md y docs/plan.md, con capturas en docs/pantallas/ (PantallaCompartida.png, PantallaCompartida-dialogo.png, y Rendimiento.png y Visual.png actualizadas).
- **Diseño actualizado (2026-10-24): Hostear es un asistente por pasos.** 7 pasos y la pantalla final Invitar gente; con el servidor creado, los mismos pasos van como pestañas en "Configuración del servidor". Está en docs/diseno.md ("Hostear: asistente por pasos") y docs/plan.md; Hostear.png se reemplazó por Hostear-1.png a Hostear-8.png (con las variantes del paso 4 y del 6). No es para la etapa en curso.
- **Diseño actualizado (2026-10-24): amigos, chats y llamadas directas, sin servidor (etapa 14).** ID con parte antispam y código corto, red repartida (DHT) para encontrarse, conexión directa o por relay, cifrado de punta a punta con MLS, grupos, llamadas grupales con reparto, y copias cifradas en Google Drive. Está en docs/diseno.md ("Amigos y llamadas directas") y docs/plan.md; capturas nuevas en docs/pantallas/ (Amigos*, LlamadaGrupal*, Recuperar-1 a 3) y Ajustes.png reemplazada. No cambia nada de la etapa en curso.
- **Etapa 2, paso 2.5, parte 2: cerrada.** Canales de texto en el protocolo, la base y el servidor: cada canal lleva su tipo, la marca "solo los admins escriben" y su contador; a los de texto no se entra, se miran (uno por vez) y se escriben desde cualquier canal de voz; el mensaje completo va a quien está en el canal de voz o mira el de texto, y a los demás solo les llega "este canal tiene N mensajes", como mucho una vez por segundo. La base del servidor pasa a la versión 2 y una de la versión 1 se actualiza sola. La consola de prueba solo se ajustó para seguir andando; lo nuevo se comprueba con pruebas automáticas contra un servidor real. En la prueba a mano apareció un retraso de la voz que crecía con el tiempo en los dos sentidos: el colchón de la voz juntaba cada traba del WiFi y no la soltaba; ahora tiene tope de 120 ms. `check.ps1` cierra solo los Ventrik abiertos desde `build` antes de compilar (menos el servidor de prueba).
- **Etapa 2, paso 2.5, parte 3a: cerrada.** El dueño eligió partir la parte 3 en dos. Al entrar a un servidor se abre su pantalla en modo Rendimiento:
  - los canales de texto y de voz, con la gente adentro y quién habla;
  - el chat, que carga lo más viejo al subir;
  - los contadores de nuevos mientras estás conectado;
  - la tecla para hablar (Alt izquierdo por defecto, se cambia y se guarda);
  - la barra de estado con el gráfico de ping del último minuto.

  Lo que en la captura no tiene etapa va apagado con "Pronto": "Buscar usuario", el nombre y la frase del servidor, la nota de GIFs y el botón del costado del chat. La lunita de "ausente" y el candado en un canal de voz no se pueden mostrar apagados (son estados que hoy nunca pasan) y quedan para cuando se diseñen. Comparada con las dos capturas sacando fotos de la ventana a 1376 × 768.
- **Etapa 2, paso 2.5, parte 3b: cerrada.**
  - Lo leído en cada canal queda guardado en la PC (programa.dat versión 2), y una lista de la versión 1 se pasa sola a la nueva sin perder los servidores.
  - El dueño crea canales con el + de cada grupo y los cambia o borra con clic derecho; borrar pide confirmación, y General no se puede borrar.
  - El gráfico de la barra de estado abre el diagnóstico de la PC, con cuántas veces se recortó el colchón de la voz.
  - Se borró la consola de prueba.
  - Prueba a mano (PC y la notebook con XP): todo anduvo. Los microcortes coinciden con saltos del ping a unos 150 ms y del procesador, y en la notebook Ventrik usa entre 22 y 40 % (picos de 75 % en cada corte). Es mucho más que lo medido para la voz en la etapa 1 (7 % hablando, 2,4 % por voz escuchada), así que lo demás se lo come otra parte del programa.
- **Lo que sigue: medir el consumo en la notebook** con un registro por segundo (procesador separado por tarea, dibujos por segundo, ping, colchón y recortes) y arreglar con esos datos. Después, el **paso 2.6**, la prueba en check.ps1 que levanta servidor y programa en las cuatro combinaciones (clásica y moderna en cada lado). Después, la prueba de estrés y ataques que pidió el dueño (propuesta como 2.7). Las notificaciones son el primer paso de la etapa 3.

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
- **Programa:** `ventrik.exe` abre la ventana. Los servidores a los que se entró quedan en `%APPDATA%\Ventrik\Programa\programa.dat` (sin el código de dueño). `ventrik.exe --entrar <ip[:puerto]> <huella> <nombre> [--dueno <código>]` abre la ventana y entra directo: es para las pruebas (los `.bat` y la del paso 2.6); el código de dueño en la línea de comandos queda solo para pruebas hasta las identidades de la etapa 4. Los `.bat` de `build\para_xp` para probar contra la PC no van a git porque llevan la IP.
- **Bibliotecas:** versiones, origen y huellas en third_party/VERSIONS.md. Para actualizar una: bajarla, anotarla, reemplazarla y correr `huellas_terceros.ps1 -Actualizar`. Lo nuestro para integrarlas está en `third_party/integracion/`.
- **Fuzzing:** `tools\fuzzing.ps1 [-Segundos N]` (10 minutos si no se dice otra cosa) corre los fuzzers de fuzz/ en 64 y 32 bits, a la vez, con AddressSanitizer. Antes hay que compilarlos con check.ps1, que además los corre 30 segundos cada vez. Se corre largo (10 minutos) solo cuando el paso cambia algo que lee datos de afuera (red, archivos, base del servidor, voz), y mientras pasa otra cosa (GitHub, documentos), nunca durante una prueba a mano en esta PC porque usa todo el procesador; en los demás pasos alcanzan los 30 segundos de check.ps1 (decisión del dueño, 2026-10-24).
  - Son 9: `fuzz_guardados` (archivo de servidores guardados, que tiene que volver a armarse byte por byte), `fuzz_voz` (BufferVoz y Opus con paquetes al azar), `fuzz_voz_servidor` (datagramas al azar contra `servidor::Voz`, crudos o cifrados por una sesión real), `fuzz_protocolo` (con todos los mensajes), `fuzz_nombres` (reglas de nombres y chat contra otra versión escrita aparte), `fuzz_base` (archivo `ventrik.db` dañado o armado a mano), `fuzz_utf`, `fuzz_textos` (huella, código de dueño y dirección) y `fuzz_identidad` (archivo de la clave).
  - Las compilaciones de fuzzing son los presets `fuzzing64` y `fuzzing32` (v143, nunca se publican). Usan el mismo `_WIN32_WINNT` que la clásica y RtlGenRandom.
  - Lo aprendido queda en `build\fuzzing\corpus`; cada caso que falla, en `build\fuzzing\fallas`. Las semillas de arranque están en `fuzz\semillas`.
  - Código nuevo que lea datos de red o archivos: sumar su fuzzer en fuzz/ y en CMakeLists.txt y fuzzing.ps1.
- **Aspecto visual:** docs/aspecto.md (colores, medidas y piezas, y cómo dibujarlas con ImDrawList) y las capturas de docs/pantallas/. Leerlos antes de tocar cualquier pantalla; al terminar una, sacar una captura del mismo tamaño y compararla.
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
- **Servidor, paso 2.5, parte 2 (canales de texto):**
  - base versión 2: `canales` suma `tipo`, `solo_admins` y `contador`, y `mensajes` suma `numero`. Una base versión 1 se migra en una sola transacción (`ALTER TABLE`, los mensajes viejos se numeran por canal en orden de id con `ROW_NUMBER()` y el contador queda en el número más alto); una de la versión 3 o más no se abre ni se toca;
  - un canal de voz con la marca no existe: el lector y el escritor del protocolo lo rechazan y `canales()` saltea esas filas (si no, una base dañada rompería el `Presentado` de todos);
  - el contador manda en memoria: el hilo de red le pone el número a cada mensaje antes de pasarlo a la base, y la base guarda `MAX(contador, numero)`, así no baja. Si la cola de la base está llena, el número se devuelve y se contesta "lleno"; si un mensaje no se llega a guardar queda un número salteado, que no molesta. En "no guardar" el mensaje sale apenas la base acepta el pedido, y la base solo sube el contador. Al cerrar, el hilo de la base termina los pedidos que quedaron en la cola, así el contador guardado alcanza al de memoria. Lo único que lo puede bajar después de reiniciar es que la base falle (disco lleno o dañado) justo con los últimos mensajes;
  - una base dañada no rompe a nadie: `canales()` saltea los canales con contador fuera de rango (o al tope, para que no dé la vuelta a 0) y el servidor no arranca si "General" no es de voz;
  - quién recibe: en uno de voz, los que están adentro; en uno de texto, los que lo miran y quien lo escribió (así recibe su número). Los demás quedan anotados y en cada vuelta del bucle se les manda `MensajesNuevos` con el contador del momento, si pasó 1 s desde el anterior (el primero sale enseguida). Mirar un canal borra su aviso pendiente;
  - al borrar un canal de texto, quien lo miraba queda mirando un id que no vuelve a existir (los ids nunca se reusan), que es lo mismo que no mirar nada;
  - la respuesta del historial vuelve a pedir `puede_leer` cuando llega de la base: si mientras tanto saliste de ese canal de voz, no te llega.
- **Programa:** `cliente::conectar` saluda, se presenta y devuelve la conexión abierta (`ConexionAbierta`) con lo que sobró de leer. `cliente::Sesion` corre la conexión en su propio hilo con una cola de órdenes y otra de novedades (la va a usar la ventana del 2.5) y manda ping cada 5 s.
- **Ventana (paso 2.5):**
  - Direct3D 9 con dispositivo HAL, proceso de vértices por hardware y, si no se puede, por software (placas viejas como la VIA de la HP 2133). `D3DCREATE_FPU_PRESERVE`: sin eso, Direct3D 9 pone el procesador en precisión simple y en la clásica cambian las cuentas con `double`. `D3DPRESENT_INTERVAL_IMMEDIATE`: en ventana, XP espera el refresco de la pantalla girando sin pausa.
  - Se dibuja solo cuando hace falta: 3 cuadros después de cada mensaje de Windows, como mucho uno cada 15 ms, y un cuadro cada 100 ms solo durante 1,5 s después del último movimiento con el mouse sobre algo (para que aparezcan los carteles) o mientras el dispositivo está perdido (pantalla bloqueada). Minimizada no dibuja. Medido en la PC: quieta y minimizada, 0 % de procesador.
  - Las novedades de la sesión se leen en cada vuelta del bucle (`atender`), aunque no se dibuje: si no, con la ventana minimizada o la PC bloqueada unas horas, la cola de 4096 se llenaba con los pings y la sesión se cortaba. La sesión avisa con `kMensajeNovedades` (uno solo pendiente a la vez).
  - Cancelar un intento de conexión es instantáneo: la sesión tiene un segundo `Despertador` (`cancelar_`) que la espera de la red mira mientras conecta y saluda. Antes, destruir la sesión esperaba hasta 15 s y congelaba la ventana. Mientras conecta, el botón dice "Cancelar" (en la tarjeta y en el formulario) y Enter no lanza otro intento.
  - Se puede usar con teclado solo (Tab, flechas, Espacio y Enter: `NavEnableKeyboard` y botones con `EnableNav`). Alt solo no abre el menú del sistema; Alt+Espacio y Alt+F4 sí andan.
  - Tamaño mínimo de 1100 × 640 (por la escala), recortado al área de trabajo del monitor para que con 125 % o más en pantallas chicas no quede más grande que la pantalla. Las dos columnas de Inicio se desplazan si no entran.
  - Si la placa no tiene texturas dinámicas (`D3DCAPS2_DYNAMICTEXTURES`), Dear ImGui no podría dibujar las letras: se muestra el aviso de "sin Direct3D". Al recuperar el dispositivo, el formato vuelve a `D3DFMT_UNKNOWN` (si el escritorio cambió de color, el `Reset` fallaría para siempre).
  - La ventana se muestra recién después del primer cuadro: no parpadea en blanco y la prueba de check.ps1 sabe que dibujó. Alt solo no abre el menú del sistema (va a ser la tecla para hablar).
  - Escala de Windows: `SetProcessDPIAware` por `GetProcAddress` y `LOGPIXELSX / 96` en `style.FontScaleDpi`; todas las medidas pasan por `px()`. Sin escala distinta por monitor.
  - Dear ImGui sin `imgui.ini`, sin mando (cargaría xinput por nombre), sin abrir enlaces solos (`IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS`) y sin cursor que titila.
  - Fuentes: recursos RCDATA de `src/client/ventrik.rc`, cargadas sin copiar. El tamaño de ImGui es 1,2 veces el de CSS (Barlow mide 1200 unidades de alto por 1000 de "em"); una prueba lo comprueba. Barlow cubre el alfabeto latino: los nombres en cirílico, griego, hebreo, árabe o chino se ven como recuadros.
  - U+2068 y U+2069 no hacen falta: Dear ImGui nunca reordena texto de derecha a izquierda, así que un nombre no puede desordenar la línea.
  - Ícono: `tools\hacer_icono.ps1` lo arma desde docs/logo.png (16 a 128 como mapas de bits de 32 bits, que XP entiende, y 256 en PNG). El emblema de la barra sale del mismo ícono (entrada de 48, 64, 96 o 128 según la escala) como textura `D3DPOOL_MANAGED`, que sobrevive al `Reset`.
  - Servidores guardados (`client/guardados.cpp`): marca `VKPR`, versión 2, tecla para hablar, tu nombre y hasta 32 servidores (IPv4, puerto, huella, nombre usado, último canal, última vez y hasta 64 pares canal · número leído, ordenados por canal y sin canal 0). Una lista de la versión 1 se lee con lo leído vacío y al guardar pasa a la 2. Lo leído vacío quiere decir "primera vez": todo cuenta como leído; si no, un canal que no está en la lista se creó mientras no estabas y arranca sin leer, y un número más alto que el contador del servidor se baja al contador. Se guarda al salir de la pantalla del servidor y al cerrar el programa; `recordar` conserva lo leído del mismo servidor. Se lee con el `Lector` y tiene una sola forma válida. Dañado: empieza vacío, avisa y se reescribe; de una versión más nueva, ilegible o de más de 32 KiB: avisa y no se toca, y antes de cada escritura se vuelve a mirar el archivo por si otra versión lo cambió. Cualquier cambio de formato tiene que subir la versión (un v1 con algo agregado al final cuenta como dañado), y la marca y la versión van siempre en los primeros 6 bytes. La clave es la dirección; el último usado va primero; `recordar` no acepta servidores inválidos. El código de dueño no se guarda (etapa 4).
  - Pantalla del servidor (parte 3a):
    - `cliente::Sala` (sala.cpp) guarda lo del servidor sin ventana: canales, personas, el chat de tu canal de voz y el del canal de texto que mirás (los otros mensajes el servidor no los manda), y lo leído por canal en memoria. Cada acción devuelve las órdenes para la sesión, así se prueba sin red;
    - cada chat tiene tope de 10 000 mensajes y 2 MiB: lo más viejo se suelta y se puede volver a pedir, y al llegar al tope no se piden más páginas. Solo se acepta el historial que se pidió (un servidor no puede llenar la memoria mandando páginas sueltas), y personas y canales tienen los topes del protocolo (1024 y 256);
    - si el servidor rechaza un pedido de historial (por ritmo), se vuelve a pedir pasado 1 s, también la primera página; si rechaza mirar un canal de texto, se vuelve al chat de voz. `fuzz_sala` prueba la Sala con secuencias al azar de todo lo que puede llegar;
    - la lista de mensajes mide la altura de cada mensaje una sola vez por ancho (los nuevos al llegar) y dibuja solo lo que se ve; queda pegada abajo si ya estabas abajo, y al cargar más viejos no salta;
    - el vigía (vigia.cpp) es un hilo que cada 20 ms lee solo la tecla elegida con `GetAsyncKeyState` (anda aunque la ventana no esté al frente) y avisa a la ventana cuando cambia algo: si transmitís, quién habla, el estado de la voz, el medidor (hasta 10 veces por segundo y solo mientras transmitís) y una vez por segundo para el gráfico. La ventana sigue dibujando solo cuando hace falta;
    - "Cambiar tecla" toma la primera tecla que se aprieta después de tocar el botón (así no toma el Espacio o Enter con que se tocó), nunca los clics izquierdo y derecho, y solo con Ventrik al frente: si cambiás de ventana se cancela, para no tomar algo que escribas en otro programa. Esc cancela. Después espera a que sueltes la tecla para empezar a transmitir;
    - el color del avatar sale de `nombre_para_comparar`, así es el mismo en todas las PCs y no cambia con las mayúsculas;
    - si la ventana es angosta, los botones de arriba quedan solo con el ícono y el nombre en el cartel;
    - los cuadros del dueño (crear, cambiar y borrar canales) son ventanas modales de Dear ImGui con las piezas de siempre. En crear y cambiar, Enter confirma; en borrar, Enter no borra (el foco queda en Cancelar) y hay que tocar "Borrar". `kCanalGeneral` está en el protocolo, así el programa no ofrece borrar General;
    - el diagnóstico junta los datos una vez por segundo en `Programa`, con el mismo `Diagnostico` que tenía la consola;
    - lo leído se guarda al salir de la pantalla del servidor, al cerrar el programa y al apagar o cerrar la sesión de Windows (`WM_ENDSESSION`); se toma de la lista solo si la huella también coincide (un servidor reinstalado en la misma dirección no hereda lo leído), y un canal con número 0 se ignora (si no, rompía el guardado de toda la lista).
  - Inicio contra la captura: se siguió la captura donde no coincide con aspecto.md (título de página Barlow Bold 27, títulos de bloque 17, buscador con fondo de panel, botones de tarjeta de 44). Las tarjetas muestran la dirección porque el protocolo todavía no tiene nombre del servidor.
- **Textos de la interfaz con datos:** `con_datos(plantilla, {...})` cambia `{1}` a `{9}`; así el orden de las palabras queda en lang/.
- **Voz (paso 2.4):**
  - la clave de DTLS no viaja: el servidor manda `ClaveVoz{identidad}` por TLS y los dos lados la derivan con `SesionTls::exportar` (exportador de TLS 1.3, etiqueta "EXPERIMENTAL ventrik voz", contexto = identidad). DTLS 1.2 solo con PSK y AES-GCM o ChaCha20-Poly1305; el servidor respeta el orden del programa (`MBEDTLS_SSL_SRV_CIPHERSUITE_ORDER_CLIENT`);
  - servidor (`servidor::Voz`, sin sockets): solo atiende UDP de IPs con conexión presentada; una sesión "escucha" para direcciones nuevas que se promueve recién cuando mandó el ServerHello (si solo mandó la cookie, no); `set_client_transport_id` después de **cada** `session_reset` (si no, la cookie nunca coincide); `CLIENT_RECONNECT` sobre una sesión armada la destruye; se ata a la conexión al terminar el saludo y "la última gana". a una sesión armada solo le llegan datagramas cuyos registros son todos datos o avisos de la época 1 (`solo_datos_cifrados`, que solo mira las cabeceras DTLS); el resto va a la escucha, con su cookie y sus topes. Mbed TLS devuelve error ante un ClientHello falso de 14 bytes (`MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE`) o un mensaje de saludo de la época 0 después del saludo, y eso cortaba la voz de cualquiera (lo encontró la revisión y después el fuzzer);
  - topes: saludos por IP (20, después 10 por segundo) y una cubeta global de techo; 4 saludos pendientes por IP y 64 en total, 10 s de plazo; por sesión, 120 datagramas por segundo antes de descifrar y 60 voces por segundo reenviadas, una sola voz por datagrama; 60 s sin datos cierra la sesión; la clave rota cada hora ±10 %;
  - `SesionTls` en modo datagrama: el datagrama entregado se olvida después de cada llamada a Mbed TLS (si no, podía quedar apuntando a memoria vieja); un datagrama vacío no corta la sesión (Mbed TLS lo toma como fin); en datagrama no se recorta lo que se escribe;
  - UDP: `SIO_UDP_CONNRESET` y `SIO_UDP_NETRESET` apagados (constantes a mano, los SDK los ponen en encabezados distintos), buffer de recepción de 256 KiB (XP trae 8 KiB), el servidor atiende UDP antes que TCP y hasta 256 datagramas por vuelta;
  - programa: `CanalVoz` en el hilo de la sesión, un socket UDP nuevo por saludo, ping cada 5 s, reintento con espera creciente de 2 a 60 s y algo de azar; el audio y la red se pasan los paquetes por dos colas fijas de un productor y un consumidor (`PuenteVoz`), sin candados ni memoria nueva en el hilo de audio. Una sola `Sesion` por `PuenteVoz` a la vez;
  - audio: miniaudio desde Vista y el puente WinMM en XP (10 ms, 8 de entrada, 3 adelantados, sin corregir desfase); Opus VOIP de 20 ms; calidad automática: Alto (48 kHz, complejidad 10, 24 kbps) o Bajo (16 kHz, complejidad 0, 20 kbps) en PCs de un núcleo físico (`GetLogicalProcessorInformation` por `GetProcAddress`: los de un núcleo con HyperThreading cuentan como uno); el puente WinMM borra `WHDR_DONE` antes de devolver cada pedazo del micrófono (si no, con el micrófono desenchufado giraba sin pausa en "tiempo crítico"); una sola voz por vez (la primera hasta 1 s de silencio) y espera fija de 40 ms hasta la etapa 3. Si el colchón de la voz pasa de 120 ms (una traba del WiFi que después llega toda junta, o placas de sonido con relojes un poco distintos), se tiran los pedazos más viejos y vuelve a 40 ms: sin eso el retraso crecía con cada traba y no bajaba nunca (oído en la PC y en la HP, y medido con una prueba que simula trabas: 580 ms que no se iban). El diagnóstico (tocar el gráfico de la barra de estado) muestra cuánta voz espera en el colchón y cuántas veces se recortó.
- **Reglas de texto (src/common/texto.cpp):** nombres de 1 a 32 caracteres (64 bytes) sin controles, invisibles ni marcas de dirección; chat hasta 4000 bytes con saltos de línea y U+200D; los nombres se comparan con `LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE)`. Los mensajes del protocolo las aplican al leer y al escribir.
- **doctest:** en la clásica va sin su informe de cierres inesperados, que usa SetThreadStackGuarantee.
- **Al arrancar:**
  - `endurecer_carga_de_dll()` (SetDefaultDllDirectories, o si no SetDllDirectoryW);
  - DEP en la clásica;
  - la clásica se niega a abrir si no es XP SP3 o posterior.
- **UTF-8:** Ventrik lo valida por su cuenta (`es_utf8_valido`), porque el de XP acepta mitades de par UTF-16. `a_utf8` cambia las mitades sueltas por U+FFFD, porque XP no lo hace.
- **Audio en XP:** puente propio con WinMM.
  - Pedazos de 10 ms, 64 lugares hacia los parlantes y 8 para el micrófono, **sin ninguna corrección de desfase**: XP avisa mal la posición de los parlantes y devuelve tarde los pedazos.
  - Pero la espera hacia los parlantes se recorta: si ya hay 120 ms o más en cola, el pedazo de 10 ms que tocaba se descarta (`kMaximoEnEspera`). Sin eso, cada traba del equipo (WiFi, procesador ocupado) dejaba más audio esperando y nunca bajaba: en la HP 2133 llegó a 580 ms y se oía 1 a 2 segundos tarde. Se dejó margen (120 ms y no 40) porque XP devuelve tarde los pedazos ya sonados. El diagnóstico muestra esa espera.
  - miniaudio tiene mínimos fijos (30 ms con DirectSound, 40 ms con WinMM) y en XP, con DirectSound, consulta cada 1 ms.
- **Consumo de la voz:** niveles Alto, Normal, Bajo, Mínimo y Automático, todos con Opus (docs/diseno.md). No sumar otros códecs.
- **Protocolo (src/common/protocol):**
  - todo lo que llega de la red pasa por `Lector`: error pegajoso, cada largo comparado con lo que queda, texto con largo u16, UTF-8 validado y sin bytes 0;
  - en TLS, cada mensaje va en un marco: largo del cuerpo (u32) + tipo (u16), cuerpo de 64 KiB como máximo (64 bytes hasta el saludo); el `Ensamblador` guarda un solo marco por vez;
  - el saludo (`Hola`, `Bienvenida`, `Rechazo`) no cambia nunca de formato; lo nuevo va en mensajes posteriores, cuando los dos ya saben qué versión hablan;
  - las funciones son bits de un u64. El bit 0 (`kFuncionPantalla`) está reservado para compartir pantalla y nadie lo anuncia todavía;
  - los medios por DTLS (`MedioSubida`, `MedioBajada`) llevan `u8 tipo de flujo · u8 flujo · u16 número · u16 largo · datos`, y la bajada empieza con `u32 origen`. Hoy solo existe la voz (tipo 1, flujo 0, Opus de 1 a 400 bytes); el `Lector` marca error con cualquier otro tipo o flujo, así se descartan sin ramas que nunca se usan. "La voz sale primero" se cumple porque el servidor atiende la voz antes que el chat; la cola con prioridades llega en la etapa 8;
  - la versión de Ventrik y el rango del protocolo están en src/common/version.h. Al subir `kProtocoloMaximo` hay que sumar en `kPrimeraVersionDelProtocolo` la primera versión que lo habla (no compila si falta): es la que se le pide al programa que no llega;
  - el rechazo tiene una sola forma válida: la versión va solo en `programa_viejo`. El programa revisa con `aceptar_bienvenida` y `aceptar_rechazo` lo que contesta el servidor;
  - `Ensamblador::siguiente` entrega un marco por vez; con error consume todo, y `Lector::restante()` da 0, para que ningún bucle quede trabado. Para aceptar un mensaje, `terminado()`, nunca `restante() == 0`.

## Trampas de las herramientas

- **PowerShell 5.1:**
  - un `.ps1` con acentos tiene que guardarse en UTF-8 **con BOM**, y check.ps1 tiene fin de línea CRLF: al reemplazar texto adentro, usar `` `r`n ``;
  - `git commit -m` con comillas dobles falla: usar `git commit -F` con un archivo UTF-8 **sin BOM**;
  - si se redirige la salida de error de un programa con `$ErrorActionPreference='Stop'`, PowerShell corta: usar `Continue`.
- **doctest:** `MESSAGE(a + b)` no compila; armar el texto en una variable antes.
- **Dear ImGui:** un texto con final nulo quiere decir "hasta el cero", y un `std::string_view` vacío por defecto tiene el puntero en nulo: ImGui leía memoria que no es suya (se caía con los botones de solo ícono). Las piezas de piezas.cpp pasan siempre `""` cuando el texto está vacío.
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
- **rc.exe** busca los archivos de un `.rc` al lado del `.rc`, no en la carpeta de compilación.
- **Para manejar la ventana desde un programa** (como hará la prueba del 2.6): las teclas van bien como `WM_CHAR`/`WM_KEYDOWN` a la ventana, pero el mouse no: Dear ImGui lee la posición real con `GetCursorPos` cuando el puntero no está sobre la ventana.
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
- Microcortes de voz: en la prueba de la parte 3a (PC y HP por WiFi) se oyen cortecitos. Pueden ser el tope de 120 ms del colchón recortando de más. El diagnóstico (tocar el gráfico de la barra de estado) muestra cuántas veces se recortó el colchón: con ese número se decide. El colchón que se ajusta solo es de la etapa 3.
- La barra de estado dice "Voz: calidad alta" o "baja" según la PC; "dinámica" llega con la calidad que se ajusta sola en la etapa 8.
- Etapa 12 (futura): compartir pantalla. La preparación está repartida: 2.4 segunda parte (paquete de medios y bit reservado), 2.5 parte 3 (botón con "Pronto"), etapa 4 (paso 6 de Hostear apagado), 8 (subida por prioridades), 9 (permiso apagado) y 10 (proceso de medios). libvpx no entra a third_party/ hasta la 12. Lo primero de esa etapa: cuando el lector acepte el tipo pantalla, separarlo de la voz en el programa (hoy todo medio va a la cola de Opus) y en el servidor (tope propio, y reenviar solo a quien anunció `kFuncionPantalla` y tocó "Mirar"). "La voz sale primero" hoy ya se cumple (el servidor atiende la voz antes que el chat); la cola con prioridades llega en la etapa 8.
- "Varias PCs" (un servidor sostenido entre varias PCs) aparece "en desarrollo" en el paso 6 de Hostear y en Ajustes → Prestar esta PC, pero todavía no está diseñado en docs/diseno.md. Sería la etapa 13, y la etapa 14 usa su algoritmo de reparto.
- Etapa 14 (futura): amigos y llamadas directas. Antes de empezar: elegir una implementación de MLS que compile para XP y preguntar antes de sumarla a third_party/, igual que la DHT y la lista de autoridades de certificados para hablar con Google. Si la identidad vive en una llave de seguridad, la copia en Drive no la puede llevar.
- Ajustes, desde que exista (etapa 3): las secciones Prestar esta PC, Amigos y privacidad y Copias de seguridad van desactivadas y marcadas "EN DESARROLLO".
- Nombres en alfabetos que Barlow no tiene (cirílico, griego, hebreo, árabe, chino): se ven como recuadros. Sumar fuentes es decisión del dueño.
- Memoria de la ventana: unos 70 MB en Windows 10 (casi todo de Direct3D 9) y 7 MB en la HP 2133 con XP (medido por el dueño, Administrador de tareas).
- Por VNC, en la HP 2133 la ventana no se ve (en la pantalla de la propia PC sí). El VNC usa un controlador espejo (sin él gasta mucho), y ese controlador no ve lo que dibuja Direct3D. Decisión del dueño: dejarlo así, sin cambios solo para el VNC; la netbook se prueba usándola directamente.
- Sin Direct3D (XP por Escritorio remoto o en una máquina virtual sin 3D) solo se ve el aviso. Mientras se arrastra el borde de la ventana no se redibuja (la imagen se estira hasta soltar). Con el escritorio de XP en 16 bits el emblema puede quedar invisible (el ícono pierde la transparencia).
- Servidores guardados: con dos programas abiertos a la vez, gana el último que guarda (no se mezclan). Si hace falta, un mutex con nombre que cubra leer, mezclar y escribir. La tecla para hablar se valida en la lectura y en la escritura (0 es Alt izquierdo; nunca los clics izquierdo y derecho ni Esc).
- `borrar_temporales_de` corre antes de leer: en un disco FAT32, un corte justo entre borrar el archivo viejo y renombrar el nuevo puede perder la lista. Muy poco probable; la función es la misma para todos los archivos.
- `Presentado` va en un solo mensaje: con unas 880 personas no entra en 64 KiB. Partirlo antes de subir `conexiones_maximas` de 512.
- Archivos que este paso no tocó todavía tienen comentarios (bytes.cpp, marcos.h, utf.*, registro.h, inicio.h, proceso.cpp, archivos.cpp): se sacan cuando se toquen.
- Para el paso 2.5 y la reconexión automática (etapa 8): **nunca** reintentar solo después de "la huella no coincide". Es un aviso de seguridad, y 5 intentos en un minuto bloquean 5 minutos la IP de toda la casa.
- Para las etapas 6 y 7: con relay, muchas personas llegan desde la misma IP; los topes por IP y el bloqueo necesitan otra regla ahí. Las pruebas de puerto del directorio no tienen que contar como fallas. Con IPv6, los topes por IP van por bloque /64.
- Ajustar los números de la protección contra inundaciones después de medir en la HP 2133 como servidor (modo servidor limitado).
