#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/secreto.h"

namespace ventrik::cifrado {

std::optional<std::vector<std::uint8_t>> proteger_con_windows(gsl::span<const std::uint8_t> datos,
                                                              std::string_view proposito);

std::optional<Secreto> abrir_con_windows(gsl::span<const std::uint8_t> protegido, std::string_view proposito);

}  // namespace ventrik::cifrado
