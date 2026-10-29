#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "common/cifrado/boveda.h"
#include "common/cifrado/cadena.h"
#include "common/cifrado/identidad.h"
#include "common/cifrado/sobre.h"

namespace ventrik::servidor {

constexpr wchar_t kArchivoBase[] = L"\\ventrik.db";
constexpr wchar_t kArchivoCadena[] = L"\\cadena_de_claves.dat";

enum class Mudanza {
  hecha,
  sin_servidor,
  ya_hay_servidor,
  archivo_existe,
  error_lectura,
  error_escritura,
  error_base,
  base_grande,
  contrasena_corta,
  contrasena,
  sin_memoria,
  respaldo_danado,
  base_danada,
  base_nueva,
  otro_esquema,
  cadena_danada,
  cadena_de_otra_clave,
  clave_vieja_sin_borrar,
  error_cifrado
};

struct CadenaGuardada {
  Mudanza resultado = Mudanza::hecha;
  cifrado::Cadena cadena;
};

struct ResultadoMudanza {
  Mudanza resultado = Mudanza::error_cifrado;
  std::optional<cifrado::Huella> huella;
  bool anterior_sin_borrar = false;
};

bool hay_servidor(const std::wstring& carpeta);
bool limpiar_restos(const std::wstring& carpeta);
bool se_puede_importar(const std::wstring& carpeta);

CadenaGuardada leer_cadena_guardada(const std::wstring& carpeta, const cifrado::Huella& huella);

Mudanza revisar_base_importada(const std::wstring& archivo, const std::string& nombre_general,
                               cifrado::Secreto clave_historial);

Mudanza exportar_servidor(const std::wstring& carpeta, const cifrado::IdentidadServidor& identidad,
                          const cifrado::Boveda& boveda, const std::wstring& archivo, std::string_view contrasena,
                          std::optional<cifrado::Argon2> fijos = std::nullopt);

ResultadoMudanza importar_servidor(const std::wstring& carpeta, const std::wstring& archivo,
                                   std::string_view contrasena, const std::string& nombre_general);

ResultadoMudanza cambiar_clave(const std::wstring& carpeta, const cifrado::IdentidadServidor& actual,
                               const cifrado::Boveda& boveda);

}
