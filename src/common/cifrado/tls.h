#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <gsl/gsl>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cookie.h>
#include <mbedtls/timing.h>

#include "common/cifrado/huella.h"
#include "common/cifrado/identidad.h"
#include "common/red/red.h"

namespace ventrik::cifrado {
class SesionTls;

using EnviarDatagrama = std::function<void(const red::Direccion&, gsl::span<const std::uint8_t>)>;
using ElegirClave =
    std::function<gsl::span<const std::uint8_t>(SesionTls&, gsl::span<const std::uint8_t>)>;

class ConfigTls {
 public:
  static std::unique_ptr<ConfigTls> para_servidor(IdentidadServidor& identidad);
  static std::unique_ptr<ConfigTls> para_servidor(mbedtls_x509_crt* certificado,
                                                  mbedtls_pk_context* clave);
  static std::unique_ptr<ConfigTls> para_programa();
  static std::unique_ptr<ConfigTls> para_voz_servidor(ElegirClave elegir);
  static std::unique_ptr<ConfigTls> para_voz_programa(gsl::span<const std::uint8_t> identidad,
                                                      gsl::span<const std::uint8_t> clave);
  ConfigTls();
  ~ConfigTls();
  ConfigTls(const ConfigTls&) = delete;
  ConfigTls& operator=(const ConfigTls&) = delete;

  mbedtls_ssl_config* crudo() { return &config_; }
  const mbedtls_ssl_config* crudo() const { return &config_; }
  bool datagrama() const { return datagrama_; }

 private:
  static int elegir_psk(void* contexto, mbedtls_ssl_context* ssl, const unsigned char* identidad,
                        std::size_t largo) noexcept;

  mbedtls_ssl_config config_;
  mbedtls_ssl_cookie_ctx cookies_;
  ElegirClave elegir_;
  bool datagrama_ = false;
};

bool procesador_acelera_aes();

enum class Paso { listo, esperar_lectura, esperar_escritura, cerrado, error };

class SesionTls {
 public:
  SesionTls(const ConfigTls& config, std::uintptr_t conexion);
  SesionTls(const ConfigTls& config, const EnviarDatagrama& enviar, const red::Direccion& otro);
  SesionTls(const ConfigTls& config, EnviarDatagrama&& enviar, const red::Direccion& otro) = delete;
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

  void entregar(gsl::span<const std::uint8_t> datagrama) { canal_.entrada = datagrama; }
  bool reiniciar(const red::Direccion& otro);
  bool exportar(gsl::span<const std::uint8_t> contexto, gsl::span<std::uint8_t> salida);
  std::size_t datagramas_enviados() const { return canal_.enviados; }
  const red::Direccion& otro_lado() const { return canal_.otro; }
  void marcar(std::uint32_t valor) { marca_ = valor; }
  std::uint32_t marca() const { return marca_; }

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
    bool datagrama = false;
    const EnviarDatagrama* enviar_a = nullptr;
    red::Direccion otro;
    gsl::span<const std::uint8_t> entrada;
    std::size_t enviados = 0;
  };

  void iniciar(const ConfigTls& config);
  static int enviar(void* contexto, const unsigned char* datos, std::size_t largo) noexcept;
  static int recibir(void* contexto, unsigned char* destino, std::size_t largo) noexcept;
  static int verificar(void* contexto, mbedtls_x509_crt* certificado, int profundidad,
                       std::uint32_t* banderas) noexcept;
  Paso traducir(int resultado);
  template <typename Llamada>
  int llamar(Llamada&& llamada);
  void renovar_presupuesto();

  mbedtls_ssl_context ssl_;
  mbedtls_timing_delay_context temporizador_{};
  Canal canal_;
  std::optional<Huella> esperada_;
  bool huella_confirmada_ = false;
  bool huella_distinta_ = false;
  bool ok_ = false;
  int ultimo_error_ = 0;
  std::size_t a_medias_ = 0;
  std::uint32_t marca_ = 0;
};

}
