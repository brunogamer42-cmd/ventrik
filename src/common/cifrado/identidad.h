#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <gsl/gsl>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

#include "common/cifrado/clave.h"
#include "common/cifrado/huella.h"
#include "common/cifrado/secreto.h"

namespace ventrik::cifrado {

constexpr std::size_t kMaximoArchivoClave = 16 * 1024;

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

enum class ResultadoIdentidad { cargada, creada, archivo_invalido, error_lectura, error_escritura, error_cifrado };

struct IdentidadCargada {
  ResultadoIdentidad resultado = ResultadoIdentidad::error_cifrado;
  std::optional<IdentidadServidor> identidad;
};

IdentidadCargada cargar_o_crear_identidad(const std::wstring& archivo);

}  // namespace ventrik::cifrado
