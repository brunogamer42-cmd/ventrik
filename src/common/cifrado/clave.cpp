#include "common/cifrado/clave.h"

#include <algorithm>
#include <utility>

#include <mbedtls/ecp.h>
#include <mbedtls/sha256.h>

#include "common/cifrado/inicio.h"

namespace ventrik::cifrado {

struct Clave::Datos {
  Datos() { mbedtls_pk_init(&pk); }
  ~Datos() { mbedtls_pk_free(&pk); }
  Datos(const Datos&) = delete;
  Datos& operator=(const Datos&) = delete;

  mbedtls_pk_context pk;
  std::optional<Huella> huella;
  ClavePublica publica{};
};

std::optional<Resumen> resumir(gsl::span<const std::uint8_t> datos) {
  Resumen resumen{};
  if (mbedtls_sha256(datos.data(), datos.size(), resumen.data(), 0) != 0) {
    return std::nullopt;
  }
  return resumen;
}

namespace {

bool es_p256(const mbedtls_pk_context& pk) {
  return mbedtls_pk_get_type(&pk) == MBEDTLS_PK_ECKEY &&
         mbedtls_ecp_keypair_get_group_id(mbedtls_pk_ec(pk)) == MBEDTLS_ECP_DP_SECP256R1;
}

bool armar_publica(const ClavePublica& publica, mbedtls_pk_context& pk) {
  mbedtls_ecp_group grupo;
  mbedtls_ecp_point punto;
  mbedtls_ecp_group_init(&grupo);
  mbedtls_ecp_point_init(&punto);
  const auto liberar = gsl::finally([&] {
    mbedtls_ecp_point_free(&punto);
    mbedtls_ecp_group_free(&grupo);
  });
  return publica[0] == 0x04 && mbedtls_ecp_group_load(&grupo, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
         mbedtls_ecp_point_read_binary(&grupo, &punto, publica.data(), publica.size()) == 0 &&
         mbedtls_ecp_check_pubkey(&grupo, &punto) == 0 &&
         mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) == 0 &&
         mbedtls_ecp_set_public_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(pk), &punto) == 0;
}

}  // namespace

Clave::Clave(std::unique_ptr<Datos> datos) : datos_(std::move(datos)) {}
Clave::Clave(Clave&&) noexcept = default;
Clave& Clave::operator=(Clave&&) noexcept = default;
Clave::~Clave() = default;

std::optional<Clave> Clave::completar(std::unique_ptr<Datos> datos) {
  std::size_t largo = 0;
  datos->huella = huella_de_clave(datos->pk);
  if (!datos->huella ||
      mbedtls_ecp_write_public_key(mbedtls_pk_ec(datos->pk), MBEDTLS_ECP_PF_UNCOMPRESSED, &largo,
                                   datos->publica.data(), datos->publica.size()) != 0 ||
      largo != kBytesClavePublica) {
    return std::nullopt;
  }
  return Clave(std::move(datos));
}

std::optional<Clave> Clave::crear_nueva() {
  auto datos = std::make_unique<Datos>();
  if (mbedtls_pk_setup(&datos->pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0 ||
      mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(datos->pk), aleatorio, nullptr) != 0) {
    return std::nullopt;
  }
  return completar(std::move(datos));
}

std::optional<Clave> Clave::desde_der(gsl::span<const std::uint8_t> der) {
  auto datos = std::make_unique<Datos>();
  if (der.empty() ||
      mbedtls_pk_parse_key(&datos->pk, der.data(), der.size(), nullptr, 0, aleatorio, nullptr) != 0 ||
      !es_p256(datos->pk) || mbedtls_pk_check_pair(&datos->pk, &datos->pk, aleatorio, nullptr) != 0) {
    return std::nullopt;
  }
  return completar(std::move(datos));
}

std::optional<Secreto> Clave::der() const {
  Secreto espacio(1024);
  const int largo = mbedtls_pk_write_key_der(&datos_->pk, espacio.bytes().data(), espacio.size());
  if (largo <= 0 || static_cast<std::size_t>(largo) > espacio.size()) {
    return std::nullopt;
  }
  const auto escrito = espacio.bytes().last(static_cast<std::size_t>(largo));
  Secreto resultado(escrito.size());
  std::copy(escrito.begin(), escrito.end(), resultado.bytes().begin());
  return resultado;
}

const Huella& Clave::huella() const {
  return *datos_->huella;
}

const ClavePublica& Clave::publica() const {
  return datos_->publica;
}

std::optional<std::vector<std::uint8_t>> Clave::firmar(gsl::span<const std::uint8_t> datos) const {
  const auto resumen = resumir(datos);
  std::array<unsigned char, MBEDTLS_PK_SIGNATURE_MAX_SIZE> firma{};
  std::size_t largo = 0;
  if (!resumen ||
      mbedtls_pk_sign(&datos_->pk, MBEDTLS_MD_SHA256, resumen->data(), resumen->size(), firma.data(), firma.size(),
                      &largo, aleatorio, nullptr) != 0 ||
      largo == 0 || largo > kMaximoFirma) {
    return std::nullopt;
  }
  return std::vector<std::uint8_t>(firma.begin(), firma.begin() + static_cast<std::ptrdiff_t>(largo));
}

mbedtls_pk_context* Clave::contexto() const {
  return &datos_->pk;
}

std::optional<Huella> huella_de_publica(const ClavePublica& publica) {
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  const auto liberar = gsl::finally([&pk] { mbedtls_pk_free(&pk); });
  return armar_publica(publica, pk) ? huella_de_clave(pk) : std::nullopt;
}

bool verificar_firma(const ClavePublica& publica, gsl::span<const std::uint8_t> datos,
                     gsl::span<const std::uint8_t> firma) {
  if (firma.empty() || firma.size() > kMaximoFirma) {
    return false;
  }
  const auto resumen = resumir(datos);
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  const auto liberar = gsl::finally([&pk] { mbedtls_pk_free(&pk); });
  return resumen && armar_publica(publica, pk) &&
         mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, resumen->data(), resumen->size(), firma.data(), firma.size()) == 0;
}

}  // namespace ventrik::cifrado
