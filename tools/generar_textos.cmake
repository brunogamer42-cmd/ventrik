# Genera textos_generados.h a partir de los archivos lang/*.txt.
#
# Lo llama CMakeLists.txt al compilar:
#   cmake -DCARPETA_LANG=<carpeta lang> -DSALIDA=<archivo .h> -P generar_textos.cmake
#
# Cada texto se escribe byte por byte como \xHH, así el resultado no
# depende de cómo el compilador interprete los acentos del archivo.

cmake_minimum_required(VERSION 3.21)

set(IDIOMA_BASE "es")      # referencia: define qué claves existen
set(IDIOMA_RESPALDO "en")  # se usa si Windows está en un idioma que no hay

if(NOT DEFINED CARPETA_LANG OR NOT DEFINED SALIDA)
  message(FATAL_ERROR "Faltan CARPETA_LANG o SALIDA")
endif()

foreach(codigo IN ITEMS ${IDIOMA_BASE} ${IDIOMA_RESPALDO})
  if(NOT EXISTS "${CARPETA_LANG}/${codigo}.txt")
    message(FATAL_ERROR "Falta lang/${codigo}.txt")
  endif()
endforeach()

# Lee un archivo de idioma y deja en el ámbito de quien llama:
#   <codigo>_CLAVES         las claves, en el orden del archivo
#   <codigo>_TEXTO_<clave>  el texto de cada clave, ya escrito como \xHH
function(leer_idioma archivo codigo)
  file(READ "${archivo}" contenido)

  # Saca la marca BOM (EF BB BF) que agregan algunos editores.
  string(SUBSTRING "${contenido}" 0 3 inicio)
  string(HEX "${inicio}" inicio_hex)
  if(inicio_hex STREQUAL "efbbbf")
    string(SUBSTRING "${contenido}" 3 -1 contenido)
  endif()

  # Las listas de CMake tratan ; \ [ ] de forma especial: se cambian por
  # marcas antes de partir el archivo en líneas y se reponen en cada texto.
  string(REPLACE "\r" "" contenido "${contenido}")
  string(REPLACE "\\" "<<BARRA>>" contenido "${contenido}")
  string(REPLACE ";" "<<PUNTOYCOMA>>" contenido "${contenido}")
  string(REPLACE "[" "<<ABRE>>" contenido "${contenido}")
  string(REPLACE "]" "<<CIERRA>>" contenido "${contenido}")
  string(REPLACE "\n" ";" lineas "${contenido}")

  set(claves "")
  list(LENGTH lineas cantidad)
  if(cantidad GREATER 0)
    math(EXPR ultima "${cantidad} - 1")
    foreach(i RANGE 0 ${ultima})
      list(GET lineas ${i} linea)
      math(EXPR numero "${i} + 1")
      string(STRIP "${linea}" linea)
      if(linea STREQUAL "" OR linea MATCHES "^#")
        continue()
      endif()
      if(NOT linea MATCHES "^([a-z][a-z0-9_]*)[ \t]*=(.*)$")
        message(FATAL_ERROR
          "lang/${codigo}.txt, línea ${numero}: tiene que ser 'clave = texto', "
          "con la clave en minúsculas, números o guion bajo")
      endif()
      set(clave "${CMAKE_MATCH_1}")
      string(STRIP "${CMAKE_MATCH_2}" texto)
      if(clave IN_LIST claves)
        message(FATAL_ERROR "lang/${codigo}.txt, línea ${numero}: la clave '${clave}' está repetida")
      endif()
      list(APPEND claves "${clave}")

      string(REPLACE "<<BARRA>>" "\\" texto "${texto}")
      string(REPLACE "<<PUNTOYCOMA>>" ";" texto "${texto}")
      string(REPLACE "<<ABRE>>" "[" texto "${texto}")
      string(REPLACE "<<CIERRA>>" "]" texto "${texto}")
      string(HEX "${texto}" hex)
      string(REGEX REPLACE "(..)" "\\\\x\\1" escapado "${hex}")
      set(${codigo}_TEXTO_${clave} "${escapado}" PARENT_SCOPE)
    endforeach()
  endif()
  set(${codigo}_CLAVES "${claves}" PARENT_SCOPE)
endfunction()

file(GLOB archivos LIST_DIRECTORIES false "${CARPETA_LANG}/*.txt")
list(SORT archivos)
set(codigos "")
foreach(archivo IN LISTS archivos)
  get_filename_component(codigo "${archivo}" NAME_WE)
  if(NOT codigo MATCHES "^[a-z][a-z][a-z]?$")
    message(FATAL_ERROR
      "lang/${codigo}.txt: el nombre tiene que ser el código del idioma "
      "en minúsculas (es, en, pt...)")
  endif()
  leer_idioma("${archivo}" "${codigo}")
  list(APPEND codigos "${codigo}")
endforeach()

set(claves_base "${${IDIOMA_BASE}_CLAVES}")
if(claves_base STREQUAL "")
  message(FATAL_ERROR "lang/${IDIOMA_BASE}.txt no tiene ningún texto")
endif()
foreach(codigo IN LISTS codigos)
  foreach(clave IN LISTS ${codigo}_CLAVES)
    if(NOT clave IN_LIST claves_base)
      message(FATAL_ERROR
        "lang/${codigo}.txt: la clave '${clave}' no existe en lang/${IDIOMA_BASE}.txt")
    endif()
  endforeach()
endforeach()

list(LENGTH claves_base cantidad_claves)
list(LENGTH codigos cantidad_idiomas)
list(FIND codigos "${IDIOMA_RESPALDO}" indice_respaldo)

set(h "// Generado por tools/generar_textos.cmake a partir de lang/*.txt. No editar a mano.\n")
string(APPEND h "#pragma once\n\n#include <cstddef>\n\nnamespace ventrik {\n\n")
string(APPEND h "// Una clave por cada texto de lang/${IDIOMA_BASE}.txt.\n")
string(APPEND h "enum class Clave : std::size_t {\n")
foreach(clave IN LISTS claves_base)
  string(APPEND h "  ${clave},\n")
endforeach()
string(APPEND h "};\n\nnamespace generado {\n\n")
string(APPEND h "inline constexpr std::size_t kCantidadClaves = ${cantidad_claves};\n")
string(APPEND h "inline constexpr std::size_t kCantidadIdiomas = ${cantidad_idiomas};\n")
string(APPEND h "inline constexpr std::size_t kIdiomaRespaldo = ${indice_respaldo};\n\n")

string(APPEND h "inline constexpr const char* kNombresClaves[kCantidadClaves] = {\n")
foreach(clave IN LISTS claves_base)
  string(APPEND h "    \"${clave}\",\n")
endforeach()
string(APPEND h "};\n\n")

string(APPEND h "inline constexpr const char* kCodigosIdiomas[kCantidadIdiomas] = {\n")
foreach(codigo IN LISTS codigos)
  string(APPEND h "    \"${codigo}\",\n")
endforeach()
string(APPEND h "};\n\n")

string(APPEND h "// Textos en UTF-8. Vacío quiere decir que falta traducirlo.\n")
string(APPEND h "inline constexpr const char* kTextos[kCantidadIdiomas][kCantidadClaves] = {\n")
foreach(codigo IN LISTS codigos)
  string(APPEND h "    {  // ${codigo}\n")
  foreach(clave IN LISTS claves_base)
    if(DEFINED ${codigo}_TEXTO_${clave})
      set(texto "${${codigo}_TEXTO_${clave}}")
    else()
      set(texto "")
    endif()
    string(APPEND h "        /* ${clave} */ \"${texto}\",\n")
  endforeach()
  string(APPEND h "    },\n")
endforeach()
string(APPEND h "};\n\n}  // namespace generado\n}  // namespace ventrik\n")

file(WRITE "${SALIDA}" "${h}")
