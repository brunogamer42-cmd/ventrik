# Bibliotecas de third_party

Cada biblioteca se guarda con versión fija y sin modificar su código. De cada paquete se guarda solo lo necesario para compilar, más su licencia. Lo que hace falta para integrarlas (archivos que cada proyecto genera con su propio sistema de compilación, la configuración de Mbed TLS y los arreglos para Windows XP) está aparte, en `integracion/`, y se explica abajo.

`HUELLAS.txt` tiene la huella SHA-256 de cada archivo guardado, y `tools\check.ps1` avisa si alguno cambia. Para actualizar una biblioteca:

1. Bajar el paquete nuevo del origen oficial.
2. Anotar acá la versión, el origen y la huella del paquete.
3. Reemplazar sus archivos.
4. Correr `tools\huellas_terceros.ps1 -Actualizar`.

| Biblioteca | Versión | Licencia | Para qué | Agregada |
| --- | --- | --- | --- | --- |
| doctest | 2.5.3 | MIT | Pruebas automáticas (solo en `tests/`) | 2026-10-21 |
| Microsoft GSL | 3.1.0 | MIT | `gsl::span` y otras ayudas de memoria segura | 2026-10-21 |
| Opus | 1.5.2 | BSD de 3 cláusulas | Comprimir la voz | 2026-10-21 |
| Mbed TLS | 3.6.7 (rama de soporte largo) | Apache 2.0 o GPL 2.0 o posterior | TLS 1.3 y DTLS 1.2 | 2026-10-21 |
| miniaudio | 0.11.25 | Dominio público o MIT-0 | Micrófono y parlantes | 2026-10-21 |
| Dear ImGui | 1.93.0 | MIT | Interfaz | 2026-10-21 |
| RNNoise | 0.2 | BSD de 3 cláusulas | Limpiar el ruido de fondo | 2026-10-21 |
| SpeexDSP | 1.2.1 | BSD de 3 cláusulas | Cancelar el eco | 2026-10-21 |
| Argon2 (implementación de referencia) | 20190702 | CC0 o Apache 2.0 | Contraseñas | 2026-10-21 |
| SQLite | 3.54.1 | Dominio público | Base de datos del servidor | 2026-10-21 |
| stb_image | 2.30 | Dominio público o MIT | GIF del modo Visual | 2026-10-21 |
| stb_vorbis | 1.22 | Dominio público o MIT | Sonidos OGG de los avisos (etapa 3) | 2026-10-26 |
| miniz | 3.0.2 | MIT | Leer y armar los paquetes de sonidos (.vksonidos, que son zip) | 2026-10-27 |
| jsmn | 1.1.0 | MIT | Leer el índice JSON de los paquetes de sonidos | 2026-10-27 |
| libwebp | 1.6.0 | BSD de 3 cláusulas | WebP del modo Visual | 2026-10-21 |
| miniupnpc | 2.3.3 | BSD de 3 cláusulas | Abrir el puerto con UPnP | 2026-10-21 |
| Barlow y Barlow Semi Condensed | 1.408 | SIL Open Font License 1.1 | Letra de la interfaz | 2026-10-23 |

## Origen de cada paquete

"Comprobada" quiere decir que la huella coincide con la que publica el propio proyecto. Las demás son las de lo que se bajó del origen oficial, que no publica huellas.

| Biblioteca | Bajado de | Huella del paquete |
| --- | --- | --- |
| doctest | `https://raw.githubusercontent.com/doctest/doctest/v2.5.3/doctest/doctest.h` y `.../v2.5.3/LICENSE.txt` | SHA-256 `cfd518a3ef90f67e1f3ba514df23fb3627437de1a2feeba78cf5062a40021421` (doctest.h), `0fe0b331fa1513dcce8604ff1fa925f32d1cea17d8aeb1c2471fad40d291adc5` (LICENSE.txt) |
| Microsoft GSL | `https://github.com/microsoft/GSL/archive/refs/tags/v3.1.0.tar.gz` | SHA-256 `d3234d7f94cea4389e3ca70619b82e8fb4c2f33bb3a070799f1e18eef500a083` |
| Opus | `https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz` | SHA-256 `65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1`, comprobada con las notas de la versión en GitHub |
| Mbed TLS | `https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2` | SHA-256 `a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6`, comprobada con `mbedtls-3.6.7-sha256sum.txt` |
| miniaudio | `https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz` | SHA-256 `b900edcffe979816e2560a0580b9b1216d674b4f17fbadeca8f777a7f8ab0274` |
| Dear ImGui | `https://github.com/ocornut/imgui/archive/refs/tags/v1.93.0.tar.gz` | SHA-256 `f7bf6bc76e5d79d40d751096663e6c5d18daf08129dd52e7f7b09e4251c8eeee` |
| RNNoise | `https://github.com/xiph/rnnoise/releases/download/v0.2/rnnoise-0.2.tar.gz` | SHA-256 `90fce4b00b9ff24c08dbfe31b82ffd43bae383d85c5535676d28b0a2b11c0d37` |
| SpeexDSP | `https://downloads.xiph.org/releases/speex/speexdsp-1.2.1.tar.gz` | SHA-256 `8c777343e4a6399569c72abc38a95b24db56882c83dbdb6c6424a5f4aeb54d3d`, comprobada con `SHA256SUMS.txt` de Xiph |
| Argon2 | `https://github.com/P-H-C/phc-winner-argon2/archive/refs/tags/20190702.tar.gz` | SHA-256 `daf972a89577f8772602bf2eb38b6a3dd3d922bf5724d45e7f9589b5e830442c` |
| SQLite | `https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip` | SHA3-256 `92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30`, comprobada con la página de descargas; SHA-256 `0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9` |
| stb_image | `https://github.com/nothings/stb/archive/013ac3beddff3dbffafd5177e7972067cd2b5083.tar.gz` (último cambio de `stb_image.h`, 2024-05-31) | SHA-256 `b01aa93e1a968aed55f43e072c98ee401d2f20e897aabdb1a166c7166886ed11` |
| stb_vorbis | El mismo paquete que stb_image (en esa versión `stb_vorbis.c` es la 1.22, del 2021-07-11) | La misma huella que stb_image, comprobada otra vez al bajarlo el 2026-10-26 |
| miniz | `https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip` (el paquete armado de la versión, con `miniz.c` y `miniz.h` en un solo par) | SHA-256 `ada38db0b703a56d3dd6d57bf84a9c5d664921d870d8fea4db153979fb5332c5` |
| jsmn | `https://github.com/zserge/jsmn/archive/refs/tags/v1.1.0.tar.gz` | SHA-256 `5f0913a10657fe7ec8d5794ccf00a01000e3e1f2f1e1f143c34a0f7b47edcb38` |
| libwebp | `https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz` | SHA-256 `e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564` (publican una firma PGP, todavía no verificada) |
| miniupnpc | `https://github.com/miniupnp/miniupnp/archive/refs/tags/miniupnpc_2_3_3.tar.gz` | SHA-256 `8cf2c833b3e76fc4893ff29c2a376e3394962449e5970e373c0a91421724d222` |
| Barlow | `https://raw.githubusercontent.com/google/fonts/3218ae2acc5bce7ccd97c477965b5b4094241fb0/ofl/barlow/` (`Barlow-Regular.ttf`, `Barlow-SemiBold.ttf`, `Barlow-Bold.ttf`, `OFL.txt`) y `.../ofl/barlowsemicondensed/BarlowSemiCondensed-SemiBold.ttf`; commit del 2026-02-26, el último que tocó esas carpetas | Comprobada con el hash de Git que muestra GitHub para cada archivo. SHA-256 `95aa02c7c43096e0dd44d787ba6216864a67157e402adab59b35572e0c1577ea` (Regular), `86577cb32f8abe3673db53ca0f4221e6856751a4f6730c867e00f720f8bb1fc5` (SemiBold), `84e6a4d61e7c3e21f3c50ea6a4f7e5303a3467864c038be6ea3759bab8d547f9` (Bold), `bd299f4bc5b44d30be8d42e9bad3a5df7d66af1cd55d0ed72a8b8916360a1424` (Semi Condensed SemiBold), `186d750eb496a4c17a76385f82be6aea2ac1cf2de074a811d63786cf374ea73f` (OFL.txt) |

## Qué se guardó de cada una y por qué

- **doctest:** `doctest.h` y la licencia.
- **Microsoft GSL:** `include/gsl/` y la licencia. Es la 3.1.0 y no la última (5.0.1) porque desde la 4.0 GSL funciona oficialmente solo desde Visual Studio 2019, y la versión clásica usa el compilador de Visual Studio 2017. La 3.1.0 es la última que lo admite oficialmente (VS 2017 15.9).
- **Opus:** su `CMakeLists.txt` y `cmake/`, los archivos de donde ese CMake saca las listas (`*.mk`, `Makefile.am`, `configure.ac`, `package_version`, `config.h.in`), `celt/`, `silk/`, `src/`, `include/` y la licencia. De `dnn/` quedan solo los encabezados (`*.h`): las funciones con redes neuronales (DRED, OSCE) están apagadas, pero el resto del código incluye esas definiciones. Sin `tests/`, `doc/` ni los modelos de `dnn/` (18 MB).
- **Mbed TLS:** `include/`, `library/` y la licencia. Sin sus pruebas (32 MB), programas ni documentación.
- **miniaudio:** `miniaudio.h`, `miniaudio.c` y la licencia.
- **Dear ImGui:** los archivos principales, los módulos `backends/imgui_impl_win32`, `imgui_impl_dx9` e `imgui_impl_null` (este último, solo para las pruebas), y la licencia.
- **RNNoise:** `include/`, `src/` (sin `dump_features.c` ni `write_weights.c`, que son herramientas con su propio `main`) y la licencia.
- **SpeexDSP:** `include/speex/`, `libspeexdsp/` (sin los `test*.c`) y la licencia.
- **Argon2:** `include/argon2.h`, la parte de `src/` de la implementación de referencia (sin la versión optimizada, las pruebas ni los programas) y la licencia.
- **SQLite:** `sqlite3.c` y `sqlite3.h`. Es de dominio público y no trae archivo de licencia (ver https://www.sqlite.org/copyright.html).
- **stb_image y stb_vorbis:** `stb_image.h`, `stb_vorbis.c` y la licencia del repositorio (una sola para los dos).
- **miniz:** `miniz.c`, `miniz.h` y la licencia. Sin los ejemplos.
- **jsmn:** `jsmn.h` y la licencia. Sin pruebas ni ejemplos. Su código se compila en `integracion/jsmn/jsmn.c`.
- **libwebp:** `src/` (decodificador, codificador, animaciones y utilidades), `sharpyuv/`, la licencia y `PATENTS`.
- **miniupnpc:** `miniupnpc/include/`, `miniupnpc/src/` (sin pruebas ni programas de ejemplo), `miniupnpc/VERSION` y la licencia (la de la raíz del repositorio; la de la carpeta es un enlace a esa).
- **Barlow (en `fuentes/`):** los cuatro pesos que usa docs/aspecto.md (Regular, SemiBold y Bold de Barlow, y SemiBold de Barlow Semi Condensed) y la licencia, que es la misma para las dos familias. Van dentro de `ventrik.exe` como recursos (`src/client/ventrik.rc`).

## Integración (`integracion/`)

Nada de esto cambia el código de las bibliotecas.

- **`mbedtls/ventrik_mbedtls_config.h`:** ajustes sobre la configuración por defecto de Mbed TLS.
  - En todas las versiones enciende el control de hilos (`MBEDTLS_THREADING_C` con `MBEDTLS_THREADING_ALT`).
  - En la versión clásica y en el fuzzing apaga BCryptGenRandom, que no existe en XP, y Mbed TLS pide los números aleatorios a `src/common/entropia_xp.cpp`, que usa RtlGenRandom.
  - En la clásica, además, los certificados usan la versión propia de `inet_pton` de Mbed TLS, porque la de Windows tampoco existe en XP.
- **`mbedtls/threading_alt.h`:** los candados de Mbed TLS son `CRITICAL_SECTION`, que existe en XP. Las funciones están en `src/common/cifrado/inicio.cpp`, y `ventrik::cifrado::Inicio` las registra al arrancar.
- **`speexdsp/speexdsp_config_types.h`:** lo que genera el `configure` de SpeexDSP.
- **`miniupnpc/miniupnpcstrings.h`:** lo que genera el CMake de miniupnpc.
- **`stb/stb_image.c`:** compila stb_image solo para GIF y solo desde memoria.
- **`jsmn/jsmn.c`:** compila el código de jsmn (que viene como un solo encabezado) con las opciones de las bibliotecas de afuera.
- **`rnnoise/os_support.h`:** RNNoise 0.2 lo necesita al compilar con Visual Studio, pero no viene en el paquete ni está en su repositorio. Define una sola cosa, `OPUS_CLEAR`, igual que en Opus.
- **`sqlite/ventrik_sqlite_xp.h` y `.c` (solo en la clásica):** SQLite 3.54 usa GetTickCount64, que existe desde Vista. En la clásica se cambia por una función propia que usa la de Windows si existe y, en XP, GetTickCount. SQLite la usa solo para la semilla de sus números al azar internos, que no son para cifrar.

## Opciones de compilación que importan

Están en `CMakeLists.txt`, junto a cada biblioteca:

- **Advertencias:** las bibliotecas se compilan con el nivel normal de Visual Studio (`/W3`), sin tratarlas como errores. `/W4 /WX` vale para el código propio.
- **Opus:** sin DRED ni OSCE, y con la biblioteca de C estática (`/MT`), como el resto.
- **miniaudio:** sin JACK (`MA_NO_JACK`), un sistema de audio de Linux cuya DLL se buscaría por nombre.
- **miniz:** solo zip en memoria (`MINIZ_NO_STDIO`, `MINIZ_NO_TIME`) y sin los nombres de zlib (`MINIZ_NO_ZLIB_COMPATIBLE_NAMES`), para que no choquen con nada.
- **jsmn:** estricto (`JSMN_STRICT`): solo acepta JSON válido.
- **Dear ImGui:** sin mando (`IMGUI_IMPL_WIN32_DISABLE_GAMEPAD`), que cargaría la DLL de xinput por nombre, y sin abrir enlaces ni programas por su cuenta (`IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS`).
- **SQLite:**
  - `SQLITE_THREADSAFE=0`: se usa desde un solo hilo, como pide el diseño. El control de hilos de SQLite 3.54 usa funciones de Windows 7.
  - `SQLITE_OMIT_LOAD_EXTENSION`: no puede cargar código de afuera.
  - `SQLITE_DQS=0`: las comillas dobles son solo para nombres, nunca para texto.
- **RNNoise:**
  - En la clásica, versión en C común, la misma que Opus usa en `dnn/vec.h`, y se apaga `restrict`, que el compilador de C de VS 2017 no conoce; es solo una pista para optimizar.
  - En la moderna elige al arrancar, según el procesador, la versión en C, SSE4.1 o AVX2 (`RNN_ENABLE_X86_RTCD` y los archivos de `src/x86/`; `nnet_sse4_1.c` con `OPUS_X86_MAY_HAVE_SSE4_1` y `nnet_avx2.c` con `/arch:AVX2`, cada uno solo en su archivo).
    - En la PC de prueba bajó de 7,2 % a 2,5 % de un núcleo, cada 10 ms a 48 kHz.
    - En la clásica no se usa, porque XP no guarda los registros de AVX.
    - RNNoise no consulta si Windows tiene prendido AVX (XGETBV). En Windows 10 y 11 siempre lo está; en alguna máquina virtual rara el hilo de la voz podría cerrarse con una instrucción inválida.
- **SpeexDSP:** sin sus avisos por consola (`DISABLE_WARNINGS`), que escribirían con `fprintf` desde el hilo de la voz.
- **stb_vorbis:** solo desde memoria y con el sonido entero (`STB_VORBIS_NO_STDIO`, `STB_VORBIS_NO_PUSHDATA_API`): no abre archivos por su cuenta. Todavía no lo usa ningún programa; se compila para comprobar que anda en las dos versiones.
- **doctest (solo en la clásica):** sin su informe de cierres inesperados (`DOCTEST_CONFIG_NO_WINDOWS_SEH`), que usa una función que XP de 32 bits no tiene.
