#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <mbedtls/ssl.h>

#include "common/cifrado/tls.h"
#include "common/red/red.h"
#include "servidor_de_prueba.h"
#include <doctest/doctest.h>

using ventrik::cifrado::ConfigTls;
using ventrik::cifrado::Paso;
using ventrik::cifrado::SesionTls;
using namespace prueba;

namespace {
const std::array<std::uint8_t, 16> kIdentidad = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
const std::array<std::uint8_t, 32> kClave = {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
                                             9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
const int kAesPrimero[] = {MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256,
                           MBEDTLS_TLS_PSK_WITH_CHACHA20_POLY1305_SHA256, 0};
const int kChachaPrimero[] = {MBEDTLS_TLS_PSK_WITH_CHACHA20_POLY1305_SHA256,
                              MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256, 0};
const int kSoloCbc[] = {MBEDTLS_TLS_PSK_WITH_AES_128_CBC_SHA256, 0};

struct Banco {
  ventrik::red::Socket udp_servidor = ventrik::red::abrir_udp({kLocal, 0});
  ventrik::red::Direccion servidor = *ventrik::red::direccion_local(udp_servidor);
  ventrik::red::Socket udp_programa = ventrik::red::conectar_udp(servidor);
  ventrik::red::Direccion programa = *ventrik::red::direccion_local(udp_programa);
  std::vector<std::size_t> enviados;
  ventrik::cifrado::EnviarDatagrama enviar = [this](const ventrik::red::Direccion& a,
                                                    gsl::span<const std::uint8_t> datos) {
    enviados.push_back(datos.size());
    ventrik::red::enviar_a(udp_servidor, a, datos);
  };
  std::unique_ptr<ConfigTls> config_servidor =
      ConfigTls::para_voz_servidor([](SesionTls&, gsl::span<const std::uint8_t> identidad) {
        return std::equal(identidad.begin(), identidad.end(), kIdentidad.begin(), kIdentidad.end())
                   ? gsl::span<const std::uint8_t>(kClave)
                   : gsl::span<const std::uint8_t>();
      });
  std::array<std::uint8_t, 2048> dato{};

  std::unique_ptr<ConfigTls> config_programa(const int* cifrados,
                                             gsl::span<const std::uint8_t> clave = kClave) {
    auto config = ConfigTls::para_voz_programa(kIdentidad, clave);
    REQUIRE(config);
    mbedtls_ssl_conf_ciphersuites(config->crudo(), cifrados);
    return config;
  }

  std::optional<gsl::span<const std::uint8_t>> llegada() {
    const auto recibido = ventrik::red::recibir_de(udp_servidor, dato);
    if (!recibido) {
      return std::nullopt;
    }
    return gsl::span<const std::uint8_t>(dato).first(recibido->largo);
  }

  bool saludar(SesionTls& cliente, SesionTls& lado_servidor) {
    bool listo_programa = false;
    bool listo_servidor = false;
    bool fallo = false;
    esperar_que([&] {
      if (!listo_programa) {
        const auto paso = cliente.apreton();
        listo_programa = paso == Paso::listo;
        fallo = fallo || paso == Paso::error || paso == Paso::cerrado;
      }
      while (const auto datos = llegada()) {
        lado_servidor.entregar(*datos);
        const auto paso = lado_servidor.apreton();
        listo_servidor = listo_servidor || paso == Paso::listo;
        if (paso == Paso::error &&
            lado_servidor.ultimo_error() == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
          REQUIRE(lado_servidor.reiniciar(programa));
        } else {
          fallo = fallo || paso == Paso::error || paso == Paso::cerrado;
        }
      }
      return fallo || (listo_programa && listo_servidor);
    });
    return !fallo && listo_programa && listo_servidor;
  }
};

void enviar_texto(SesionTls& sesion, const std::string& texto) {
  std::size_t escritos = 0;
  REQUIRE(sesion.escribir(gsl::make_span(reinterpret_cast<const std::uint8_t*>(texto.data()),
                                         texto.size()),
                          escritos) == Paso::listo);
  REQUIRE(escritos == texto.size());
}

std::string leer_texto(SesionTls& sesion) {
  std::array<std::uint8_t, 2048> leido{};
  std::size_t largo = 0;
  return sesion.leer(leido, largo) == Paso::listo ? std::string(leido.begin(), leido.begin() + largo)
                                                  : std::string();
}
}

TEST_CASE("La voz arma DTLS 1.2 con clave compartida y respeta el orden de cifrados del programa") {
  for (const auto& [cifrados, nombre] : {std::make_pair(kAesPrimero, "AES-128-GCM"),
                                         std::make_pair(kChachaPrimero, "CHACHA20")}) {
    Banco banco;
    REQUIRE(banco.config_servidor);
    const auto config = banco.config_programa(cifrados);
    SesionTls programa(*config, banco.udp_programa.get());
    SesionTls servidor(*banco.config_servidor, banco.enviar, banco.programa);
    REQUIRE(banco.saludar(programa, servidor));
    CAPTURE(servidor.cifrado());
    CHECK(servidor.cifrado().find(nombre) != std::string::npos);
    CHECK(programa.cifrado() == servidor.cifrado());
    enviar_texto(programa, "hola");
    REQUIRE(esperar_que([&] {
      const auto datos = banco.llegada();
      return datos && (servidor.entregar(*datos), leer_texto(servidor) == "hola");
    }));
    enviar_texto(servidor, "chau");
    CHECK(esperar_que([&] { return leer_texto(programa) == "chau"; }));
  }
}

TEST_CASE("Una clave equivocada, una identidad desconocida o un cifrado débil no arman la voz") {
  const std::array<std::uint8_t, 32> otra_clave{};
  Banco banco;
  const auto equivocada = banco.config_programa(kAesPrimero, otra_clave);
  SesionTls programa(*equivocada, banco.udp_programa.get());
  SesionTls servidor(*banco.config_servidor, banco.enviar, banco.programa);
  CHECK_FALSE(banco.saludar(programa, servidor));
  CHECK(servidor.ultimo_error() == MBEDTLS_ERR_SSL_INVALID_MAC);

  Banco otro;
  auto desconocida = ConfigTls::para_voz_programa(std::array<std::uint8_t, 16>{}, kClave);
  REQUIRE(desconocida);
  SesionTls programa2(*desconocida, otro.udp_programa.get());
  SesionTls servidor2(*otro.config_servidor, otro.enviar, otro.programa);
  CHECK_FALSE(otro.saludar(programa2, servidor2));
  CHECK(servidor2.ultimo_error() == MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY);

  Banco debil;
  const auto cbc = debil.config_programa(kSoloCbc);
  SesionTls programa3(*cbc, debil.udp_programa.get());
  SesionTls servidor3(*debil.config_servidor, debil.enviar, debil.programa);
  CHECK_FALSE(debil.saludar(programa3, servidor3));
  CHECK(servidor3.ultimo_error() == MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE);
}

TEST_CASE("Sin cookie, el servidor contesta algo más chico que lo que recibió") {
  Banco banco;
  const auto config = banco.config_programa(kAesPrimero);
  SesionTls programa(*config, banco.udp_programa.get());
  SesionTls servidor(*banco.config_servidor, banco.enviar, banco.programa);
  REQUIRE(programa.apreton() == Paso::esperar_lectura);
  REQUIRE(ventrik::red::esperar(banco.udp_servidor, true, false, 2s) == ventrik::red::Espera::lista);
  const auto hola = banco.llegada();
  REQUIRE(hola);
  servidor.entregar(*hola);
  CHECK(servidor.apreton() == Paso::error);
  CHECK(servidor.ultimo_error() == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED);
  REQUIRE(banco.enviados.size() == 1);
  CHECK(banco.enviados[0] <= hola->size());
}

TEST_CASE("Un registro repetido o basura no pasan y la voz sigue andando") {
  Banco banco;
  const auto config = banco.config_programa(kAesPrimero);
  SesionTls programa(*config, banco.udp_programa.get());
  SesionTls servidor(*banco.config_servidor, banco.enviar, banco.programa);
  REQUIRE(banco.saludar(programa, servidor));
  enviar_texto(programa, "uno");
  std::vector<std::uint8_t> copia;
  REQUIRE(esperar_que([&] {
    const auto datos = banco.llegada();
    if (!datos) {
      return false;
    }
    copia.assign(datos->begin(), datos->end());
    servidor.entregar(copia);
    return leer_texto(servidor) == "uno";
  }));
  servidor.entregar(copia);
  CHECK(leer_texto(servidor).empty());
  const std::vector<std::uint8_t> basura(100, 0x17);
  servidor.entregar(basura);
  CHECK(leer_texto(servidor).empty());
  enviar_texto(programa, "dos");
  REQUIRE(esperar_que([&] {
    const auto datos = banco.llegada();
    return datos && (servidor.entregar(*datos), leer_texto(servidor) == "dos");
  }));
}

TEST_CASE("Un datagrama vacío no corta la voz del programa") {
  Banco banco;
  const auto config = banco.config_programa(kAesPrimero);
  SesionTls programa(*config, banco.udp_programa.get());
  SesionTls servidor(*banco.config_servidor, banco.enviar, banco.programa);
  REQUIRE(banco.saludar(programa, servidor));
  REQUIRE(ventrik::red::enviar_a(banco.udp_servidor, banco.programa, {}));
  REQUIRE(ventrik::red::esperar(banco.udp_programa, true, false, 2s) == ventrik::red::Espera::lista);
  std::array<std::uint8_t, 64> leido{};
  std::size_t largo = 0;
  CHECK(programa.leer(leido, largo) == Paso::esperar_lectura);
  enviar_texto(servidor, "sigue");
  CHECK(esperar_que([&] { return leer_texto(programa) == "sigue"; }));
}

TEST_CASE("Una cookie dada a una dirección no sirve para otra") {
  Banco banco;
  const auto config = banco.config_programa(kAesPrimero);
  SesionTls programa(*config, banco.udp_programa.get());
  SesionTls servidor(*banco.config_servidor, banco.enviar, banco.programa);
  REQUIRE(programa.apreton() == Paso::esperar_lectura);
  REQUIRE(ventrik::red::esperar(banco.udp_servidor, true, false, 2s) == ventrik::red::Espera::lista);
  servidor.entregar(*banco.llegada());
  REQUIRE(servidor.apreton() == Paso::error);
  REQUIRE(esperar_que([&] {
    programa.apreton();
    return ventrik::red::esperar(banco.udp_servidor, true, false, 10ms) ==
           ventrik::red::Espera::lista;
  }));
  const auto con_cookie = banco.llegada();
  REQUIRE(con_cookie);
  const std::vector<std::uint8_t> copia(con_cookie->begin(), con_cookie->end());
  REQUIRE(servidor.reiniciar({kLocal, 1}));
  servidor.entregar(copia);
  CHECK(servidor.apreton() == Paso::error);
  CHECK(servidor.ultimo_error() == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED);
  REQUIRE(servidor.reiniciar(banco.programa));
  servidor.entregar(copia);
  CHECK(servidor.apreton() == Paso::esperar_lectura);
}

TEST_CASE("Con un solo algoritmo permitido, la voz que ofrece solo el otro no se arma") {
  using ventrik::Algoritmo;
  for (const auto& [servidor_permite, programa_ofrece, nombre] :
       {std::make_tuple(Algoritmo::solo_chacha20, Algoritmo::solo_aes_gcm, ""),
        std::make_tuple(Algoritmo::solo_aes_gcm, Algoritmo::solo_chacha20, ""),
        std::make_tuple(Algoritmo::solo_chacha20, Algoritmo::automatico, "CHACHA20"),
        std::make_tuple(Algoritmo::solo_aes_gcm, Algoritmo::automatico, "AES")}) {
    Banco banco;
    const auto config_servidor = ConfigTls::para_voz_servidor(
        [](SesionTls&, gsl::span<const std::uint8_t>) { return gsl::span<const std::uint8_t>(kClave); }, servidor_permite);
    const auto config_programa = ConfigTls::para_voz_programa(kIdentidad, kClave, programa_ofrece);
    REQUIRE(config_servidor);
    REQUIRE(config_programa);
    SesionTls programa(*config_programa, banco.udp_programa.get());
    SesionTls servidor(*config_servidor, banco.enviar, banco.programa);
    const bool armada = banco.saludar(programa, servidor);
    CHECK(armada == (std::string(nombre) != ""));
    if (armada) {
      CHECK(servidor.cifrado().find(nombre) != std::string::npos);
    }
  }
}