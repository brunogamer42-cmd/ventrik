#include "common/cifrado/identidad.h"

#include <algorithm>
#include <array>
#include <utility>

#include <mbedtls/ecp.h>
#include <mbedtls/x509_crt.h>

#include "common/archivos.h"
#include "common/cifrado/inicio.h"
#include "common/pendiente.h"

namespace ventrik::cifrado {

struct IdentidadServidor::Datos {
  explicit Datos(Clave nueva) : clave(std::move(nueva)) { mbedtls_x509_crt_init(&certificado); }
  ~Datos() { mbedtls_x509_crt_free(&certificado); }
  Datos(const Datos&) = delete;
  Datos& operator=(const Datos&) = delete;

  Clave clave;
  mbedtls_x509_crt certificado;
};

namespace {

bool armar_certificado(mbedtls_pk_context& clave, mbedtls_x509_crt& certificado) {
  mbedtls_x509write_cert escritor;
  mbedtls_x509write_crt_init(&escritor);
  const auto liberar = gsl::finally([&escritor] { mbedtls_x509write_crt_free(&escritor); });

  std::array<unsigned char, 16> serie{};
  if (aleatorio(nullptr, serie.data(), serie.size()) != 0) {
    return false;
  }
  serie[0] = static_cast<unsigned char>((serie[0] & 0x7F) | 0x40);

  mbedtls_x509write_crt_set_version(&escritor, MBEDTLS_X509_CRT_VERSION_3);
  mbedtls_x509write_crt_set_md_alg(&escritor, MBEDTLS_MD_SHA256);
  mbedtls_x509write_crt_set_subject_key(&escritor, &clave);
  mbedtls_x509write_crt_set_issuer_key(&escritor, &clave);
  if (mbedtls_x509write_crt_set_subject_name(&escritor, "CN=Ventrik") != 0 ||
      mbedtls_x509write_crt_set_issuer_name(&escritor, "CN=Ventrik") != 0 ||
      mbedtls_x509write_crt_set_serial_raw(&escritor, serie.data(), serie.size()) != 0 ||
      mbedtls_x509write_crt_set_validity(&escritor, "20000101000000", "20991231235959") != 0) {
    return false;
  }
  std::array<unsigned char, 2048> der{};
  const int largo = mbedtls_x509write_crt_der(&escritor, der.data(), der.size(), aleatorio, nullptr);
  if (largo <= 0 || static_cast<std::size_t>(largo) > der.size()) {
    return false;
  }
  const auto escrito = gsl::span<const unsigned char>(der).last(static_cast<std::size_t>(largo));
  return mbedtls_x509_crt_parse_der(&certificado, escrito.data(), escrito.size()) == 0;
}

}  // namespace

IdentidadServidor::IdentidadServidor(std::unique_ptr<Datos> datos) : datos_(std::move(datos)) {}
IdentidadServidor::IdentidadServidor(IdentidadServidor&&) noexcept = default;
IdentidadServidor& IdentidadServidor::operator=(IdentidadServidor&&) noexcept = default;
IdentidadServidor::~IdentidadServidor() = default;

std::optional<IdentidadServidor> IdentidadServidor::completar(std::optional<Clave> clave) {
  if (!clave) {
    return std::nullopt;
  }
  auto datos = std::make_unique<Datos>(std::move(*clave));
  if (!armar_certificado(*datos->clave.contexto(), datos->certificado)) {
    return std::nullopt;
  }
  return IdentidadServidor(std::move(datos));
}

std::optional<IdentidadServidor> IdentidadServidor::crear_nueva() {
  return completar(Clave::crear_nueva());
}

std::optional<IdentidadServidor> IdentidadServidor::desde_der(gsl::span<const std::uint8_t> der) {
  return completar(Clave::desde_der(der));
}

std::optional<Secreto> IdentidadServidor::clave_der() const {
  return datos_->clave.der();
}

const Huella& IdentidadServidor::huella() const {
  return datos_->clave.huella();
}

mbedtls_pk_context* IdentidadServidor::clave() {
  return datos_->clave.contexto();
}

mbedtls_x509_crt* IdentidadServidor::certificado() {
  return &datos_->certificado;
}

namespace {

IdentidadCargada cargar(const std::wstring& archivo, bool& no_existe) {
  IdentidadCargada cargada;
  Secreto contenido;
  no_existe = false;
  switch (leer_archivo(archivo, kMaximoArchivoClave, contenido)) {
    case LecturaArchivo::leido:
      cargada.identidad = IdentidadServidor::desde_der(contenido.bytes());
      cargada.resultado =
          cargada.identidad ? ResultadoIdentidad::cargada : ResultadoIdentidad::archivo_invalido;
      return cargada;
    case LecturaArchivo::demasiado_grande:
      cargada.resultado = ResultadoIdentidad::archivo_invalido;
      return cargada;
    case LecturaArchivo::error:
      cargada.resultado = ResultadoIdentidad::error_lectura;
      return cargada;
    case LecturaArchivo::no_existe:
      no_existe = true;
      break;
  }
  return cargada;
}

}  // namespace

IdentidadCargada cargar_o_crear_identidad(const std::wstring& archivo) {
  borrar_temporales_de(archivo);
  bool no_existe = false;
  IdentidadCargada cargada = cargar(archivo, no_existe);
  if (!no_existe) {
    return cargada;
  }

  auto nueva = IdentidadServidor::crear_nueva();
  if (!nueva) {
    cargada.resultado = ResultadoIdentidad::error_cifrado;
    return cargada;
  }
  VENTRIK_PENDIENTE("clave del servidor sin cifrar en disco: DPAPI o contrasena en la etapa 4");
  const auto der = nueva->clave_der();
  const auto escritura = der ? escribir_archivo_nuevo(archivo, der->bytes()) : EscrituraNueva::error;
  if (escritura == EscrituraNueva::ya_existe) {
    return cargar(archivo, no_existe);
  }
  if (escritura != EscrituraNueva::escrito) {
    cargada.resultado = ResultadoIdentidad::error_escritura;
    return cargada;
  }
  cargada.identidad = std::move(nueva);
  cargada.resultado = ResultadoIdentidad::creada;
  return cargada;
}

}  // namespace ventrik::cifrado
