#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gsl/gsl>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

#include "common/cifrado/boveda.h"
#include "common/cifrado/clave.h"
#include "common/cifrado/huella.h"
#include "common/cifrado/secreto.h"

namespace ventrik::cifrado {

constexpr std::size_t kMaximoArchivoClave = 16 * 1024;
constexpr wchar_t kArchivoBoveda[] = L"\\boveda.dat";
constexpr wchar_t kArchivoClave[] = L"\\clave_servidor.dat";
constexpr wchar_t kArchivoClaveVieja[] = L"\\clave_servidor.der";

class IdentidadServidor {
 public:
  static std::optional<IdentidadServidor> crear_nueva();
  static std::optional<IdentidadServidor> desde_der(gsl::span<const std::uint8_t> der);

  IdentidadServidor(IdentidadServidor&&) noexcept;
  IdentidadServidor& operator=(IdentidadServidor&&) noexcept;
  ~IdentidadServidor();

  std::optional<Secreto> clave_der() const;
  const Huella& huella() const;
  mbedtls_pk_context* clave();
  mbedtls_x509_crt* certificado();

 private:
  struct Datos;
  explicit IdentidadServidor(std::unique_ptr<Datos> datos);
  static std::optional<IdentidadServidor> completar(std::optional<Clave> clave);

  std::unique_ptr<Datos> datos_;
};

enum class ResultadoIdentidad {
  cargada,
  creada,
  convertida,
  archivo_invalido,
  error_lectura,
  error_escritura,
  error_cifrado,
  falta_contrasena,
  contrasena_incorrecta,
  sin_memoria,
  boveda_de_otro_windows,
  boveda_danada,
  falta_boveda,
  falta_clave,
  claves_distintas
};

struct IdentidadCargada {
  ResultadoIdentidad resultado = ResultadoIdentidad::error_cifrado;
  std::optional<IdentidadServidor> identidad;
  std::optional<Boveda> boveda;
  std::wstring archivo;
};

std::optional<std::vector<std::uint8_t>> cerrar_clave(const IdentidadServidor& identidad, const Boveda& boveda);
std::optional<IdentidadServidor> abrir_clave(gsl::span<const std::uint8_t> archivo, const Boveda& boveda);

IdentidadCargada cargar_o_crear_identidad(const std::wstring& carpeta, std::optional<std::string_view> contrasena,
                                          bool base_con_dueno);

}
