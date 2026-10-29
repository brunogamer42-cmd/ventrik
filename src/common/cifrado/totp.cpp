#include "common/cifrado/totp.h"

#include <array>

#include <mbedtls/md.h>

#include "common/cifrado/base32.h"
#include "common/cifrado/inicio.h"

extern "C" {
#include <mbedtls/constant_time.h>
}

namespace ventrik::cifrado {
namespace {
std::optional<std::uint64_t> verificar_despues_de(gsl::span<const std::uint8_t> secreto, std::uint32_t codigo,
                                                  std::uint64_t segundos_unix, std::uint64_t ultimo) {
  const std::uint64_t paso = segundos_unix / kSegundosPasoTotp;
  std::optional<std::uint64_t> aceptado;
  for (std::uint64_t candidato = paso == 0 ? 0 : paso - 1; candidato <= paso + 1; ++candidato) {
    const auto esperado = codigo_totp(secreto, candidato);
    if (esperado && codigos_totp_iguales(*esperado, codigo) && candidato > ultimo) {
      aceptado = candidato;
    }
  }
  return aceptado;
}

}

std::optional<Secreto> secreto_totp_nuevo() {
  Secreto secreto(kBytesSecretoTotp);
  if (aleatorio(nullptr, secreto.bytes().data(), secreto.size()) != 0) {
    return std::nullopt;
  }
  return secreto;
}

std::optional<std::uint32_t> codigo_totp(gsl::span<const std::uint8_t> secreto, std::uint64_t paso) {
  std::array<unsigned char, 8> mensaje{};
  for (auto byte = mensaje.rbegin(); byte != mensaje.rend(); ++byte, paso >>= 8) {
    *byte = static_cast<unsigned char>(paso);
  }
  std::array<unsigned char, 20> resumen{};
  if (secreto.size() != kBytesSecretoTotp ||
      mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), secreto.data(), secreto.size(), mensaje.data(),
                      mensaje.size(), resumen.data()) != 0) {
    return std::nullopt;
  }
  const std::size_t desde = resumen.back() & 0x0Fu;
  std::uint32_t numero = resumen[desde] & 0x7Fu;
  for (std::size_t i = 1; i < 4; ++i) {
    numero = numero << 8 | resumen[desde + i];
  }
  return numero % kTopeCodigoTotp;
}

bool codigos_totp_iguales(std::uint32_t a, std::uint32_t b) {
  return mbedtls_ct_memcmp(&a, &b, sizeof a) == 0;
}

std::optional<std::uint64_t> verificar_totp(gsl::span<const std::uint8_t> secreto, std::uint32_t codigo,
                                            std::uint64_t segundos_unix, std::uint64_t ultimo_aceptado) {
  return verificar_despues_de(secreto, codigo, segundos_unix, ultimo_aceptado);
}

std::optional<std::uint32_t> leer_codigo_totp(std::string_view texto) {
  const std::size_t medio = kCifrasTotp / 2;
  const bool con_espacio = texto.size() == kCifrasTotp + 1 && texto[medio] == ' ';
  if (texto.size() != kCifrasTotp && !con_espacio) {
    return std::nullopt;
  }
  std::uint32_t codigo = 0;
  for (std::size_t i = 0; i < texto.size(); ++i) {
    if (con_espacio && i == medio) {
      continue;
    }
    if (texto[i] < '0' || texto[i] > '9') {
      return std::nullopt;
    }
    codigo = codigo * 10 + static_cast<std::uint32_t>(texto[i] - '0');
  }
  return codigo;
}

std::string texto_otpauth(std::string_view nombre, gsl::span<const std::uint8_t> secreto) {
  constexpr std::string_view kTalCual = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  constexpr char kHexadecimal[] = "0123456789ABCDEF";
  std::string texto = "otpauth://totp/Ventrik:";
  for (const char letra : nombre) {
    const auto byte = static_cast<unsigned char>(letra);
    if (kTalCual.find(letra) != std::string_view::npos) {
      texto.push_back(letra);
    } else {
      texto += {'%', kHexadecimal[byte >> 4], kHexadecimal[byte & 0x0Fu]};
    }
  }
  return texto + "?secret=" + a_base32(secreto, false) + "&issuer=Ventrik&digits=6&period=30";
}

}
