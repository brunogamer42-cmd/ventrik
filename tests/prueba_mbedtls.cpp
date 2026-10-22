// Para poder mirar qué fuentes de números aleatorios registró Mbed TLS.
// No cambia la forma de las estructuras: solo deja ver campos privados.
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include <array>
#include <cstddef>
#include <initializer_list>
#include <string>

#include <doctest/doctest.h>
#include <mbedtls/aes.h>
#include <mbedtls/chachapoly.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/gcm.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl_ciphersuites.h>
#include <mbedtls/version.h>
#include <mbedtls/x509.h>
#include <psa/crypto.h>

#if defined(VENTRIK_CLASICA)
#include "common/entropia_xp.h"
#endif

#if !defined(MBEDTLS_SSL_PROTO_TLS1_3) || !defined(MBEDTLS_SSL_PROTO_DTLS)
#error "Mbed TLS tiene que traer TLS 1.3 y DTLS"
#endif
#if !defined(MBEDTLS_THREADING_C)
#error "Mbed TLS tiene que tener control de hilos: el servidor lo usa desde varios"
#endif

// De dónde salen los números aleatorios: en la clásica, solo de
// RtlGenRandom; en la moderna, solo de BCryptGenRandom. Nada más.
#if defined(VENTRIK_CLASICA)
#if !defined(MBEDTLS_NO_PLATFORM_ENTROPY) || !defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
#error "En la clásica, Mbed TLS tiene que usar mbedtls_hardware_poll (RtlGenRandom)"
#endif
#else
#if defined(MBEDTLS_NO_PLATFORM_ENTROPY) || defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
#error "En la moderna, Mbed TLS tiene que usar BCryptGenRandom"
#endif
#endif
#if defined(MBEDTLS_ENTROPY_NV_SEED) || defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG) || \
    defined(MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES)
#error "Mbed TLS no tiene que tomar números aleatorios de ningún otro lado"
#endif

TEST_CASE("Mbed TLS es la versión 3.6.7") {
  CHECK(std::string(MBEDTLS_VERSION_STRING) == "3.6.7");
}

TEST_CASE("Mbed TLS pasa sus pruebas oficiales de cifrado") {
  CHECK(mbedtls_sha256_self_test(0) == 0);
  CHECK(mbedtls_aes_self_test(0) == 0);
  CHECK(mbedtls_gcm_self_test(0) == 0);
  CHECK(mbedtls_chachapoly_self_test(0) == 0);
  CHECK(mbedtls_ctr_drbg_self_test(0) == 0);
}

TEST_CASE("Mbed TLS trae los cifrados que pide el diseño") {
  for (const char* nombre : {"TLS1-3-AES-128-GCM-SHA256", "TLS1-3-AES-256-GCM-SHA384",
                             "TLS1-3-CHACHA20-POLY1305-SHA256",
                             "TLS-ECDHE-ECDSA-WITH-AES-128-GCM-SHA256",
                             "TLS-ECDHE-ECDSA-WITH-CHACHA20-POLY1305-SHA256"}) {
    CAPTURE(nombre);
    CHECK(mbedtls_ssl_ciphersuite_from_string(nombre) != nullptr);
  }
}

TEST_CASE("Mbed TLS tiene una sola fuente de números aleatorios, y fuerte") {
  mbedtls_entropy_context entropia;
  mbedtls_entropy_init(&entropia);
  CHECK(entropia.source_count == 1);
  CHECK(entropia.source[0].strong == MBEDTLS_ENTROPY_SOURCE_STRONG);
  CHECK(entropia.source[0].threshold == 32u);  // pide 32 bytes antes de usarla
#if defined(VENTRIK_CLASICA)
  CHECK(entropia.source[0].f_source == &mbedtls_hardware_poll);  // RtlGenRandom
#else
  CHECK(entropia.source[0].f_source == &mbedtls_platform_entropy_poll);  // BCryptGenRandom
#endif
  mbedtls_entropy_free(&entropia);
}

TEST_CASE("Mbed TLS lee direcciones IP de los certificados") {
  // En la clásica usa su propia versión de inet_pton, que XP no tiene. Con
  // esta prueba, ese código queda dentro del .exe y check.ps1 lo revisa.
  std::array<unsigned char, 16> direccion{};
  CHECK(mbedtls_x509_crt_parse_cn_inet_pton("192.168.0.1", direccion.data()) == 4u);
  CHECK(direccion[0] == 192);
  CHECK(direccion[3] == 1);
  CHECK(mbedtls_x509_crt_parse_cn_inet_pton("::1", direccion.data()) == 16u);
  CHECK(direccion[15] == 1);
  CHECK(mbedtls_x509_crt_parse_cn_inet_pton("no es una IP", direccion.data()) == 0u);
}

TEST_CASE("Mbed TLS obtiene números aleatorios del sistema") {
  // En la clásica salen de RtlGenRandom; en la moderna, de BCryptGenRandom.
  CHECK(mbedtls_entropy_self_test(0) == 0);
  REQUIRE(psa_crypto_init() == PSA_SUCCESS);
  std::array<unsigned char, 32> primero{};
  std::array<unsigned char, 32> segundo{};
  CHECK(psa_generate_random(primero.data(), primero.size()) == PSA_SUCCESS);
  CHECK(psa_generate_random(segundo.data(), segundo.size()) == PSA_SUCCESS);
  CHECK(primero != segundo);
}

#if defined(VENTRIK_CLASICA)

TEST_CASE("en la versión clásica, RtlGenRandom da números aleatorios") {
  std::array<unsigned char, 64> primero{};
  std::array<unsigned char, 64> segundo{};
  std::size_t obtenidos = 0;
  REQUIRE(mbedtls_hardware_poll(nullptr, primero.data(), primero.size(), &obtenidos) == 0);
  CHECK(obtenidos == primero.size());
  REQUIRE(mbedtls_hardware_poll(nullptr, segundo.data(), segundo.size(), &obtenidos) == 0);
  CHECK(obtenidos == segundo.size());
  CHECK(primero != segundo);
}

TEST_CASE("en la versión clásica, mbedtls_hardware_poll atiende bien los casos borde") {
  std::array<unsigned char, 16> buffer{};
  std::size_t obtenidos = 99;
  CHECK(mbedtls_hardware_poll(nullptr, buffer.data(), 0, &obtenidos) == 0);
  CHECK(obtenidos == 0);
  CHECK(mbedtls_hardware_poll(nullptr, buffer.data(), buffer.size(), nullptr) != 0);
  obtenidos = 99;
  CHECK(mbedtls_hardware_poll(nullptr, nullptr, buffer.size(), &obtenidos) != 0);
  CHECK(obtenidos == 0);
}

#endif
