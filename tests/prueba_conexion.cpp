#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <winsock2.h>

#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include "client/conexion.h"
#include "common/cifrado/identidad.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/tls.h"
#include "common/red/red.h"
#include "server/servidor.h"
#include <doctest/doctest.h>

using namespace std::chrono_literals;
using ventrik::cifrado::ConfigTls;
using ventrik::cifrado::IdentidadServidor;
using ventrik::cliente::EstadoConexion;
using ventrik::servidor::Evento;

namespace {

constexpr std::uint32_t kLocal = 0x7F000001;

IdentidadServidor identidad_nueva() {
  auto identidad = IdentidadServidor::crear_nueva();
  REQUIRE(identidad);
  return std::move(*identidad);
}

bool esperar_que(const std::function<bool()>& condicion, std::chrono::milliseconds limite = 5s) {
  const auto hasta = std::chrono::steady_clock::now() + limite;
  while (!condicion()) {
    if (std::chrono::steady_clock::now() >= hasta) {
      return false;
    }
    std::this_thread::sleep_for(10ms);
  }
  return true;
}

// true si el otro lado cierra antes de `limite`.
bool se_cierra(const ventrik::red::Socket& canal, std::chrono::milliseconds limite) {
  const auto hasta = std::chrono::steady_clock::now() + limite;
  while (std::chrono::steady_clock::now() < hasta) {
    const auto falta = std::chrono::duration_cast<std::chrono::milliseconds>(
        hasta - std::chrono::steady_clock::now());
    if (ventrik::red::esperar(canal, true, false, falta) != ventrik::red::Espera::lista) {
      continue;
    }
    std::array<char, 256> basura{};
    const int leidos = recv(static_cast<SOCKET>(canal.get()), basura.data(),
                            static_cast<int>(basura.size()), 0);
    if (leidos == 0 || (leidos < 0 && WSAGetLastError() != WSAEWOULDBLOCK)) {
      return true;
    }
  }
  return false;
}

bool enviar_todo(const ventrik::red::Socket& canal, const std::string& datos) {
  return send(static_cast<SOCKET>(canal.get()), datos.data(), static_cast<int>(datos.size()), 0) ==
         static_cast<int>(datos.size());
}

class ServidorDePrueba {
 public:
  explicit ServidorDePrueba(ventrik::servidor::Opciones opciones = {})
      : identidad_(identidad_nueva()) {
    opciones.escuchar = ventrik::red::Direccion{kLocal, 0};
    servidor_ = std::make_unique<ventrik::servidor::Servidor>(
        identidad_, opciones,
        [this](Evento evento, const ventrik::red::Direccion&) { contar(evento); });
    REQUIRE(servidor_->escuchar());
    hilo_ = std::thread([this] { servidor_->correr(seguir_); });
  }
  ~ServidorDePrueba() {
    seguir_ = false;
    hilo_.join();
  }
  ServidorDePrueba(const ServidorDePrueba&) = delete;
  ServidorDePrueba& operator=(const ServidorDePrueba&) = delete;

  ventrik::red::Direccion direccion() const { return {kLocal, servidor_->puerto()}; }
  const ventrik::cifrado::Huella& huella() const { return identidad_.huella(); }
  int cuantos(Evento evento) const { return eventos_[static_cast<std::size_t>(evento)].load(); }

 private:
  void contar(Evento evento) { ++eventos_[static_cast<std::size_t>(evento)]; }

  IdentidadServidor identidad_;
  std::array<std::atomic<int>, 16> eventos_{};
  std::atomic<bool> seguir_{true};
  std::unique_ptr<ventrik::servidor::Servidor> servidor_;
  std::thread hilo_;
};

// Atiende una sola conexión: o con una configuración TLS armada por la prueba, o mandando bytes fijos.
class ServidorFalso {
 public:
  explicit ServidorFalso(std::unique_ptr<ConfigTls> config) : config_(std::move(config)) {
    REQUIRE(config_);
    empezar();
  }
  explicit ServidorFalso(std::string respuesta_fija) : respuesta_fija_(std::move(respuesta_fija)) {
    empezar();
  }
  ~ServidorFalso() { hilo_.join(); }
  ServidorFalso(const ServidorFalso&) = delete;
  ServidorFalso& operator=(const ServidorFalso&) = delete;

  ventrik::red::Direccion direccion() const { return {kLocal, puerto_}; }
  bool completo() const { return completo_.load(); }

 private:
  void empezar() {
    escucha_ = ventrik::red::escuchar(ventrik::red::Direccion{kLocal, 0});
    REQUIRE(escucha_.valido());
    puerto_ = ventrik::red::direccion_local(escucha_)->puerto;
    hilo_ = std::thread([this] { atender(); });
  }

  void atender() {
    const auto hasta = std::chrono::steady_clock::now() + 5s;
    std::optional<std::pair<ventrik::red::Socket, ventrik::red::Direccion>> aceptada;
    while (!aceptada && std::chrono::steady_clock::now() < hasta) {
      bool fallo = false;
      ventrik::red::esperar(escucha_, true, false, 100ms);
      aceptada = ventrik::red::aceptar(escucha_, fallo);
    }
    if (!aceptada) {
      return;
    }
    const auto& canal = aceptada->first;
    if (!config_) {
      ventrik::red::esperar(canal, true, false, 2s);
      std::array<char, 2048> hola{};
      recv(static_cast<SOCKET>(canal.get()), hola.data(), static_cast<int>(hola.size()), 0);
      enviar_todo(canal, respuesta_fija_);
      se_cierra(canal, 3s);
      return;
    }
    ventrik::cifrado::SesionTls tls(*config_, canal.get());
    while (std::chrono::steady_clock::now() < hasta) {
      const auto paso = tls.apreton();
      if (paso == ventrik::cifrado::Paso::listo) {
        completo_ = true;
        return;
      }
      if (paso != ventrik::cifrado::Paso::esperar_lectura &&
          paso != ventrik::cifrado::Paso::esperar_escritura) {
        return;
      }
      ventrik::red::esperar(canal, paso == ventrik::cifrado::Paso::esperar_lectura,
                            paso == ventrik::cifrado::Paso::esperar_escritura, 100ms);
    }
  }

  std::unique_ptr<ConfigTls> config_;
  std::string respuesta_fija_;
  ventrik::red::Socket escucha_;
  std::uint16_t puerto_ = 0;
  std::atomic<bool> completo_{false};
  std::thread hilo_;
};

std::unique_ptr<ConfigTls> config_ajustada(IdentidadServidor& identidad,
                                           void (*ajustar)(mbedtls_ssl_config*)) {
  auto config = ConfigTls::para_servidor(identidad);
  REQUIRE(config);
  ajustar(config->crudo());
  return config;
}

// Un programa tramposo: devuelve 0 si el servidor lo dejó terminar el apretón de manos, o el error.
int apreton_tramposo(const ventrik::red::Direccion& destino, const ventrik::cifrado::Huella& huella,
                     void (*ajustar)(mbedtls_ssl_config*)) {
  auto config = ConfigTls::para_programa();
  REQUIRE(config);
  ajustar(config->crudo());
  const auto canal = ventrik::red::conectar(destino, 2s);
  REQUIRE(canal.valido());
  ventrik::cifrado::SesionTls tls(*config, canal.get());
  tls.exigir_huella(huella);
  const auto hasta = std::chrono::steady_clock::now() + 5s;
  for (;;) {
    const auto paso = tls.apreton();
    if (paso == ventrik::cifrado::Paso::listo) {
      return 0;
    }
    if ((paso != ventrik::cifrado::Paso::esperar_lectura &&
         paso != ventrik::cifrado::Paso::esperar_escritura) ||
        std::chrono::steady_clock::now() >= hasta) {
      return tls.ultimo_error();
    }
    ventrik::red::esperar(canal, paso == ventrik::cifrado::Paso::esperar_lectura,
                          paso == ventrik::cifrado::Paso::esperar_escritura, 100ms);
  }
}

const int kSoloTls12[] = {MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0};
const int kSoloCcm8[] = {MBEDTLS_TLS1_3_AES_128_CCM_8_SHA256, 0};

void solo_tls12(mbedtls_ssl_config* config) {
  mbedtls_ssl_conf_min_tls_version(config, MBEDTLS_SSL_VERSION_TLS1_2);
  mbedtls_ssl_conf_max_tls_version(config, MBEDTLS_SSL_VERSION_TLS1_2);
  mbedtls_ssl_conf_ciphersuites(config, kSoloTls12);
}

void solo_ccm8(mbedtls_ssl_config* config) {
  mbedtls_ssl_conf_ciphersuites(config, kSoloCcm8);
}

// Un certificado de autoridad con la clave pública de `publica`, firmado con `firmante`.
std::vector<unsigned char> autoridad_con_clave_ajena(mbedtls_pk_context& publica,
                                                     mbedtls_pk_context& firmante) {
  mbedtls_x509write_cert escritor;
  mbedtls_x509write_crt_init(&escritor);
  mbedtls_x509write_crt_set_version(&escritor, MBEDTLS_X509_CRT_VERSION_3);
  mbedtls_x509write_crt_set_md_alg(&escritor, MBEDTLS_MD_SHA256);
  mbedtls_x509write_crt_set_subject_key(&escritor, &publica);
  mbedtls_x509write_crt_set_issuer_key(&escritor, &firmante);
  unsigned char serie[] = {0x42};
  std::vector<unsigned char> der(2048);
  const bool armado =
      mbedtls_x509write_crt_set_subject_name(&escritor, "CN=Ventrik") == 0 &&
      mbedtls_x509write_crt_set_issuer_name(&escritor, "CN=Ventrik") == 0 &&
      mbedtls_x509write_crt_set_serial_raw(&escritor, serie, sizeof(serie)) == 0 &&
      mbedtls_x509write_crt_set_validity(&escritor, "20000101000000", "20991231235959") == 0 &&
      mbedtls_x509write_crt_set_basic_constraints(&escritor, 1, -1) == 0 &&
      mbedtls_x509write_crt_set_key_usage(&escritor, MBEDTLS_X509_KU_KEY_CERT_SIGN) == 0;
  const int largo = armado ? mbedtls_x509write_crt_der(&escritor, der.data(), der.size(),
                                                      ventrik::cifrado::aleatorio, nullptr)
                           : -1;
  mbedtls_x509write_crt_free(&escritor);
  REQUIRE(largo > 0);
  return std::vector<unsigned char>(der.end() - largo, der.end());
}

// Un ServerHello de TLS 1.2 (sin la extensión de versiones) que elige un cifrado que el programa ofreció.
std::string server_hello_tls12() {
  std::string cuerpo = "\x03\x03";
  cuerpo += std::string(32, '\x11');
  cuerpo += std::string("\x00", 1);
  cuerpo += std::string("\x13\x01\x00\x00\x00", 5);
  const std::string mensaje = std::string("\x02\x00\x00", 3) + static_cast<char>(cuerpo.size()) + cuerpo;
  return std::string("\x16\x03\x03\x00", 4) + static_cast<char>(mensaje.size()) + mensaje;
}

}  // namespace

TEST_CASE("Con la huella correcta, el programa se conecta por TLS 1.3 y saluda") {
  ServidorDePrueba servidor;
  const auto resultado = ventrik::cliente::conectar(servidor.direccion(), servidor.huella());
  REQUIRE(resultado.estado == EstadoConexion::conectado);
  CHECK(resultado.protocolo == ventrik::kProtocoloMaximo);
  CAPTURE(resultado.cifrado);
  CHECK((resultado.cifrado == "TLS1-3-AES-128-GCM-SHA256" ||
         resultado.cifrado == "TLS1-3-AES-256-GCM-SHA384" ||
         resultado.cifrado == "TLS1-3-CHACHA20-POLY1305-SHA256"));
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::conectada) == 1; }));
}

TEST_CASE("Con otra huella, el programa corta sin mandar nada") {
  ServidorDePrueba servidor;
  const auto otra = identidad_nueva().huella();
  const auto resultado = ventrik::cliente::conectar(servidor.direccion(), otra);
  CHECK(resultado.estado == EstadoConexion::huella_distinta);
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::rechazo_cifrado) == 1; }));
  CHECK(servidor.cuantos(Evento::conectada) == 0);
  CHECK(servidor.cuantos(Evento::rechazo_saludo) == 0);
}

TEST_CASE("Un servidor que muestra el certificado verdadero sin tener su clave no engaña al programa") {
  auto verdadera = identidad_nueva();
  auto falsa = identidad_nueva();
  ServidorFalso falso(ConfigTls::para_servidor(verdadera.certificado(), falsa.clave()));
  const auto resultado = ventrik::cliente::conectar(falso.direccion(), verdadera.huella());
  CHECK(resultado.estado == EstadoConexion::cifrado_rechazado);
  CHECK_FALSE(falso.completo());
}

TEST_CASE("Una cadena con la clave verdadera en segundo lugar no engaña al programa") {
  auto verdadera = identidad_nueva();
  auto falsa = identidad_nueva();
  mbedtls_x509_crt cadena;
  mbedtls_x509_crt_init(&cadena);
  REQUIRE(mbedtls_x509_crt_parse_der(&cadena, falsa.certificado()->raw.p,
                                     falsa.certificado()->raw.len) == 0);
  // Una autoridad falsa con la clave pública verdadera: Mbed TLS la pone en segundo lugar.
  const auto autoridad = autoridad_con_clave_ajena(*verdadera.clave(), *falsa.clave());
  REQUIRE(mbedtls_x509_crt_parse_der(&cadena, autoridad.data(), autoridad.size()) == 0);
  {
    ServidorFalso falso(ConfigTls::para_servidor(&cadena, falsa.clave()));
    const auto resultado = ventrik::cliente::conectar(falso.direccion(), verdadera.huella());
    CHECK(resultado.estado == EstadoConexion::huella_distinta);
    CHECK_FALSE(falso.completo());
  }
  mbedtls_x509_crt_free(&cadena);
}

TEST_CASE("Un servidor que contesta con TLS 1.2 no engaña al programa") {
  ServidorFalso falso(server_hello_tls12());
  ventrik::cliente::OpcionesConexion opciones;
  opciones.tiempo_para_saludar = 3s;
  const auto resultado = ventrik::cliente::conectar(falso.direccion(), identidad_nueva().huella(), opciones);
  CHECK(resultado.estado == EstadoConexion::cifrado_rechazado);
  CHECK(resultado.codigo == MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER);
}

TEST_CASE("Un servidor que solo habla TLS 1.2 no engaña al programa") {
  auto identidad = identidad_nueva();
  ServidorFalso falso(config_ajustada(identidad, solo_tls12));
  const auto resultado = ventrik::cliente::conectar(falso.direccion(), identidad.huella());
  CHECK(resultado.estado == EstadoConexion::cifrado_rechazado);
  CHECK_FALSE(falso.completo());
}

TEST_CASE("Un servidor que solo acepta un cifrado más débil no engaña al programa") {
  auto identidad = identidad_nueva();
  ServidorFalso falso(config_ajustada(identidad, solo_ccm8));
  const auto resultado = ventrik::cliente::conectar(falso.direccion(), identidad.huella());
  CHECK(resultado.estado == EstadoConexion::cifrado_rechazado);
  CHECK_FALSE(falso.completo());
}

TEST_CASE("El servidor rechaza a un programa que intenta TLS 1.2 y sigue atendiendo") {
  ServidorDePrueba servidor;
  CHECK(apreton_tramposo(servidor.direccion(), servidor.huella(), solo_tls12) ==
        MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE);
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::rechazo_cifrado) == 1; }));
  CHECK(ventrik::cliente::conectar(servidor.direccion(), servidor.huella()).estado ==
        EstadoConexion::conectado);
}

TEST_CASE("El servidor rechaza a un programa que solo ofrece un cifrado más débil") {
  ServidorDePrueba servidor;
  CHECK(apreton_tramposo(servidor.direccion(), servidor.huella(), solo_ccm8) ==
        MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE);
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::rechazo_cifrado) == 1; }));
}

TEST_CASE("Basura en lugar de TLS: el servidor corta y sigue atendiendo") {
  ServidorDePrueba servidor;
  const auto canal = ventrik::red::conectar(servidor.direccion(), 2s);
  REQUIRE(canal.valido());
  REQUIRE(enviar_todo(canal, "GET / HTTP/1.1\r\nHost: ventrik\r\n\r\n"));
  CHECK(se_cierra(canal, 5s));
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::rechazo_cifrado) == 1; }));
  CHECK(ventrik::cliente::conectar(servidor.direccion(), servidor.huella()).estado ==
        EstadoConexion::conectado);
}

TEST_CASE("Una inundación de registros que Mbed TLS descarta no traba al servidor") {
  // Con 10 s para saludar, solo el tope de bytes antes del saludo puede cortarla tan rápido.
  ventrik::servidor::Opciones opciones;
  opciones.tiempo_para_saludar = 10s;
  ServidorDePrueba servidor(opciones);
  const auto canal = ventrik::red::conectar(servidor.direccion(), 2s);
  REQUIRE(canal.valido());
  std::atomic<bool> cortada{false};
  const auto inicio = std::chrono::steady_clock::now();
  std::thread inundar([&canal, &cortada] {
    std::string registros;
    for (int i = 0; i < 1000; ++i) {
      registros += std::string("\x14\x03\x03\x00\x01\x01", 6);
    }
    const auto hasta = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < hasta) {
      const int enviados = send(static_cast<SOCKET>(canal.get()), registros.data(),
                                static_cast<int>(registros.size()), 0);
      if (enviados < 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
        cortada = true;
        return;
      }
      if (enviados < 0) {
        ventrik::red::esperar(canal, false, true, 50ms);
      }
    }
  });
  const auto resultado = ventrik::cliente::conectar(servidor.direccion(), servidor.huella());
  CHECK(resultado.estado == EstadoConexion::conectado);
  CHECK(esperar_que([&] { return cortada.load(); }, 3s));
  CHECK(std::chrono::steady_clock::now() - inicio < 4s);
  inundar.join();
  CHECK(servidor.cuantos(Evento::rechazo_cifrado) == 1);
}

TEST_CASE("Si no saluda a tiempo, el servidor corta la conexión") {
  ventrik::servidor::Opciones opciones;
  opciones.tiempo_para_saludar = 200ms;
  ServidorDePrueba servidor(opciones);
  const auto canal = ventrik::red::conectar(servidor.direccion(), 2s);
  REQUIRE(canal.valido());
  CHECK(se_cierra(canal, 5s));
  CHECK(servidor.cuantos(Evento::tiempo_vencido) == 1);
}

TEST_CASE("Hay un tope de conexiones a medio saludar por IP") {
  ventrik::servidor::Opciones opciones;
  opciones.pendientes_por_ip = 2;
  ServidorDePrueba servidor(opciones);
  std::vector<ventrik::red::Socket> abiertas;
  for (int i = 0; i < 2; ++i) {
    abiertas.push_back(ventrik::red::conectar(servidor.direccion(), 2s));
    REQUIRE(abiertas.back().valido());
  }
  const auto tercera = ventrik::red::conectar(servidor.direccion(), 2s);
  REQUIRE(tercera.valido());
  CHECK(se_cierra(tercera, 5s));
  CHECK(servidor.cuantos(Evento::demasiadas_pendientes) == 1);
}

TEST_CASE("Hay un tope de conexiones por IP, también para las ya saludadas") {
  ventrik::servidor::Opciones opciones;
  opciones.conexiones_por_ip = 2;
  ServidorDePrueba servidor(opciones);
  std::vector<ventrik::cliente::ResultadoConexion> saludadas;
  for (int i = 0; i < 2; ++i) {
    saludadas.push_back(ventrik::cliente::conectar(servidor.direccion(), servidor.huella()));
    REQUIRE(saludadas.back().estado == EstadoConexion::conectado);
    REQUIRE(saludadas.back().abierta);
  }
  REQUIRE(esperar_que([&] { return servidor.cuantos(Evento::conectada) == 2; }));
  const auto tercera = ventrik::red::conectar(servidor.direccion(), 2s);
  REQUIRE(tercera.valido());
  CHECK(se_cierra(tercera, 5s));
  CHECK(servidor.cuantos(Evento::demasiadas_por_ip) == 1);
}

TEST_CASE("Una IP que falla varias veces seguidas queda bloqueada") {
  ventrik::servidor::Opciones opciones;
  opciones.proteccion.fallas_para_bloquear = 2;
  ServidorDePrueba servidor(opciones);
  for (int i = 0; i < 2; ++i) {
    const auto canal = ventrik::red::conectar(servidor.direccion(), 2s);
    REQUIRE(canal.valido());
    REQUIRE(enviar_todo(canal, "basura que no es TLS"));
    CHECK(se_cierra(canal, 5s));
  }
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::bloqueada) == 1; }));
  CHECK(ventrik::cliente::conectar(servidor.direccion(), servidor.huella()).estado ==
        EstadoConexion::cortada);
  CHECK(servidor.cuantos(Evento::rechazo_bloqueada) == 1);
}

TEST_CASE("Una IP que se conecta demasiado seguido espera su turno") {
  ventrik::servidor::Opciones opciones;
  opciones.proteccion.rafaga_por_ip = 2;
  opciones.proteccion.conexiones_por_segundo_por_ip = 0.01;
  ServidorDePrueba servidor(opciones);
  std::vector<ventrik::red::Socket> abiertas;
  for (int i = 0; i < 2; ++i) {
    abiertas.push_back(ventrik::red::conectar(servidor.direccion(), 2s));
    REQUIRE(abiertas.back().valido());
  }
  const auto tercera = ventrik::red::conectar(servidor.direccion(), 2s);
  REQUIRE(tercera.valido());
  CHECK(se_cierra(tercera, 5s));
  CHECK(servidor.cuantos(Evento::demasiado_rapido) == 1);
}

TEST_CASE("Un programa demasiado viejo recibe a qué versión actualizar") {
  ServidorDePrueba servidor;
  ventrik::cliente::OpcionesConexion opciones;
  opciones.hola.programa = {0, 0, 9};
  const auto resultado = ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), opciones);
  CHECK(resultado.estado == EstadoConexion::programa_viejo);
  CHECK(resultado.version_pedida == ventrik::kPrimeraVersionDelProtocolo[0]);
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::rechazo_saludo) == 1; }));
}

TEST_CASE("Un servidor demasiado viejo se avisa como tal") {
  ServidorDePrueba servidor;
  ventrik::cliente::OpcionesConexion opciones;
  opciones.hola.protocolo_minimo = ventrik::kProtocoloMaximo + 1;
  opciones.hola.protocolo_maximo = ventrik::kProtocoloMaximo + 1;
  CHECK(ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), opciones).estado ==
        EstadoConexion::servidor_viejo);
}

TEST_CASE("Varios programas se conectan a la vez") {
  ventrik::servidor::Opciones opciones;
  opciones.pendientes_por_ip = 8;
  ServidorDePrueba servidor(opciones);
  std::array<std::atomic<bool>, 8> conectados{};
  std::vector<std::thread> hilos;
  for (auto& conectado : conectados) {
    hilos.emplace_back([&servidor, &conectado] {
      conectado = ventrik::cliente::conectar(servidor.direccion(), servidor.huella()).estado ==
                  EstadoConexion::conectado;
    });
  }
  for (auto& hilo : hilos) {
    hilo.join();
  }
  for (const auto& conectado : conectados) {
    CHECK(conectado.load());
  }
}

TEST_CASE("Sin servidor, el programa avisa que no responde") {
  ventrik::red::Direccion cerrado{kLocal, 0};
  {
    const auto escucha = ventrik::red::escuchar(ventrik::red::Direccion{kLocal, 0});
    REQUIRE(escucha.valido());
    cerrado = *ventrik::red::direccion_local(escucha);
  }
  ventrik::cliente::OpcionesConexion opciones;
  opciones.tiempo_para_conectar = 3s;
  CHECK(ventrik::cliente::conectar(cerrado, identidad_nueva().huella(), opciones).estado ==
        EstadoConexion::sin_respuesta);
}
