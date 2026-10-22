cmake_minimum_required(VERSION 3.21)
project(Ventrik LANGUAGES C CXX)

# Qué versión se compila. La eligen los presets de CMakePresets.json:
#   cmake --preset moderna   (Windows 10 y 11, 64 bits)
#   cmake --preset clasica   (Windows XP SP3 a 8.1 y Windows 10 de 32 bits)
#   cmake --preset fuzzing64 o fuzzing32   (solo los fuzzers, nunca se publica)
set(VENTRIK_VERSION "" CACHE STRING "Version a compilar: moderna, clasica o fuzzing")

if(NOT MSVC)
  message(FATAL_ERROR "Ventrik se compila con Visual Studio")
endif()
if(VENTRIK_VERSION STREQUAL "moderna")
  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "La versión moderna es de 64 bits (x64)")
  endif()
  set(VENTRIK_WINDOWS_MINIMO 0x0A00)
elseif(VENTRIK_VERSION STREQUAL "clasica")
  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4 OR NOT CMAKE_GENERATOR_TOOLSET MATCHES "^v141_xp")
    message(FATAL_ERROR "La versión clásica es de 32 bits (Win32) con el toolset v141_xp")
  endif()
  set(VENTRIK_WINDOWS_MINIMO 0x0501)
  # Microsoft avisa en cada proyecto que el soporte para XP está en desuso.
  # Ya se sabe (ver docs/diseno.md): se apaga el aviso para no tapar otros.
  set(CMAKE_VS_GLOBALS "XPDeprecationWarning=false")
elseif(VENTRIK_VERSION STREQUAL "fuzzing")
  if(NOT CMAKE_GENERATOR_TOOLSET MATCHES "^v143")
    message(FATAL_ERROR "El fuzzing usa Visual Studio 2022 (v143), que trae AddressSanitizer")
  endif()
  # Las mismas funciones de Windows que la clásica: el código común se prueba como compila para XP.
  set(VENTRIK_WINDOWS_MINIMO 0x0501)
  get_filename_component(VENTRIK_CARPETA_COMPILADOR "${CMAKE_CXX_COMPILER}" DIRECTORY)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(VENTRIK_DLL_ASAN "${VENTRIK_CARPETA_COMPILADOR}/clang_rt.asan_dynamic-x86_64.dll")
  else()
    set(VENTRIK_DLL_ASAN "${VENTRIK_CARPETA_COMPILADOR}/clang_rt.asan_dynamic-i386.dll")
  endif()
  if(NOT EXISTS "${VENTRIK_DLL_ASAN}")
    message(FATAL_ERROR "Falta AddressSanitizer de Visual Studio (${VENTRIK_DLL_ASAN})")
  endif()
else()
  message(FATAL_ERROR "Elegí la versión con: cmake --preset moderna, clasica, fuzzing64 o fuzzing32")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Biblioteca de C estática (/MT): un solo .exe, sin instalar nada aparte.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Lo que vale para todo, propio y de terceros: la misma versión mínima de
# Windows (en la clásica, solo funciones que existan en XP) y las mismas
# protecciones.
add_library(ventrik_opciones_base INTERFACE)
target_compile_definitions(ventrik_opciones_base INTERFACE
  _WIN32_WINNT=${VENTRIK_WINDOWS_MINIMO} WINVER=${VENTRIK_WINDOWS_MINIMO})
target_compile_options(ventrik_opciones_base INTERFACE
  /GS)             # protección contra desbordes de pila
target_link_options(ventrik_opciones_base INTERFACE
  /WX /DYNAMICBASE /NXCOMPAT)
if(NOT VENTRIK_VERSION STREQUAL "fuzzing")
  # Control Flow Guard (Windows 8.1 en adelante).
  target_compile_options(ventrik_opciones_base INTERFACE /guard:cf)
  target_link_options(ventrik_opciones_base INTERFACE /guard:cf)
endif()
if(VENTRIK_VERSION STREQUAL "clasica")
  target_compile_definitions(ventrik_opciones_base INTERFACE VENTRIK_CLASICA=1)
  # SafeSEH existe solo en 32 bits. /DEPENDENTLOADFLAG:0x800: XP lo ignora,
  # y en Windows 10 de 32 bits las DLL se buscan solo en la carpeta de sistema.
  target_link_options(ventrik_opciones_base INTERFACE /SAFESEH /DEPENDENTLOADFLAG:0x800)
elseif(VENTRIK_VERSION STREQUAL "fuzzing")
  target_compile_definitions(ventrik_opciones_base INTERFACE VENTRIK_FUZZING=1)
  target_compile_options(ventrik_opciones_base INTERFACE /fsanitize=address /fsanitize=fuzzer /Zi)
  target_link_options(ventrik_opciones_base INTERFACE /DEBUG /INCREMENTAL:NO)
else()
  target_compile_definitions(ventrik_opciones_base INTERFACE VENTRIK_MODERNA=1)
  # /DEPENDENTLOADFLAG:0x800: las DLL que el .exe pide al abrir se buscan
  # solo en la carpeta de sistema (Windows 10 1607 en adelante).
  target_link_options(ventrik_opciones_base INTERFACE /HIGHENTROPYVA /DEPENDENTLOADFLAG:0x800)
endif()

# Opciones del código propio: advertencias como errores. Los archivos que
# se incluyen con <...> (Windows y bibliotecas de terceros) quedan afuera,
# porque no son nuestros y no se pueden corregir.
add_library(ventrik_opciones INTERFACE)
target_link_libraries(ventrik_opciones INTERFACE ventrik_opciones_base)
target_compile_options(ventrik_opciones INTERFACE
  /W4 /WX          # advertencias como errores
  /utf-8           # código y textos en UTF-8
  /permissive-     # C++ estándar estricto
  /Zc:__cplusplus
  /external:anglebrackets /external:W0)
if(VENTRIK_VERSION STREQUAL "clasica")
  # El compilador de VS 2017 todavía lo tiene como experimental.
  target_compile_options(ventrik_opciones BEFORE INTERFACE /experimental:external)
endif()
target_compile_definitions(ventrik_opciones INTERFACE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

# Bibliotecas de third_party/. Los fuzzers solo usan GSL y Mbed TLS.
if(VENTRIK_VERSION STREQUAL "fuzzing")
  add_subdirectory(third_party EXCLUDE_FROM_ALL)
else()
  add_subdirectory(third_party)
endif()

# Números aleatorios para Mbed TLS en la versión clásica (RtlGenRandom). El
# fuzzing también, porque compila con las funciones de Windows de XP.
if(VENTRIK_VERSION STREQUAL "clasica" OR VENTRIK_VERSION STREQUAL "fuzzing")
  add_library(ventrik_entropia_xp STATIC src/common/entropia_xp.cpp)
  target_include_directories(ventrik_entropia_xp PRIVATE src)
  target_link_libraries(ventrik_entropia_xp PRIVATE ventrik_opciones mbedtls gsl)
endif()

# Textos de lang/*.txt, metidos dentro del .exe.
file(GLOB VENTRIK_ARCHIVOS_LANG CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/lang/*.txt")
set(VENTRIK_GENERADO "${PROJECT_BINARY_DIR}/generado")
add_custom_command(
  OUTPUT "${VENTRIK_GENERADO}/textos_generados.h"
  COMMAND "${CMAKE_COMMAND}"
          "-DCARPETA_LANG=${PROJECT_SOURCE_DIR}/lang"
          "-DSALIDA=${VENTRIK_GENERADO}/textos_generados.h"
          -P "${PROJECT_SOURCE_DIR}/tools/generar_textos.cmake"
  DEPENDS ${VENTRIK_ARCHIVOS_LANG} "${PROJECT_SOURCE_DIR}/tools/generar_textos.cmake"
  COMMENT "Generando los textos desde lang/"
  VERBATIM)

# Código común al programa, al servidor y a las pruebas.
add_library(ventrik_comun STATIC
  src/common/archivos.cpp
  src/common/cifrado/huella.cpp
  src/common/cifrado/identidad.cpp
  src/common/cifrado/inicio.cpp
  src/common/cifrado/tls.cpp
  src/common/proceso.cpp
  src/common/protocol/bytes.cpp
  src/common/protocol/marcos.cpp
  src/common/protocol/mensajes.cpp
  src/common/protocol/negociacion.cpp
  src/common/red/red.cpp
  src/common/textos.cpp
  src/common/utf.cpp
  "${VENTRIK_GENERADO}/textos_generados.h")
target_include_directories(ventrik_comun PUBLIC src "${VENTRIK_GENERADO}")
target_link_libraries(ventrik_comun PUBLIC ventrik_opciones gsl mbedtls ws2_32 shell32)

if(VENTRIK_VERSION STREQUAL "fuzzing")
  foreach(fuzzer fuzz_identidad fuzz_protocolo fuzz_textos fuzz_utf)
    add_executable(${fuzzer} fuzz/${fuzzer}.cpp)
    target_link_libraries(${fuzzer} PRIVATE ventrik_comun)
    add_custom_command(TARGET ${fuzzer} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${VENTRIK_DLL_ASAN}" "$<TARGET_FILE_DIR:${fuzzer}>"
      VERBATIM)
  endforeach()
  return()
endif()

# El servidor y el programa, en bibliotecas para que las pruebas los usen.
add_library(ventrik_servidor_base STATIC
  src/server/proteccion.cpp src/server/registro.cpp src/server/servidor.cpp)
target_link_libraries(ventrik_servidor_base PUBLIC ventrik_comun)
# select() de Winsock atiende 64 sockets si no se dice otra cosa.
target_compile_definitions(ventrik_servidor_base PRIVATE FD_SETSIZE=1024)
add_library(ventrik_cliente_base STATIC src/client/conexion.cpp)
target_link_libraries(ventrik_cliente_base PUBLIC ventrik_comun)

add_executable(ventrik WIN32 src/client/main.cpp)
target_link_libraries(ventrik PRIVATE ventrik_cliente_base user32)

add_executable(ventrik_servidor src/server/main.cpp)
target_link_libraries(ventrik_servidor PRIVATE ventrik_servidor_base)

# Pruebas automáticas.
enable_testing()
add_executable(ventrik_pruebas
  tests/main.cpp
  tests/prueba_argon2.cpp
  tests/prueba_conexion.cpp
  tests/prueba_gsl.cpp
  tests/prueba_huella.cpp
  tests/prueba_identidad.cpp
  tests/prueba_imgui.cpp
  tests/prueba_libwebp.cpp
  tests/prueba_mbedtls.cpp
  tests/prueba_miniaudio.cpp
  tests/prueba_miniupnpc.cpp
  tests/prueba_opus.cpp
  tests/prueba_proceso.cpp
  tests/prueba_proteccion.cpp
  tests/prueba_protocolo.cpp
  tests/prueba_red.cpp
  tests/prueba_registro.cpp
  tests/prueba_rnnoise.cpp
  tests/prueba_speexdsp.cpp
  tests/prueba_sqlite.cpp
  tests/prueba_stb_image.cpp
  tests/prueba_textos.cpp
  tests/prueba_utf.cpp)
target_link_libraries(ventrik_pruebas PRIVATE
  ventrik_servidor_base ventrik_cliente_base doctest
  argon2 gsl imgui imgui_null libwebp mbedtls miniaudio miniupnpc opus
  rnnoise speexdsp sqlite stb_image)
add_test(NAME pruebas COMMAND ventrik_pruebas)
