#pragma once

// Textos visibles del programa. Salen de lang/*.txt, que se meten dentro
// del .exe al compilar (ver tools/generar_textos.cmake).

#include <cstddef>
#include <string_view>

#include "textos_generados.h"

namespace ventrik {

// Uno de los idiomas que hay en lang/.
struct Idioma {
  std::size_t indice = generado::kIdiomaRespaldo;
};

std::size_t cantidad_idiomas();
std::size_t cantidad_claves();

// El idioma con ese código ISO 639 ("es", "en"...). Si no está, inglés.
Idioma elegir_idioma(std::string_view codigo_iso);

// El idioma en que está Windows. Si Ventrik no lo tiene, inglés.
Idioma idioma_de_windows();

std::string_view codigo_de(Idioma idioma);
std::string_view nombre_de_clave(Clave clave);

// Texto en UTF-8. Si a ese idioma le falta, devuelve el de inglés.
std::string_view texto(Idioma idioma, Clave clave);

// true si a ese idioma le falta traducir esa clave.
bool falta_traduccion(Idioma idioma, Clave clave);

}  // namespace ventrik
