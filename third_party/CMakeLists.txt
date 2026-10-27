# Bibliotecas de terceros (versiones y origen en VERSIONS.md).
#
# Se compilan tal como vienen, sin modificar su código. Las advertencias
# como errores (/W4 /WX) valen solo para el código propio; acá se usa el
# nivel normal de Visual Studio. Sí llevan las mismas protecciones y la
# misma versión mínima de Windows que el resto (ventrik_opciones_base).
#
# Lo que hace falta para integrarlas (archivos que en cada proyecto genera
# su propio sistema de compilación, y la configuración de Mbed TLS) está
# en integracion/.

add_library(ventrik_opciones_terceros INTERFACE)
target_link_libraries(ventrik_opciones_terceros INTERFACE ventrik_opciones_base)
target_compile_options(ventrik_opciones_terceros INTERFACE /W3)
target_compile_definitions(ventrik_opciones_terceros INTERFACE
  _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)

set(INTEGRACION "${CMAKE_CURRENT_SOURCE_DIR}/integracion")

# --- doctest: pruebas automáticas (solo en tests/) ---
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
if(VENTRIK_VERSION STREQUAL "clasica")
  # Para informar cierres inesperados usa SetThreadStackGuarantee, que no
  # existe en XP de 32 bits. Sin eso, un cierre igual hace fallar la prueba.
  target_compile_definitions(doctest INTERFACE DOCTEST_CONFIG_NO_WINDOWS_SEH)
endif()

# --- Microsoft GSL: gsl::span y otras ayudas (solo encabezados) ---
add_library(gsl INTERFACE)
target_include_directories(gsl INTERFACE gsl/include)

# --- Opus: compresión de la voz. Se compila con su propio CMake. ---
# Sin las funciones con redes neuronales (DRED, OSCE): el diseño no las usa
# y de su carpeta dnn/ solo quedaron los encabezados.
set(OPUS_BUILD_SHARED_LIBRARY OFF)
set(OPUS_BUILD_TESTING OFF)
set(OPUS_BUILD_PROGRAMS OFF)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF)
set(OPUS_DRED OFF)
set(OPUS_OSCE OFF)
set(OPUS_STATIC_RUNTIME ON)  # /MT, como el resto; si no, Opus pone /MD
add_subdirectory(opus EXCLUDE_FROM_ALL)
target_link_libraries(opus PRIVATE ventrik_opciones_terceros)

# --- Mbed TLS: TLS 1.3 y DTLS 1.2 ---
file(GLOB MBEDTLS_FUENTES CONFIGURE_DEPENDS mbedtls/library/*.c)
add_library(mbedtls STATIC ${MBEDTLS_FUENTES})
target_include_directories(mbedtls
  PUBLIC mbedtls/include "${INTEGRACION}/mbedtls"
  PRIVATE mbedtls/library)
target_compile_definitions(mbedtls PUBLIC
  "MBEDTLS_USER_CONFIG_FILE=<ventrik_mbedtls_config.h>")
target_link_libraries(mbedtls PRIVATE ventrik_opciones_terceros)
if(VENTRIK_VERSION STREQUAL "clasica" OR VENTRIK_VERSION STREQUAL "fuzzing")
  # BCryptGenRandom no existe en XP: los números aleatorios los da
  # mbedtls_hardware_poll (src/common/entropia_xp.cpp), con RtlGenRandom.
  # El fuzzing compila con las funciones de XP, así que también.
  target_compile_definitions(mbedtls PUBLIC VENTRIK_MBEDTLS_RTLGENRANDOM=1)
  target_link_libraries(mbedtls PUBLIC ventrik_entropia_xp)
else()
  target_link_libraries(mbedtls PUBLIC bcrypt)
endif()

# --- miniaudio: micrófono y parlantes ---
add_library(miniaudio STATIC miniaudio/miniaudio.c)
target_include_directories(miniaudio PUBLIC miniaudio)
# Sin JACK (un sistema de audio de Linux): en Windows buscaría su DLL por
# nombre, y en XP eso incluye la carpeta del .exe. PUBLIC porque cambia la
# forma de las estructuras de miniaudio.
target_compile_definitions(miniaudio PUBLIC MA_NO_JACK)
target_link_libraries(miniaudio PRIVATE ventrik_opciones_terceros)

# --- Dear ImGui: interfaz, con Win32. El dibujo con Direct3D 9 es propio (src/client/ui/dibujante.cpp) ---
add_library(imgui STATIC
  imgui/imgui.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/backends/imgui_impl_win32.cpp)
target_include_directories(imgui PUBLIC imgui imgui/backends)
target_compile_definitions(imgui
  PUBLIC IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS IMGUI_USE_BGRA_PACKED_COLOR
  PRIVATE IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
target_link_libraries(imgui PRIVATE ventrik_opciones_terceros)

# El módulo sin pantalla de Dear ImGui, solo para las pruebas.
add_library(imgui_null STATIC imgui/backends/imgui_impl_null.cpp)
target_link_libraries(imgui_null PUBLIC imgui PRIVATE ventrik_opciones_terceros)

# --- RNNoise: limpieza del ruido de fondo ---
# Con Visual Studio compila la versión en C común (sin SSE ni AVX), que es
# la misma que Opus usa y prueba en dnn/vec.h. Le falta un archivo que el
# paquete no trae: está en integracion/rnnoise/.
add_library(rnnoise STATIC
  rnnoise/src/celt_lpc.c
  rnnoise/src/denoise.c
  rnnoise/src/kiss_fft.c
  rnnoise/src/nnet.c
  rnnoise/src/nnet_default.c
  rnnoise/src/parse_lpcnet_weights.c
  rnnoise/src/pitch.c
  rnnoise/src/rnn.c
  rnnoise/src/rnnoise_data.c
  rnnoise/src/rnnoise_tables.c)
target_include_directories(rnnoise
  PUBLIC rnnoise/include
  PRIVATE rnnoise/src "${INTEGRACION}/rnnoise")
# Los pesos de la red están escritos como double y se guardan como float:
# son miles de avisos iguales (C4305) que no dicen nada.
target_compile_options(rnnoise PRIVATE /wd4305)
if(VENTRIK_VERSION STREQUAL "clasica")
  # El compilador de C de VS 2017 no conoce "restrict" (C99). Es solo una
  # pista para optimizar: sin ella el resultado es el mismo.
  target_compile_definitions(rnnoise PRIVATE restrict=)
endif()
if(VENTRIK_VERSION STREQUAL "moderna")
  # En la moderna elige al arrancar la versión con SSE4.1 o AVX2 según el
  # procesador (en C común gastaba 7 % de un núcleo; ver docs/estado.md, paso
  # 3.4). En la clásica no: XP no guarda los registros de AVX.
  target_sources(rnnoise PRIVATE
    rnnoise/src/x86/x86cpu.c rnnoise/src/x86/x86_dnn_map.c
    rnnoise/src/x86/nnet_sse4_1.c rnnoise/src/x86/nnet_avx2.c)
  target_compile_definitions(rnnoise PRIVATE RNN_ENABLE_X86_RTCD)
  set_source_files_properties(rnnoise/src/x86/nnet_sse4_1.c PROPERTIES COMPILE_DEFINITIONS OPUS_X86_MAY_HAVE_SSE4_1)
  set_source_files_properties(rnnoise/src/x86/nnet_avx2.c PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
endif()
target_link_libraries(rnnoise PRIVATE ventrik_opciones_terceros)

# --- SpeexDSP: cancelación de eco, preprocesado y cambio de frecuencia ---
add_library(speexdsp STATIC
  speexdsp/libspeexdsp/buffer.c
  speexdsp/libspeexdsp/fftwrap.c
  speexdsp/libspeexdsp/filterbank.c
  speexdsp/libspeexdsp/jitter.c
  speexdsp/libspeexdsp/mdf.c
  speexdsp/libspeexdsp/preprocess.c
  speexdsp/libspeexdsp/resample.c
  speexdsp/libspeexdsp/scal.c
  speexdsp/libspeexdsp/smallft.c)
target_include_directories(speexdsp PUBLIC speexdsp/include "${INTEGRACION}/speexdsp")
target_compile_definitions(speexdsp PRIVATE FLOATING_POINT USE_SMALLFT EXPORT= DISABLE_WARNINGS)
target_link_libraries(speexdsp PRIVATE ventrik_opciones_terceros)

# --- Argon2 (implementación de referencia): contraseñas ---
add_library(argon2 STATIC
  argon2/src/argon2.c
  argon2/src/core.c
  argon2/src/encoding.c
  argon2/src/ref.c
  argon2/src/thread.c
  argon2/src/blake2/blake2b.c)
target_include_directories(argon2 PUBLIC argon2/include)
target_link_libraries(argon2 PRIVATE ventrik_opciones_terceros)

# --- SQLite: base de datos del servidor ---
# Sin control de hilos (SQLITE_THREADSAFE=0): el diseño usa la base desde un
# solo hilo con su cola de pedidos, y el control de hilos de SQLite 3.54 usa
# funciones de Windows 7 que XP no tiene. Igual en las dos versiones.
add_library(sqlite STATIC sqlite/sqlite3.c)
target_include_directories(sqlite PUBLIC sqlite)
target_compile_definitions(sqlite PRIVATE
  SQLITE_THREADSAFE=0         # usar siempre desde un mismo hilo
  SQLITE_OMIT_LOAD_EXTENSION  # no puede cargar código de afuera (DLL)
  SQLITE_DQS=0)               # comillas dobles solo para nombres, nunca texto
if(VENTRIK_VERSION STREQUAL "clasica" OR VENTRIK_VERSION STREQUAL "fuzzing")
  # SQLite usa GetTickCount64 (Vista en adelante): en XP va una propia.
  target_sources(sqlite PRIVATE "${INTEGRACION}/sqlite/ventrik_sqlite_xp.c")
  set_source_files_properties(sqlite/sqlite3.c PROPERTIES
    COMPILE_OPTIONS "/FI${INTEGRACION}/sqlite/ventrik_sqlite_xp.h")
endif()
target_link_libraries(sqlite PRIVATE ventrik_opciones_terceros)

# --- stb_image: GIF del modo Visual ---
add_library(stb_image STATIC "${INTEGRACION}/stb/stb_image.c")
target_include_directories(stb_image PUBLIC stb)
target_link_libraries(stb_image PRIVATE ventrik_opciones_terceros)

# --- stb_vorbis: sonidos OGG de los avisos (se usa desde la etapa 3) ---
add_library(stb_vorbis STATIC stb/stb_vorbis.c)
target_include_directories(stb_vorbis PUBLIC stb)
# Solo desde memoria y con el sonido entero: sin abrir archivos ni leer por partes.
target_compile_definitions(stb_vorbis PUBLIC STB_VORBIS_NO_STDIO STB_VORBIS_NO_PUSHDATA_API)
target_link_libraries(stb_vorbis PRIVATE ventrik_opciones_terceros)

# --- libwebp: WebP del modo Visual (con animaciones y codificador) ---
file(GLOB LIBWEBP_FUENTES CONFIGURE_DEPENDS
  libwebp/src/dec/*.c libwebp/src/demux/*.c libwebp/src/dsp/*.c
  libwebp/src/enc/*.c libwebp/src/mux/*.c libwebp/src/utils/*.c
  libwebp/sharpyuv/*.c)
add_library(libwebp STATIC ${LIBWEBP_FUENTES})
target_include_directories(libwebp PUBLIC libwebp/src PRIVATE libwebp)
target_compile_definitions(libwebp PRIVATE WEBP_USE_THREAD)
target_link_libraries(libwebp PRIVATE ventrik_opciones_terceros)

# --- miniupnpc: abrir el puerto con UPnP ---
add_library(miniupnpc STATIC
  miniupnpc/miniupnpc/src/addr_is_reserved.c
  miniupnpc/miniupnpc/src/connecthostport.c
  miniupnpc/miniupnpc/src/igd_desc_parse.c
  miniupnpc/miniupnpc/src/minisoap.c
  miniupnpc/miniupnpc/src/minissdpc.c
  miniupnpc/miniupnpc/src/miniupnpc.c
  miniupnpc/miniupnpc/src/miniwget.c
  miniupnpc/miniupnpc/src/minixml.c
  miniupnpc/miniupnpc/src/portlistingparse.c
  miniupnpc/miniupnpc/src/receivedata.c
  miniupnpc/miniupnpc/src/upnpcommands.c
  miniupnpc/miniupnpc/src/upnpdev.c
  miniupnpc/miniupnpc/src/upnperrors.c
  miniupnpc/miniupnpc/src/upnpreplyparse.c)
target_include_directories(miniupnpc
  PUBLIC miniupnpc/miniupnpc/include
  PRIVATE miniupnpc/miniupnpc/src "${INTEGRACION}/miniupnpc")
target_compile_definitions(miniupnpc PUBLIC MINIUPNP_STATICLIB)
target_link_libraries(miniupnpc PRIVATE ventrik_opciones_terceros PUBLIC ws2_32 iphlpapi)
