#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/clave.h"
#include "common/cifrado/secreto.h"
#include "common/cifrado/sobre.h"

namespace ventrik::cliente {

constexpr std::size_t kMaximoArchivoIdentidad = 16 * 1024;
constexpr std::size_t kMinimoContrasena = cifrado::kMinimoContrasena;
constexpr std::size_t kBytesSecretoDeEquipo = 32;

enum class EstadoIdentidad { lista, creada, con_contrasena, danada, error };

struct MiIdentidad {
  EstadoIdentidad estado = EstadoIdentidad::error;
  std::optional<cifrado::Clave> clave;
  std::vector<std::uint8_t> cerrada;
};

MiIdentidad cargar_o_crear_mi_identidad(const std::wstring& carpeta);
bool guardar_mi_identidad(const std::wstring& carpeta, const cifrado::Clave& clave,
                          const cifrado::LlaveDeSobre* maestra = nullptr);

std::optional<std::vector<std::uint8_t>> armar_archivo_de_identidad(const cifrado::Clave& clave,
                                                                    const cifrado::LlaveDeSobre* maestra = nullptr);
MiIdentidad leer_archivo_de_identidad(gsl::span<const std::uint8_t> archivo);

std::optional<std::vector<std::uint8_t>> armar_copia(gsl::span<const std::uint8_t> der, std::string_view contrasena,
                                                     std::optional<cifrado::Argon2> fijos = std::nullopt);

struct CopiaAbierta {
  cifrado::AperturaSobre resultado = cifrado::AperturaSobre::danado;
  std::optional<cifrado::Clave> clave;
  std::optional<cifrado::LlaveDeSobre> llave;
};

CopiaAbierta abrir_copia(gsl::span<const std::uint8_t> copia, std::string_view contrasena);
std::optional<cifrado::Clave> leer_contenido_de_copia(gsl::span<const std::uint8_t> contenido);

std::optional<cifrado::LlaveDeSobre> nueva_maestra(std::string_view contrasena,
                                                   std::optional<cifrado::Argon2> fijos = std::nullopt);
std::optional<std::vector<std::uint8_t>> cerrar_con_maestra(const cifrado::Clave& clave,
                                                            const cifrado::LlaveDeSobre& maestra);
CopiaAbierta abrir_con_maestra(gsl::span<const std::uint8_t> cerrada, std::string_view contrasena);
bool hay_que_bloquear(std::uint8_t eleccion, bool windows_bloqueado, std::uint32_t sin_usar_ms);

std::optional<cifrado::Secreto> secreto_de_equipo(const std::wstring& carpeta);
bool guardar_apartando_la_vieja(const std::wstring& carpeta, const cifrado::Clave& clave);
std::optional<cifrado::Secreto> leer_archivo_de_equipo(gsl::span<const std::uint8_t> archivo);

std::string identidad_corta(const cifrado::Huella& huella);
std::array<std::uint8_t, kBytesSecretoDeEquipo> codigo_de_equipo(const cifrado::Secreto& secreto,
                                                                 const cifrado::Huella& huella);

}  // namespace ventrik::cliente
