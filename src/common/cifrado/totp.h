#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <gsl/gsl>

#include "common/cifrado/secreto.h"

namespace ventrik::cifrado {
constexpr std::size_t kBytesSecretoTotp = 20;
constexpr std::uint64_t kSegundosPasoTotp = 30;
constexpr std::size_t kCifrasTotp = 6;
constexpr std::uint32_t kTopeCodigoTotp = 1000000;

std::optional<Secreto> secreto_totp_nuevo();

std::optional<std::uint32_t> codigo_totp(gsl::span<const std::uint8_t> secreto, std::uint64_t paso);

bool codigos_totp_iguales(std::uint32_t a, std::uint32_t b);

std::optional<std::uint64_t> verificar_totp(gsl::span<const std::uint8_t> secreto, std::uint32_t codigo,
                                            std::uint64_t segundos_unix);

std::optional<std::uint64_t> verificar_totp(gsl::span<const std::uint8_t> secreto, std::uint32_t codigo,
                                            std::uint64_t segundos_unix, std::uint64_t ultimo_aceptado);

std::optional<std::uint32_t> leer_codigo_totp(std::string_view texto);

std::string texto_otpauth(std::string_view nombre, gsl::span<const std::uint8_t> secreto);

}
