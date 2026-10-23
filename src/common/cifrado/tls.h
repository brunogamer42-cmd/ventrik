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
class ConfigTls {
 public:
  static std::unique_ptr<ConfigTls> para_servidor(IdentidadServidor& identidad);
  static std::unique_ptr<ConfigTls> para_servidor(mbedtls_x509_crt* certificado,
                                                  mbedtls_pk_context* clave);
  static std::unique_ptr<ConfigTls> para_programa();
  ConfigTls();
  ~ConfigTls();
  ConfigTls(const ConfigTls&) = delete;
  ConfigTls& operator=(const ConfigTls&) = delete;

  mbedtls_ssl_config* crudo() { return &config_; }
  const mbedtls_ssl_config* crudo() const { return &config_; }

 private:
  mbedtls_ssl_config config_;
};

bool procesador_acelera_aes();

enum class Paso { listo, esperar_lectura, esperar_escritura, cerrado, error };

class SesionTls {
 public:
  SesionTls(const ConfigTls& config, std::uintptr_t conexion);
  ~SesionTls();
  SesionTls(const SesionTls&) = delete;
  SesionTls& operator=(const SesionTls&) = delete;

  bool ok() const { return ok_; }

  void exigir_huella(const Huella& esperada);
  bool huella_confirmada() const { return huella_confirmada_; }
  bool huella_distinta() const { return huella_distinta_; }

  Paso apreton();
  Paso leer(gsl::span<std::uint8_t> destino, std::size_t& leidos);
  Paso escribir(gsl::span<const std::uint8_t> datos, std::size_t& escritos);
  Paso cerrar();

  int ultimo_error() const { return ultimo_error_; }
  std::size_t bytes_recibidos() const { return canal_.total; }
  bool hay_datos_guardados() const { return mbedtls_ssl_check_pending(&ssl_) != 0; }
  std::string cifrado() const;
  bool es_tls13() const;

 private:
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
  std::size_t a_medias_ = 0;
};

}
