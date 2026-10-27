#pragma once

#include <string>

namespace ventrik::cliente {
constexpr wchar_t kClaveDeArranque[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool arranca_con_windows(const std::wstring& clave = kClaveDeArranque);
bool arrancar_con_windows(bool si, const std::wstring& clave = kClaveDeArranque);

}
