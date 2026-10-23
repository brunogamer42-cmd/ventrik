#include "common/cifrado/tls.h"

#include <algorithm>
#include <array>
#include <limits>

#include <intrin.h>
#include <winsock2.h>

#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/x509.h>

#include "common/cifrado/inicio.h"

namespace ventrik::cifrado {

namespace {

const int kCifradosAesPrimero[] = {MBEDTLS_TLS1_3_AES_128_GCM_SHA256,
                                   MBEDTLS_TLS1_3_AES_256_GCM_SHA384,
                                   MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256, 0};
const int kCifradosChachaPrimero[] = {MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256,
                                      MBEDTLS_TLS1_3_AES_128_GCM_SHA256,
                                      MBEDTLS_TLS1_3_AES_256_GCM_SHA384, 0};
const std::uint16_t kGrupos[] = {MBEDTLS_SSL_IANA_TLS_GROUP_X25519,
                                 MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1,
                                 MBEDTLS_SSL_IANA_TLS_GROUP_NONE};
const std::uint16_t kFirmas[] = {MBEDTLS_TLS1_3_SIG_ECDSA_SECP256R1_SHA256,
                                 MBEDTLS_TLS1_3_SIG_NONE};

// Alcanza para el registro TLS más grande (16 KiB y algo) sin dejar que uno solo trabe el hilo.
constexpr std::size_t kPresupuestoPorLlamada = 32 * 1024;
// Lo caro es cada registro y no cada byte: miles de registros de 6 bytes también traban.
constexpr int kLecturasPorLlamada = 64;

int largo_para_winsock(std::size_t largo) {
  return static_cast<int>(std::min<std::size_t>(largo, std::numeric_limits<int>::max()));
}

int error_de_winsock(int si_espera) {
  switch (WSAGetLastError()) {
    case WSAEWOULDBLOCK:
      return si_espera;
    case WSAECONNRESET:
    case WSAECONNABORTED:
      return MBEDTLS_ERR_NET_CONN_RESET;
    default:
      return si_espera == MBEDTLS_ERR_SSL_WANT_READ ? MBEDTLS_ERR_NET_RECV_FAILED
                                                    : MBEDTLS_ERR_NET_SEND_FAILED;
  }
}

bool preparar(mbedtls_ssl_config* config, int lado, const int* cifrados) {
  if (mbedtls_ssl_config_defaults(config, lado, MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    return false;
  }
  mbedtls_ssl_conf_min_tls_version(config, MBEDTLS_SSL_VERSION_TLS1_3);
  mbedtls_ssl_conf_max_tls_version(config, MBEDTLS_SSL_VERSION_TLS1_3);
  mbedtls_ssl_conf_ciphersuites(config, cifrados);
  mbedtls_ssl_conf_groups(config, kGrupos);
  mbedtls_ssl_conf_sig_algs(config, kFirmas);
  // Sin claves compartidas ni reanudación: cada conexión vuelve a mostrar la clave del servidor.
  mbedtls_ssl_conf_tls13_key_exchange_modes(config, MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL);
  mbedtls_ssl_conf_rng(config, aleatorio, nullptr);
  return true;
}

}  // namespace

ConfigTls::ConfigTls() {
  mbedtls_ssl_config_init(&config_);
}

ConfigTls::~ConfigTls() {
  mbedtls_ssl_config_free(&config_);
}

std::unique_ptr<ConfigTls> ConfigTls::para_servidor(IdentidadServidor& identidad) {
  return para_servidor(identidad.certificado(), identidad.clave());
}

std::unique_ptr<ConfigTls> ConfigTls::para_servidor(mbedtls_x509_crt* certificado,
                                                    mbedtls_pk_context* clave) {
  auto config = std::make_unique<ConfigTls>();
  if (!preparar(config->crudo(), MBEDTLS_SSL_IS_SERVER, kCifradosAesPrimero) ||
      mbedtls_ssl_conf_own_cert(config->crudo(), certificado, clave) != 0) {
    return nullptr;
  }
  mbedtls_ssl_conf_authmode(config->crudo(), MBEDTLS_SSL_VERIFY_NONE);
  return config;
}

std::unique_ptr<ConfigTls> ConfigTls::para_programa() {
  auto config = std::make_unique<ConfigTls>();
  if (!preparar(config->crudo(), MBEDTLS_SSL_IS_CLIENT,
                procesador_acelera_aes() ? kCifradosAesPrimero : kCifradosChachaPrimero)) {
    return nullptr;
  }
  // Con REQUIRED, Mbed TLS exige una autoridad certificante; acá decide la huella (SesionTls::verificar).
  mbedtls_ssl_conf_authmode(config->crudo(), MBEDTLS_SSL_VERIFY_OPTIONAL);
  mbedtls_ssl_conf_session_tickets(config->crudo(), MBEDTLS_SSL_SESSION_TICKETS_DISABLED);
  return config;
}

bool procesador_acelera_aes() {
  std::array<int, 4> registros{};
  __cpuid(registros.data(), 1);
  return (registros[2] & (1 << 25)) != 0;
}

SesionTls::SesionTls(const ConfigTls& config, std::uintptr_t conexion) {
  canal_.socket = conexion;
  mbedtls_ssl_init(&ssl_);
  ok_ = mbedtls_ssl_setup(&ssl_, config.crudo()) == 0;
  if (!ok_) {
    return;
  }
  mbedtls_ssl_set_bio(&ssl_, &canal_, enviar, recibir, nullptr);
  if (mbedtls_ssl_conf_get_endpoint(config.crudo()) == MBEDTLS_SSL_IS_CLIENT) {
    // El servidor se reconoce por la huella, no por un nombre en el certificado.
    ok_ = mbedtls_ssl_set_hostname(&ssl_, nullptr) == 0;
    mbedtls_ssl_set_verify(&ssl_, verificar, this);
  }
}

SesionTls::~SesionTls() {
  mbedtls_ssl_free(&ssl_);
}

void SesionTls::exigir_huella(const Huella& esperada) {
  esperada_ = esperada;
}

int SesionTls::enviar(void* contexto, const unsigned char* datos, std::size_t largo) noexcept {
  const auto s = static_cast<SOCKET>(static_cast<const Canal*>(contexto)->socket);
  const int enviados = send(s, reinterpret_cast<const char*>(datos), largo_para_winsock(largo), 0);
  return enviados >= 0 ? enviados : error_de_winsock(MBEDTLS_ERR_SSL_WANT_WRITE);
}

int SesionTls::recibir(void* contexto, unsigned char* destino, std::size_t largo) noexcept {
  auto& canal = *static_cast<Canal*>(contexto);
  if (canal.presupuesto == 0 || canal.lecturas == 0) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }
  --canal.lecturas;
  const int recibidos = recv(static_cast<SOCKET>(canal.socket), reinterpret_cast<char*>(destino),
                             largo_para_winsock(std::min(largo, canal.presupuesto)), 0);
  if (recibidos < 0) {
    return error_de_winsock(MBEDTLS_ERR_SSL_WANT_READ);
  }
  canal.presupuesto -= static_cast<std::size_t>(recibidos);
  canal.total += static_cast<std::size_t>(recibidos);
  return recibidos;
}

void SesionTls::renovar_presupuesto() {
  canal_.presupuesto = kPresupuestoPorLlamada;
  canal_.lecturas = kLecturasPorLlamada;
}

int SesionTls::verificar(void* contexto, mbedtls_x509_crt* certificado, int profundidad,
                         std::uint32_t* banderas) noexcept {
  auto* sesion = static_cast<SesionTls*>(contexto);
  // Solo importa la clave del primer certificado: es la que firma el apretón de manos.
  if (profundidad != 0) {
    *banderas = 0;
    return 0;
  }
  const auto huella = huella_de_clave(certificado->pk);
  if (!sesion->esperada_ || !huella || *huella != *sesion->esperada_) {
    sesion->huella_distinta_ = true;
    return MBEDTLS_ERR_X509_FATAL_ERROR;
  }
  sesion->huella_confirmada_ = true;
  *banderas = 0;
  return 0;
}

Paso SesionTls::traducir(int resultado) {
  switch (resultado) {
    case MBEDTLS_ERR_SSL_WANT_READ:
      return Paso::esperar_lectura;
    case MBEDTLS_ERR_SSL_WANT_WRITE:
      return Paso::esperar_escritura;
    case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
    case MBEDTLS_ERR_SSL_CONN_EOF:
    case MBEDTLS_ERR_NET_CONN_RESET:
      ultimo_error_ = resultado;
      return Paso::cerrado;
    default:
      ultimo_error_ = resultado;
      return Paso::error;
  }
}

Paso SesionTls::apreton() {
  if (!ok_) {
    return Paso::error;
  }
  renovar_presupuesto();
  const int resultado = mbedtls_ssl_handshake(&ssl_);
  return resultado == 0 ? Paso::listo : traducir(resultado);
}

Paso SesionTls::leer(gsl::span<std::uint8_t> destino, std::size_t& leidos) {
  leidos = 0;
  renovar_presupuesto();
  const int resultado = mbedtls_ssl_read(&ssl_, destino.data(), destino.size());
  if (resultado > 0) {
    leidos = static_cast<std::size_t>(resultado);
    return Paso::listo;
  }
  return resultado == 0 ? Paso::cerrado : traducir(resultado);
}

Paso SesionTls::escribir(gsl::span<const std::uint8_t> datos, std::size_t& escritos) {
  escritos = 0;
  renovar_presupuesto();
  const int maximo = mbedtls_ssl_get_max_out_record_payload(&ssl_);
  const std::size_t largo = std::min(
      datos.size(), a_medias_ != 0 ? a_medias_
                                   : maximo > 0 ? static_cast<std::size_t>(maximo) : datos.size());
  const int resultado = mbedtls_ssl_write(&ssl_, datos.data(), largo);
  a_medias_ = resultado == MBEDTLS_ERR_SSL_WANT_WRITE || resultado == MBEDTLS_ERR_SSL_WANT_READ
                  ? largo
                  : 0;
  if (resultado >= 0) {
    escritos = static_cast<std::size_t>(resultado);
    return Paso::listo;
  }
  return traducir(resultado);
}

Paso SesionTls::cerrar() {
  renovar_presupuesto();
  const int resultado = mbedtls_ssl_close_notify(&ssl_);
  return resultado == 0 ? Paso::listo : traducir(resultado);
}

std::string SesionTls::cifrado() const {
  const char* nombre = mbedtls_ssl_get_ciphersuite(&ssl_);
  return nombre != nullptr ? nombre : "";
}

bool SesionTls::es_tls13() const {
  return mbedtls_ssl_get_version_number(&ssl_) == MBEDTLS_SSL_VERSION_TLS1_3;
}

}  // namespace ventrik::cifrado
