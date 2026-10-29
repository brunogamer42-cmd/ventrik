#include "common/cifrado/tls.h"

#include <algorithm>
#include <array>
#include <atomic>
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
const int kVozAesPrimero[] = {MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256,
                              MBEDTLS_TLS_PSK_WITH_AES_256_GCM_SHA384,
                              MBEDTLS_TLS_PSK_WITH_CHACHA20_POLY1305_SHA256, 0};
const int kVozChachaPrimero[] = {MBEDTLS_TLS_PSK_WITH_CHACHA20_POLY1305_SHA256,
                                 MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256,
                                 MBEDTLS_TLS_PSK_WITH_AES_256_GCM_SHA384, 0};
const int kSoloAes[] = {MBEDTLS_TLS1_3_AES_128_GCM_SHA256, MBEDTLS_TLS1_3_AES_256_GCM_SHA384, 0};
const int kSoloChacha[] = {MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256, 0};
const int kVozSoloAes[] = {MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256, MBEDTLS_TLS_PSK_WITH_AES_256_GCM_SHA384, 0};
const int kVozSoloChacha[] = {MBEDTLS_TLS_PSK_WITH_CHACHA20_POLY1305_SHA256, 0};
const std::uint16_t kGrupos[] = {MBEDTLS_SSL_IANA_TLS_GROUP_X25519,
                                 MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1,
                                 MBEDTLS_SSL_IANA_TLS_GROUP_NONE};
const std::uint16_t kFirmas[] = {MBEDTLS_TLS1_3_SIG_ECDSA_SECP256R1_SHA256,
                                 MBEDTLS_TLS1_3_SIG_NONE};

constexpr char kEtiquetaVoz[] = "EXPERIMENTAL ventrik voz";
constexpr char kEtiquetaIdentidad[] = "EXPERIMENTAL ventrik identidad";
constexpr std::size_t kPresupuestoPorLlamada = 32 * 1024;
constexpr int kLecturasPorLlamada = 64;

std::atomic<std::uint64_t> tcp_enviados{0};
std::atomic<std::uint64_t> tcp_recibidos{0};
std::atomic<std::uint64_t> udp_enviados{0};
std::atomic<std::uint64_t> udp_recibidos{0};

void contar(bool datagrama, bool enviado, std::size_t bytes) {
  (datagrama ? (enviado ? udp_enviados : udp_recibidos) : (enviado ? tcp_enviados : tcp_recibidos))
      .fetch_add(bytes, std::memory_order_relaxed);
}

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

const int* cifrados(Algoritmo algoritmo, bool voz, bool aes_primero) {
  if (algoritmo == Algoritmo::solo_aes_gcm) {
    return voz ? kVozSoloAes : kSoloAes;
  }
  if (algoritmo == Algoritmo::solo_chacha20) {
    return voz ? kVozSoloChacha : kSoloChacha;
  }
  return voz ? (aes_primero ? kVozAesPrimero : kVozChachaPrimero)
             : (aes_primero ? kCifradosAesPrimero : kCifradosChachaPrimero);
}

bool preparar(mbedtls_ssl_config* config, int lado, const int* cifrados, bool datagrama = false) {
  if (mbedtls_ssl_config_defaults(config, lado,
                                  datagrama ? MBEDTLS_SSL_TRANSPORT_DATAGRAM
                                            : MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    return false;
  }
  const auto version = datagrama ? MBEDTLS_SSL_VERSION_TLS1_2 : MBEDTLS_SSL_VERSION_TLS1_3;
  mbedtls_ssl_conf_min_tls_version(config, version);
  mbedtls_ssl_conf_max_tls_version(config, version);
  mbedtls_ssl_conf_ciphersuites(config, cifrados);
  mbedtls_ssl_conf_groups(config, kGrupos);
  mbedtls_ssl_conf_sig_algs(config, kFirmas);
  mbedtls_ssl_conf_tls13_key_exchange_modes(config, MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL);
  mbedtls_ssl_conf_rng(config, aleatorio, nullptr);
  if (datagrama) {
    mbedtls_ssl_conf_handshake_timeout(config, 1000, 8000);
    mbedtls_ssl_conf_dtls_anti_replay(config, MBEDTLS_SSL_ANTI_REPLAY_ENABLED);
  }
  return true;
}

}

ConfigTls::ConfigTls() {
  mbedtls_ssl_config_init(&config_);
  mbedtls_ssl_cookie_init(&cookies_);
}

ConfigTls::~ConfigTls() {
  mbedtls_ssl_cookie_free(&cookies_);
  mbedtls_ssl_config_free(&config_);
}

std::unique_ptr<ConfigTls> ConfigTls::para_servidor(IdentidadServidor& identidad, Algoritmo algoritmo) {
  return para_servidor(identidad.certificado(), identidad.clave(), algoritmo);
}

std::unique_ptr<ConfigTls> ConfigTls::para_servidor(mbedtls_x509_crt* certificado, mbedtls_pk_context* clave,
                                                    Algoritmo algoritmo) {
  auto config = std::make_unique<ConfigTls>();
  if (!preparar(config->crudo(), MBEDTLS_SSL_IS_SERVER, cifrados(algoritmo, false, true)) ||
      mbedtls_ssl_conf_own_cert(config->crudo(), certificado, clave) != 0) {
    return nullptr;
  }
  mbedtls_ssl_conf_authmode(config->crudo(), MBEDTLS_SSL_VERIFY_NONE);
  return config;
}

std::unique_ptr<ConfigTls> ConfigTls::para_programa(Algoritmo algoritmo) {
  auto config = std::make_unique<ConfigTls>();
  if (!preparar(config->crudo(), MBEDTLS_SSL_IS_CLIENT, cifrados(algoritmo, false, procesador_acelera_aes()))) {
    return nullptr;
  }
  mbedtls_ssl_conf_authmode(config->crudo(), MBEDTLS_SSL_VERIFY_OPTIONAL);
  mbedtls_ssl_conf_session_tickets(config->crudo(), MBEDTLS_SSL_SESSION_TICKETS_DISABLED);
  return config;
}

std::unique_ptr<ConfigTls> ConfigTls::para_voz_servidor(ElegirClave elegir, Algoritmo algoritmo) {
  if (!elegir) {
    return nullptr;
  }
  auto config = std::make_unique<ConfigTls>();
  config->datagrama_ = true;
  config->elegir_ = std::move(elegir);
  if (!preparar(config->crudo(), MBEDTLS_SSL_IS_SERVER, cifrados(algoritmo, true, true), true) ||
      mbedtls_ssl_cookie_setup(&config->cookies_, aleatorio, nullptr) != 0) {
    return nullptr;
  }
  mbedtls_ssl_conf_dtls_cookies(config->crudo(), mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check,
                                &config->cookies_);
  mbedtls_ssl_conf_preference_order(config->crudo(), MBEDTLS_SSL_SRV_CIPHERSUITE_ORDER_CLIENT);
  mbedtls_ssl_conf_psk_cb(config->crudo(), elegir_psk, config.get());
  return config;
}

std::unique_ptr<ConfigTls> ConfigTls::para_voz_programa(gsl::span<const std::uint8_t> identidad,
                                                        gsl::span<const std::uint8_t> clave, Algoritmo algoritmo) {
  auto config = std::make_unique<ConfigTls>();
  config->datagrama_ = true;
  if (!preparar(config->crudo(), MBEDTLS_SSL_IS_CLIENT, cifrados(algoritmo, true, procesador_acelera_aes()), true) ||
      mbedtls_ssl_conf_psk(config->crudo(), clave.data(), clave.size(), identidad.data(),
                           identidad.size()) != 0) {
    return nullptr;
  }
  mbedtls_ssl_conf_authmode(config->crudo(), MBEDTLS_SSL_VERIFY_NONE);
  return config;
}

int ConfigTls::elegir_psk(void* contexto, mbedtls_ssl_context* ssl, const unsigned char* identidad,
                          std::size_t largo) noexcept {
  auto& config = *static_cast<ConfigTls*>(contexto);
  auto* sesion = static_cast<SesionTls*>(mbedtls_ssl_get_user_data_p(ssl));
  const auto clave = sesion != nullptr ? config.elegir_(*sesion, gsl::make_span(identidad, largo))
                                       : gsl::span<const std::uint8_t>();
  return clave.empty() ? MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY
                       : mbedtls_ssl_set_hs_psk(ssl, clave.data(), clave.size());
}

bool procesador_acelera_aes() {
  std::array<int, 4> registros{};
  __cpuid(registros.data(), 1);
  return (registros[2] & (1 << 25)) != 0;
}

SesionTls::SesionTls(const ConfigTls& config, std::uintptr_t conexion) {
  canal_.socket = conexion;
  iniciar(config);
}

SesionTls::SesionTls(const ConfigTls& config, const EnviarDatagrama& enviar,
                     const red::Direccion& otro) {
  canal_.enviar_a = &enviar;
  iniciar(config);
  ok_ = ok_ && reiniciar(otro);
}

void SesionTls::iniciar(const ConfigTls& config) {
  mbedtls_ssl_init(&ssl_);
  ok_ = mbedtls_ssl_setup(&ssl_, config.crudo()) == 0;
  if (!ok_) {
    return;
  }
  canal_.datagrama = config.datagrama();
  mbedtls_ssl_set_user_data_p(&ssl_, this);
  mbedtls_ssl_set_bio(&ssl_, &canal_, enviar, recibir, nullptr);
  if (canal_.datagrama) {
    mbedtls_ssl_set_timer_cb(&ssl_, &temporizador_, mbedtls_timing_set_delay,
                             mbedtls_timing_get_delay);
  }
  if (mbedtls_ssl_conf_get_endpoint(config.crudo()) == MBEDTLS_SSL_IS_CLIENT) {
    ok_ = mbedtls_ssl_set_hostname(&ssl_, nullptr) == 0;
    mbedtls_ssl_set_verify(&ssl_, verificar, this);
  }
}

SesionTls::~SesionTls() {
  mbedtls_ssl_free(&ssl_);
}

bool SesionTls::reiniciar(const red::Direccion& otro) {
  if (!ok_) {
    return false;
  }
  canal_.otro = otro;
  canal_.entrada = {};
  a_medias_ = 0;
  marca_ = 0;
  const std::array<unsigned char, 6> id = {
      static_cast<unsigned char>(otro.ipv4 >> 24), static_cast<unsigned char>(otro.ipv4 >> 16),
      static_cast<unsigned char>(otro.ipv4 >> 8),  static_cast<unsigned char>(otro.ipv4),
      static_cast<unsigned char>(otro.puerto >> 8), static_cast<unsigned char>(otro.puerto)};
  const bool servidor = mbedtls_ssl_conf_get_endpoint(mbedtls_ssl_context_get_config(&ssl_)) ==
                        MBEDTLS_SSL_IS_SERVER;
  return mbedtls_ssl_session_reset(&ssl_) == 0 &&
         (!servidor || mbedtls_ssl_set_client_transport_id(&ssl_, id.data(), id.size()) == 0);
}

bool SesionTls::exportar(gsl::span<const std::uint8_t> contexto, gsl::span<std::uint8_t> salida) {
  return ok_ && mbedtls_ssl_export_keying_material(&ssl_, salida.data(), salida.size(), kEtiquetaVoz,
                                                   sizeof(kEtiquetaVoz) - 1, contexto.data(),
                                                   contexto.size(), 1) == 0;
}

bool SesionTls::valor_para_identidad(gsl::span<std::uint8_t> salida) {
  return ok_ && mbedtls_ssl_export_keying_material(&ssl_, salida.data(), salida.size(), kEtiquetaIdentidad,
                                                   sizeof(kEtiquetaIdentidad) - 1, nullptr, 0, 0) == 0;
}

Trafico trafico() {
  return {tcp_enviados.load(std::memory_order_relaxed), tcp_recibidos.load(std::memory_order_relaxed),
          udp_enviados.load(std::memory_order_relaxed), udp_recibidos.load(std::memory_order_relaxed)};
}

void SesionTls::exigir_huella(const Huella& esperada) {
  esperada_ = esperada;
}

int SesionTls::enviar(void* contexto, const unsigned char* datos, std::size_t largo) noexcept {
  auto& canal = *static_cast<Canal*>(contexto);
  if (canal.enviar_a != nullptr) {
    (*canal.enviar_a)(canal.otro, gsl::make_span(datos, largo));
    ++canal.enviados;
    contar(true, true, largo);
    return largo_para_winsock(largo);
  }
  const int enviados = send(static_cast<SOCKET>(canal.socket), reinterpret_cast<const char*>(datos),
                            largo_para_winsock(largo), 0);
  if (enviados >= 0) {
    contar(canal.datagrama, true, static_cast<std::size_t>(enviados));
    return enviados;
  }
  return canal.datagrama ? largo_para_winsock(largo) : error_de_winsock(MBEDTLS_ERR_SSL_WANT_WRITE);
}

int SesionTls::recibir(void* contexto, unsigned char* destino, std::size_t largo) noexcept {
  auto& canal = *static_cast<Canal*>(contexto);
  if (canal.presupuesto == 0 || canal.lecturas == 0) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }
  --canal.lecturas;
  if (canal.enviar_a != nullptr) {
    const auto copia = canal.entrada.first(std::min(largo, canal.entrada.size()));
    canal.entrada = {};
    if (copia.empty()) {
      return MBEDTLS_ERR_SSL_WANT_READ;
    }
    std::copy(copia.begin(), copia.end(), gsl::make_span(destino, largo).begin());
    canal.total += copia.size();
    contar(true, false, copia.size());
    return largo_para_winsock(copia.size());
  }
  const int recibidos = recv(static_cast<SOCKET>(canal.socket), reinterpret_cast<char*>(destino),
                             largo_para_winsock(std::min(largo, canal.presupuesto)), 0);
  if (canal.datagrama && recibidos <= 0) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }
  if (recibidos < 0) {
    return error_de_winsock(MBEDTLS_ERR_SSL_WANT_READ);
  }
  canal.presupuesto -= static_cast<std::size_t>(recibidos);
  canal.total += static_cast<std::size_t>(recibidos);
  contar(canal.datagrama, false, static_cast<std::size_t>(recibidos));
  return recibidos;
}

void SesionTls::renovar_presupuesto() {
  canal_.presupuesto = kPresupuestoPorLlamada;
  canal_.lecturas = kLecturasPorLlamada;
}

int SesionTls::verificar(void* contexto, mbedtls_x509_crt* certificado, int profundidad,
                         std::uint32_t* banderas) noexcept {
  auto* sesion = static_cast<SesionTls*>(contexto);
  if (profundidad != 0) {
    *banderas = 0;
    return 0;
  }
  sesion->huella_del_otro_ = huella_de_clave(certificado->pk);
  sesion->huella_confirmada_ =
      sesion->esperada_ && sesion->huella_del_otro_ && *sesion->huella_del_otro_ == *sesion->esperada_;
  sesion->huella_distinta_ = !sesion->huella_confirmada_;
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

template <typename Llamada>
int SesionTls::llamar(Llamada&& llamada) {
  renovar_presupuesto();
  int resultado = llamada();
  if (resultado == MBEDTLS_ERR_SSL_WANT_READ && !canal_.entrada.empty()) {
    resultado = llamada();
  }
  canal_.entrada = {};
  return resultado;
}

Paso SesionTls::apreton() {
  if (!ok_) {
    return Paso::error;
  }
  const int resultado = llamar([this] { return mbedtls_ssl_handshake(&ssl_); });
  return resultado == 0 ? Paso::listo : traducir(resultado);
}

Paso SesionTls::leer(gsl::span<std::uint8_t> destino, std::size_t& leidos) {
  leidos = 0;
  const int resultado =
      llamar([&] { return mbedtls_ssl_read(&ssl_, destino.data(), destino.size()); });
  if (resultado > 0) {
    leidos = static_cast<std::size_t>(resultado);
    return Paso::listo;
  }
  return resultado == 0 ? Paso::cerrado : traducir(resultado);
}

Paso SesionTls::escribir(gsl::span<const std::uint8_t> datos, std::size_t& escritos) {
  escritos = 0;
  const int maximo = mbedtls_ssl_get_max_out_record_payload(&ssl_);
  const std::size_t largo =
      canal_.datagrama
          ? datos.size()
          : std::min(datos.size(), a_medias_ != 0 ? a_medias_
                                   : maximo > 0   ? static_cast<std::size_t>(maximo)
                                                  : datos.size());
  const int resultado = llamar([&] { return mbedtls_ssl_write(&ssl_, datos.data(), largo); });
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
  const int resultado = llamar([this] { return mbedtls_ssl_close_notify(&ssl_); });
  return resultado == 0 ? Paso::listo : traducir(resultado);
}

std::string SesionTls::cifrado() const {
  const char* nombre = mbedtls_ssl_get_ciphersuite(&ssl_);
  return nombre != nullptr ? nombre : "";
}

bool SesionTls::es_tls13() const {
  return mbedtls_ssl_get_version_number(&ssl_) == MBEDTLS_SSL_VERSION_TLS1_3;
}

}
