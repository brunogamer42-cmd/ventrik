/*
 * Ajustes de Ventrik sobre la configuración por defecto de Mbed TLS 3.6
 * (third_party/mbedtls/include/mbedtls/mbedtls_config.h). Mbed TLS lo
 * carga con MBEDTLS_USER_CONFIG_FILE (ver third_party/CMakeLists.txt).
 *
 * Las versiones de TLS y los algoritmos permitidos no se tocan acá: los
 * elige src/common/cifrado/tls.cpp en cada conexión.
 */

/* Sin control de hilos, el generador aleatorio podría repetir números (candados: threading_alt.h). */
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_ALT

#if defined(VENTRIK_MBEDTLS_RTLGENRANDOM)
/*
 * Mbed TLS saca los números aleatorios de BCryptGenRandom, que no existe en
 * Windows XP. En la versión clásica se apaga esa fuente y Mbed TLS los pide
 * a mbedtls_hardware_poll (src/common/entropia_xp.cpp), que usa
 * RtlGenRandom. Mbed TLS la registra como fuente fuerte.
 */
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT

/*
 * inet_pton no existe en XP. Con esto, los certificados (x509_crt.c) usan
 * la versión propia de Mbed TLS en lugar de la de Windows.
 */
#define MBEDTLS_TEST_SW_INET_PTON
#endif
