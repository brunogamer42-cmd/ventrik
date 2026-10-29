#pragma once

// Conversión entre UTF-8 (lo que usa Ventrik) y UTF-16 (lo que usa Windows).

#include <cstddef>
#include <string>
#include <string_view>

namespace ventrik {

// true si el texto es UTF-8 bien formado según Unicode (tabla 3-7): sin
// formas más largas de lo necesario, sin mitades de pares UTF-16 sueltas y
// sin pasarse de U+10FFFF. Se revisa acá y no con Windows porque XP acepta
// cosas que los Windows modernos rechazan, y las dos versiones tienen que
// tratar igual el mismo texto.
bool es_utf8_valido(std::string_view texto);

// Pasa texto UTF-8 a UTF-16. Si no es UTF-8 válido, devuelve vacío.
std::wstring a_utf16(std::string_view utf8);

// Para mostrar rutas y argumentos. Las mitades de par sueltas pasan a U+FFFD acá: XP no lo hace.
std::string a_utf8(std::wstring_view utf16);

std::size_t cantidad_de_letras(std::string_view texto);

}  // namespace ventrik
