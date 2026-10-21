#pragma once

// Conversión entre UTF-8 (lo que usa Ventrik) y UTF-16 (lo que usa Windows).

#include <string>
#include <string_view>

namespace ventrik {

// Pasa texto UTF-8 a UTF-16. Si no es UTF-8 válido, devuelve vacío.
std::wstring a_utf16(std::string_view utf8);

}  // namespace ventrik
