cmake_minimum_required(VERSION 3.21)
project(Ventrik LANGUAGES C CXX)

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
  set(CMAKE_VS_GLOBALS "XPDeprecationWarning=false")
elseif(VENTRIK_VERSION STREQUAL "fuzzing")
  if(NOT CMAKE_GENERATOR_TOOLSET MATCHES "^v143")
    message(FATAL_ERROR "El fuzzing usa Visual Studio 2022 (v143), que trae AddressSanitizer")
  endif()
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
elseif(VENTRIK_VERSION STREQUAL "asan")
  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT CMAKE_GENERATOR_TOOLSET MATCHES "^v143")
    message(FATAL_ERROR "La versión asan es de 64 bits (x64) con Visual Studio 2022 (v143)")
  endif()
  set(VENTRIK_WINDOWS_MINIMO 0x0A00)
  get_filename_component(VENTRIK_CARPETA_COMPILADOR "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(VENTRIK_DLL_ASAN "${VENTRIK_CARPETA_COMPILADOR}/clang_rt.asan_dynamic-x86_64.dll")
  if(NOT EXISTS "${VENTRIK_DLL_ASAN}")
    message(FATAL_ERROR "Falta AddressSanitizer de Visual Studio (${VENTRIK_DLL_ASAN})")
  endif()
else()
  message(FATAL_ERROR "Elegí la versión con: cmake --preset moderna, clasica, fuzzing64, fuzzing32 o asan64")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(ventrik_opciones_base INTERFACE)
target_compile_definitions(ventrik_opciones_base INTERFACE
  _WIN32_WINNT=${VENTRIK_WINDOWS_MINIMO} WINVER=${VENTRIK_WINDOWS_MINIMO})
target_compile_options(ventrik_opciones_base INTERFACE
  /GS)
target_link_options(ventrik_opciones_base INTERFACE
  /WX /DYNAMICBASE /NXCOMPAT)
if(NOT VENTRIK_VERSION STREQUAL "fuzzing" AND NOT VENTRIK_VERSION STREQUAL "asan")
  target_compile_options(ventrik_opciones_base INTERFACE /guard:cf)
  target_link_options(ventrik_opciones_base INTERFACE /guard:cf)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()
if(VENTRIK_VERSION STREQUAL "clasica")
  target_compile_definitions(ventrik_opciones_base INTERFACE VENTRIK_CLASICA=1)
  target_link_options(ventrik_opciones_base INTERFACE /SAFESEH /DEPENDENTLOADFLAG:0x800)
elseif(VENTRIK_VERSION STREQUAL "fuzzing")
  target_compile_definitions(ventrik_opciones_base INTERFACE VENTRIK_FUZZING=1)
  target_compile_options(ventrik_opciones_base INTERFACE /fsanitize=address /fsanitize=fuzzer /Zi)
  target_link_options(ventrik_opciones_base INTERFACE /DEBUG /INCREMENTAL:NO)
elseif(VENTRIK_VERSION STREQUAL "asan")
  target_compile_definitions(ventrik_opciones_base INTERFACE VENTRIK_MODERNA=1)
  target_compile_options(ventrik_opciones_base INTERFACE /fsanitize=address /Zi)
  target_link_options(ventrik_opciones_base INTERFACE /DEBUG /INCREMENTAL:NO)
else()
  target_compile_definitions(ventrik_opciones_base INTERFACE VENTRIK_MODERNA=1)
  target_link_options(ventrik_opciones_base INTERFACE /HIGHENTROPYVA /DEPENDENTLOADFLAG:0x800)
endif()

add_library(ventrik_opciones INTERFACE)
target_link_libraries(ventrik_opciones INTERFACE ventrik_opciones_base)
target_compile_options(ventrik_opciones INTERFACE
  /W4 /WX
  /utf-8
  /permissive-
  /Zc:__cplusplus
  /external:anglebrackets /external:W0)
if(VENTRIK_VERSION STREQUAL "clasica")
  target_compile_options(ventrik_opciones BEFORE INTERFACE /experimental:external)
endif()
target_compile_definitions(ventrik_opciones INTERFACE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if(VENTRIK_VERSION STREQUAL "fuzzing" OR VENTRIK_VERSION STREQUAL "asan")
  add_subdirectory(third_party EXCLUDE_FROM_ALL)
else()
  add_subdirectory(third_party)
endif()

add_library(ventrik_sistema STATIC src/common/sistema.cpp)
target_include_directories(ventrik_sistema PUBLIC src)
target_link_libraries(ventrik_sistema PRIVATE ventrik_opciones)

if(VENTRIK_VERSION STREQUAL "clasica" OR VENTRIK_VERSION STREQUAL "fuzzing")
  add_library(ventrik_entropia_xp STATIC src/common/entropia_xp.cpp)
  target_include_directories(ventrik_entropia_xp PRIVATE src)
  target_link_libraries(ventrik_entropia_xp PRIVATE ventrik_opciones ventrik_sistema mbedtls gsl)
endif()

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

add_library(ventrik_comun STATIC
  src/common/archivos.cpp
  src/common/niveles.cpp
  src/common/cifrado/base32.cpp
  src/common/cifrado/boveda.cpp
  src/common/cifrado/cadena.cpp
  src/common/cifrado/clave.cpp
  src/common/cifrado/codigo_dueno.cpp
  src/common/cifrado/dpapi.cpp
  src/common/cifrado/huella.cpp
  src/common/cifrado/identidad.cpp
  src/common/cifrado/inicio.cpp
  src/common/cifrado/llave.cpp
  src/common/cifrado/sobre.cpp
  src/common/cifrado/tls.cpp
  src/common/cifrado/totp.cpp
  src/common/proceso.cpp
  src/common/protocol/bytes.cpp
  src/common/protocol/marcos.cpp
  src/common/protocol/mensajes.cpp
  src/common/protocol/hostear.cpp
  src/common/protocol/negociacion.cpp
  src/common/protocol/pedidos.cpp
  src/common/red/red.cpp
  src/common/texto.cpp
  src/common/textos.cpp
  src/common/utf.cpp
  "${VENTRIK_GENERADO}/textos_generados.h")
target_include_directories(ventrik_comun PUBLIC src "${VENTRIK_GENERADO}")
target_link_libraries(ventrik_comun PUBLIC ventrik_opciones ventrik_sistema gsl mbedtls argon2 ws2_32 shell32 crypt32)

if(VENTRIK_VERSION STREQUAL "fuzzing")
  foreach(fuzzer fuzz_base fuzz_cadena fuzz_firma fuzz_guardados fuzz_hostear fuzz_identidad fuzz_nombres fuzz_paquete
          fuzz_protocolo fuzz_respaldo fuzz_sala fuzz_sonido fuzz_textos fuzz_utf fuzz_voz fuzz_voz_servidor)
    add_executable(${fuzzer} fuzz/${fuzzer}.cpp)
    target_link_libraries(${fuzzer} PRIVATE ventrik_comun)
    add_custom_command(TARGET ${fuzzer} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${VENTRIK_DLL_ASAN}" "$<TARGET_FILE_DIR:${fuzzer}>"
      VERBATIM)
  endforeach()
  target_sources(fuzz_base PRIVATE src/server/base.cpp)
  target_sources(fuzz_guardados PRIVATE src/client/guardados.cpp src/client/silencios.cpp)
  target_sources(fuzz_identidad PRIVATE src/client/mi_identidad.cpp)
  target_sources(fuzz_sala PRIVATE src/client/avisos.cpp src/client/guardados.cpp src/client/horas.cpp src/client/sala.cpp
                                   src/client/silencios.cpp)
  target_sources(fuzz_textos PRIVATE src/client/avisos.cpp src/client/horas.cpp src/client/silencios.cpp
                                     src/server/contrasena.cpp)
  target_sources(fuzz_respaldo PRIVATE src/server/base.cpp src/server/contrasena.cpp src/server/mudanza.cpp src/server/respaldo.cpp)
  target_link_libraries(fuzz_base PRIVATE sqlite)
  target_link_libraries(fuzz_respaldo PRIVATE sqlite)
  target_sources(fuzz_voz_servidor PRIVATE src/server/proteccion.cpp src/server/voz.cpp)
  target_sources(fuzz_voz PRIVATE src/client/buffer_voz.cpp src/client/canal_voz.cpp src/client/limpieza.cpp
                 src/client/motor_voz.cpp src/client/voz_primero.cpp)
  target_link_libraries(fuzz_voz PRIVATE opus rnnoise speexdsp)
  foreach(fuzzer fuzz_paquete fuzz_sonido)
    target_sources(${fuzzer} PRIVATE src/client/audio.cpp src/client/buffer_voz.cpp src/client/limpieza.cpp
                   src/client/motor_voz.cpp src/client/paquete_de_sonidos.cpp src/client/sonidos.cpp
                   src/client/voz_primero.cpp)
    target_link_libraries(${fuzzer} PRIVATE opus rnnoise speexdsp miniaudio miniz jsmn winmm ole32)
  endforeach()
  return()
endif()

add_library(ventrik_servidor_base STATIC
  src/server/base.cpp src/server/comprobador.cpp src/server/contrasena.cpp src/server/crear.cpp src/server/permisos.cpp
  src/server/mudanza.cpp src/server/proteccion.cpp src/server/registro.cpp src/server/respaldo.cpp
  src/server/servidor.cpp src/server/voz.cpp)
target_link_libraries(ventrik_servidor_base PUBLIC ventrik_comun sqlite)
target_compile_definitions(ventrik_servidor_base PRIVATE FD_SETSIZE=1024)

if(VENTRIK_VERSION STREQUAL "asan")
  add_executable(ventrik_servidor src/server/main.cpp)
  target_link_libraries(ventrik_servidor PRIVATE ventrik_servidor_base)
  add_custom_command(TARGET ventrik_servidor POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${VENTRIK_DLL_ASAN}" "$<TARGET_FILE_DIR:ventrik_servidor>"
    VERBATIM)
  return()
endif()
add_library(ventrik_cliente_base STATIC
  src/client/arranque.cpp src/client/audio.cpp src/client/avisos.cpp src/client/buffer_voz.cpp src/client/canal_voz.cpp
  src/client/conexion.cpp src/client/cuando_suena.cpp src/client/prueba_de_audio.cpp
  src/client/diagnostico.cpp src/client/guardados.cpp src/client/horas.cpp src/client/limpieza.cpp src/client/mi_identidad.cpp
  src/client/mis_servidores.cpp
  src/client/motor_voz.cpp src/client/paquete_de_sonidos.cpp src/client/sala.cpp src/client/salida_de_windows.cpp
  src/client/sesion.cpp src/client/silencios.cpp src/client/sonidos.cpp src/client/vigia.cpp src/client/webauthn.cpp)
target_sources(ventrik_cliente_base PRIVATE src/client/voz_primero.cpp)
target_link_libraries(ventrik_cliente_base PUBLIC ventrik_comun psapi opus miniaudio winmm rnnoise speexdsp ole32 miniz jsmn
                      comdlg32)
add_library(ventrik_interfaz STATIC
  src/client/ui/aviso_ventana.cpp src/client/ui/aviso_windows.cpp src/client/ui/dibujante.cpp src/client/ui/iconos.cpp src/client/ui/inicio.cpp
  src/client/ui/ajustes.cpp src/client/ui/hostear.cpp src/client/ui/menu_avisos.cpp src/client/ui/piezas.cpp
  src/client/ui/programa.cpp src/client/ui/qr.cpp src/client/ui/servidor.cpp src/client/ui/ventana.cpp)
target_link_libraries(ventrik_interfaz PUBLIC ventrik_cliente_base imgui qrcodegen d3d9 user32 gdi32)

add_executable(ventrik WIN32 src/client/main.cpp src/client/ventrik.rc)
target_link_libraries(ventrik PRIVATE ventrik_interfaz)

add_executable(ventrik_servidor src/server/main.cpp)
target_link_libraries(ventrik_servidor PRIVATE ventrik_servidor_base)

add_executable(ventrik_sonidos tools/sonidos.cpp)
target_link_libraries(ventrik_sonidos PRIVATE ventrik_cliente_base)

enable_testing()
add_executable(ventrik_pruebas
  tests/main.cpp
  tests/prueba_argon2.cpp
  tests/prueba_aviso_windows.cpp
  tests/prueba_avisos.cpp
  tests/prueba_base.cpp
  tests/prueba_base32.cpp
  tests/prueba_cadena.cpp
  tests/prueba_combinaciones.cpp
  tests/prueba_conexion.cpp
  tests/prueba_contrasena.cpp
  tests/prueba_dtls.cpp
  tests/prueba_entrar.cpp
  tests/prueba_estres.cpp
  tests/prueba_guardados.cpp
  tests/prueba_hostear.cpp
  tests/prueba_gsl.cpp
  tests/prueba_huella.cpp
  tests/prueba_identidad.cpp
  tests/prueba_imgui.cpp
  tests/prueba_libwebp.cpp
  tests/prueba_mbedtls.cpp
  tests/prueba_mi_identidad.cpp
  tests/prueba_motor_voz.cpp
  tests/prueba_mudanza.cpp
  tests/prueba_niveles.cpp
  tests/prueba_miniaudio.cpp
  tests/prueba_miniupnpc.cpp
  tests/prueba_opus.cpp
  tests/prueba_pantallas.cpp
  tests/prueba_pedidos.cpp
  tests/prueba_permisos.cpp
  tests/prueba_piezas.cpp
  tests/prueba_proceso.cpp
  tests/prueba_proteccion.cpp
  tests/prueba_protocolo.cpp
  tests/prueba_qr.cpp
  tests/prueba_red.cpp
  tests/prueba_registro.cpp
  tests/prueba_respaldo.cpp
  tests/prueba_rnnoise.cpp
  tests/prueba_sala.cpp
  tests/prueba_sesion.cpp
  tests/prueba_sobre.cpp
  tests/prueba_sonidos.cpp
  tests/prueba_speexdsp.cpp
  tests/prueba_sqlite.cpp
  tests/prueba_stb_image.cpp
  tests/prueba_texto.cpp
  tests/prueba_totp.cpp
  tests/prueba_arranque.cpp
  tests/prueba_limpieza.cpp
  tests/prueba_llave.cpp
  tests/prueba_voz_servidor.cpp
  tests/prueba_textos.cpp
  tests/prueba_utf.cpp
  tests/datos.rc
  src/client/ventrik.rc)
target_link_libraries(ventrik_pruebas PRIVATE
  ventrik_servidor_base ventrik_interfaz doctest
  argon2 gsl imgui imgui_null libwebp mbedtls miniaudio miniupnpc opus
  rnnoise speexdsp sqlite stb_image miniz qrcodegen)
add_test(NAME pruebas COMMAND ventrik_pruebas)
