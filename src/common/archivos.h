#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include <gsl/gsl>

#include "common/cifrado/secreto.h"

namespace ventrik {

enum class LecturaArchivo { leido, no_existe, demasiado_grande, error };

LecturaArchivo leer_archivo(const std::wstring& ruta, std::size_t maximo,
                            cifrado::Secreto& contenido);

bool escribir_archivo_completo(const std::wstring& ruta, gsl::span<const std::uint8_t> contenido);

enum class EscrituraNueva { escrito, ya_existe, error };

EscrituraNueva escribir_archivo_nuevo(const std::wstring& ruta,
                                      gsl::span<const std::uint8_t> contenido);

void borrar_temporales_de(const std::wstring& ruta);

bool borrar_con_ceros(const std::wstring& ruta);

bool crear_carpetas(const std::wstring& ruta);

std::wstring carpeta_de_datos(const std::wstring& nombre);

std::wstring ruta_completa(const std::wstring& ruta);

}
