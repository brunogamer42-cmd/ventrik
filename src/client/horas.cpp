#include "client/horas.h"

#include <array>
#include <chrono>

#include <windows.h>

#include "common/utf.h"

namespace ventrik::cliente {
namespace {
bool partes_locales(std::uint64_t milisegundos, SYSTEMTIME& partes) {
  const std::uint64_t desde_1601 = milisegundos * 10000 + 116444736000000000ull;
  const FILETIME universal = {static_cast<DWORD>(desde_1601), static_cast<DWORD>(desde_1601 >> 32)};
  SYSTEMTIME utc = {};
  return FileTimeToSystemTime(&universal, &utc) && SystemTimeToTzSpecificLocalTime(nullptr, &utc, &partes);
}

}

std::uint64_t ahora_ms() {
  const auto desde_1970 = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(desde_1970).count());
}

std::string hora_local(std::uint64_t milisegundos) {
  SYSTEMTIME partes = {};
  std::array<wchar_t, 32> hora{};
  if (!partes_locales(milisegundos, partes) ||
      GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &partes, nullptr, hora.data(),
                     static_cast<int>(hora.size())) == 0) {
    return {};
  }
  return a_utf8(hora.data());
}

std::string fecha_local(std::uint64_t milisegundos) {
  SYSTEMTIME partes = {};
  std::array<wchar_t, 64> fecha{};
  if (!partes_locales(milisegundos, partes) ||
      GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &partes, nullptr, fecha.data(),
                     static_cast<int>(fecha.size())) == 0) {
    return {};
  }
  return a_utf8(fecha.data());
}

std::string fecha_y_hora_local(std::uint64_t milisegundos) {
  const std::string fecha = fecha_local(milisegundos);
  const std::string hora = hora_local(milisegundos);
  return fecha.empty() || hora.empty() ? std::string() : fecha + " " + hora;
}

std::uint32_t dia_local(std::uint64_t milisegundos) {
  SYSTEMTIME partes = {};
  return partes_locales(milisegundos, partes)
             ? static_cast<std::uint32_t>(partes.wYear) * 10000u + partes.wMonth * 100u + partes.wDay
             : 0;
}

}
