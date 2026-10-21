cmake_minimum_required(VERSION 3.21)
project(Ventrik LANGUAGES C CXX)

# Qué versión se compila. La eligen los presets de CMakePresets.json:
#   cmake --preset moderna   (Windows 10 y 11, 64 bits)
#   cmake --preset clasica   (Windows XP SP3 a 8.1 y Windows 10 de 32 bits)
set(VENTRIK_VERSION "" CACHE STRING "Version a compilar: moderna o clasica")

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
else()
  message(FATAL_ERROR "Elegí la versión con: cmake --preset moderna  o  cmake --preset clasica")
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
  /GS              # protección contra desbordes de pila
  /guard:cf)       # Control Flow Guard (Windows 8.1 en adelante)
target_link_options(ventrik_opciones_base INTERFACE
  /WX /DYNAMICBASE /NXCOMPAT /guard:cf)
if(VENTRIK_VERSION STREQUAL "clasica")
  target_compile_definitions(ventrik_opciones_base INTERFACE VENTRIK_CLASICA=1)
  # SafeSEH existe solo en 32 bits.
  target_link_options(ventrik_opciones_base INTERFACE /SAFESEH)
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

# Bibliotecas de third_party/.
add_subdirectory(third_party)

# Números aleatorios para Mbed TLS en la versión clásica (RtlGenRandom).
if(VENTRIK_VERSION STREQUAL "clasica")
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
  src/common/proceso.cpp
  src/common/textos.cpp
  src/common/utf.cpp
  "${VENTRIK_GENERADO}/textos_generados.h")
target_include_directories(ventrik_comun PUBLIC src "${VENTRIK_GENERADO}")
target_link_libraries(ventrik_comun PUBLIC ventrik_opciones)

# El programa.
add_executable(ventrik WIN32 src/client/main.cpp)
target_link_libraries(ventrik PRIVATE ventrik_comun user32)

# Pruebas automáticas.
enable_testing()
add_executable(ventrik_pruebas
  tests/main.cpp
  tests/prueba_argon2.cpp
  tests/prueba_gsl.cpp
  tests/prueba_imgui.cpp
  tests/prueba_libwebp.cpp
  tests/prueba_mbedtls.cpp
  tests/prueba_miniaudio.cpp
  tests/prueba_miniupnpc.cpp
  tests/prueba_opus.cpp
  tests/prueba_proceso.cpp
  tests/prueba_rnnoise.cpp
  tests/prueba_speexdsp.cpp
  tests/prueba_sqlite.cpp
  tests/prueba_stb_image.cpp
  tests/prueba_textos.cpp
  tests/prueba_utf.cpp)
target_link_libraries(ventrik_pruebas PRIVATE
  ventrik_comun doctest
  argon2 gsl imgui imgui_null libwebp mbedtls miniaudio miniupnpc opus
  rnnoise speexdsp sqlite stb_image)
add_test(NAME pruebas COMMAND ventrik_pruebas)
