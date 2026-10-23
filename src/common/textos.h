#pragma once

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

#include "textos_generados.h"

namespace ventrik {
struct Idioma {
  std::size_t indice = generado::kIdiomaRespaldo;
};

std::size_t cantidad_idiomas();
std::size_t cantidad_claves();

Idioma elegir_idioma(std::string_view codigo_iso);

Idioma idioma_de_windows();

std::string_view codigo_de(Idioma idioma);
std::string_view nombre_de_clave(Clave clave);

std::string_view texto(Idioma idioma, Clave clave);

bool falta_traduccion(Idioma idioma, Clave clave);

std::string con_datos(std::string_view plantilla, std::initializer_list<std::string_view> datos);

}
