#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "client/webauthn.h"
#include "common/cifrado/clave.h"
#include "common/cifrado/llave.h"
#include "identidad_de_prueba.h"
#include <doctest/doctest.h>

namespace cifrado = ventrik::cifrado;
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

class LlaveDeMentira {
 public:
  Bytes datos(std::uint8_t marcas, std::uint32_t contador, const std::string& aplicacion = "ventrik") const {
    Bytes datos = resumen_de(bytes_de(aplicacion));
    datos.push_back(marcas);
    sumar_grande(datos, contador, 4);
    return datos;
  }

  Bytes firmar(const Bytes& datos, const Bytes& desafio = kDesafio, Ceremonia ceremonia = Ceremonia::firmar) const {
    Bytes firmado = datos;
    sumar(firmado, resumen_de(bytes_de(cifrado::texto_de_cliente(ceremonia, desafio))));
    const auto firma = clave_->firmar(firmado);
    REQUIRE(firma);
    return *firma;
  }

  Bytes cose() const {
    const auto& publica = clave_->publica();
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

  const cifrado::ClavePublica& publica() const { return clave_->publica(); }

 private:
  std::shared_ptr<const cifrado::Clave> clave_ = prueba::identidad_nueva_de_prueba();
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

TEST_CASE("El permiso de la llave firma una etiqueta propia, las dos claves y la generación") {
  const auto identidad = prueba::identidad_nueva_de_prueba();
  const LlaveDeMentira llave;
  const Bytes firmado = cifrado::lo_que_firma_el_permiso(identidad->publica(), llave.publica(), 0x01020304);
  Bytes esperado = bytes_de("ventrik permiso de llave v1");
  esperado.insert(esperado.end(), identidad->publica().begin(), identidad->publica().end());
  esperado.insert(esperado.end(), llave.publica().begin(), llave.publica().end());
  sumar(esperado, {0x04, 0x03, 0x02, 0x01});
  CHECK(firmado == esperado);
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
