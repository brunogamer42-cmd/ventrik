#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "common/protocol/pedidos.h"
#include <doctest/doctest.h>

using namespace ventrik::protocolo;
using ventrik::cifrado::AlgoritmoDeLlave;
using Bytes = std::vector<std::uint8_t>;

namespace {
template <typename Mensaje>
Bytes en_bytes(const Mensaje& mensaje) {
  Escritor escritor;
  escribir(escritor, mensaje);
  REQUIRE(escritor.ok());
  return escritor.datos();
}

template <typename Mensaje>
bool se_escribe(const Mensaje& mensaje) {
  Escritor escritor;
  escribir(escritor, mensaje);
  return escritor.ok();
}

template <typename Mensaje>
void ida_y_vuelta(const Mensaje& mensaje) {
  const Bytes bytes = en_bytes(mensaje);
  const auto leido = leer_mensaje<Mensaje>(bytes);
  REQUIRE(leido);
  CHECK(en_bytes(*leido) == bytes);
  for (std::size_t largo = 0; largo < bytes.size(); ++largo) {
    CHECK_FALSE(leer_mensaje<Mensaje>(gsl::span<const std::uint8_t>(bytes).first(largo)));
  }
  Bytes de_mas = bytes;
  de_mas.push_back(0);
  CHECK_FALSE(leer_mensaje<Mensaje>(de_mas));
}

Bytes con(Bytes bytes, const std::string& texto) {
  bytes.insert(bytes.end(), texto.begin(), texto.end());
  return bytes;
}

Presentarse presentarse(std::optional<std::array<std::uint8_t, 16>> codigo = {}) {
  Presentarse armado{"Ana", codigo};
  armado.clave.fill(4);
  armado.equipo.fill(5);
  armado.firma = {1, 2, 3};
  return armado;
}

Bytes cola_de_presentarse() {
  Bytes cola(kBytesClavePublica, 4);
  cola.insert(cola.end(), kBytesEquipo, 5);
  cola.insert(cola.end(), {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 3, 1, 2, 3});
  return cola;
}
}

TEST_CASE("Los pedidos nuevos tienen bytes fijos") {
  Bytes sin_codigo = {3, 0, 'A', 'n', 'a', 0};
  const Bytes cola = cola_de_presentarse();
  sin_codigo.insert(sin_codigo.end(), cola.begin(), cola.end());
  CHECK(en_bytes(presentarse()) == sin_codigo);
  std::array<std::uint8_t, 16> codigo{};
  codigo.fill(7);
  Bytes con_codigo = {3, 0, 'A', 'n', 'a', 16};
  con_codigo.insert(con_codigo.end(), codigo.begin(), codigo.end());
  con_codigo.insert(con_codigo.end(), cola.begin(), cola.end());
  CHECK(en_bytes(presentarse(codigo)) == con_codigo);
  Presentarse con_contrasena = presentarse();
  con_contrasena.contrasena = "abc";
  Bytes con_contrasena_bytes = {3, 0, 'A', 'n', 'a', 0};
  con_contrasena_bytes.insert(con_contrasena_bytes.end(), kBytesClavePublica, 4);
  con_contrasena_bytes.insert(con_contrasena_bytes.end(), kBytesEquipo, 5);
  con_contrasena_bytes.insert(con_contrasena_bytes.end(), {3, 0, 'a', 'b', 'c', 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 3, 1, 2, 3});
  CHECK(en_bytes(con_contrasena) == con_contrasena_bytes);
  CHECK(en_bytes(PresentacionRechazada{MotivoPresentacion::contrasena_incorrecta, 0x0102}) == Bytes{8, 2, 1});
  CHECK(en_bytes(Desplazado{}).empty());
  CHECK(en_bytes(MensajeChat{0x0102030405060708u, 2, 9, 3, "Ana", 0x10, "hola"}) ==
        Bytes{8, 7, 6, 5, 4, 3, 2, 1, 2, 0, 0, 0, 9, 0, 0, 0, 3, 0, 0, 0, 3, 0, 'A', 'n', 'a',
              0x10, 0, 0, 0, 0, 0, 0, 0, 4, 0, 'h', 'o', 'l', 'a'});
  CHECK(en_bytes(CanalCreado{{3, "Ana", TipoCanal::texto, true, 0x01020304}}) ==
        Bytes{3, 0, 0, 0, 3, 0, 'A', 'n', 'a', 2, 1, 4, 3, 2, 1});
  CHECK(en_bytes(EnviarChat{5, "hola"}) == Bytes{5, 0, 0, 0, 4, 0, 'h', 'o', 'l', 'a'});
  CHECK(en_bytes(CrearCanal{"Ana", TipoCanal::texto, true}) == Bytes{3, 0, 'A', 'n', 'a', 2, 1});
  CHECK(en_bytes(CambiarCanal{2, "Ana", true}) == Bytes{2, 0, 0, 0, 3, 0, 'A', 'n', 'a', 1});
  CHECK(en_bytes(MirarCanal{0x01020304}) == Bytes{4, 3, 2, 1});
  CHECK(en_bytes(MensajesNuevos{{{3, 7}, {4, 0x0100}}}) ==
        Bytes{2, 0, 3, 0, 0, 0, 7, 0, 0, 0, 4, 0, 0, 0, 0, 1, 0, 0});
  CHECK(en_bytes(Negado{TipoMensaje::mirar_canal, MotivoNegado::tipo_de_canal}) == Bytes{18, 0, 7});
  CHECK(en_bytes(Negado{TipoMensaje::crear_canal, MotivoNegado::sin_permiso}) == Bytes{14, 0, 1});
  CHECK(en_bytes(PedirHistorial{5, 9}) == Bytes{5, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1});
  CHECK(en_bytes(PedirHistorial{5, 0x0102, Sentido::nuevos, 64}) ==
        Bytes{5, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 2, 64, 0});
  CHECK(en_bytes(Historial{2, 9, Sentido::nuevos, true, {{10, 3, "Ana", 5, "hola"}}}) ==
        Bytes{2, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 0, 10, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0,
              3, 0, 'A', 'n', 'a', 5, 0, 0, 0, 0, 0, 0, 0, 4, 0, 'h', 'o', 'l', 'a'});
  CHECK(en_bytes(CanalBorrado{4, 1}) == Bytes{4, 0, 0, 0, 1, 0, 0, 0});
  CHECK(en_bytes(UsuarioEntro{{8, "Ana", 2, 3}}) == Bytes{8, 0, 0, 0, 3, 0, 'A', 'n', 'a', 2, 0, 0, 0, 3, 0});
  CHECK(en_bytes(Silenciar{1}) == Bytes{1});
  CHECK(en_bytes(UsuarioSilencio{0x01020304, 3}) == Bytes{4, 3, 2, 1, 3});
  CHECK(en_bytes(Negado{TipoMensaje::silenciar, MotivoNegado::demasiado_rapido}) == Bytes{19, 0, 4});
}

TEST_CASE("Silenciar personas lleva la lista entera, ordenada, sin el 0 y con tope") {
  CHECK(en_bytes(SilenciarPersonas{{2, 0x01020304}}) == Bytes{2, 0, 2, 0, 0, 0, 4, 3, 2, 1});
  ida_y_vuelta(SilenciarPersonas{});
  std::vector<std::uint32_t> tope(kMaximoSilenciados);
  std::iota(tope.begin(), tope.end(), 1u);
  ida_y_vuelta(SilenciarPersonas{tope});
  tope.push_back(kMaximoSilenciados + 1);
  CHECK_FALSE(se_escribe(SilenciarPersonas{tope}));
  for (const auto& mala : std::initializer_list<std::vector<std::uint32_t>>{{0}, {0, 1}, {3, 2}, {2, 2}}) {
    CHECK_FALSE(se_escribe(SilenciarPersonas{mala}));
  }
  CHECK_FALSE(leer_mensaje<SilenciarPersonas>(Bytes{2, 0, 3, 0, 0, 0, 2, 0, 0, 0}));
  CHECK_FALSE(leer_mensaje<SilenciarPersonas>(Bytes{1, 0, 0, 0, 0, 0}));
  CHECK_FALSE(leer_mensaje<SilenciarPersonas>(Bytes{33, 0}));
  CHECK(en_bytes(Negado{TipoMensaje::silenciar_personas, MotivoNegado::demasiado_rapido}) == Bytes{20, 0, 4});
}

TEST_CASE("El silencio solo vale 0, 1 o 3: el audio apagado exige el micrófono apagado") {
  for (const std::uint8_t silencio : std::initializer_list<std::uint8_t>{0, 1, 3}) {
    CHECK(leer_mensaje<Silenciar>(Bytes{silencio}));
    CHECK(leer_mensaje<UsuarioSilencio>(Bytes{1, 0, 0, 0, silencio}));
  }
  for (const std::uint8_t silencio : std::initializer_list<std::uint8_t>{2, 4, 7, 255}) {
    CHECK_FALSE(leer_mensaje<Silenciar>(Bytes{silencio}));
    CHECK_FALSE(leer_mensaje<UsuarioSilencio>(Bytes{1, 0, 0, 0, silencio}));
    CHECK_FALSE(leer_mensaje<UsuarioEntro>(Bytes{8, 0, 0, 0, 3, 0, 'A', 'n', 'a', 2, 0, 0, 0, silencio}));
  }
}

TEST_CASE("Los mensajes de admins y de seguridad vuelven iguales, y con un rol o un tipo desconocido no se leen") {
  ida_y_vuelta(NombrarAdmin{8, true});
  ida_y_vuelta(UsuarioRol{8, Rol::admin});
  ida_y_vuelta(LiberarNombre{"Tano"});
  ida_y_vuelta(AvisoDeSeguridad{Seguridad::canal_borrado, 1761234567890ull, "Tano", "partidas"});
  ida_y_vuelta(AvisoDeSeguridad{Seguridad::equipo_nuevo, 5, "", ""});
  CHECK(en_bytes(UsuarioRol{0x01020304, Rol::dueno}) == Bytes{4, 3, 2, 1, 2});
  CHECK_FALSE(leer_mensaje<UsuarioRol>(Bytes{1, 0, 0, 0, 3}));
  CHECK_FALSE(leer_mensaje<NombrarAdmin>(Bytes{1, 0, 0, 0, 2}));
  CHECK_FALSE(se_escribe(UsuarioRol{1, static_cast<Rol>(3)}));
  CHECK_FALSE(se_escribe(LiberarNombre{""}));
  CHECK_FALSE(se_escribe(AvisoDeSeguridad{static_cast<Seguridad>(0), 1, "", ""}));
  CHECK_FALSE(se_escribe(AvisoDeSeguridad{Seguridad::canal_borrado, 1, " Tano", ""}));
  Bytes malo = en_bytes(AvisoDeSeguridad{Seguridad::canal_borrado, 1, "Tano", "partidas"});
  for (const int tipo : {0, 14, 255}) {
    malo[0] = static_cast<std::uint8_t>(tipo);
    CHECK_FALSE(leer_mensaje<AvisoDeSeguridad>(malo));
  }
}

TEST_CASE("Cada mensaje nuevo se lee tal como se escribió, y cortado o con bytes de más no") {
  std::array<std::uint8_t, 16> codigo{};
  codigo.fill(0xAB);
  ida_y_vuelta(presentarse());
  ida_y_vuelta(presentarse(codigo));
  ida_y_vuelta(Presentado{7, Rol::dueno, "Mi servidor", {{1, "General"}, {2, "Juegos", TipoCanal::voz, false, 4},
                                                  {3, "Charla", TipoCanal::texto, true, 9}},
                          {{7, "Ana", 1, 0, Rol::admin}, {8, "Beto", 2}}});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::nombre_en_uso});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::firma_invalida});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::demasiadas});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::falta_contrasena});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::contrasena_incorrecta, 300});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::ocupado});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::esperar, 65535});
  Presentarse con_contrasena = presentarse();
  con_contrasena.contrasena.assign(kMaximoBytesContrasena, 'a');
  ida_y_vuelta(con_contrasena);
  con_contrasena.contrasena.push_back('a');
  CHECK_FALSE(se_escribe(con_contrasena));
  ida_y_vuelta(UsuarioEntro{{8, "Beto", 1}});
  ida_y_vuelta(UsuarioSalio{8});
  ida_y_vuelta(UsuarioCambioCanal{8, 2});
  ida_y_vuelta(UsuarioEntro{{8, "Beto", 0, 1}});
  ida_y_vuelta(Silenciar{3});
  ida_y_vuelta(UsuarioSilencio{8, 1});
  ida_y_vuelta(CanalCreado{{3, "Música"}});
  ida_y_vuelta(CanalCambiado{{3, "Música 2", TipoCanal::texto, false, 5}});
  ida_y_vuelta(CanalBorrado{3, 1});
  ida_y_vuelta(EntrarACanal{2});
  ida_y_vuelta(EnviarChat{2, "hola\nqué tal 😀"});
  ida_y_vuelta(MensajeChat{1, 2, 5, 3, "Ana", 4, "hola"});
  ida_y_vuelta(PedirHistorial{2, 100});
  ida_y_vuelta(PedirHistorial{2, 100, Sentido::nuevos, 1});
  ida_y_vuelta(Historial{2, 0, Sentido::viejos, true, {{9, 2, "Ana", 5, "uno"}, {8, 1, "Beto", 4, "dos"}}});
  ida_y_vuelta(Historial{2, 10, Sentido::viejos, false, {{9, 2, "Ana", 5, "uno"}}});
  ida_y_vuelta(Historial{2, 7, Sentido::nuevos, true, {{8, 1, "Beto", 4, "dos"}, {9, 2, "Ana", 5, "uno"}}});
  ida_y_vuelta(Historial{2, 7, Sentido::nuevos, false, {}});
  ida_y_vuelta(CrearCanal{"Juegos"});
  ida_y_vuelta(CrearCanal{"Avisos", TipoCanal::texto, true});
  ida_y_vuelta(CambiarCanal{2, "Juegos 2", true});
  ida_y_vuelta(MirarCanal{0});
  ida_y_vuelta(MirarCanal{3});
  ida_y_vuelta(MensajesNuevos{});
  ida_y_vuelta(MensajesNuevos{{{3, 7}, {4, 1}}});
  ida_y_vuelta(Negado{TipoMensaje::mirar_canal, MotivoNegado::tipo_de_canal});
  ida_y_vuelta(BorrarCanal{2});
  ida_y_vuelta(Negado{TipoMensaje::enviar_chat, MotivoNegado::demasiado_rapido});
  ida_y_vuelta(Ping{123});
  ida_y_vuelta(Pong{123});
}

TEST_CASE("Valores que no existen no se aceptan") {
  CHECK_FALSE(leer_mensaje<Presentarse>(Bytes{3, 0, 'A', 'n', 'a', 5, 1, 2, 3, 4, 5}));
  CHECK_FALSE(leer_mensaje<PresentacionRechazada>(Bytes{0, 0, 0}));
  CHECK_FALSE(leer_mensaje<PresentacionRechazada>(Bytes{17, 0, 0}));
  Bytes sin_firma = {3, 0, 'A', 'n', 'a', 0};
  sin_firma.insert(sin_firma.end(), kBytesClavePublica + kBytesEquipo, 1);
  Bytes contrasena_larga = sin_firma;
  contrasena_larga.insert(contrasena_larga.end(), {kMaximoBytesContrasena + 1, 0});
  contrasena_larga.insert(contrasena_larga.end(), kMaximoBytesContrasena + 1, 'a');
  contrasena_larga.insert(contrasena_larga.end(), {3, 1, 2, 3});
  CHECK_FALSE(leer_mensaje<Presentarse>(contrasena_larga));
  sin_firma.insert(sin_firma.end(), {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0});
  Bytes firma_larga = sin_firma;
  sin_firma.push_back(0);
  CHECK_FALSE(leer_mensaje<Presentarse>(sin_firma));
  firma_larga.push_back(static_cast<std::uint8_t>(kMaximoFirma + 1));
  firma_larga.insert(firma_larga.end(), kMaximoFirma + 1, 1);
  CHECK_FALSE(leer_mensaje<Presentarse>(firma_larga));
  CHECK_FALSE(leer_mensaje<Desplazado>(Bytes{0}));
  CHECK_FALSE(leer_mensaje<Presentado>(Bytes{7, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}));
  CHECK_FALSE(leer_mensaje<Negado>(Bytes{30, 0, 1}));
  CHECK_FALSE(leer_mensaje<Negado>(Bytes{1, 0, 1}));
  CHECK_FALSE(leer_mensaje<Negado>(Bytes{14, 0, 0}));
  CHECK_FALSE(leer_mensaje<Negado>(Bytes{14, 0, 11}));
  CHECK_FALSE(leer_mensaje<Negado>(Bytes{32, 0, 1}));
  CHECK_FALSE(leer_mensaje<Negado>(Bytes{53, 0, 1}));
  CHECK_FALSE(leer_mensaje<Presentado>(Bytes{1, 0, 0, 0, 2, 0, 0, 0, 0}));
  CHECK_FALSE(leer_mensaje<Historial>(Bytes{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1}));
}

TEST_CASE("El segundo paso: el código va firmado dentro de Presentarse, tiene 6 cifras y sus mensajes vuelven iguales") {
  Presentarse con_codigo = presentarse();
  con_codigo.segundo_paso = 123456;
  Bytes esperado = {3, 0, 'A', 'n', 'a', 0};
  esperado.insert(esperado.end(), kBytesClavePublica, 4);
  esperado.insert(esperado.end(), kBytesEquipo, 5);
  esperado.insert(esperado.end(), {0, 0, 0x40, 0xE2, 0x01, 0x00, 0, 0, 0, 0, 3, 1, 2, 3});
  CHECK(en_bytes(con_codigo) == esperado);
  ida_y_vuelta(con_codigo);
  con_codigo.segundo_paso = 0;
  ida_y_vuelta(con_codigo);
  const std::array<std::uint8_t, 32> nada{};
  CHECK(lo_que_se_firma(con_codigo, 8, nada, nada) != lo_que_se_firma(presentarse(), 8, nada, nada));
  con_codigo.segundo_paso = kTopeCodigoSegundoPaso;
  CHECK_FALSE(se_escribe(con_codigo));
  esperado[esperado.size() - 12] = 0x40;
  esperado[esperado.size() - 11] = 0x42;
  esperado[esperado.size() - 10] = 0x0F;
  CHECK_FALSE(leer_mensaje<Presentarse>(esperado));
  CHECK(en_bytes(SegundoPaso{123456}) == Bytes{0x40, 0xE2, 0x01, 0x00});
  ida_y_vuelta(SegundoPaso{0});
  ida_y_vuelta(SegundoPaso{999999});
  CHECK_FALSE(se_escribe(SegundoPaso{kTopeCodigoSegundoPaso}));
  CHECK_FALSE(leer_mensaje<SegundoPaso>(Bytes{0x40, 0x42, 0x0F, 0x00}));
  ida_y_vuelta(QuitarSegundoPaso{"Tano"});
  CHECK_FALSE(se_escribe(QuitarSegundoPaso{""}));
  ConfigurarSegundoPaso oferta;
  oferta.secreto.fill(9);
  CHECK(en_bytes(oferta) == Bytes(kBytesSecretoSegundoPaso, 9));
  ida_y_vuelta(oferta);
  ida_y_vuelta(Negado{TipoMensaje::segundo_paso, MotivoNegado::codigo_incorrecto});
  ida_y_vuelta(Negado{TipoMensaje::quitar_segundo_paso, MotivoNegado::no_existe});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::falta_segundo_paso});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::segundo_paso_incorrecto, 4});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::segundo_paso_bloqueado});
  ida_y_vuelta(AvisoDeSeguridad{Seguridad::segundo_paso_configurado, 5, "Tano", ""});
  ida_y_vuelta(AvisoDeSeguridad{Seguridad::segundo_paso_quitado, 5, "Ana", "Tano"});
}

TEST_CASE("La configuración de seguridad viaja con el nivel y cada opción, y una que no es válida no se lee") {
  ventrik::Seguridad seguridad;
  ventrik::elegir_nivel(seguridad, ventrik::Nivel::maximo);
  Bytes esperado = {2};
  esperado.insert(esperado.end(), seguridad.opciones.valores.begin(), seguridad.opciones.valores.end());
  CHECK(en_bytes(CambiarConfigSeguridad{seguridad}) == esperado);
  ida_y_vuelta(CambiarConfigSeguridad{seguridad});
  ida_y_vuelta(ConfigSeguridad{seguridad, true});
  esperado.push_back(1);
  CHECK(en_bytes(ConfigSeguridad{seguridad, true}) == esperado);
  REQUIRE(ventrik::cambiar_opcion(seguridad, ventrik::Opcion::algoritmo, 2));
  ida_y_vuelta(CambiarConfigSeguridad{seguridad});
  CHECK(en_bytes(PedirConfigSeguridad{}).empty());
  ida_y_vuelta(Negado{TipoMensaje::cambiar_config_seguridad, MotivoNegado::sin_permiso});
  ida_y_vuelta(AvisoDeSeguridad{Seguridad::config_cambiada, 5, "Tano", ""});
  ventrik::Seguridad mala = seguridad;
  mala.nivel = ventrik::Nivel::basico;
  CHECK_FALSE(se_escribe(CambiarConfigSeguridad{mala}));
  Bytes mal_nivel = en_bytes(CambiarConfigSeguridad{seguridad});
  mal_nivel[0] = 0;
  CHECK_FALSE(leer_mensaje<CambiarConfigSeguridad>(mal_nivel));
  mal_nivel[0] = 4;
  CHECK_FALSE(leer_mensaje<CambiarConfigSeguridad>(mal_nivel));
  Bytes mal_valor = en_bytes(CambiarConfigSeguridad{seguridad});
  mal_valor[1] = 3;
  CHECK_FALSE(leer_mensaje<CambiarConfigSeguridad>(mal_valor));
}

TEST_CASE("El pedido de historial tiene un sentido conocido y de 1 a 256 mensajes") {
  const auto pedido = [](std::uint8_t sentido, std::uint8_t bajo, std::uint8_t alto) {
    return leer_mensaje<PedirHistorial>(Bytes{1, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, sentido, bajo, alto});
  };
  CHECK(pedido(1, 1, 0));
  CHECK(pedido(2, 0, 1));
  CHECK(pedido(2, 0, 1)->cantidad == kMaximoMensajesEnHistorial);
  CHECK_FALSE(pedido(0, 1, 0));
  CHECK_FALSE(pedido(3, 1, 0));
  CHECK_FALSE(pedido(1, 0, 0));
  CHECK_FALSE(pedido(1, 1, 1));
  CHECK_FALSE(se_escribe(PedirHistorial{1, 0, static_cast<Sentido>(0), 1}));
  CHECK_FALSE(se_escribe(PedirHistorial{1, 0, static_cast<Sentido>(3), 1}));
  CHECK_FALSE(se_escribe(PedirHistorial{1, 0, Sentido::viejos, 0}));
  CHECK_FALSE(se_escribe(PedirHistorial{1, 0, Sentido::nuevos, 257}));
}

TEST_CASE("Un historial viene en el orden de su sentido y sin pasarse del número pedido") {
  const MensajeGuardado ocho{8, 1, "Ana", 1, "a"};
  const MensajeGuardado nueve{9, 2, "Ana", 1, "b"};
  CHECK(se_escribe(Historial{1, 0, Sentido::viejos, false, {nueve, ocho}}));
  CHECK_FALSE(se_escribe(Historial{1, 0, Sentido::viejos, false, {ocho, nueve}}));
  CHECK_FALSE(se_escribe(Historial{1, 0, Sentido::viejos, false, {ocho, ocho}}));
  CHECK_FALSE(se_escribe(Historial{1, 9, Sentido::viejos, false, {nueve}}));
  CHECK(se_escribe(Historial{1, 7, Sentido::nuevos, false, {ocho, nueve}}));
  CHECK_FALSE(se_escribe(Historial{1, 7, Sentido::nuevos, false, {nueve, ocho}}));
  CHECK_FALSE(se_escribe(Historial{1, 8, Sentido::nuevos, false, {ocho}}));
  CHECK_FALSE(se_escribe(Historial{1, 0, static_cast<Sentido>(3), false, {}}));
  const auto con_ids = [](std::uint8_t sentido, std::uint8_t desde, std::uint8_t primero, std::uint8_t segundo) {
    Bytes bytes{1, 0, 0, 0, desde, 0, 0, 0, 0, 0, 0, 0, sentido, 0, 2, 0};
    for (const std::uint8_t id : {primero, segundo}) {
      const Bytes uno{id, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 'a', 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'b'};
      bytes.insert(bytes.end(), uno.begin(), uno.end());
    }
    return leer_mensaje<Historial>(bytes).has_value();
  };
  CHECK(con_ids(1, 0, 9, 8));
  CHECK_FALSE(con_ids(1, 0, 8, 9));
  CHECK_FALSE(con_ids(1, 9, 9, 8));
  CHECK(con_ids(2, 7, 8, 9));
  CHECK_FALSE(con_ids(2, 8, 8, 9));
  CHECK_FALSE(con_ids(2, 7, 9, 9));
  CHECK_FALSE(con_ids(0, 7, 8, 9));
}

TEST_CASE("Nombres y chats con trampas no se escriben ni se leen") {
  CHECK_FALSE(se_escribe(CrearCanal{" Juegos"}));
  CHECK_FALSE(se_escribe(CrearCanal{"a\xE2\x80\xAE" "b"}));
  CHECK_FALSE(se_escribe(Presentarse{"", {}}));
  Presentarse sin_firma = presentarse();
  sin_firma.firma.clear();
  CHECK_FALSE(se_escribe(sin_firma));
  sin_firma.firma.assign(kMaximoFirma + 1, 1);
  CHECK_FALSE(se_escribe(sin_firma));
  CHECK_FALSE(se_escribe(Presentado{7, Rol::dueno, ""}));
  CHECK_FALSE(se_escribe(EnviarChat{1, ""}));
  CHECK_FALSE(se_escribe(EnviarChat{1, "a\xE2\x80\xAE" "b"}));
  CHECK_FALSE(se_escribe(EnviarChat{1, std::string(4001, 'x')}));
  CHECK_FALSE(leer_mensaje<CrearCanal>(con(Bytes{7, 0}, " Juegos")));
  CHECK_FALSE(leer_mensaje<CrearCanal>(con(Bytes{5, 0}, "a\xE2\x80\xAE" "b")));
  CHECK_FALSE(leer_mensaje<EnviarChat>(con(Bytes{1, 0, 0, 0, 3, 0}, "a\tb")));
  CHECK(leer_mensaje<EnviarChat>(con(Bytes{1, 0, 0, 0, 3, 0}, "a\nb")));
}

TEST_CASE("Las listas tienen tope al escribir y al leer") {
  Presentado lleno;
  lleno.servidor = "Servidor";
  lleno.canales.resize(kMaximoCanalesEnLista + 1, CanalInfo{1, "a"});
  CHECK_FALSE(se_escribe(lleno));
  lleno.canales.resize(kMaximoCanalesEnLista);
  CHECK(se_escribe(lleno));
  CHECK_FALSE(leer_mensaje<Presentado>(Bytes{1, 0, 0, 0, 0, 1, 1, 0, 0}));
  Historial historial;
  historial.mensajes.resize(kMaximoMensajesEnHistorial + 1, MensajeGuardado{1, 1, "a", 1, "b"});
  CHECK_FALSE(se_escribe(historial));
  MensajesNuevos nuevos;
  nuevos.canales.resize(kMaximoCanalesEnLista + 1);
  CHECK_FALSE(se_escribe(nuevos));
  CHECK_FALSE(leer_mensaje<MensajesNuevos>(Bytes{1, 1}));
}

TEST_CASE("Un canal es de voz sin marca o de texto con o sin ella; nada más se escribe ni se lee") {
  const auto canal_con = [](std::uint8_t tipo, std::uint8_t solo_admins) {
    return leer_mensaje<CanalCreado>(Bytes{3, 0, 0, 0, 1, 0, 'a', tipo, solo_admins, 0, 0, 0, 0})
        .has_value();
  };
  CHECK(canal_con(1, 0));
  CHECK(canal_con(2, 0));
  CHECK(canal_con(2, 1));
  CHECK_FALSE(canal_con(1, 1));
  CHECK_FALSE(canal_con(0, 0));
  CHECK_FALSE(canal_con(3, 0));
  CHECK_FALSE(canal_con(2, 2));
  CHECK_FALSE(leer_mensaje<CrearCanal>(Bytes{1, 0, 'a', 1, 1}));
  CHECK_FALSE(leer_mensaje<CambiarCanal>(Bytes{2, 0, 0, 0, 1, 0, 'a', 2}));
  CHECK_FALSE(se_escribe(CrearCanal{"a", TipoCanal::voz, true}));
  CHECK_FALSE(se_escribe(CanalCreado{{1, "a", static_cast<TipoCanal>(0)}}));
}

TEST_CASE("Los paquetes de medios llevan el tipo y el número de flujo, con bytes fijos") {
  ClaveVoz clave;
  clave.identidad.fill(1);
  Bytes clave_bytes(16, 1);
  clave_bytes.push_back(0);
  CHECK(en_bytes(clave) == clave_bytes);
  clave.voz_fija = true;
  clave_bytes.back() = 1;
  CHECK(en_bytes(clave) == clave_bytes);
  ida_y_vuelta(clave);
  clave_bytes.back() = 2;
  CHECK_FALSE(leer_mensaje<ClaveVoz>(clave_bytes));
  CHECK(en_bytes(MedioSubida{{TipoFlujo::voz, 0, 0x0102, {9, 8, 7}, 0x0304, 90}}) ==
        Bytes{1, 0, 2, 1, 4, 3, 90, 3, 0, 9, 8, 7});
  CHECK(en_bytes(MedioBajada{5, {TipoFlujo::voz, 0, 0x0102, {9}}}) == Bytes{5, 0, 0, 0, 1, 0, 2, 1, 0, 0, 0, 1, 0, 9});
  ida_y_vuelta(clave);
  ida_y_vuelta(MedioSubida{{TipoFlujo::voz, 0, 7, std::vector<std::uint8_t>(kMaximoOpus, 0xAA), 65535, kNivelMaximo}});
  ida_y_vuelta(MedioBajada{3, {TipoFlujo::voz, 0, 65535, {1}, 20, 0}});
}

TEST_CASE("Un medio de tipo o flujo desconocido, vacío, más largo que el tope o con nivel de más no se escribe ni se lee") {
  CHECK_FALSE(se_escribe(MedioSubida{{TipoFlujo::voz, 0, 1, {}}}));
  CHECK_FALSE(se_escribe(MedioBajada{1, {TipoFlujo::voz, 0, 1, std::vector<std::uint8_t>(kMaximoOpus + 1, 1)}}));
  CHECK_FALSE(se_escribe(MedioSubida{{static_cast<TipoFlujo>(2), 0, 1, {5}}}));
  CHECK_FALSE(se_escribe(MedioSubida{{TipoFlujo::voz, 5, 1, {5}}}));
  CHECK_FALSE(se_escribe(MedioSubida{{TipoFlujo::voz, 0, 1, {5}, 0, kNivelMaximo + 1}}));
  CHECK(leer_mensaje<MedioSubida>(Bytes{1, 0, 1, 0, 0, 0, 127, 1, 0, 5}));
  CHECK_FALSE(leer_mensaje<MedioSubida>(Bytes{1, 0, 1, 0, 0, 0, 128, 1, 0, 5}));
  CHECK_FALSE(leer_mensaje<MedioSubida>(Bytes{0, 0, 1, 0, 0, 0, 0, 1, 0, 5}));
  CHECK_FALSE(leer_mensaje<MedioSubida>(Bytes{2, 0, 1, 0, 0, 0, 0, 1, 0, 5}));
  CHECK_FALSE(leer_mensaje<MedioSubida>(Bytes{1, 5, 1, 0, 0, 0, 0, 1, 0, 5}));
  CHECK_FALSE(leer_mensaje<MedioSubida>(Bytes{1, 0, 1, 0, 0, 0, 0, 0, 0}));
  CHECK_FALSE(leer_mensaje<MedioSubida>(Bytes{1, 0, 1, 0, 0, 0, 0, 1, 0, 5, 6}));
  Bytes largo{1, 0, 1, 0, 0, 0, 0, 0x91, 0x01};
  largo.resize(9 + kMaximoOpus + 1, 0);
  CHECK_FALSE(leer_mensaje<MedioSubida>(largo));
}

TEST_CASE("Los destinos de un susurro llevan el lugar de 1 a 4, el canal y hasta 8 personas en orden y sin el 0") {
  CHECK(en_bytes(DestinosDeSusurro{2, 0x01020304, {5}}) == Bytes{2, 4, 3, 2, 1, 1, 0, 5, 0, 0, 0});
  ida_y_vuelta(DestinosDeSusurro{1, 0, {}});
  ida_y_vuelta(DestinosDeSusurro{4, 7, {1, 2, 3, 4, 5, 6, 7, 8}});
  CHECK_FALSE(se_escribe(DestinosDeSusurro{0, 1, {}}));
  CHECK_FALSE(se_escribe(DestinosDeSusurro{5, 1, {}}));
  CHECK_FALSE(se_escribe(DestinosDeSusurro{1, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9}}));
  CHECK_FALSE(se_escribe(DestinosDeSusurro{1, 0, {3, 2}}));
  CHECK_FALSE(se_escribe(DestinosDeSusurro{1, 0, {0, 2}}));
  CHECK_FALSE(leer_mensaje<DestinosDeSusurro>(Bytes{0, 0, 0, 0, 0, 0, 0}));
  CHECK_FALSE(leer_mensaje<DestinosDeSusurro>(Bytes{5, 0, 0, 0, 0, 0, 0}));
  CHECK_FALSE(leer_mensaje<DestinosDeSusurro>(Bytes{1, 0, 0, 0, 0, 9, 0}));
  CHECK_FALSE(leer_mensaje<DestinosDeSusurro>(Bytes{1, 0, 0, 0, 0, 2, 0, 3, 0, 0, 0, 2, 0, 0, 0}));
  CHECK(en_bytes(MedioSubida{{TipoFlujo::voz, 4, 1, {5}}}) == Bytes{1, 4, 1, 0, 0, 0, 0, 1, 0, 5});
  ida_y_vuelta(MedioBajada{3, {TipoFlujo::voz, 4, 9, {1}, 20, 0}});
  CHECK(en_bytes(Negado{TipoMensaje::destinos_de_susurro, MotivoNegado::no_existe}) == Bytes{21, 0, 2});
}

TEST_CASE("Un datagrama lleva el tipo adelante y nada más") {
  Bytes datagrama;
  REQUIRE(armar_datagrama(MedioSubida{{TipoFlujo::voz, 0, 1, {5}}}, datagrama));
  CHECK(datagrama == Bytes{50, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 5});
  const auto marco = separar_datagrama(datagrama);
  REQUIRE(marco);
  CHECK(marco->tipo == 50);
  CHECK(leer_mensaje<MedioSubida>(marco->cuerpo));
  CHECK_FALSE(separar_datagrama(Bytes{50}));
  CHECK_FALSE(separar_datagrama(Bytes(kMaximoDatagrama + 1, 0)));
  CHECK(separar_datagrama(Bytes{17, 0})->cuerpo.empty());
}

TEST_CASE("Presentarse con llave lleva la llave con su algoritmo y su largo, el permiso, 37 bytes de datos y la firma, en lugar de la firma") {
  Presentarse con_llave = presentarse();
  con_llave.firma.clear();
  con_llave.generacion = 0x01020304;
  EntradaConLlave llave;
  llave.llave = {AlgoritmoDeLlave::es256, Bytes(kBytesClavePublica, 6)};
  llave.llave.clave[0] = 4;
  llave.permiso = {7, 7};
  llave.datos.fill(8);
  llave.firma = {9};
  con_llave.llave = llave;
  Bytes esperado = {3, 0, 'A', 'n', 'a', 0};
  esperado.insert(esperado.end(), kBytesClavePublica, 4);
  esperado.insert(esperado.end(), kBytesEquipo, 5);
  esperado.insert(esperado.end(), {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 4, 3, 2, 1, 0, 1, 65, 0});
  esperado.insert(esperado.end(), llave.llave.clave.begin(), llave.llave.clave.end());
  esperado.insert(esperado.end(), {2, 7, 7});
  esperado.insert(esperado.end(), kBytesDatosDeLlave, 8);
  esperado.insert(esperado.end(), {1, 0, 9});
  CHECK(en_bytes(con_llave) == esperado);
  ida_y_vuelta(con_llave);
  Presentarse con_las_dos = con_llave;
  con_las_dos.firma = {1};
  CHECK_FALSE(se_escribe(con_las_dos));
  for (auto& [permiso, firma] : {std::pair<Bytes, Bytes>{{}, {9}}, {{7}, {}}, {Bytes(kMaximoFirma + 1, 7), {9}},
                                 {{7}, Bytes(kMaximoFirma + 1, 9)}, {{7}, Bytes(256, 9)}}) {
    Presentarse mala = con_llave;
    mala.llave->permiso = permiso;
    mala.llave->firma = firma;
    CHECK_FALSE(se_escribe(mala));
  }
  Bytes permiso_vacio = esperado;
  permiso_vacio[permiso_vacio.size() - kBytesDatosDeLlave - 6] = 0;
  CHECK_FALSE(leer_mensaje<Presentarse>(permiso_vacio));

  Presentarse con_rsa = con_llave;
  con_rsa.llave->llave = {AlgoritmoDeLlave::rs256, Bytes(256, 0x81)};
  con_rsa.llave->firma = Bytes(256, 9);
  const Bytes rsa = en_bytes(con_rsa);
  const std::size_t desde = esperado.size() - (3 + 65 + 3 + kBytesDatosDeLlave + 3);
  Bytes cola_rsa = {2, 0, 1};
  cola_rsa.insert(cola_rsa.end(), 256, 0x81);
  cola_rsa.insert(cola_rsa.end(), {2, 7, 7});
  cola_rsa.insert(cola_rsa.end(), kBytesDatosDeLlave, 8);
  cola_rsa.insert(cola_rsa.end(), {0, 1});
  cola_rsa.insert(cola_rsa.end(), 256, 9);
  CHECK(Bytes(rsa.begin(), rsa.begin() + static_cast<std::ptrdiff_t>(desde)) ==
        Bytes(esperado.begin(), esperado.begin() + static_cast<std::ptrdiff_t>(desde)));
  CHECK(Bytes(rsa.begin() + static_cast<std::ptrdiff_t>(desde), rsa.end()) == cola_rsa);
  ida_y_vuelta(con_rsa);
  for (const auto& firma : {Bytes{9}, Bytes(255, 9), Bytes(257, 9), Bytes(kMaximoFirma, 9)}) {
    Presentarse mala = con_rsa;
    mala.llave->firma = firma;
    CHECK_FALSE(se_escribe(mala));
  }
  for (auto& clave : {Bytes(255, 0x81), Bytes(257, 0x81), Bytes(256, 0x7F), Bytes(256, 0x80), Bytes(65, 4)}) {
    Presentarse mala = con_rsa;
    mala.llave->llave.clave = clave;
    CHECK_FALSE(se_escribe(mala));
  }
  const auto cambiada = [&rsa, desde](std::size_t lugar, std::uint8_t valor) {
    Bytes bytes = rsa;
    bytes[desde + lugar] = valor;
    return leer_mensaje<Presentarse>(bytes).has_value();
  };
  CHECK_FALSE(cambiada(0, 1));
  CHECK_FALSE(cambiada(0, 3));
  CHECK_FALSE(cambiada(0, 0));
  CHECK_FALSE(cambiada(1, 0xFF));
  CHECK_FALSE(cambiada(3, 0x01));
  CHECK_FALSE(cambiada(3 + 255, 0x80));
  CHECK_FALSE(cambiada(3 + 256 + 3 + kBytesDatosDeLlave, 0xFF));
  Bytes firma_corta = rsa;
  firma_corta.pop_back();
  firma_corta[firma_corta.size() - 257] = 0xFF;
  firma_corta[firma_corta.size() - 256] = 0;
  CHECK_FALSE(leer_mensaje<Presentarse>(firma_corta));

  const std::array<std::uint8_t, 32> nada{};
  const auto desafio = desafio_de_llave(con_llave, 8, nada, nada);
  REQUIRE(desafio);
  CHECK_FALSE(desafio_de_llave(presentarse(), 8, nada, nada));
  Presentarse otra_llave = con_llave;
  otra_llave.llave->llave.clave[1] ^= 1;
  Presentarse otra_generacion = con_llave;
  ++otra_generacion.generacion;
  std::array<std::uint8_t, 32> otro_valor{};
  otro_valor[0] = 1;
  for (const auto& otro : {desafio_de_llave(otra_llave, 8, nada, nada), desafio_de_llave(otra_generacion, 8, nada, nada),
                           desafio_de_llave(con_llave, 7, nada, nada), desafio_de_llave(con_llave, 8, otro_valor, nada),
                           desafio_de_llave(con_llave, 8, nada, otro_valor), desafio_de_llave(con_rsa, 8, nada, nada)}) {
    REQUIRE(otro);
    CHECK(*otro != *desafio);
  }
  Presentarse misma_otra_firma = con_llave;
  misma_otra_firma.llave->datos.fill(1);
  misma_otra_firma.llave->firma = {2, 2};
  misma_otra_firma.llave->permiso = {3};
  CHECK(desafio_de_llave(misma_otra_firma, 8, nada, nada) == desafio);
  Presentarse llave_rota = con_llave;
  llave_rota.llave->llave.algoritmo = AlgoritmoDeLlave::rs256;
  CHECK_FALSE(desafio_de_llave(llave_rota, 8, nada, nada));
}

TEST_CASE("El Presentarse más grande, con una llave RS256, entra en el tope de la presentación") {
  std::string nombre;
  for (std::size_t i = 0; i < 32; ++i) {
    nombre += "\xC3\xB1";
  }
  Presentarse grande = presentarse(std::array<std::uint8_t, 16>{});
  grande.nombre = nombre;
  grande.contrasena = std::string(kMaximoBytesContrasena, 'c');
  grande.segundo_paso = kTopeCodigoSegundoPaso - 1;
  grande.firma = Bytes(kMaximoFirma, 1);
  CHECK(en_bytes(grande).size() == 390);
  grande.firma.clear();
  grande.llave = EntradaConLlave{{AlgoritmoDeLlave::rs256, Bytes(256, 0x81)}, Bytes(kMaximoFirma, 7), {}, Bytes(256, 9)};
  const Bytes bytes = en_bytes(grande);
  CHECK(bytes.size() == 945);
  CHECK(bytes.size() <= kMaximoCuerpoPresentacion);
  CHECK(leer_mensaje<Presentarse>(bytes));
}

TEST_CASE("Permitir el segundo paso y el estado del segundo paso vuelven iguales, y un estado desconocido no se lee") {
  CHECK(en_bytes(PermitirSegundoPaso{0x01020304}) == Bytes{4, 3, 2, 1});
  ida_y_vuelta(PermitirSegundoPaso{8});
  for (const auto falta : {FaltaSegundoPaso::nada, FaltaSegundoPaso::activar, FaltaSegundoPaso::pedir_permiso,
                           FaltaSegundoPaso::volver_a_entrar}) {
    ida_y_vuelta(EstadoSegundoPaso{falta, true});
    ida_y_vuelta(EstadoSegundoPaso{falta, false});
  }
  CHECK(en_bytes(EstadoSegundoPaso{FaltaSegundoPaso::pedir_permiso, true}) == Bytes{2, 1});
  CHECK_FALSE(leer_mensaje<EstadoSegundoPaso>(Bytes{4, 0}));
  CHECK_FALSE(se_escribe(EstadoSegundoPaso{static_cast<FaltaSegundoPaso>(4), false}));
  ida_y_vuelta(Negado{TipoMensaje::permitir_segundo_paso, MotivoNegado::no_existe});
  ida_y_vuelta(Negado{TipoMensaje::enviar_chat, MotivoNegado::falta_segundo_paso});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::llave_anulada});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::llave_clonada});
  for (const auto tipo : {Seguridad::segundo_paso_bloqueado, Seguridad::llave_nueva, Seguridad::posible_llave_clonada}) {
    ida_y_vuelta(AvisoDeSeguridad{tipo, 5, "Tano", ""});
  }
  CHECK_FALSE(leer_mensaje<AvisoDeSeguridad>(Bytes{14, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
  CHECK_FALSE(leer_mensaje<Negado>(Bytes{56, 0, 1}));
  CHECK(leer_mensaje<Negado>(Bytes{55, 0, 1}));
}

TEST_CASE("La oferta del segundo paso borra su secreto al destruirse") {
  std::optional<ConfigurarSegundoPaso> oferta(std::in_place);
  oferta->secreto.fill(0xAB);
  const auto* desde = reinterpret_cast<const unsigned char*>(&oferta);
  const std::ptrdiff_t lugar = reinterpret_cast<const unsigned char*>(oferta->secreto.data()) - desde;
  oferta.reset();
  CHECK(std::all_of(desde + lugar, desde + lugar + kBytesSecretoSegundoPaso, [](unsigned char byte) { return byte == 0; }));
}
