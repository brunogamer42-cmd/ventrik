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
  Datos() {
    mbedtls_pk_init(&clave);
    mbedtls_x509_crt_init(&certificado);
  }
  ~Datos() {
    mbedtls_x509_crt_free(&certificado);
    mbedtls_pk_free(&clave);
  }
  Datos(const Datos&) = delete;
  Datos& operator=(const Datos&) = delete;

  mbedtls_pk_context clave;
  mbedtls_x509_crt certificado;
  std::optional<Huella> huella;
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
  // Positivo y sin ceros adelante, como pide DER.
  serie[0] = static_cast<unsigned char>((serie[0] & 0x7F) | 0x40);

  mbedtls_x509write_crt_set_version(&escritor, MBEDTLS_X509_CRT_VERSION_3);
  mbedtls_x509write_crt_set_md_alg(&escritor, MBEDTLS_MD_SHA256);
  mbedtls_x509write_crt_set_subject_key(&escritor, &clave);
  mbedtls_x509write_crt_set_issuer_key(&escritor, &clave);
  // La validez no importa (el programa mira la huella), pero una PC con la hora mal no tiene que fallar.
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

std::optional<IdentidadServidor> IdentidadServidor::completar(std::unique_ptr<Datos> datos) {
  datos->huella = huella_de_clave(datos->clave);
  if (!datos->huella || !armar_certificado(datos->clave, datos->certificado)) {
    return std::nullopt;
  }
  return IdentidadServidor(std::move(datos));
}

std::optional<IdentidadServidor> IdentidadServidor::crear_nueva() {
  auto datos = std::make_unique<Datos>();
  if (mbedtls_pk_setup(&datos->clave, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0 ||
      mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(datos->clave), aleatorio,
                          nullptr) != 0) {
    return std::nullopt;
  }
  return completar(std::move(datos));
}

std::optional<IdentidadServidor> IdentidadServidor::desde_der(gsl::span<const std::uint8_t> der) {
  auto datos = std::make_unique<Datos>();
  if (der.empty() ||
      mbedtls_pk_parse_key(&datos->clave, der.data(), der.size(), nullptr, 0, aleatorio,
                           nullptr) != 0 ||
      mbedtls_pk_get_type(&datos->clave) != MBEDTLS_PK_ECKEY ||
      mbedtls_ecp_keypair_get_group_id(mbedtls_pk_ec(datos->clave)) != MBEDTLS_ECP_DP_SECP256R1 ||
      // El archivo trae la pública aparte de la privada: si no coinciden, la huella mentiría.
      mbedtls_pk_check_pair(&datos->clave, &datos->clave, aleatorio, nullptr) != 0) {
    return std::nullopt;
  }
  return completar(std::move(datos));
}

std::optional<Secreto> IdentidadServidor::clave_der() const {
  Secreto espacio(1024);
  const int largo =
      mbedtls_pk_write_key_der(&datos_->clave, espacio.bytes().data(), espacio.size());
  if (largo <= 0 || static_cast<std::size_t>(largo) > espacio.size()) {
    return std::nullopt;
  }
  const auto escrito = espacio.bytes().last(static_cast<std::size_t>(largo));
  Secreto resultado(escrito.size());
  std::copy(escrito.begin(), escrito.end(), resultado.bytes().begin());
  return resultado;
}

const Huella& IdentidadServidor::huella() const {
  return *datos_->huella;
}

mbedtls_pk_context* IdentidadServidor::clave() {
  return &datos_->clave;
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
    // Otro servidor la creó mientras tanto: vale la suya, que ya mostró su huella.
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
