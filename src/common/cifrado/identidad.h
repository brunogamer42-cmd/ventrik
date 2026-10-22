#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <gsl/gsl>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

#include "common/cifrado/huella.h"
#include "common/cifrado/secreto.h"

namespace ventrik::cifrado {

constexpr std::size_t kMaximoArchivoClave = 16 * 1024;

// Clave ECDSA P-256 del servidor y su certificado autofirmado, hecho de nuevo en cada arranque.
class IdentidadServidor {
 public:
  static std::optional<IdentidadServidor> crear_nueva();
  // Solo claves privadas ECDSA P-256: cualquier otra cosa se rechaza.
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
  static std::optional<IdentidadServidor> completar(std::unique_ptr<Datos> datos);

  std::unique_ptr<Datos> datos_;
};

enum class ResultadoIdentidad { cargada, creada, archivo_invalido, error_lectura, error_escritura, error_cifrado };

struct IdentidadCargada {
  ResultadoIdentidad resultado = ResultadoIdentidad::error_cifrado;
  std::optional<IdentidadServidor> identidad;
};

// Si el archivo existe pero no se puede usar, no crea otra: la huella cambiaría sin avisar.
IdentidadCargada cargar_o_crear_identidad(const std::wstring& archivo);

}  // namespace ventrik::cifrado
