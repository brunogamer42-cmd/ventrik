#include "common/utf.h"

#include <cstddef>
#include <limits>

#include <windows.h>

namespace ventrik {

std::wstring a_utf16(std::string_view utf8) {
  if (utf8.empty() ||
      utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return {};
  }
  const int largo = static_cast<int>(utf8.size());
  const int necesario = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                            utf8.data(), largo, nullptr, 0);
  if (necesario <= 0) {
    return {};
  }
  std::wstring resultado(static_cast<std::size_t>(necesario), L'\0');
  const int escrito = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          utf8.data(), largo, resultado.data(),
                                          necesario);
  if (escrito != necesario) {
    return {};
  }
  return resultado;
}

}  // namespace ventrik
