#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include <gsl/gsl>

#include "common/cifrado/secreto.h"

namespace ventrik {

enum class LecturaArchivo { leido, no_existe, demasiado_grande, error };

// Se lee como Secreto porque puede ser una clave; borrarlo al final no cuesta nada.
LecturaArchivo leer_archivo(const std::wstring& ruta, std::size_t maximo,
                            cifrado::Secreto& contenido);

// Escribe en un archivo temporal y lo renombra: si se corta a la mitad, queda el anterior.
bool escribir_archivo_completo(const std::wstring& ruta, gsl::span<const std::uint8_t> contenido);

enum class EscrituraNueva { escrito, ya_existe, error };

// Igual, pero nunca reemplaza: si otro lo creó mientras tanto, gana el otro.
EscrituraNueva escribir_archivo_nuevo(const std::wstring& ruta,
                                      gsl::span<const std::uint8_t> contenido);

// Los temporales que dejó un corte entre escribir y renombrar (pueden tener una clave adentro).
void borrar_temporales_de(const std::wstring& ruta);

bool crear_carpetas(const std::wstring& ruta);

// %APPDATA%\Ventrik\<nombre>, o vacío si Windows no la da.
std::wstring carpeta_de_datos(const std::wstring& nombre);

}  // namespace ventrik
