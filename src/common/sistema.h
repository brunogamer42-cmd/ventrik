#pragma once

#include <windows.h>

namespace ventrik {
HMODULE cargar_de_sistema(const wchar_t* dll) noexcept;
}
