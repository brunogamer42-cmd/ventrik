#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <gsl/gsl>

namespace ventrik::cifrado {
std::string a_base32(gsl::span<const std::uint8_t> bytes, bool con_guiones = true);

bool de_base32(std::string_view texto, gsl::span<std::uint8_t> destino);

}
