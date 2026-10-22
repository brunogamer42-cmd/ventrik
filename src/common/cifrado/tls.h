#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <gsl/gsl>
#include <mbedtls/ssl.h>

#include "common/cifrado/huella.h"
#include "common/cifrado/identidad.h"

namespace ventrik::cifrado {

// TLS 1.3 solo, con AES-GCM o ChaCha20-Poly1305, X25519 o P-256 y firmas ECDSA P-256.
class ConfigTls {
 public:
  // La identidad tiene que vivir más que la configuración.
  static std::unique_ptr<ConfigTls> para_servidor(IdentidadServidor& identidad);
  // Las pruebas la usan con un certificado y una clave que no se corresponden.
  static std::unique_ptr<ConfigTls> para_servidor(mbedtls_x509_crt* certificado,
                                                  mbedtls_pk_context* clave);
  static std::unique_ptr<ConfigTls> para_programa();
  ConfigTls();
  ~ConfigTls();
  ConfigTls(const ConfigTls&) = delete;
  ConfigTls& operator=(const ConfigTls&) = delete;

  // Para las pruebas que arman un servidor o un programa tramposo.
  mbedtls_ssl_config* crudo() { return &config_; }
  const mbedtls_ssl_config* crudo() const { return &config_; }

 private:
  mbedtls_ssl_config config_;
};

// Sin AES-NI (como en la HP 2133), ChaCha20 es más rápido que AES-GCM.
bool procesador_acelera_aes();

enum class Paso { listo, esperar_lectura, esperar_escritura, cerrado, error };

class SesionTls {
 public:
  // `conexion` es un SOCKET de Winsock sin trabarse; tiene que vivir más que la sesión.
  SesionTls(const ConfigTls& config, std::uintptr_t conexion);
  ~SesionTls();
  SesionTls(const SesionTls&) = delete;
  SesionTls& operator=(const SesionTls&) = delete;

  bool ok() const { return ok_; }

  // Solo el programa: el apretón de manos se corta si la clave del servidor no tiene esta huella.
  void exigir_huella(const Huella& esperada);
  bool huella_confirmada() const { return huella_confirmada_; }
  bool huella_distinta() const { return huella_distinta_; }

  Paso apreton();
  Paso leer(gsl::span<std::uint8_t> destino, std::size_t& leidos);
  Paso escribir(gsl::span<const std::uint8_t> datos, std::size_t& escritos);
  Paso cerrar();

  int ultimo_error() const { return ultimo_error_; }
  std::size_t bytes_recibidos() const { return canal_.total; }
  std::string cifrado() const;
  bool es_tls13() const;

 private:
  // Topes por llamada: Mbed TLS descarta sin límite ciertos registros chicos y trabaría el hilo.
  struct Canal {
    std::uintptr_t socket = 0;
    std::size_t presupuesto = 0;
    int lecturas = 0;
    std::size_t total = 0;
  };

  static int enviar(void* contexto, const unsigned char* datos, std::size_t largo) noexcept;
  static int recibir(void* contexto, unsigned char* destino, std::size_t largo) noexcept;
  static int verificar(void* contexto, mbedtls_x509_crt* certificado, int profundidad,
                       std::uint32_t* banderas) noexcept;
  Paso traducir(int resultado);
  void renovar_presupuesto();

  mbedtls_ssl_context ssl_;
  Canal canal_;
  std::optional<Huella> esperada_;
  bool huella_confirmada_ = false;
  bool huella_distinta_ = false;
  bool ok_ = false;
  int ultimo_error_ = 0;
};

}  // namespace ventrik::cifrado
