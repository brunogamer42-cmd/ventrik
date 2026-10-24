#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "common/protocol/pedidos.h"
#include <doctest/doctest.h>

using namespace ventrik::protocolo;
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
}

TEST_CASE("Los pedidos nuevos tienen bytes fijos") {
  CHECK(en_bytes(Presentarse{"Ana", {}}) == Bytes{3, 0, 'A', 'n', 'a', 0});
  std::array<std::uint8_t, 16> codigo{};
  codigo.fill(7);
  Bytes con_codigo = {3, 0, 'A', 'n', 'a', 16};
  con_codigo.insert(con_codigo.end(), codigo.begin(), codigo.end());
  CHECK(en_bytes(Presentarse{"Ana", codigo}) == con_codigo);
  CHECK(en_bytes(MensajeChat{0x0102030405060708u, 2, 3, "Ana", 0x10, "hola"}) ==
        Bytes{8, 7, 6, 5, 4, 3, 2, 1, 2, 0, 0, 0, 3, 0, 0, 0, 3, 0, 'A', 'n', 'a',
              0x10, 0, 0, 0, 0, 0, 0, 0, 4, 0, 'h', 'o', 'l', 'a'});
  CHECK(en_bytes(Negado{TipoMensaje::crear_canal, MotivoNegado::sin_permiso}) == Bytes{14, 0, 1});
  CHECK(en_bytes(PedirHistorial{5, 9}) == Bytes{5, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0});
  CHECK(en_bytes(CanalBorrado{4, 1}) == Bytes{4, 0, 0, 0, 1, 0, 0, 0});
}

TEST_CASE("Cada mensaje nuevo se lee tal como se escribió, y cortado o con bytes de más no") {
  std::array<std::uint8_t, 16> codigo{};
  codigo.fill(0xAB);
  ida_y_vuelta(Presentarse{"Ana", {}});
  ida_y_vuelta(Presentarse{"Ana", codigo});
  ida_y_vuelta(Presentado{7, true, {{1, "General"}, {2, "Juegos"}}, {{7, "Ana", 1}, {8, "Beto", 2}}});
  ida_y_vuelta(PresentacionRechazada{MotivoPresentacion::nombre_en_uso});
  ida_y_vuelta(UsuarioEntro{{8, "Beto", 1}});
  ida_y_vuelta(UsuarioSalio{8});
  ida_y_vuelta(UsuarioCambioCanal{8, 2});
  ida_y_vuelta(CanalCreado{{3, "Música"}});
  ida_y_vuelta(CanalRenombrado{{3, "Música 2"}});
  ida_y_vuelta(CanalBorrado{3, 1});
  ida_y_vuelta(EntrarACanal{2});
  ida_y_vuelta(EnviarChat{"hola\nqué tal 😀"});
  ida_y_vuelta(MensajeChat{1, 2, 3, "Ana", 4, "hola"});
  ida_y_vuelta(PedirHistorial{2, 100});
  ida_y_vuelta(Historial{2, true, {{9, "Ana", 5, "uno"}, {8, "Beto", 4, "dos"}}});
  ida_y_vuelta(CrearCanal{"Juegos"});
  ida_y_vuelta(RenombrarCanal{2, "Juegos 2"});
  ida_y_vuelta(BorrarCanal{2});
  ida_y_vuelta(Negado{TipoMensaje::enviar_chat, MotivoNegado::demasiado_rapido});
  ida_y_vuelta(Ping{123});
  ida_y_vuelta(Pong{123});
}

TEST_CASE("Valores que no existen no se aceptan") {
  CHECK_FALSE(leer_mensaje<Presentarse>(Bytes{3, 0, 'A', 'n', 'a', 5, 1, 2, 3, 4, 5}));
  CHECK_FALSE(leer_mensaje<PresentacionRechazada>(Bytes{0}));
  CHECK_FALSE(leer_mensaje<PresentacionRechazada>(Bytes{5}));
  CHECK_FALSE(leer_mensaje<Negado>(Bytes{30, 0, 1}));
  CHECK_FALSE(leer_mensaje<Negado>(Bytes{1, 0, 1}));
  CHECK_FALSE(leer_mensaje<Negado>(Bytes{14, 0, 0}));
  CHECK_FALSE(leer_mensaje<Negado>(Bytes{14, 0, 7}));
  CHECK_FALSE(leer_mensaje<Presentado>(Bytes{1, 0, 0, 0, 2, 0, 0, 0, 0}));
  CHECK_FALSE(leer_mensaje<Historial>(Bytes{1, 0, 0, 0, 0, 1, 1}));
}

TEST_CASE("Nombres y chats con trampas no se escriben ni se leen") {
  CHECK_FALSE(se_escribe(CrearCanal{" Juegos"}));
  CHECK_FALSE(se_escribe(CrearCanal{"a\xE2\x80\xAE" "b"}));
  CHECK_FALSE(se_escribe(Presentarse{"", {}}));
  CHECK_FALSE(se_escribe(EnviarChat{""}));
  CHECK_FALSE(se_escribe(EnviarChat{"a\xE2\x80\xAE" "b"}));
  CHECK_FALSE(se_escribe(EnviarChat{std::string(4001, 'x')}));
  CHECK_FALSE(leer_mensaje<CrearCanal>(con(Bytes{7, 0}, " Juegos")));
  CHECK_FALSE(leer_mensaje<CrearCanal>(con(Bytes{5, 0}, "a\xE2\x80\xAE" "b")));
  CHECK_FALSE(leer_mensaje<EnviarChat>(con(Bytes{3, 0}, "a\tb")));
  CHECK(leer_mensaje<EnviarChat>(con(Bytes{3, 0}, "a\nb")));
}

TEST_CASE("Las listas tienen tope al escribir y al leer") {
  Presentado lleno;
  lleno.canales.resize(kMaximoCanalesEnLista + 1, CanalInfo{1, "a"});
  CHECK_FALSE(se_escribe(lleno));
  lleno.canales.resize(kMaximoCanalesEnLista);
  CHECK(se_escribe(lleno));
  CHECK_FALSE(leer_mensaje<Presentado>(Bytes{1, 0, 0, 0, 0, 1, 1, 0, 0}));
  Historial historial;
  historial.mensajes.resize(kMaximoMensajesEnHistorial + 1, MensajeGuardado{1, "a", 1, "b"});
  CHECK_FALSE(se_escribe(historial));
}

TEST_CASE("Los paquetes de medios llevan el tipo y el número de flujo, con bytes fijos") {
  ClaveVoz clave;
  clave.identidad.fill(1);
  CHECK(en_bytes(clave) == Bytes(16, 1));
  CHECK(en_bytes(MedioSubida{{TipoFlujo::voz, 0, 0x0102, {9, 8, 7}}}) == Bytes{1, 0, 2, 1, 3, 0, 9, 8, 7});
  CHECK(en_bytes(MedioBajada{5, {TipoFlujo::voz, 0, 0x0102, {9}}}) == Bytes{5, 0, 0, 0, 1, 0, 2, 1, 1, 0, 9});
  ida_y_vuelta(clave);
  ida_y_vuelta(MedioSubida{{TipoFlujo::voz, 0, 7, std::vector<std::uint8_t>(kMaximoOpus, 0xAA)}});
  ida_y_vuelta(MedioBajada{3, {TipoFlujo::voz, 0, 65535, {1}}});
}

TEST_CASE("Un medio de tipo o flujo desconocido, vacío o más largo que el tope no se escribe ni se lee") {
  CHECK_FALSE(se_escribe(MedioSubida{{TipoFlujo::voz, 0, 1, {}}}));
  CHECK_FALSE(se_escribe(MedioBajada{1, {TipoFlujo::voz, 0, 1, std::vector<std::uint8_t>(kMaximoOpus + 1, 1)}}));
  CHECK_FALSE(se_escribe(MedioSubida{{static_cast<TipoFlujo>(2), 0, 1, {5}}}));
  CHECK_FALSE(se_escribe(MedioSubida{{TipoFlujo::voz, 1, 1, {5}}}));
  CHECK(leer_mensaje<MedioSubida>(Bytes{1, 0, 1, 0, 1, 0, 5}));
  CHECK_FALSE(leer_mensaje<MedioSubida>(Bytes{0, 0, 1, 0, 1, 0, 5}));
  CHECK_FALSE(leer_mensaje<MedioSubida>(Bytes{2, 0, 1, 0, 1, 0, 5}));
  CHECK_FALSE(leer_mensaje<MedioSubida>(Bytes{1, 1, 1, 0, 1, 0, 5}));
  CHECK_FALSE(leer_mensaje<MedioSubida>(Bytes{1, 0, 1, 0, 0, 0}));
  CHECK_FALSE(leer_mensaje<MedioSubida>(Bytes{1, 0, 1, 0, 1, 0, 5, 6}));
  Bytes largo{1, 0, 1, 0, 0x91, 0x01};
  largo.resize(6 + kMaximoOpus + 1, 0);
  CHECK_FALSE(leer_mensaje<MedioSubida>(largo));
}

TEST_CASE("Un datagrama lleva el tipo adelante y nada más") {
  Bytes datagrama;
  REQUIRE(armar_datagrama(MedioSubida{{TipoFlujo::voz, 0, 1, {5}}}, datagrama));
  CHECK(datagrama == Bytes{50, 0, 1, 0, 1, 0, 1, 0, 5});
  const auto marco = separar_datagrama(datagrama);
  REQUIRE(marco);
  CHECK(marco->tipo == 50);
  CHECK(leer_mensaje<MedioSubida>(marco->cuerpo));
  CHECK_FALSE(separar_datagrama(Bytes{50}));
  CHECK_FALSE(separar_datagrama(Bytes(kMaximoDatagrama + 1, 0)));
  CHECK(separar_datagrama(Bytes{17, 0})->cuerpo.empty());
}
