#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>

#include <mbedtls/rsa.h>

#include "client/webauthn.h"
#include "cliente_de_prueba.h"
#include "common/cifrado/clave.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/llave.h"
#include "identidad_de_prueba.h"
#include "rsa_de_prueba.h"
#include "server/base.h"
#include <doctest/doctest.h>

namespace cifrado = ventrik::cifrado;
using Algoritmo = ventrik::cifrado::AlgoritmoDeLlave;
using ventrik::cifrado::Ceremonia;
using ventrik::cifrado::ResultadoLlave;
using Bytes = std::vector<std::uint8_t>;

namespace {

constexpr std::uint8_t kPresente = cifrado::kLlavePresente;
constexpr std::uint8_t kConPin = cifrado::kLlavePresente | cifrado::kLlaveVerificada;
constexpr std::uint8_t kCreada = kConPin | cifrado::kLlaveConCredencial;

Bytes bytes_de(const std::string& texto) {
  return Bytes(texto.begin(), texto.end());
}

Bytes resumen_de(const Bytes& datos) {
  const auto resumen = cifrado::resumir(datos);
  REQUIRE(resumen);
  return Bytes(resumen->begin(), resumen->end());
}

void sumar(Bytes& destino, const Bytes& mas) {
  destino.insert(destino.end(), mas.begin(), mas.end());
}

void sumar_grande(Bytes& destino, std::uint32_t valor, int bytes) {
  for (int corrimiento = 8 * (bytes - 1); corrimiento >= 0; corrimiento -= 8) {
    destino.push_back(static_cast<std::uint8_t>(valor >> corrimiento));
  }
}

const Bytes kDesafio(32, 0x5D);
const Bytes kCoseAntesDeN = {0xA4, 0x01, 0x03, 0x03, 0x39, 0x01, 0x00, 0x20, 0x59, 0x01, 0x00};
const Bytes kCoseExponente = {0x21, 0x43, 0x01, 0x00, 0x01};
constexpr Algoritmo kRs256 = Algoritmo::rs256;

class RsaDeMentira {
 public:
  explicit RsaDeMentira(std::size_t cual) {
    mbedtls_rsa_init(&rsa_);
    REQUIRE(prueba::armar_rsa_de_prueba(rsa_, cual));
  }
  ~RsaDeMentira() { mbedtls_rsa_free(&rsa_); }
  RsaDeMentira(const RsaDeMentira&) = delete;
  RsaDeMentira& operator=(const RsaDeMentira&) = delete;

  Bytes modulo() const {
    Bytes modulo(cifrado::kBytesModuloRsa);
    REQUIRE(mbedtls_rsa_export_raw(&rsa_, modulo.data(), modulo.size(), nullptr, 0, nullptr, 0, nullptr, 0, nullptr,
                                   0) == 0);
    return modulo;
  }

  Bytes firmar(const Bytes& datos) const {
    const Bytes resumen = resumen_de(datos);
    Bytes firma(cifrado::kBytesModuloRsa);
    REQUIRE(mbedtls_rsa_rsassa_pkcs1_v15_sign(&rsa_, cifrado::aleatorio, nullptr, MBEDTLS_MD_SHA256,
                                              static_cast<unsigned int>(resumen.size()), resumen.data(),
                                              firma.data()) == 0);
    return firma;
  }

 private:
  mutable mbedtls_rsa_context rsa_;
};

const RsaDeMentira& rsa_de_prueba(std::size_t cual) {
  static std::array<std::unique_ptr<RsaDeMentira>, 2> claves;
  auto& clave = claves.at(cual);
  if (!clave) {
    clave = std::make_unique<RsaDeMentira>(cual);
  }
  return *clave;
}

class LlaveDeMentira {
 public:
  explicit LlaveDeMentira(Algoritmo algoritmo = Algoritmo::es256, std::size_t cual = 0) {
    if (algoritmo == kRs256) {
      rsa_ = &rsa_de_prueba(cual);
    } else {
      clave_ = prueba::identidad_nueva_de_prueba();
    }
    const auto publica = rsa_ != nullptr ? cifrado::armar_llave(kRs256, rsa_->modulo())
                                         : cifrado::armar_llave(Algoritmo::es256, clave_->publica());
    REQUIRE(publica);
    publica_ = *publica;
  }

  Bytes datos(std::uint8_t marcas, std::uint32_t contador, const std::string& aplicacion = "ventrik") const {
    Bytes datos = resumen_de(bytes_de(aplicacion));
    datos.push_back(marcas);
    sumar_grande(datos, contador, 4);
    return datos;
  }

  Bytes firmar(const Bytes& datos, const Bytes& desafio = kDesafio, Ceremonia ceremonia = Ceremonia::firmar) const {
    Bytes firmado = datos;
    sumar(firmado, resumen_de(bytes_de(cifrado::texto_de_cliente(ceremonia, desafio))));
    if (rsa_ != nullptr) {
      return rsa_->firmar(firmado);
    }
    const auto firma = clave_->firmar(firmado);
    REQUIRE(firma);
    return *firma;
  }

  Bytes cose() const {
    const auto& publica = publica_.clave;
    if (rsa_ != nullptr) {
      Bytes cose = kCoseAntesDeN;
      sumar(cose, publica);
      sumar(cose, kCoseExponente);
      return cose;
    }
    Bytes cose = {0xA5, 0x01, 0x02, 0x03, 0x26, 0x20, 0x01, 0x21, 0x58, 0x20};
    cose.insert(cose.end(), publica.begin() + 1, publica.begin() + 33);
    sumar(cose, {0x22, 0x58, 0x20});
    cose.insert(cose.end(), publica.begin() + 33, publica.end());
    return cose;
  }

  Bytes datos_de_creacion(std::uint8_t marcas, const Bytes& id, const Bytes& clave) const {
    Bytes datos = this->datos(marcas, 0);
    sumar(datos, Bytes(16, 0xAA));
    sumar_grande(datos, static_cast<std::uint32_t>(id.size()), 2);
    sumar(datos, id);
    sumar(datos, clave);
    return datos;
  }

  const cifrado::LlavePublica& publica() const { return publica_; }

 private:
  std::shared_ptr<const cifrado::Clave> clave_;
  const RsaDeMentira* rsa_ = nullptr;
  cifrado::LlavePublica publica_;
};

cifrado::LlaveVerificada verificar(const LlaveDeMentira& llave, std::uint32_t guardado, const Bytes& datos,
                                   const Bytes& firma, const Bytes& desafio = kDesafio) {
  return cifrado::verificar_llave(llave.publica(), guardado, desafio, datos, firma);
}

}

TEST_CASE("El texto de cliente de la llave se arma siempre igual a partir del desafío") {
  Bytes de_0_a_31;
  for (std::uint8_t i = 0; i < 32; ++i) {
    de_0_a_31.push_back(i);
  }
  CHECK(cifrado::texto_de_cliente(Ceremonia::firmar, de_0_a_31) ==
        R"({"type":"webauthn.get","challenge":"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8","origin":"ventrik"})");
  CHECK(cifrado::texto_de_cliente(Ceremonia::crear, Bytes{0xFB, 0xFF}) ==
        R"({"type":"webauthn.create","challenge":"-_8","origin":"ventrik"})");
  CHECK(cifrado::texto_de_cliente(Ceremonia::firmar, kDesafio) ==
        cifrado::texto_de_cliente(Ceremonia::firmar, kDesafio));
}

TEST_CASE("Una llave con PIN o huella entra y cuenta como segundo paso") {
  const LlaveDeMentira llave;
  const Bytes datos = llave.datos(kConPin, 0);
  REQUIRE(datos.size() == cifrado::kBytesDatosDeLlave);
  const auto resultado = verificar(llave, 0, datos, llave.firmar(datos));
  CHECK(resultado.resultado == ResultadoLlave::valida);
  CHECK(resultado.segundo_paso);
  CHECK(resultado.contador == 0);
}

TEST_CASE("Una llave sin PIN ni huella entra pero no cuenta como segundo paso") {
  const LlaveDeMentira llave;
  const Bytes datos = llave.datos(kPresente, 0);
  const auto resultado = verificar(llave, 0, datos, llave.firmar(datos));
  CHECK(resultado.resultado == ResultadoLlave::valida);
  CHECK_FALSE(resultado.segundo_paso);
}

TEST_CASE("Una llave sin la marca de tocada se rechaza") {
  const LlaveDeMentira llave;
  for (const std::uint8_t marcas : {std::uint8_t{0}, cifrado::kLlaveVerificada}) {
    const Bytes datos = llave.datos(marcas, 0);
    const auto resultado = verificar(llave, 0, datos, llave.firmar(datos));
    CHECK(resultado.resultado == ResultadoLlave::datos_invalidos);
    CHECK_FALSE(resultado.segundo_paso);
  }
}

TEST_CASE("Datos de la llave alterados o de otro largo se rechazan") {
  const LlaveDeMentira llave;
  const Bytes datos = llave.datos(kConPin, 7);
  const Bytes firma = llave.firmar(datos);
  REQUIRE(verificar(llave, 0, datos, firma).resultado == ResultadoLlave::valida);

  for (const std::size_t lugar : {std::size_t{0}, std::size_t{31}, std::size_t{36}}) {
    Bytes alterados = datos;
    alterados[lugar] ^= 0x01;
    CAPTURE(lugar);
    CHECK(verificar(llave, 0, alterados, firma).resultado != ResultadoLlave::valida);
  }
  const Bytes sin_pin = llave.datos(kPresente, 7);
  CHECK(verificar(llave, 0, sin_pin, firma).resultado == ResultadoLlave::firma_invalida);

  Bytes corto(datos.begin(), datos.end() - 1);
  CHECK(verificar(llave, 0, corto, llave.firmar(corto)).resultado == ResultadoLlave::datos_invalidos);
  Bytes largo = datos;
  largo.push_back(0);
  CHECK(verificar(llave, 0, largo, llave.firmar(largo)).resultado == ResultadoLlave::datos_invalidos);
  CHECK(verificar(llave, 0, {}, firma).resultado == ResultadoLlave::datos_invalidos);

  for (const std::uint8_t marca : {cifrado::kLlaveConCredencial, cifrado::kLlaveConExtensiones}) {
    const Bytes raros = llave.datos(static_cast<std::uint8_t>(kConPin | marca), 7);
    CHECK(verificar(llave, 0, raros, llave.firmar(raros)).resultado == ResultadoLlave::datos_invalidos);
  }
  const Bytes de_otra_aplicacion = llave.datos(kConPin, 7, "otra");
  CHECK(verificar(llave, 0, de_otra_aplicacion, llave.firmar(de_otra_aplicacion)).resultado ==
        ResultadoLlave::datos_invalidos);

  const LlaveDeMentira otra;
  CHECK(verificar(otra, 0, datos, firma).resultado == ResultadoLlave::firma_invalida);
  CHECK(verificar(llave, 0, datos, otra.firmar(datos)).resultado == ResultadoLlave::firma_invalida);
  CHECK(verificar(llave, 0, datos, {}).resultado == ResultadoLlave::firma_invalida);
  CHECK(verificar(llave, 0, datos, Bytes(firma.begin(), firma.end() - 1)).resultado ==
        ResultadoLlave::firma_invalida);
}

TEST_CASE("Una llave que firmó un texto de cliente con otro desafío se rechaza") {
  const LlaveDeMentira llave;
  const Bytes datos = llave.datos(kConPin, 0);
  const Bytes otro_desafio(32, 0x5E);
  CHECK(verificar(llave, 0, datos, llave.firmar(datos, otro_desafio)).resultado == ResultadoLlave::firma_invalida);
  CHECK(verificar(llave, 0, datos, llave.firmar(datos), otro_desafio).resultado == ResultadoLlave::firma_invalida);
  for (const std::size_t largo : {0, 31, 33}) {
    const Bytes corto(largo, 0x5D);
    CHECK(verificar(llave, 0, datos, llave.firmar(datos, corto), corto).resultado == ResultadoLlave::datos_invalidos);
  }
  CHECK(verificar(llave, 0, datos, llave.firmar(datos, kDesafio, Ceremonia::crear)).resultado ==
        ResultadoLlave::firma_invalida);
}

TEST_CASE("El contador de la llave en 0, que sube o que baja") {
  const LlaveDeMentira llave;
  const auto con_contador = [&llave](std::uint32_t guardado, std::uint32_t contador) {
    const Bytes datos = llave.datos(kConPin, contador);
    return verificar(llave, guardado, datos, llave.firmar(datos));
  };
  for (int vez = 0; vez < 2; ++vez) {
    const auto siempre_cero = con_contador(0, 0);
    CHECK(siempre_cero.resultado == ResultadoLlave::valida);
    CHECK(siempre_cero.contador == 0);
  }
  const auto sube = con_contador(5, 6);
  CHECK(sube.resultado == ResultadoLlave::valida);
  CHECK(sube.contador == 6);
  CHECK(con_contador(0, 0xFFFFFFFF).contador == 0xFFFFFFFF);
  for (const std::uint32_t contador : {6u, 3u, 0u}) {
    CAPTURE(contador);
    const auto baja = con_contador(6, contador);
    CHECK(baja.resultado == ResultadoLlave::posible_llave_clonada);
    CHECK_FALSE(baja.segundo_paso);
  }
}

TEST_CASE("El permiso de la llave vale solo con su identidad y su generación") {
  const auto identidad = prueba::identidad_nueva_de_prueba();
  const auto otra_identidad = prueba::identidad_nueva_de_prueba();
  const LlaveDeMentira llave;
  const LlaveDeMentira otra_llave;
  const auto firma = identidad->firmar(cifrado::lo_que_firma_el_permiso(identidad->publica(), llave.publica(), 3));
  REQUIRE(firma);

  CHECK(cifrado::verificar_permiso(identidad->publica(), llave.publica(), 3, 3, *firma));
  CHECK(cifrado::verificar_permiso(identidad->publica(), llave.publica(), 3, 1, *firma));
  CHECK_FALSE(cifrado::verificar_permiso(identidad->publica(), llave.publica(), 3, 4, *firma));
  CHECK_FALSE(cifrado::verificar_permiso(identidad->publica(), llave.publica(), 4, 4, *firma));
  CHECK_FALSE(cifrado::verificar_permiso(otra_identidad->publica(), llave.publica(), 3, 3, *firma));
  CHECK_FALSE(cifrado::verificar_permiso(identidad->publica(), otra_llave.publica(), 3, 3, *firma));
  CHECK_FALSE(cifrado::verificar_permiso(identidad->publica(), llave.publica(), 3, 3, {}));

  const auto de_otra = otra_identidad->firmar(
      cifrado::lo_que_firma_el_permiso(identidad->publica(), llave.publica(), 3));
  REQUIRE(de_otra);
  CHECK_FALSE(cifrado::verificar_permiso(identidad->publica(), llave.publica(), 3, 3, *de_otra));
}

TEST_CASE("El permiso de la llave firma una etiqueta propia, la identidad, el algoritmo y la llave con su largo, y la generación") {
  const auto identidad = prueba::identidad_nueva_de_prueba();
  for (const auto& [llave, cabecera] : {std::make_pair(LlaveDeMentira(), Bytes{1, 65, 0}),
                                        std::make_pair(LlaveDeMentira(kRs256), Bytes{2, 0, 1})}) {
    const Bytes firmado = cifrado::lo_que_firma_el_permiso(identidad->publica(), llave.publica(), 0x01020304);
    Bytes esperado = bytes_de("ventrik permiso de llave v2");
    esperado.insert(esperado.end(), identidad->publica().begin(), identidad->publica().end());
    sumar(esperado, cabecera);
    sumar(esperado, llave.publica().clave);
    sumar(esperado, {0x04, 0x03, 0x02, 0x01});
    CHECK(firmado == esperado);
  }
  CHECK(cifrado::lo_que_firma_el_permiso(identidad->publica(), {kRs256, LlaveDeMentira().publica().clave}, 1).empty());
  CHECK(cifrado::lo_que_firma_el_permiso(identidad->publica(), {Algoritmo::es256, LlaveDeMentira(kRs256).publica().clave},
                                         1)
            .empty());
  CHECK(cifrado::lo_que_firma_el_permiso(identidad->publica(), {static_cast<Algoritmo>(3), Bytes(65, 4)}, 1).empty());
}

TEST_CASE("El permiso de una llave RS256 vale solo para esa llave y con la firma de su identidad") {
  const auto identidad = prueba::identidad_nueva_de_prueba();
  const LlaveDeMentira llave(kRs256);
  const LlaveDeMentira otra(kRs256, 1);
  const auto firma = identidad->firmar(cifrado::lo_que_firma_el_permiso(identidad->publica(), llave.publica(), 3));
  REQUIRE(firma);
  CHECK(cifrado::verificar_permiso(identidad->publica(), llave.publica(), 3, 3, *firma));
  CHECK_FALSE(cifrado::verificar_permiso(identidad->publica(), otra.publica(), 3, 3, *firma));
  CHECK_FALSE(cifrado::verificar_permiso(identidad->publica(), LlaveDeMentira().publica(), 3, 3, *firma));
  CHECK_FALSE(cifrado::verificar_permiso(identidad->publica(), {Algoritmo::es256, llave.publica().clave}, 3, 3, *firma));
}

TEST_CASE("El lector COSE de la llave acepta solo una clave P-256 justa") {
  const LlaveDeMentira llave;
  const Bytes buena = llave.cose();
  REQUIRE(buena.size() == 77);
  const auto leida = cifrado::leer_clave_cose(buena);
  REQUIRE(leida);
  CHECK(*leida == llave.publica());

  CHECK_FALSE(cifrado::leer_clave_cose({}));
  CHECK_FALSE(cifrado::leer_clave_cose(Bytes(buena.begin(), buena.end() - 1)));
  Bytes con_cola = buena;
  con_cola.push_back(0x00);
  CHECK_FALSE(cifrado::leer_clave_cose(con_cola));
  Bytes con_campo_de_mas = buena;
  con_campo_de_mas[0] = 0xA6;
  sumar(con_campo_de_mas, {0x23, 0x01});
  CHECK_FALSE(cifrado::leer_clave_cose(con_campo_de_mas));

  const auto con_cambio = [&buena](std::size_t lugar, std::uint8_t valor) {
    Bytes cambiada = buena;
    cambiada[lugar] = valor;
    return cifrado::leer_clave_cose(cambiada);
  };
  CHECK_FALSE(con_cambio(2, 0x03));
  CHECK_FALSE(con_cambio(4, 0x27));
  CHECK_FALSE(con_cambio(6, 0x02));
  CHECK_FALSE(con_cambio(9, 0x21));
  CHECK_FALSE(con_cambio(9, 0x1F));
  CHECK_FALSE(con_cambio(44, 0x21));
  CHECK_FALSE(con_cambio(76, static_cast<std::uint8_t>(buena[76] ^ 0x01)));

  Bytes otro_alg = buena;
  otro_alg[4] = 0x38;
  otro_alg.insert(otro_alg.begin() + 5, 0x22);
  CHECK_FALSE(cifrado::leer_clave_cose(otro_alg));
  Bytes x_corta = buena;
  x_corta[9] = 0x1F;
  x_corta.erase(x_corta.begin() + 10);
  CHECK_FALSE(cifrado::leer_clave_cose(x_corta));
  Bytes y_larga = buena;
  y_larga[44] = 0x21;
  y_larga.push_back(0x00);
  CHECK_FALSE(cifrado::leer_clave_cose(y_larga));
}

TEST_CASE("Los datos de una llave recién creada dan su id y su clave") {
  const LlaveDeMentira llave;
  const Bytes id(48, 0x5A);
  const auto creada = cifrado::leer_llave_creada(llave.datos_de_creacion(kCreada, id, llave.cose()));
  REQUIRE(creada);
  CHECK(creada->id == id);
  CHECK(creada->publica == llave.publica());

  CHECK_FALSE(cifrado::leer_llave_creada({}));
  CHECK_FALSE(cifrado::leer_llave_creada(llave.datos_de_creacion(kConPin, id, llave.cose())));
  CHECK_FALSE(cifrado::leer_llave_creada(
      llave.datos_de_creacion(static_cast<std::uint8_t>(cifrado::kLlaveVerificada | cifrado::kLlaveConCredencial), id,
                              llave.cose())));
  CHECK_FALSE(cifrado::leer_llave_creada(llave.datos_de_creacion(kCreada, {}, llave.cose())));
  CHECK(cifrado::leer_llave_creada(llave.datos_de_creacion(kCreada, Bytes(cifrado::kMaximoIdDeLlave, 1), llave.cose())));
  CHECK_FALSE(cifrado::leer_llave_creada(
      llave.datos_de_creacion(kCreada, Bytes(cifrado::kMaximoIdDeLlave + 1, 1), llave.cose())));
  const Bytes entera = llave.datos_de_creacion(kCreada, id, llave.cose());
  CHECK_FALSE(cifrado::leer_llave_creada(Bytes(entera.begin(), entera.end() - 1)));
  Bytes con_cola = entera;
  con_cola.push_back(0xA0);
  CHECK_FALSE(cifrado::leer_llave_creada(con_cola));
  con_cola[32] |= cifrado::kLlaveConExtensiones;
  CHECK(cifrado::leer_llave_creada(con_cola));
  Bytes de_otra_aplicacion = entera;
  de_otra_aplicacion[0] ^= 0x01;
  CHECK_FALSE(cifrado::leer_llave_creada(de_otra_aplicacion));
  Bytes clave_rota = llave.cose();
  clave_rota[6] = 0x02;
  CHECK_FALSE(cifrado::leer_llave_creada(llave.datos_de_creacion(kCreada, id, clave_rota)));
}

TEST_CASE("Una llave RS256 como la de Windows Hello entra con el contador en 0, con PIN cuenta como segundo paso y sin PIN no") {
  const LlaveDeMentira llave(kRs256);
  REQUIRE(llave.publica().clave.size() == cifrado::kBytesModuloRsa);
  for (int vez = 0; vez < 2; ++vez) {
    const Bytes con_pin = llave.datos(kConPin, 0);
    const Bytes firma = llave.firmar(con_pin);
    REQUIRE(firma.size() == cifrado::kBytesModuloRsa);
    const auto resultado = verificar(llave, 0, con_pin, firma);
    CHECK(resultado.resultado == ResultadoLlave::valida);
    CHECK(resultado.segundo_paso);
    CHECK(resultado.contador == 0);
  }
  const Bytes sin_pin = llave.datos(kPresente, 0);
  const auto resultado = verificar(llave, 0, sin_pin, llave.firmar(sin_pin));
  CHECK(resultado.resultado == ResultadoLlave::valida);
  CHECK_FALSE(resultado.segundo_paso);
  const Bytes sin_tocar = llave.datos(cifrado::kLlaveVerificada, 0);
  CHECK(verificar(llave, 0, sin_tocar, llave.firmar(sin_tocar)).resultado == ResultadoLlave::datos_invalidos);
  const Bytes baja = llave.datos(kConPin, 4);
  CHECK(verificar(llave, 5, baja, llave.firmar(baja)).resultado == ResultadoLlave::posible_llave_clonada);
}

TEST_CASE("Una firma RS256 de otra llave, alterada, de otro largo o cruzada con ES256 se rechaza") {
  const LlaveDeMentira rsa(kRs256);
  const LlaveDeMentira otra_rsa(kRs256, 1);
  const LlaveDeMentira p256;
  const Bytes datos = rsa.datos(kConPin, 0);
  const Bytes firma = rsa.firmar(datos);
  REQUIRE(verificar(rsa, 0, datos, firma).resultado == ResultadoLlave::valida);

  CHECK(verificar(rsa, 0, datos, otra_rsa.firmar(datos)).resultado == ResultadoLlave::firma_invalida);
  CHECK(verificar(otra_rsa, 0, datos, firma).resultado == ResultadoLlave::firma_invalida);
  for (const std::size_t lugar : {std::size_t{0}, std::size_t{128}, std::size_t{255}}) {
    Bytes alterada = firma;
    alterada[lugar] ^= 0x01;
    CAPTURE(lugar);
    CHECK(verificar(rsa, 0, datos, alterada).resultado == ResultadoLlave::firma_invalida);
  }
  Bytes con_cero = firma;
  con_cero.insert(con_cero.begin(), 0);
  CHECK(verificar(rsa, 0, datos, con_cero).resultado == ResultadoLlave::firma_invalida);
  CHECK(verificar(rsa, 0, datos, Bytes(firma.begin() + 1, firma.end())).resultado == ResultadoLlave::firma_invalida);
  CHECK(verificar(rsa, 0, datos, {}).resultado == ResultadoLlave::firma_invalida);
  Bytes otros_datos = datos;
  otros_datos[36] ^= 0x01;
  CHECK(verificar(rsa, 0, otros_datos, firma).resultado == ResultadoLlave::firma_invalida);

  CHECK(verificar(rsa, 0, datos, p256.firmar(datos)).resultado == ResultadoLlave::firma_invalida);
  CHECK(verificar(p256, 0, datos, firma).resultado == ResultadoLlave::firma_invalida);
  const auto con = [&](const cifrado::LlavePublica& llave, const Bytes& cual) {
    return cifrado::verificar_llave(llave, 0, kDesafio, datos, cual).resultado;
  };
  CHECK(con({Algoritmo::es256, rsa.publica().clave}, firma) == ResultadoLlave::firma_invalida);
  CHECK(con({kRs256, p256.publica().clave}, p256.firmar(datos)) == ResultadoLlave::firma_invalida);
  CHECK(con({static_cast<Algoritmo>(0), rsa.publica().clave}, firma) == ResultadoLlave::firma_invalida);
  Bytes otro_largo = rsa.publica().clave;
  otro_largo.push_back(0x01);
  CHECK(con({kRs256, otro_largo}, firma) == ResultadoLlave::firma_invalida);
}

TEST_CASE("El lector COSE de la llave acepta solo una clave RSA de 2048 bits justos con exponente 65537") {
  const LlaveDeMentira llave(kRs256);
  const Bytes buena = llave.cose();
  REQUIRE(buena.size() == 272);
  const auto leida = cifrado::leer_clave_cose(buena);
  REQUIRE(leida);
  CHECK(*leida == llave.publica());
  CHECK(leida->algoritmo == kRs256);

  CHECK_FALSE(cifrado::leer_clave_cose(Bytes(buena.begin(), buena.end() - 1)));
  Bytes con_cola = buena;
  con_cola.push_back(0x00);
  CHECK_FALSE(cifrado::leer_clave_cose(con_cola));
  const auto con_cambio = [&buena](std::size_t lugar, std::uint8_t valor) {
    Bytes cambiada = buena;
    cambiada[lugar] = valor;
    return cifrado::leer_clave_cose(cambiada);
  };
  CHECK_FALSE(con_cambio(0, 0xA5));
  CHECK_FALSE(con_cambio(2, 0x02));
  CHECK_FALSE(con_cambio(5, 0x02));
  CHECK_FALSE(con_cambio(7, 0x21));
  CHECK_FALSE(con_cambio(10, 0x01));
  CHECK_FALSE(con_cambio(11, static_cast<std::uint8_t>(buena[11] & 0x7F)));
  CHECK_FALSE(con_cambio(266, static_cast<std::uint8_t>(buena[266] & 0xFE)));
  CHECK_FALSE(con_cambio(267, 0x20));
  CHECK_FALSE(con_cambio(270, 0x03));
  CHECK_FALSE(con_cambio(268, 0x41));

  const auto armada = [](const Bytes& antes, const Bytes& modulo, const Bytes& despues) {
    Bytes cose = antes;
    sumar(cose, modulo);
    sumar(cose, despues);
    return cifrado::leer_clave_cose(cose);
  };
  const Bytes& modulo = llave.publica().clave;
  CHECK_FALSE(armada({0xA4, 0x01, 0x03, 0x03, 0x39, 0x01, 0x00, 0x20, 0x59, 0x00, 0xFF},
                     Bytes(modulo.begin() + 1, modulo.end()), kCoseExponente));
  Bytes con_cero = modulo;
  con_cero.insert(con_cero.begin(), 0);
  CHECK_FALSE(armada({0xA4, 0x01, 0x03, 0x03, 0x39, 0x01, 0x00, 0x20, 0x59, 0x01, 0x01}, con_cero, kCoseExponente));
  Bytes grande = modulo;
  grande.insert(grande.end(), modulo.begin(), modulo.begin() + 128);
  CHECK_FALSE(armada({0xA4, 0x01, 0x03, 0x03, 0x39, 0x01, 0x00, 0x20, 0x59, 0x01, 0x80}, grande, kCoseExponente));
  CHECK_FALSE(armada(kCoseAntesDeN, modulo, {0x21, 0x41, 0x03}));
  CHECK_FALSE(armada(kCoseAntesDeN, modulo, {0x21, 0x44, 0x00, 0x01, 0x00, 0x01}));
  CHECK_FALSE(armada(kCoseAntesDeN, modulo, {0x21, 0x43, 0x01, 0x00, 0x01, 0x23, 0x01}));
  Bytes e_antes = {0xA4, 0x01, 0x03, 0x03, 0x39, 0x01, 0x00};
  sumar(e_antes, kCoseExponente);
  sumar(e_antes, {0x20, 0x59, 0x01, 0x00});
  CHECK_FALSE(armada(e_antes, modulo, {}));
}

TEST_CASE("Los datos de una llave RS256 recién creada dan su id y su clave") {
  const LlaveDeMentira llave(kRs256);
  const Bytes id(70, 0x3C);
  const auto creada = cifrado::leer_llave_creada(llave.datos_de_creacion(kCreada, id, llave.cose()));
  REQUIRE(creada);
  CHECK(creada->id == id);
  CHECK(creada->publica == llave.publica());
  Bytes con_cola = llave.datos_de_creacion(kCreada, id, llave.cose());
  con_cola.push_back(0xA0);
  CHECK_FALSE(cifrado::leer_llave_creada(con_cola));
  con_cola[32] |= cifrado::kLlaveConExtensiones;
  CHECK(cifrado::leer_llave_creada(con_cola));
  Bytes rota = llave.cose();
  rota[270] = 0x03;
  CHECK_FALSE(cifrado::leer_llave_creada(llave.datos_de_creacion(kCreada, id, rota)));
}

TEST_CASE("Se piden ES256 y RS256 antes de Windows 11 22H2 o sin saber el build, y desde ahí solo ES256") {
  using Lista = std::vector<std::int32_t>;
  const Lista las_dos = {cifrado::kCoseEs256, cifrado::kCoseRs256};
  const Lista solo_es256 = {cifrado::kCoseEs256};
  CHECK(ventrik::cliente::algoritmos_de_llave(19045) == las_dos);
  CHECK(ventrik::cliente::algoritmos_de_llave(22000) == las_dos);
  CHECK(ventrik::cliente::algoritmos_de_llave(22620) == las_dos);
  CHECK(ventrik::cliente::algoritmos_de_llave(std::nullopt) == las_dos);
  CHECK(ventrik::cliente::algoritmos_de_llave(22621) == solo_es256);
  CHECK(ventrik::cliente::algoritmos_de_llave(26100) == solo_es256);
  CHECK(cifrado::kCoseEs256 == -7);
  CHECK(cifrado::kCoseRs256 == -257);
}

TEST_CASE("La llave se escribe con su algoritmo y su largo, y se lee solo con el largo justo de su algoritmo") {
  const LlaveDeMentira p256;
  const LlaveDeMentira rsa(kRs256);
  for (const auto& llave : {p256.publica(), rsa.publica()}) {
    ventrik::protocolo::Escritor escritor;
    cifrado::escribir_llave(escritor, llave);
    REQUIRE(escritor.ok());
    Bytes esperado = {static_cast<std::uint8_t>(llave.algoritmo), static_cast<std::uint8_t>(llave.clave.size() & 0xFF),
                      static_cast<std::uint8_t>(llave.clave.size() >> 8)};
    sumar(esperado, llave.clave);
    CHECK(escritor.datos() == esperado);
    ventrik::protocolo::Lector lector(escritor.datos());
    const auto leida = cifrado::leer_llave(lector);
    REQUIRE(leida);
    CHECK(*leida == llave);
    CHECK(lector.terminado());
  }
  const auto se_lee = [](const Bytes& bytes) {
    ventrik::protocolo::Lector lector(bytes);
    const auto leida = cifrado::leer_llave(lector);
    CHECK(leida.has_value() == lector.ok());
    return leida.has_value();
  };
  const auto con_cabecera = [](std::uint8_t algoritmo, std::size_t largo, const Bytes& clave) {
    Bytes bytes = {algoritmo, static_cast<std::uint8_t>(largo & 0xFF), static_cast<std::uint8_t>(largo >> 8)};
    sumar(bytes, clave);
    return bytes;
  };
  const Bytes& modulo = rsa.publica().clave;
  const Bytes& punto = p256.publica().clave;
  CHECK(se_lee(con_cabecera(1, 65, punto)));
  CHECK(se_lee(con_cabecera(2, 256, modulo)));
  CHECK_FALSE(se_lee(con_cabecera(1, 256, modulo)));
  CHECK_FALSE(se_lee(con_cabecera(2, 65, punto)));
  CHECK_FALSE(se_lee(con_cabecera(0, 65, punto)));
  CHECK_FALSE(se_lee(con_cabecera(3, 256, modulo)));
  CHECK_FALSE(se_lee(con_cabecera(2, 255, Bytes(modulo.begin(), modulo.end() - 1))));
  Bytes rsa_grande = modulo;
  rsa_grande.insert(rsa_grande.end(), modulo.begin(), modulo.end());
  CHECK_FALSE(se_lee(con_cabecera(2, 512, rsa_grande)));
  CHECK_FALSE(se_lee(con_cabecera(2, 0xFFFF, modulo)));
  Bytes chico = modulo;
  chico[0] &= 0x7F;
  CHECK_FALSE(se_lee(con_cabecera(2, 256, chico)));
  Bytes par = modulo;
  par[255] &= 0xFE;
  CHECK_FALSE(se_lee(con_cabecera(2, 256, par)));
  Bytes comprimido = punto;
  comprimido[0] = 0x02;
  CHECK_FALSE(se_lee(con_cabecera(1, 65, comprimido)));
  CHECK(cifrado::firma_de_llave_valida(Algoritmo::es256, 1));
  CHECK(cifrado::firma_de_llave_valida(Algoritmo::es256, cifrado::kMaximoFirma));
  CHECK_FALSE(cifrado::firma_de_llave_valida(Algoritmo::es256, 0));
  CHECK_FALSE(cifrado::firma_de_llave_valida(Algoritmo::es256, cifrado::kMaximoFirma + 1));
  CHECK_FALSE(cifrado::firma_de_llave_valida(Algoritmo::es256, cifrado::kBytesModuloRsa));
  CHECK(cifrado::firma_de_llave_valida(kRs256, cifrado::kBytesModuloRsa));
  CHECK_FALSE(cifrado::firma_de_llave_valida(kRs256, cifrado::kBytesModuloRsa - 1));
  CHECK_FALSE(cifrado::firma_de_llave_valida(kRs256, cifrado::kMaximoFirma));
  CHECK_FALSE(cifrado::firma_de_llave_valida(static_cast<Algoritmo>(3), 64));
}

TEST_CASE("La detección de llaves y Windows Hello no se rompe") {
  using ventrik::cliente::Autenticadores;
  const auto primera = ventrik::cliente::autenticadores_disponibles();
  CHECK(ventrik::cliente::autenticadores_disponibles() == primera);
  const auto sin_llaves = ventrik::cliente::firmar_con_llave(nullptr, {}, kDesafio);
  const auto sin_nombre = ventrik::cliente::crear_llave(nullptr, L"", {});
  CHECK_FALSE(sin_llaves.valor);
  CHECK_FALSE(sin_nombre.valor);
  const HRESULT esperado = primera == Autenticadores::ninguno ? E_NOTIMPL : E_INVALIDARG;
  CHECK(sin_llaves.error == esperado);
  CHECK(sin_nombre.error == esperado);
#if defined(VENTRIK_CLASICA)
  CHECK(primera == Autenticadores::ninguno);
  CHECK(ventrik::cliente::firmar_con_llave(nullptr, {Bytes(16, 1)}, kDesafio).error == E_NOTIMPL);
  CHECK(ventrik::cliente::crear_llave(nullptr, L"prueba", {}).error == E_NOTIMPL);
#endif
  MESSAGE("autenticadores en esta PC: " << static_cast<int>(primera));
}

namespace {
namespace proto = ventrik::protocolo;
using namespace std::chrono_literals;
using ventrik::cliente::EstadoConexion;
using ventrik::servidor::Evento;

class ConLlave {
 public:
  explicit ConLlave(std::shared_ptr<const cifrado::Clave> identidad = prueba::identidad_nueva_de_prueba())
      : identidad_(std::move(identidad)) {}

  ventrik::cliente::Presentacion presentacion(std::string nombre, std::string_view equipo = "casa",
                                               std::optional<std::uint32_t> codigo = {},
                                               std::optional<cifrado::CodigoDueno> dueno = {}) const {
    ventrik::cliente::Presentacion quien{std::move(nombre), dueno, nullptr, prueba::secreto_de(equipo), nullptr, codigo};
    quien.generacion = generacion;
    auto llaves = std::make_shared<ventrik::cliente::MisLlaves>();
    llaves->generacion = generacion;
    llaves->identidad = identidad_->publica();
    const cifrado::Clave& firmante = firmante_del_permiso ? *firmante_del_permiso : *identidad_;
    const auto permiso = firmante.firmar(cifrado::lo_que_firma_el_permiso(
        identidad_->publica(), llave.publica(), generacion_del_permiso.value_or(generacion)));
    REQUIRE(permiso);
    llaves->llaves.push_back({llave.publica(), Bytes{1, 2, 3}, *permiso});
    quien.llaves = std::move(llaves);
    quien.tocar_llave = [llave = firma_de_otra.value_or(llave), marcas = marcas, contador = contador,
                         alterar = alterar](const Bytes&, gsl::span<const std::uint8_t> desafio) {
      Bytes datos = llave.datos(marcas, contador);
      const Bytes firma = llave.firmar(datos, Bytes(desafio.begin(), desafio.end()));
      if (alterar) {
        alterar(datos);
      }
      return std::optional<ventrik::cliente::LlaveTocada>(ventrik::cliente::LlaveTocada{datos, firma});
    };
    return quien;
  }

  EstadoConexion entrar(const prueba::ServidorDePrueba& servidor, std::string nombre = "Ana",
                        std::string_view equipo = "casa") const {
    return ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), presentacion(std::move(nombre), equipo))
        .estado;
  }

  ventrik::cliente::Presentacion con_identidad(std::string nombre) const {
    return {std::move(nombre), std::nullopt, identidad_, prueba::secreto_de("casa"), nullptr};
  }

  LlaveDeMentira llave;
  std::optional<LlaveDeMentira> firma_de_otra;
  std::uint32_t generacion = 1;
  std::optional<std::uint32_t> generacion_del_permiso;
  std::uint8_t marcas = kConPin;
  std::uint32_t contador = 0;
  std::shared_ptr<const cifrado::Clave> firmante_del_permiso;
  std::function<void(Bytes&)> alterar;

 private:
  std::shared_ptr<const cifrado::Clave> identidad_;
};

std::wstring base_temporal() {
  std::array<wchar_t, MAX_PATH> carpeta{};
  REQUIRE(GetTempPathW(static_cast<DWORD>(carpeta.size()), carpeta.data()) > 0);
  static int numero = 0;
  const std::wstring archivo = std::wstring(carpeta.data()) + L"ventrik_llave_" + std::to_wstring(GetCurrentProcessId()) +
                               L"_" + std::to_wstring(++numero) + L".db";
  DeleteFileW(archivo.c_str());
  return archivo;
}

void borrar_base_de_prueba(const std::wstring& archivo) {
  for (const auto* final : {L"", L"-wal", L"-shm"}) {
    DeleteFileW((archivo + final).c_str());
  }
}

bool anotado(const std::wstring& archivo, proto::Seguridad tipo, std::size_t veces) {
  auto base = ventrik::servidor::Base::abrir(archivo, "General");
  REQUIRE(base);
  const auto registro = base->registro(1000);
  return static_cast<std::size_t>(std::count_if(registro.begin(), registro.end(), [tipo](const proto::AvisoDeSeguridad& aviso) {
           return aviso.tipo == tipo;
         })) == veces;
}

std::uint64_t ms_de_ahora() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

ventrik::servidor::Opciones sin_bloqueo() {
  ventrik::servidor::Opciones opciones;
  opciones.proteccion.fallas_para_bloquear = 1000;
  opciones.proteccion.rafaga_por_ip = 1000;
  opciones.espera_maxima = std::chrono::seconds{0};
  return opciones;
}

}

TEST_CASE("La generación sale de la hora en minutos desde 2026 y el tope es la hora del servidor más dos días") {
  constexpr std::uint64_t kDesde2026 = 1767225600000ull;
  CHECK(cifrado::generacion_de_la_hora(0) == 0);
  CHECK(cifrado::generacion_de_la_hora(kDesde2026) == 0);
  CHECK(cifrado::generacion_de_la_hora(kDesde2026 + 59999) == 0);
  CHECK(cifrado::generacion_de_la_hora(kDesde2026 + 60000) == 1);
  CHECK(cifrado::generacion_tope(kDesde2026 + 60000) == 1 + cifrado::kAdelantoDeGeneracion);
  CHECK(cifrado::generacion_tope(UINT64_MAX) == UINT32_MAX);
}

TEST_CASE("Entrar con llave: el permiso vale con su identidad y su generación, y no más allá del tope") {
  prueba::ServidorDePrueba servidor(sin_bloqueo());
  ConLlave ana;
  CHECK(ana.entrar(servidor) == EstadoConexion::conectado);
  CHECK(ana.entrar(servidor) == EstadoConexion::conectado);
  ConLlave impostor = ana;
  impostor.firmante_del_permiso = prueba::identidad_nueva_de_prueba();
  CHECK(impostor.entrar(servidor) == EstadoConexion::firma_rechazada);
  ConLlave otra_generacion = ana;
  otra_generacion.generacion_del_permiso = 2;
  CHECK(otra_generacion.entrar(servidor) == EstadoConexion::firma_rechazada);
  ana.generacion = 5;
  CHECK(ana.entrar(servidor) == EstadoConexion::conectado);
  ana.generacion = 4;
  CHECK(ana.entrar(servidor) == EstadoConexion::llave_anulada);
  ana.generacion = cifrado::generacion_tope(ms_de_ahora()) + 10;
  CHECK(ana.entrar(servidor) == EstadoConexion::llave_anulada);
  ana.generacion = cifrado::generacion_de_la_hora(ms_de_ahora());
  CHECK(ana.entrar(servidor) == EstadoConexion::conectado);
  auto con_identidad = ana.con_identidad("Ana");
  con_identidad.generacion = 3;
  CHECK(ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), con_identidad).estado ==
        EstadoConexion::conectado);
  con_identidad.generacion = cifrado::generacion_tope(ms_de_ahora()) + 3000;
  CHECK(ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), con_identidad).estado ==
        EstadoConexion::conectado);
  CHECK(ana.entrar(servidor) == EstadoConexion::llave_anulada);
  ana.generacion = cifrado::generacion_tope(ms_de_ahora());
  CHECK(ana.entrar(servidor) == EstadoConexion::conectado);
  CHECK(servidor.cuantos(Evento::firma_invalida) == 2);
}

TEST_CASE("Con los datos de la llave alterados, sin la marca de tocada o de otro largo no se entra") {
  prueba::ServidorDePrueba servidor(sin_bloqueo());
  ConLlave ana;
  ana.alterar = [](Bytes& datos) { datos[36] ^= 0x01; };
  CHECK(ana.entrar(servidor) == EstadoConexion::firma_rechazada);
  ana.alterar = [](Bytes& datos) { datos[0] ^= 0x01; };
  CHECK(ana.entrar(servidor) == EstadoConexion::firma_rechazada);
  ana.alterar = nullptr;
  ana.marcas = cifrado::kLlaveVerificada;
  CHECK(ana.entrar(servidor) == EstadoConexion::firma_rechazada);
  ana.marcas = kConPin;
  ana.alterar = [](Bytes& datos) { datos.push_back(0); };
  CHECK(ana.entrar(servidor) == EstadoConexion::sin_llave);
  ana.alterar = nullptr;
  CHECK(ana.entrar(servidor) == EstadoConexion::conectado);
  CHECK(servidor.cuantos(Evento::firma_invalida) == 3);
}

TEST_CASE("El contador de la llave: 0 vale mientras no subió; si baja se rechaza, se anota y se avisa al dueño y a la identidad") {
  const std::wstring archivo = base_temporal();
  const auto codigo = cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  auto opciones = sin_bloqueo();
  opciones.archivo_base = archivo;
  opciones.codigo_dueno = codigo;
  {
    prueba::ServidorDePrueba servidor(opciones);
    prueba::Cliente tano(servidor, "Tano", codigo);
    tano.entrar();
    ConLlave ana;
    for (int vez = 0; vez < 2; ++vez) {
      CHECK(ana.entrar(servidor) == EstadoConexion::conectado);
    }
    for (const std::uint32_t contador : {5u, 6u}) {
      ana.contador = contador;
      CHECK(ana.entrar(servidor) == EstadoConexion::conectado);
    }
    for (const std::uint32_t contador : {6u, 3u, 0u}) {
      CAPTURE(contador);
      ana.contador = contador;
      CHECK(ana.entrar(servidor) == EstadoConexion::llave_clonada);
    }
    CHECK(servidor.cuantos(Evento::llave_clonada) == 3);
    CHECK(tano.esperar<proto::AvisoDeSeguridad>([](const proto::AvisoDeSeguridad& aviso) {
      return aviso.tipo == proto::Seguridad::posible_llave_clonada && aviso.quien == "Ana";
    }));
    ana.contador = 7;
    prueba::Cliente vuelve(servidor, ana.presentacion("Ana"));
    vuelve.entrar();
    CHECK(vuelve.esperar<proto::AvisoDeSeguridad>(
        [](const proto::AvisoDeSeguridad& aviso) { return aviso.tipo == proto::Seguridad::posible_llave_clonada; }));
  }
  CHECK(anotado(archivo, proto::Seguridad::posible_llave_clonada, 3));
  CHECK(anotado(archivo, proto::Seguridad::llave_nueva, 1));
  {
    auto base = ventrik::servidor::Base::abrir(archivo, "General");
    REQUIRE(base);
    const auto llaves = base->llaves();
    REQUIRE(llaves);
    REQUIRE(llaves->size() == 1);
    REQUIRE(llaves->at(0).llaves.size() == 1);
    CHECK(llaves->at(0).llaves.at(0).contador == 7);
  }
  borrar_base_de_prueba(archivo);
}

TEST_CASE("Una llave sin PIN ni huella entra, pero no cuenta como segundo paso; con PIN cuenta") {
  const auto codigo = cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  ventrik::servidor::Opciones opciones;
  opciones.codigo_dueno = codigo;
  opciones.segundo_paso = ventrik::SegundoPaso::admins;
  prueba::ServidorDePrueba servidor(opciones);
  ConLlave tano;
  {
    prueba::Cliente primera(servidor, tano.presentacion("Tano", "casa", {}, codigo));
    const auto id = primera.entrar().tu_id;
    CHECK(primera.esperar<proto::UsuarioRol>(
        [id](const proto::UsuarioRol& rol) { return rol.id == id && rol.rol == proto::Rol::dueno; }));
    CHECK(primera.no_llega<proto::ConfigurarSegundoPaso>());
  }
  tano.marcas = kPresente;
  {
    prueba::Cliente sin_pin(servidor, tano.presentacion("Tano"));
    CHECK(sin_pin.entrar().rol == proto::Rol::ninguno);
    sin_pin.ordenar(proto::CrearCanal{"Partidas"});
    CHECK(sin_pin.esperar<proto::Negado>(
        [](const proto::Negado& negado) { return negado.motivo == proto::MotivoNegado::sin_permiso; }));
  }
  tano.marcas = kConPin;
  prueba::Cliente con_pin(servidor, tano.presentacion("Tano"));
  CHECK(con_pin.entrar().rol == proto::Rol::dueno);
}

TEST_CASE("Entrar con una llave RS256 como la de Windows Hello: alta, permiso, contador en 0, con y sin PIN, y queda en la base") {
  const std::wstring archivo = base_temporal();
  const auto codigo = cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  auto opciones = sin_bloqueo();
  opciones.archivo_base = archivo;
  opciones.codigo_dueno = codigo;
  opciones.segundo_paso = ventrik::SegundoPaso::admins;
  auto identidad = prueba::identidad_nueva();
  ConLlave tano;
  tano.llave = LlaveDeMentira(kRs256);
  {
    prueba::ServidorDePrueba servidor(opciones, &identidad);
    {
      prueba::Cliente primera(servidor, tano.presentacion("Tano", "casa", {}, codigo));
      const auto id = primera.entrar().tu_id;
      CHECK(primera.esperar<proto::UsuarioRol>(
          [id](const proto::UsuarioRol& rol) { return rol.id == id && rol.rol == proto::Rol::dueno; }));
    }
    for (int vez = 0; vez < 2; ++vez) {
      prueba::Cliente otra_vez(servidor, tano.presentacion("Tano"));
      CHECK(otra_vez.entrar().rol == proto::Rol::dueno);
    }
    tano.marcas = kPresente;
    {
      prueba::Cliente sin_pin(servidor, tano.presentacion("Tano"));
      CHECK(sin_pin.entrar().rol == proto::Rol::ninguno);
    }
    tano.marcas = kConPin;
    ConLlave impostor = tano;
    impostor.firma_de_otra = LlaveDeMentira(kRs256, 1);
    CHECK(impostor.entrar(servidor, "Tano") == EstadoConexion::firma_rechazada);
    impostor.firma_de_otra.reset();
    impostor.alterar = [](Bytes& datos) { datos[36] ^= 0x01; };
    CHECK(impostor.entrar(servidor, "Tano") == EstadoConexion::firma_rechazada);
    impostor.alterar = nullptr;
    impostor.firmante_del_permiso = prueba::identidad_nueva_de_prueba();
    CHECK(impostor.entrar(servidor, "Tano") == EstadoConexion::firma_rechazada);
    ConLlave con_es256 = tano;
    con_es256.firma_de_otra = LlaveDeMentira();
    CHECK(con_es256.entrar(servidor, "Tano") == EstadoConexion::sin_llave);
    CHECK(servidor.cuantos(Evento::firma_invalida) == 3);
  }
  {
    auto base = ventrik::servidor::Base::abrir(archivo, "General");
    REQUIRE(base);
    const auto llaves = base->llaves();
    REQUIRE(llaves);
    REQUIRE(llaves->size() == 1);
    REQUIRE(llaves->at(0).llaves.size() == 1);
    CHECK(llaves->at(0).llaves.at(0).llave == tano.llave.publica());
    CHECK(llaves->at(0).llaves.at(0).contador == 0);
  }
  opciones.codigo_dueno.reset();
  {
    prueba::ServidorDePrueba servidor(opciones, &identidad);
    prueba::Cliente vuelve(servidor, tano.presentacion("Tano"));
    CHECK(vuelve.entrar().rol == proto::Rol::dueno);
  }
  borrar_base_de_prueba(archivo);
}

TEST_CASE("Una llave nueva no cuenta como segundo paso hasta confirmarla: con TOTP, la primera vez pide también el código") {
  const auto codigo = cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  ventrik::servidor::Opciones opciones;
  opciones.codigo_dueno = codigo;
  opciones.segundo_paso = ventrik::SegundoPaso::admins;
  prueba::ServidorDePrueba servidor(opciones);
  const auto identidad = prueba::identidad_nueva_de_prueba();
  proto::ConfigurarSegundoPaso oferta;
  {
    prueba::Cliente primera(servidor, {"Tano", codigo, identidad, prueba::secreto_de("casa"), nullptr});
    primera.entrar();
    oferta = prueba::activar_segundo_paso(primera);
  }
  ConLlave tano(identidad);
  CHECK(tano.entrar(servidor, "Tano") == EstadoConexion::falta_segundo_paso);
  {
    const auto con_codigo = ventrik::cliente::conectar(
        servidor.direccion(), servidor.huella(),
        tano.presentacion("Tano", "casa", prueba::codigo_totp_de(oferta, prueba::paso_totp_de_ahora() + 1)));
    REQUIRE(con_codigo.estado == EstadoConexion::conectado);
    CHECK(con_codigo.presentado->rol == proto::Rol::dueno);
  }
  const auto sola = ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), tano.presentacion("Tano"));
  REQUIRE(sola.estado == EstadoConexion::conectado);
  CHECK(sola.presentado->rol == proto::Rol::dueno);
}

TEST_CASE("Sin segundo paso, una llave nueva vale desde un equipo conocido o con el permiso que el dueño da desde el programa") {
  const auto codigo = cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  ventrik::servidor::Opciones opciones;
  opciones.codigo_dueno = codigo;
  opciones.segundo_paso = ventrik::SegundoPaso::admins;
  prueba::ServidorDePrueba servidor(opciones);
  prueba::Cliente tano(servidor, "Tano", codigo);
  tano.entrar();
  prueba::activar_segundo_paso(tano);
  const auto admin_sin_paso = [&](const std::shared_ptr<const cifrado::Clave>& identidad, const char* nombre) {
    prueba::Cliente casa(servidor, {nombre, std::nullopt, identidad, prueba::secreto_de("casa"), nullptr});
    const auto id = casa.entrar().tu_id;
    tano.ordenar(proto::NombrarAdmin{id, true});
    REQUIRE(casa.esperar<proto::ConfigurarSegundoPaso>());
  };
  const ConLlave dani(prueba::identidad_nueva_de_prueba());
  const ConLlave beto(prueba::identidad_nueva_de_prueba());
  admin_sin_paso(dani.con_identidad("Dani").identidad, "Dani");
  admin_sin_paso(beto.con_identidad("Beto").identidad, "Beto");
  {
    prueba::Cliente desde_casa(servidor, dani.presentacion("Dani"));
    const auto id = desde_casa.entrar().tu_id;
    CHECK(desde_casa.esperar<proto::UsuarioRol>(
        [id](const proto::UsuarioRol& rol) { return rol.id == id && rol.rol == proto::Rol::admin; }));
  }
  const auto pide_permiso = [&](std::string_view equipo) {
    prueba::Cliente cliente(servidor, {"Beto", std::nullopt, beto.con_identidad("Beto").identidad,
                                       prueba::secreto_de(equipo), nullptr});
    cliente.entrar();
    return cliente.esperar<proto::EstadoSegundoPaso>([](const proto::EstadoSegundoPaso& estado) {
      return estado.falta == proto::FaltaSegundoPaso::pedir_permiso;
    });
  };
  CHECK(pide_permiso("tercera"));
  {
    prueba::Cliente otra(servidor, beto.presentacion("Beto", "otra"));
    const auto id = otra.entrar().tu_id;
    CHECK(otra.esperar<proto::EstadoSegundoPaso>([](const proto::EstadoSegundoPaso& estado) {
      return estado.falta == proto::FaltaSegundoPaso::pedir_permiso && !estado.todos;
    }));
    CHECK(otra.no_llega<proto::ConfigurarSegundoPaso>());
    const auto negado = [](proto::MotivoNegado motivo) {
      return [motivo](const proto::Negado& recibido) {
        return recibido.pedido == proto::TipoMensaje::permitir_segundo_paso && recibido.motivo == motivo;
      };
    };
    otra.ordenar(proto::PermitirSegundoPaso{id});
    CHECK(otra.esperar<proto::Negado>(negado(proto::MotivoNegado::sin_permiso)));
    tano.ordenar(proto::PermitirSegundoPaso{id + 1000});
    CHECK(tano.esperar<proto::Negado>(negado(proto::MotivoNegado::no_existe)));
    tano.ordenar(proto::PermitirSegundoPaso{id});
    CHECK(otra.esperar<proto::UsuarioRol>(
        [id](const proto::UsuarioRol& rol) { return rol.id == id && rol.rol == proto::Rol::admin; }));
    CHECK(otra.esperar<proto::EstadoSegundoPaso>(
        [](const proto::EstadoSegundoPaso& estado) { return estado.falta == proto::FaltaSegundoPaso::nada; }));
  }
  CHECK(pide_permiso("tercera"));
  prueba::Cliente vuelve(servidor, beto.presentacion("Beto", "otra"));
  CHECK(vuelve.entrar().rol == proto::Rol::admin);
  CHECK(servidor.cuantos(Evento::segundo_paso_permitido) == 1);
}

TEST_CASE("Con segundo paso para todos, quien no lo hizo no chatea, no recibe chat ni historial y no entra a la voz, hasta activarlo") {
  const auto codigo = cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  ventrik::servidor::Opciones opciones;
  opciones.codigo_dueno = codigo;
  opciones.segundo_paso = ventrik::SegundoPaso::todos;
  prueba::ServidorDePrueba servidor(opciones);
  prueba::Cliente tano(servidor, "Tano", codigo);
  tano.entrar();
  prueba::activar_segundo_paso(tano);
  tano.ordenar(proto::CrearCanal{"Charla", proto::TipoCanal::texto});
  const auto charla =
      tano.esperar<proto::CanalCreado>([](const proto::CanalCreado& creado) { return creado.canal.nombre == "Charla"; });
  REQUIRE(charla);
  const std::uint32_t canal = charla->canal.id;
  tano.ordenar(proto::MirarCanal{canal});
  prueba::Cliente ana(servidor, "Ana");
  const auto id = ana.entrar().tu_id;
  CHECK(ana.esperar<proto::EstadoSegundoPaso>([](const proto::EstadoSegundoPaso& estado) {
    return estado.falta == proto::FaltaSegundoPaso::activar && estado.todos;
  }));
  const auto oferta = ana.esperar<proto::ConfigurarSegundoPaso>();
  REQUIRE(oferta);
  const auto negado = [](proto::TipoMensaje pedido) {
    return [pedido](const proto::Negado& recibido) {
      return recibido.pedido == pedido && recibido.motivo == proto::MotivoNegado::falta_segundo_paso;
    };
  };
  ana.ordenar(proto::EnviarChat{canal, "hola"});
  CHECK(ana.esperar<proto::Negado>(negado(proto::TipoMensaje::enviar_chat)));
  ana.ordenar(proto::PedirHistorial{canal, 0});
  CHECK(ana.esperar<proto::Negado>(negado(proto::TipoMensaje::pedir_historial)));
  ana.ordenar(proto::MirarCanal{canal});
  CHECK(ana.esperar<proto::Negado>(negado(proto::TipoMensaje::mirar_canal)));
  ana.ordenar(proto::EntrarACanal{ventrik::servidor::kCanalGeneral});
  CHECK(ana.esperar<proto::Negado>(negado(proto::TipoMensaje::entrar_a_canal)));
  tano.ordenar(proto::EnviarChat{canal, "secreto"});
  CHECK(tano.esperar<proto::MensajeChat>());
  CHECK(ana.no_llega<proto::MensajeChat>(1500ms));
  CHECK(ana.no_llega<proto::MensajesNuevos>(0ms));
  ana.ordenar(proto::SegundoPaso{prueba::codigo_totp_de(*oferta, prueba::paso_totp_de_ahora())});
  CHECK(ana.esperar<proto::EstadoSegundoPaso>(
      [](const proto::EstadoSegundoPaso& estado) { return estado.falta == proto::FaltaSegundoPaso::nada; }));
  ana.ordenar(proto::MirarCanal{canal});
  ana.ordenar(proto::PedirHistorial{canal, 0});
  const auto historial = ana.esperar<proto::Historial>();
  REQUIRE(historial);
  CHECK(historial->mensajes.size() == 1);
  tano.ordenar(proto::EnviarChat{canal, "listo"});
  CHECK(ana.esperar<proto::MensajeChat>([](const proto::MensajeChat& mensaje) { return mensaje.texto == "listo"; }));
  ana.ordenar(proto::EntrarACanal{ventrik::servidor::kCanalGeneral});
  CHECK(ana.esperar<proto::UsuarioCambioCanal>([id](const proto::UsuarioCambioCanal& cambio) { return cambio.id == id; }));
}

TEST_CASE("Al pasar a segundo paso para todos en vivo, quien no lo activó sale de la voz y vuelve al activarlo") {
  const auto codigo = cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  ventrik::servidor::Opciones opciones;
  opciones.codigo_dueno = codigo;
  prueba::ServidorDePrueba servidor(opciones);
  prueba::Cliente tano(servidor, "Tano", codigo);
  tano.entrar();
  prueba::Cliente ana(servidor, "Ana");
  const auto id = ana.entrar_con_voz().tu_id;
  ventrik::Seguridad todos;
  REQUIRE(ventrik::cambiar_opcion(todos, ventrik::Opcion::segundo_paso, static_cast<std::uint8_t>(ventrik::SegundoPaso::todos)));
  tano.ordenar(proto::CambiarConfigSeguridad{todos});
  CHECK(ana.esperar<proto::UsuarioCambioCanal>(
      [id](const proto::UsuarioCambioCanal& cambio) { return cambio.id == id && cambio.canal == 0; }));
  CHECK(ana.esperar<proto::EstadoSegundoPaso>([](const proto::EstadoSegundoPaso& estado) {
    return estado.falta == proto::FaltaSegundoPaso::activar && estado.todos;
  }));
  prueba::activar_segundo_paso(ana);
  CHECK(ana.esperar<proto::EstadoSegundoPaso>(
      [](const proto::EstadoSegundoPaso& estado) { return estado.falta == proto::FaltaSegundoPaso::nada; }));
  ana.ordenar(proto::EntrarACanal{ventrik::servidor::kCanalGeneral});
  CHECK(ana.esperar<proto::UsuarioCambioCanal>([id](const proto::UsuarioCambioCanal& cambio) {
    return cambio.id == id && cambio.canal == ventrik::servidor::kCanalGeneral;
  }));
}

TEST_CASE("Diez códigos equivocados bloquean el segundo paso: queda anotado y al dueño le llega el aviso") {
  const std::wstring archivo = base_temporal();
  const auto codigo = cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  auto opciones = sin_bloqueo();
  opciones.archivo_base = archivo;
  opciones.codigo_dueno = codigo;
  opciones.segundo_paso = ventrik::SegundoPaso::admins;
  const auto beto = prueba::identidad_nueva_de_prueba();
  const auto desde_casa = [&beto](std::optional<std::uint32_t> con) {
    return ventrik::cliente::Presentacion{"Beto", std::nullopt, beto, prueba::secreto_de("casa"), nullptr, con};
  };
  {
    prueba::ServidorDePrueba servidor(opciones);
    prueba::Cliente tano(servidor, "Tano", codigo);
    tano.entrar();
    prueba::activar_segundo_paso(tano);
    proto::ConfigurarSegundoPaso oferta;
    {
      prueba::Cliente casa(servidor, desde_casa({}));
      const auto id = casa.entrar().tu_id;
      tano.ordenar(proto::NombrarAdmin{id, true});
      oferta = prueba::activar_segundo_paso(casa);
    }
    const std::uint32_t malo = prueba::codigo_totp_de(oferta, prueba::paso_totp_de_ahora() + 7);
    for (int falla = 0; falla < 10; ++falla) {
      CHECK(ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), desde_casa(malo)).estado ==
            (falla < 9 ? EstadoConexion::segundo_paso_incorrecto : EstadoConexion::segundo_paso_bloqueado));
    }
    const auto aviso = tano.esperar<proto::AvisoDeSeguridad>(
        [](const proto::AvisoDeSeguridad& recibido) { return recibido.tipo == proto::Seguridad::segundo_paso_bloqueado; });
    REQUIRE(aviso);
    CHECK(aviso->quien == "Beto");
  }
  CHECK(anotado(archivo, proto::Seguridad::segundo_paso_bloqueado, 1));
  borrar_base_de_prueba(archivo);
}

TEST_CASE("Una generación anulada no se olvida al liberar el nombre: la llave vieja sigue sin entrar, también al reiniciar") {
  const std::wstring archivo = base_temporal();
  const auto codigo = cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  auto opciones = sin_bloqueo();
  opciones.archivo_base = archivo;
  opciones.codigo_dueno = codigo;
  auto identidad = prueba::identidad_nueva();
  const auto de_ana = prueba::identidad_nueva_de_prueba();
  ConLlave ana(de_ana);
  {
    auto base = ventrik::servidor::Base::abrir(archivo, "General");
    REQUIRE(base);
    ventrik::servidor::UsuarioGuardado guardada;
    guardada.clave = de_ana->publica();
    guardada.nombre = "Ana";
    guardada.creado_ms = guardada.visto_ms = ms_de_ahora();
    guardada.entradas = 3;
    REQUIRE(base->guardar_usuario(guardada));
    REQUIRE(base->guardar_llaves({guardada.clave, 5, {}}));
  }
  {
    prueba::ServidorDePrueba servidor(opciones, &identidad);
    prueba::Cliente tano(servidor, "Tano", codigo);
    tano.entrar();
    ana.generacion = 4;
    CHECK(ana.entrar(servidor) == EstadoConexion::llave_anulada);
    tano.ordenar(proto::LiberarNombre{"Ana"});
    REQUIRE(tano.esperar<proto::AvisoDeSeguridad>(
        [](const proto::AvisoDeSeguridad& aviso) { return aviso.tipo == proto::Seguridad::nombre_liberado; }));
    ana.generacion = 4;
    CHECK(ana.entrar(servidor) == EstadoConexion::llave_anulada);
  }
  {
    opciones.codigo_dueno.reset();
    prueba::ServidorDePrueba servidor(opciones, &identidad);
    CHECK(ana.entrar(servidor) == EstadoConexion::llave_anulada);
    ana.generacion = 5;
    CHECK(ana.entrar(servidor) == EstadoConexion::conectado);
  }
  borrar_base_de_prueba(archivo);
}

TEST_CASE("Al agregar una llave se avisa si el reloj de esta PC va más de dos días adelante del servidor") {
  constexpr std::uint64_t kServidor = 1767225600000ull + 100ull * 24 * 60 * 60 * 1000;
  constexpr std::uint64_t kMinuto = 60000;
  CHECK_FALSE(ventrik::cliente::reloj_adelantado(kServidor, kServidor));
  CHECK_FALSE(ventrik::cliente::reloj_adelantado(kServidor + cifrado::kAdelantoDeGeneracion * kMinuto, kServidor));
  CHECK(ventrik::cliente::reloj_adelantado(kServidor + (cifrado::kAdelantoDeGeneracion + 1) * kMinuto, kServidor));
  CHECK_FALSE(ventrik::cliente::reloj_adelantado(kServidor - 100 * kMinuto, kServidor));
  const std::string_view anulada = ventrik::texto(ventrik::elegir_idioma("es"), ventrik::Clave::conectar_llave_anulada);
  CHECK(anulada.find("la fecha y la hora de esta PC") != std::string_view::npos);
}
