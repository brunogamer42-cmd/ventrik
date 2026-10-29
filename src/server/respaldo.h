#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/identidad.h"
#include "common/cifrado/secreto.h"
#include "common/cifrado/sobre.h"
#include "common/protocol/pedidos.h"

namespace ventrik::servidor {

constexpr std::string_view kMarcaRespaldo = "VKSV";
constexpr std::size_t kMaximoClaveRespaldo = cifrado::kMaximoArchivoClave;
constexpr std::size_t kMaximoBovedaRespaldo = cifrado::kBytesBoveda;
constexpr std::size_t kMaximoBaseRespaldo = 64 * 1024 * 1024;
constexpr std::size_t kMaximoCadenaRespaldo = protocolo::kMaximoCuerpoCadena;
constexpr std::size_t kMaximoArchivoRespaldo =
    kMaximoClaveRespaldo + kMaximoBovedaRespaldo + kMaximoBaseRespaldo + kMaximoCadenaRespaldo + 1024;

struct PartesRespaldo {
  gsl::span<const std::uint8_t> clave;
  gsl::span<const std::uint8_t> boveda;
  gsl::span<const std::uint8_t> base;
  gsl::span<const std::uint8_t> cadena;
};

std::optional<PartesRespaldo> leer_contenido(gsl::span<const std::uint8_t> contenido);

std::optional<std::vector<std::uint8_t>> armar_respaldo(const PartesRespaldo& partes, std::string_view contrasena,
                                                        std::optional<cifrado::Argon2> fijos = std::nullopt);

struct RespaldoAbierto {
  cifrado::AperturaSobre resultado = cifrado::AperturaSobre::danado;
  std::optional<cifrado::Secreto> contenido;
  PartesRespaldo partes;
};

RespaldoAbierto abrir_respaldo(gsl::span<const std::uint8_t> archivo, std::string_view contrasena);

bool copiar_base(const std::wstring& origen, const std::wstring& destino);
void borrar_base(const std::wstring& archivo);

enum class RevisionBase { buena, error, danada, version_vieja, otra_version, otro_esquema };

RevisionBase revisar_base(const std::wstring& archivo);

}
