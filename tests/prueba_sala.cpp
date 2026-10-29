#include <algorithm>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "client/sala.h"
#include "client/vigia.h"
#include "identidad_de_prueba.h"
#include "servidor_de_prueba.h"
#include <doctest/doctest.h>

using namespace ventrik::protocolo;
using ventrik::cliente::Orden;
using ventrik::cliente::PingMedido;
using ventrik::cliente::Sala;

namespace {
Presentado presentado(bool dueno = false) {
  Presentado armado;
  armado.tu_id = 7;
  armado.rol = dueno ? ventrik::protocolo::Rol::dueno : ventrik::protocolo::Rol::ninguno;
  armado.canales = {{1, "General"},
                    {2, "Juegos"},
                    {3, "charla", TipoCanal::texto, false, 5},
                    {4, "anuncios", TipoCanal::texto, true, 2}};
  armado.usuarios = {{7, "Ana", 1}, {8, "Beto", 1}, {9, "Carla", 2}};
  return armado;
}

template <typename T>
T como(const Orden& orden) {
  const T* cual = std::get_if<T>(&orden);
  REQUIRE(cual != nullptr);
  return *cual;
}

MensajeChat mensaje(std::uint64_t id, std::uint32_t canal, std::uint32_t numero,
                    std::string texto = "hola") {
  return MensajeChat{id, canal, numero, 8, "Beto", 1000, std::move(texto)};
}

std::vector<std::uint32_t> en(const Sala& sala, std::uint32_t canal) {
  std::vector<std::uint32_t> lista;
  for (const auto* usuario : sala.en_canal(canal)) {
    lista.push_back(usuario->id);
  }
  return lista;
}

std::vector<std::uint64_t> ids(const Sala& sala) {
  std::vector<std::uint64_t> lista;
  for (const auto& guardado : sala.chat().mensajes) {
    lista.push_back(guardado.id);
  }
  return lista;
}

Historial pagina(std::uint32_t canal, std::uint64_t desde, Sentido sentido, std::uint64_t primero,
                 std::uint64_t ultimo, bool hay_mas) {
  Historial historial{canal, desde, sentido, hay_mas, {}};
  for (std::uint64_t id = primero; id <= ultimo; ++id) {
    historial.mensajes.push_back({id, static_cast<std::uint32_t>(id), id % 2 == 0 ? "Ana" : "Beto", 1000, "hola"});
  }
  if (sentido == Sentido::viejos) {
    std::reverse(historial.mensajes.begin(), historial.mensajes.end());
  }
  return historial;
}
}

TEST_CASE("La sala arranca con lo presentado, todo leído, y pide el historial de tu canal") {
  Sala sala(presentado());
  CHECK(sala.tu_canal() == 1);
  CHECK(sala.mirando() == 0);
  CHECK(sala.nuevos(3) == 0);
  CHECK(sala.nuevos(4) == 0);
  CHECK(sala.usuarios().size() == 3);
  const auto ordenes = sala.empezar();
  REQUIRE(ordenes.size() == 1);
  CHECK(como<PedirHistorial>(ordenes[0]).canal == 1);
  CHECK(como<PedirHistorial>(ordenes[0]).desde == 0);
  CHECK(como<PedirHistorial>(ordenes[0]).sentido == Sentido::viejos);
  CHECK(como<PedirHistorial>(ordenes[0]).cantidad == ventrik::cliente::kMensajesAlEntrar);
  CHECK(sala.chat().canal == 1);
  CHECK(sala.chat().pidiendo);
  sala.aplicar(PingMedido{20});
  CHECK(sala.ping() == 20u);
}

TEST_CASE("El silencio de cada uno se actualiza, y el de alguien que no está se ignora") {
  Sala sala(presentado());
  CHECK(sala.usuarios().at(8).silencio == 0);
  sala.aplicar(UsuarioSilencio{8, 3});
  CHECK(sala.usuarios().at(8).silencio == 3);
  sala.aplicar(UsuarioSilencio{99, 1});
  CHECK(sala.usuarios().count(99) == 0);
  CHECK(en(sala, 1) == std::vector<std::uint32_t>{7, 8});
}

TEST_CASE("Sin voz, al empezar se mira el último canal de texto, si no #general, si no el primero, y si no hay, nada") {
  auto sin_voz = presentado();
  sin_voz.usuarios[0].canal = 0;
  const auto mira = [](const Presentado& armado, std::uint32_t texto) {
    Sala sala(armado, {}, texto);
    const auto ordenes = sala.empezar();
    CHECK(sala.tu_canal() == 0);
    return ordenes.empty() ? 0u : como<MirarCanal>(ordenes[0]).canal;
  };
  CHECK(mira(sin_voz, 4) == 4);
  CHECK(mira(sin_voz, 2) == 3);
  CHECK(mira(sin_voz, 99) == 3);
  auto con_general = sin_voz;
  con_general.canales[0].nombre = "Lobby";
  con_general.canales.push_back({5, "GENERAL", TipoCanal::texto, false, 0});
  CHECK(mira(con_general, 0) == 5);
  CHECK(mira(con_general, 4) == 4);
  auto solo_voz = sin_voz;
  solo_voz.canales.resize(2);
  CHECK(mira(solo_voz, 3) == 0);
  Sala sala(sin_voz);
  CHECK(en(sala, 0) == std::vector<std::uint32_t>{7});
  sala.empezar();
  CHECK(sala.chat().canal == 3);
  CHECK(sala.ultimo_texto() == 3);
}

TEST_CASE("Al tocar un canal de voz sin voz, la sala espera la respuesta antes de volver a un chat de texto") {
  auto sin_voz = presentado();
  sin_voz.usuarios[0].canal = 0;
  Sala sala(sin_voz, {}, 3);
  sala.empezar();
  REQUIRE(sala.chat().canal == 3);
  const auto ordenes = sala.elegir_voz(2);
  REQUIRE(ordenes.size() == 2);
  CHECK(como<EntrarACanal>(ordenes[1]).canal == 2);
  CHECK(sala.revisar().empty());
  sala.aplicar(UsuarioCambioCanal{8, 2});
  CHECK(sala.revisar().empty());
  sala.aplicar(UsuarioCambioCanal{7, 2});
  CHECK(sala.chat().canal == 2);
  CHECK(sala.revisar().empty());

  Sala negada(sin_voz, {}, 3);
  negada.empezar();
  negada.elegir_voz(2);
  CHECK(negada.revisar().empty());
  negada.aplicar(Negado{TipoMensaje::entrar_a_canal, MotivoNegado::demasiado_rapido});
  const auto vuelta = negada.revisar();
  REQUIRE(vuelta.size() == 2);
  CHECK(como<MirarCanal>(vuelta[0]).canal == 3);
}

TEST_CASE("Salir de la voz pasa al chat de texto, y si niegan mirarlo se vuelve a probar pasado 1 s") {
  Sala sala(presentado(), {}, 4);
  sala.empezar();
  REQUIRE(sala.salir_de_la_voz().size() == 1);
  CHECK(como<EntrarACanal>(sala.salir_de_la_voz()[0]).canal == 0);
  CHECK(sala.aplicar(UsuarioCambioCanal{7, 0}).empty());
  CHECK(sala.tu_canal() == 0);
  CHECK(sala.salir_de_la_voz().empty());
  CHECK(en(sala, 0) == std::vector<std::uint32_t>{7});
  auto ordenes = sala.revisar();
  REQUIRE(ordenes.size() == 2);
  CHECK(como<MirarCanal>(ordenes[0]).canal == 4);
  CHECK(como<PedirHistorial>(ordenes[1]).canal == 4);
  CHECK(sala.revisar().empty());
  sala.aplicar(Negado{TipoMensaje::mirar_canal, MotivoNegado::demasiado_rapido});
  CHECK(sala.chat().canal == 0);
  CHECK(sala.revisar().empty());
  std::this_thread::sleep_for(ventrik::cliente::kEsperaTrasRechazo + std::chrono::milliseconds(50));
  ordenes = sala.revisar();
  REQUIRE(ordenes.size() == 2);
  CHECK(como<MirarCanal>(ordenes[0]).canal == 4);
  sala.aplicar(CanalBorrado{4, 1});
  ordenes = sala.revisar();
  REQUIRE(ordenes.size() == 2);
  CHECK(como<MirarCanal>(ordenes[0]).canal == 3);
  CHECK(sala.ultimo_texto() == 3);
}

TEST_CASE("Con lo leído guardado, cada canal sigue desde donde quedó y uno nuevo arranca sin leer") {
  Sala sala(presentado(), {{1, 0}, {3, 2}, {4, 99}, {7, 5}});
  CHECK(sala.nuevos(3) == 3);
  CHECK(sala.nuevos(4) == 0);
  CHECK(sala.nuevos(2) == 0);
  CHECK(sala.leido(3) == 2);
  CHECK(sala.leido(2) == 0);
  sala.aplicar(MensajesNuevos{{{2, 6}, {4, 3}}});
  CHECK(sala.nuevos(2) == 6);
  CHECK(sala.nuevos(4) == 1);
  const auto leidos = sala.leidos();
  REQUIRE(leidos.size() == 3);
  CHECK(leidos[0].canal == 1);
  CHECK(leidos[1].canal == 3);
  CHECK(leidos[1].numero == 2);
  CHECK(leidos[2].canal == 4);
  CHECK(leidos[2].numero == 2);
  sala.elegir_texto(3);
  CHECK(sala.leidos()[1].numero == 5);
}

TEST_CASE("Un canal con número 0 se ignora, así lo leído siempre se puede guardar") {
  auto con_cero = presentado();
  con_cero.canales.push_back({0, "cero", TipoCanal::texto, false, 3});
  Sala sala(con_cero);
  sala.aplicar(CanalCreado{{0, "otro cero", TipoCanal::texto, false, 1}});
  CHECK(sala.canal(0) == nullptr);
  CHECK(sala.canales().size() == 4);
  for (const auto& leido : sala.leidos()) {
    CHECK(leido.canal != 0);
  }
}

TEST_CASE("Sin nada guardado todo cuenta como leído, y lo leído que se guarda tiene tope") {
  Sala sala(presentado());
  CHECK(sala.leidos().size() == 4);
  CHECK(sala.leidos()[2].numero == 5);
  for (std::uint32_t id = 10; id < 200; ++id) {
    sala.aplicar(CanalCreado{{id, "c", TipoCanal::texto, false, 0}});
    sala.elegir_texto(id);
  }
  CHECK(sala.leidos().size() == ventrik::cliente::kMaximoLeidosPorServidor);
}

TEST_CASE("Las personas entran, salen y se mueven; si te movés vos, se pide el historial del canal nuevo") {
  Sala sala(presentado());
  CHECK(en(sala, 1) == std::vector<std::uint32_t>{7, 8});
  CHECK(en(sala, 2) == std::vector<std::uint32_t>{9});
  sala.aplicar(UsuarioEntro{{10, "Dani", 2}});
  sala.aplicar(UsuarioCambioCanal{8, 2});
  sala.aplicar(UsuarioSalio{9});
  CHECK(sala.usuarios().size() == 3);
  CHECK(sala.usuarios().at(8).canal == 2);
  CHECK(en(sala, 1) == std::vector<std::uint32_t>{7});
  CHECK(en(sala, 2) == std::vector<std::uint32_t>{8, 10});
  CHECK(sala.aplicar(UsuarioCambioCanal{99, 2}).empty());
  const auto ordenes = sala.aplicar(UsuarioCambioCanal{7, 2});
  REQUIRE(ordenes.size() == 1);
  CHECK(como<PedirHistorial>(ordenes[0]).canal == 2);
  CHECK(sala.tu_canal() == 2);
  CHECK(sala.chat().canal == 2);
  CHECK(en(sala, 1).empty());
  CHECK(en(sala, 2) == std::vector<std::uint32_t>{7, 8, 10});
}

TEST_CASE("Elegir un canal de texto lo mira y pide su historial; tocar tu canal de voz vuelve a su chat") {
  Sala sala(presentado());
  sala.aplicar(MensajesNuevos{{{3, 8}}});
  CHECK(sala.nuevos(3) == 3);
  auto ordenes = sala.elegir_texto(3);
  REQUIRE(ordenes.size() == 2);
  CHECK(como<MirarCanal>(ordenes[0]).canal == 3);
  CHECK(como<PedirHistorial>(ordenes[1]).canal == 3);
  CHECK(sala.mirando() == 3);
  CHECK(sala.nuevos(3) == 0);
  CHECK(sala.elegir_texto(3).empty());
  CHECK(sala.elegir_texto(1).empty());
  CHECK(sala.elegir_texto(99).empty());
  CHECK(sala.elegir_voz(3).empty());
  CHECK(sala.mirando() == 3);
  ordenes = sala.elegir_voz(1);
  REQUIRE(ordenes.size() == 1);
  CHECK(como<MirarCanal>(ordenes[0]).canal == 0);
  CHECK(sala.mirando() == 0);
  CHECK(sala.elegir_voz(1).empty());
  sala.elegir_texto(4);
  ordenes = sala.elegir_voz(2);
  REQUIRE(ordenes.size() == 2);
  CHECK(como<MirarCanal>(ordenes[0]).canal == 0);
  CHECK(como<EntrarACanal>(ordenes[1]).canal == 2);
}

TEST_CASE("Los mensajes van al chat de su canal y suben los contadores") {
  Sala sala(presentado());
  sala.aplicar(mensaje(10, 1, 1));
  CHECK(ids(sala) == std::vector<std::uint64_t>{10});
  sala.aplicar(mensaje(11, 4, 3));
  CHECK(sala.nuevos(4) == 1);
  CHECK(ids(sala) == std::vector<std::uint64_t>{10});
  sala.elegir_texto(3);
  sala.aplicar(mensaje(12, 3, 9));
  sala.aplicar(mensaje(12, 3, 9));
  sala.aplicar(mensaje(13, 1, 2));
  CHECK(ids(sala) == std::vector<std::uint64_t>{12});
  CHECK(sala.nuevos(3) == 0);
  CHECK(sala.canal(3)->contador == 9);
  sala.elegir_voz(1);
  CHECK(ids(sala) == std::vector<std::uint64_t>{10, 13});
}

TEST_CASE("El historial se junta sin repetir, en orden y de a un pedido") {
  Sala sala(presentado());
  sala.elegir_texto(3);
  CHECK_FALSE(sala.pedir_mas_viejos());
  sala.aplicar(mensaje(50, 3, 6));
  sala.aplicar(Historial{3, 0, Sentido::viejos, true,
                         {{50, 6, "Beto", 1, "c"}, {40, 5, "Ana", 1, "b"}, {30, 4, "Beto", 1, "a"}}});
  CHECK(ids(sala) == std::vector<std::uint64_t>{30, 40, 50});
  CHECK(sala.chat().hay_mas);
  CHECK_FALSE(sala.chat().pidiendo);
  const auto pedido = sala.pedir_mas_viejos();
  REQUIRE(pedido);
  CHECK(como<PedirHistorial>(*pedido).canal == 3);
  CHECK(como<PedirHistorial>(*pedido).desde == 30);
  CHECK(como<PedirHistorial>(*pedido).sentido == Sentido::viejos);
  CHECK(como<PedirHistorial>(*pedido).cantidad == ventrik::cliente::kMensajesAlMoverse);
  CHECK_FALSE(sala.pedir_mas_viejos());
  sala.aplicar(Historial{2, 30, Sentido::viejos, false, {{5, 1, "Ana", 1, "otro canal"}}});
  sala.aplicar(Historial{3, 0, Sentido::viejos, false, {{15, 1, "Ana", 1, "otro pedido"}}});
  sala.aplicar(Historial{3, 30, Sentido::nuevos, false, {{31, 1, "Ana", 1, "otro sentido"}}});
  CHECK(sala.chat().pidiendo);
  sala.aplicar(Historial{3, 30, Sentido::viejos, false, {{20, 3, "Ana", 1, "z"}}});
  CHECK(ids(sala) == std::vector<std::uint64_t>{20, 30, 40, 50});
  CHECK_FALSE(sala.pedir_mas_viejos());
  sala.aplicar(Historial{3, 20, Sentido::viejos, true, {{10, 2, "Ana", 1, "sin pedir"}}});
  CHECK(ids(sala) == std::vector<std::uint64_t>{20, 30, 40, 50});
  CHECK_FALSE(sala.chat().hay_mas);
}

TEST_CASE("Cada nombre se guarda una sola vez por chat") {
  Sala sala(presentado());
  sala.empezar();
  sala.aplicar(Historial{1, 0, Sentido::viejos, false,
                         {{4, 4, "Ana", 1, "d"}, {3, 3, "Beto", 1, "c"}, {2, 2, "Ana", 1, "b"}, {1, 1, "Ana", 1, "a"}}});
  sala.aplicar(mensaje(5, 1, 5));
  const auto& chat = sala.chat();
  REQUIRE(chat.mensajes.size() == 5);
  CHECK(chat.autores.size() == 2);
  CHECK(chat.autor(chat.mensajes[0]) == "Ana");
  CHECK(chat.autor(chat.mensajes[2]) == "Beto");
  CHECK(chat.autor(chat.mensajes[4]) == "Beto");
  CHECK(chat.mensajes[0].autor == chat.mensajes[3].autor);
  CHECK(chat.bytes == 4 + 4);
}

TEST_CASE("Al entrar se piden 64; al subir se piden más y, pasada la ventana, se sueltan los más nuevos") {
  using ventrik::cliente::kVentanaChat;
  Sala sala(presentado());
  sala.empezar();
  sala.aplicar(pagina(1, 0, Sentido::viejos, 937, 1000, true));
  CHECK(sala.chat().mensajes.size() == 64);
  CHECK_FALSE(sala.pedir_mas_nuevos());
  for (std::uint64_t primero = 937; primero > 553; primero -= 128) {
    sala.a_la_vista(primero);
    const auto pedido = sala.pedir_mas_viejos();
    REQUIRE(pedido);
    CHECK(como<PedirHistorial>(*pedido).desde == primero);
    sala.aplicar(pagina(1, primero, Sentido::viejos, primero - 128, primero - 1, true));
  }
  CHECK(sala.chat().mensajes.size() == kVentanaChat);
  CHECK(sala.chat().mensajes.front().id == 553);
  CHECK(sala.chat().mensajes.back().id == 852);
  CHECK(sala.chat().hay_mas);
  CHECK(sala.chat().hay_nuevos);
  sala.aplicar(mensaje(1001, 1, 1001));
  CHECK(sala.chat().mensajes.back().id == 852);
  CHECK(sala.canal(1)->contador == 1001);

  sala.a_la_vista(840);
  const auto nuevos = sala.pedir_mas_nuevos();
  REQUIRE(nuevos);
  CHECK(como<PedirHistorial>(*nuevos).desde == 852);
  CHECK(como<PedirHistorial>(*nuevos).sentido == Sentido::nuevos);
  CHECK(como<PedirHistorial>(*nuevos).cantidad == ventrik::cliente::kMensajesAlMoverse);
  sala.aplicar(pagina(1, 852, Sentido::nuevos, 853, 980, true));
  CHECK(sala.chat().mensajes.size() == kVentanaChat);
  CHECK(sala.chat().mensajes.back().id == 980);
  CHECK(sala.chat().hay_nuevos);
  sala.a_la_vista(970);
  REQUIRE(sala.pedir_mas_nuevos());
  sala.aplicar(pagina(1, 980, Sentido::nuevos, 981, 1001, false));
  CHECK_FALSE(sala.chat().hay_nuevos);
  sala.a_la_vista(0);
  sala.aplicar(mensaje(1002, 1, 1002));
  CHECK(sala.chat().mensajes.back().id == 1002);
  CHECK(sala.chat().mensajes.size() == kVentanaChat);
  CHECK(sala.chat().mensajes.front().id == 703);
}

TEST_CASE("Ir a lo último descarta la ventana y pide la última página") {
  Sala sala(presentado());
  sala.empezar();
  CHECK_FALSE(sala.ir_a_lo_ultimo());
  sala.aplicar(pagina(1, 0, Sentido::viejos, 937, 1000, true));
  sala.a_la_vista(937);
  for (std::uint64_t primero = 937; primero > 553; primero -= 128) {
    REQUIRE(sala.pedir_mas_viejos());
    sala.aplicar(pagina(1, primero, Sentido::viejos, primero - 128, primero - 1, true));
    sala.a_la_vista(primero - 128);
  }
  REQUIRE(sala.chat().hay_nuevos);
  const auto ultimo = sala.ir_a_lo_ultimo();
  REQUIRE(ultimo);
  CHECK(como<PedirHistorial>(*ultimo).desde == 0);
  CHECK(como<PedirHistorial>(*ultimo).cantidad == ventrik::cliente::kMensajesAlEntrar);
  CHECK(sala.chat().mensajes.empty());
  CHECK_FALSE(sala.chat().hay_nuevos);
  sala.aplicar(mensaje(1001, 1, 1001));
  sala.aplicar(pagina(1, 0, Sentido::viejos, 938, 1001, true));
  CHECK(sala.chat().mensajes.size() == 64);
  CHECK(sala.chat().mensajes.back().id == 1001);
}

TEST_CASE("Soltar lo lejano deja una página alrededor de lo que se ve y en el otro chat lo último") {
  Sala sala(presentado());
  sala.empezar();
  sala.aplicar(pagina(1, 0, Sentido::viejos, 701, 1000, true));
  sala.elegir_texto(3);
  sala.aplicar(pagina(3, 0, Sentido::viejos, 1, 200, false));
  sala.a_la_vista(100);
  sala.soltar_lejanos();
  const auto& texto = sala.chat();
  CHECK(texto.mensajes.size() == ventrik::cliente::kMensajesAlEntrar);
  CHECK(texto.mensajes.front().id == 68);
  CHECK(texto.mensajes.back().id == 131);
  CHECK(texto.hay_mas);
  CHECK(texto.hay_nuevos);
  CHECK(texto.autores.size() == 2);
  sala.elegir_voz(1);
  CHECK(sala.chat().mensajes.size() == ventrik::cliente::kMensajesAlEntrar);
  CHECK(sala.chat().mensajes.back().id == 1000);
  CHECK_FALSE(sala.chat().hay_nuevos);
}

TEST_CASE("Si el servidor rechaza pedir los más nuevos, se vuelven a pedir y no se toman como más viejos") {
  Sala sala(presentado());
  sala.empezar();
  sala.aplicar(pagina(1, 0, Sentido::viejos, 1, 64, false));
  sala.a_la_vista(1);
  for (std::uint64_t id = 65; id < 400; ++id) {
    sala.aplicar(mensaje(id, 1, static_cast<std::uint32_t>(id)));
  }
  REQUIRE(sala.chat().hay_nuevos);
  CHECK_FALSE(sala.chat().hay_mas);
  REQUIRE(sala.pedir_mas_nuevos());
  sala.aplicar(Negado{TipoMensaje::pedir_historial, MotivoNegado::demasiado_rapido});
  CHECK(sala.chat().hay_nuevos);
  CHECK_FALSE(sala.chat().hay_mas);
  CHECK_FALSE(sala.pedir_mas_nuevos());
  std::this_thread::sleep_for(ventrik::cliente::kEsperaTrasRechazo + std::chrono::milliseconds(50));
  REQUIRE(sala.pedir_mas_nuevos());
}

TEST_CASE("Una respuesta que ya no toca el borde del tramo se descarta y se vuelve a pedir, sin dejar huecos") {
  Sala sala(presentado());
  sala.empezar();
  sala.aplicar(pagina(1, 0, Sentido::viejos, 1, 64, false));
  sala.a_la_vista(1);
  for (std::uint64_t id = 65; id < 400; ++id) {
    sala.aplicar(mensaje(id, 1, static_cast<std::uint32_t>(id)));
  }
  const auto pedido = sala.pedir_mas_nuevos();
  REQUIRE(pedido);
  const std::uint64_t borde = como<PedirHistorial>(*pedido).desde;
  sala.soltar_lejanos();
  REQUIRE(sala.chat().mensajes.back().id < borde);
  sala.aplicar(pagina(1, borde, Sentido::nuevos, borde + 1, borde + 50, true));
  CHECK(sala.chat().mensajes.back().id < borde);
  CHECK_FALSE(sala.chat().pidiendo);
  CHECK(sala.chat().hay_nuevos);
  const auto otra = sala.pedir_mas_nuevos();
  REQUIRE(otra);
  CHECK(como<PedirHistorial>(*otra).desde == sala.chat().mensajes.back().id);
}

TEST_CASE("Borrar el canal que mirabas vuelve al chat de voz, y borrar tu canal de voz te pasa al que dice") {
  Sala sala(presentado());
  sala.elegir_texto(3);
  CHECK(sala.aplicar(CanalBorrado{3, 1}).empty());
  CHECK(sala.mirando() == 0);
  CHECK(sala.canal(3) == nullptr);
  CHECK(sala.chat().canal == 1);
  sala.aplicar(UsuarioCambioCanal{7, 2});
  const auto ordenes = sala.aplicar(CanalBorrado{2, 1});
  REQUIRE(ordenes.size() == 1);
  CHECK(como<PedirHistorial>(ordenes[0]).canal == 1);
  CHECK(sala.tu_canal() == 1);
  CHECK(sala.usuarios().at(9).canal == 1);
}

TEST_CASE("Un canal nuevo arranca sin leer y uno cambiado no pierde su contador") {
  Sala sala(presentado());
  sala.aplicar(CanalCreado{{5, "nuevo", TipoCanal::texto, false, 0}});
  sala.aplicar(CanalCreado{{5, "repetido", TipoCanal::texto, false, 0}});
  sala.aplicar(MensajesNuevos{{{5, 2}, {99, 4}}});
  CHECK(sala.canales().size() == 5);
  CHECK(sala.canal(5)->nombre == "nuevo");
  CHECK(sala.nuevos(5) == 2);
  sala.aplicar(CanalCambiado{{4, "avisos", TipoCanal::texto, false, 1}});
  CHECK(sala.canal(4)->nombre == "avisos");
  CHECK_FALSE(sala.canal(4)->solo_admins);
  CHECK(sala.canal(4)->contador == 2);
}

TEST_CASE("Solo el dueño escribe donde solo escriben los admins, y no se manda un chat inválido") {
  Sala invitado(presentado());
  const auto en_voz = invitado.enviar("hola");
  REQUIRE(en_voz);
  CHECK(como<EnviarChat>(*en_voz).canal == 1);
  CHECK(como<EnviarChat>(*en_voz).texto == "hola");
  CHECK_FALSE(invitado.enviar(""));
  invitado.elegir_texto(4);
  CHECK_FALSE(invitado.puede_escribir());
  CHECK_FALSE(invitado.enviar("hola"));
  Sala dueno(presentado(true));
  dueno.elegir_texto(4);
  CHECK(dueno.puede_escribir());
  const auto en_anuncios = dueno.enviar("hola");
  REQUIRE(en_anuncios);
  CHECK(como<EnviarChat>(*en_anuncios).canal == 4);
}

TEST_CASE("El rol de cada uno se actualiza; si te hacen admin escribís donde solo escriben los admins") {
  Sala sala(presentado());
  sala.elegir_texto(4);
  CHECK_FALSE(sala.admin());
  CHECK_FALSE(sala.puede_escribir());
  sala.aplicar(UsuarioRol{8, Rol::admin});
  CHECK(sala.usuarios().at(8).rol == Rol::admin);
  CHECK_FALSE(sala.admin());
  sala.aplicar(UsuarioRol{7, Rol::admin});
  CHECK(sala.admin());
  CHECK_FALSE(sala.dueno());
  CHECK(sala.puede_escribir());
  sala.aplicar(UsuarioRol{99, Rol::admin});
  CHECK(sala.usuarios().count(99) == 0);
  sala.aplicar(UsuarioRol{7, Rol::ninguno});
  CHECK_FALSE(sala.puede_escribir());
}

TEST_CASE("Un rechazo queda como aviso hasta la próxima acción y destraba el historial") {
  Sala sala(presentado());
  sala.elegir_texto(3);
  sala.aplicar(Negado{TipoMensaje::pedir_historial, MotivoNegado::demasiado_rapido});
  REQUIRE(sala.negado());
  CHECK(sala.negado()->motivo == MotivoNegado::demasiado_rapido);
  CHECK_FALSE(sala.chat().pidiendo);
  sala.enviar("hola");
  CHECK_FALSE(sala.negado());
}

TEST_CASE("Al elegir la tecla para hablar solo cuenta una que se apretó después de empezar") {
  using ventrik::cliente::Teclas;
  using ventrik::cliente::tecla_nueva;
  Teclas antes;
  antes.set(0x20);
  antes.set(0x01);
  Teclas ahora = antes;
  CHECK(tecla_nueva(antes, ahora) == -1);
  ahora.set(0x02);
  CHECK(tecla_nueva(antes, ahora) == -1);
  ahora.set(0x05);
  CHECK(tecla_nueva(antes, ahora) == 0x05);
  ahora.set(0x1B);
  CHECK(tecla_nueva(antes, ahora) == 0);
  antes.reset();
  ahora.reset();
  ahora.set(0x12);
  CHECK(tecla_nueva(antes, ahora) == -1);
  ahora.set(0xA4);
  CHECK(tecla_nueva(antes, ahora) == 0xA4);
}

TEST_CASE("Si el servidor no deja mirar un canal, se vuelve al chat de voz") {
  Sala sala(presentado());
  sala.elegir_texto(3);
  sala.aplicar(Negado{TipoMensaje::mirar_canal, MotivoNegado::demasiado_rapido});
  CHECK(sala.mirando() == 0);
  CHECK(sala.chat().canal == 1);
}

TEST_CASE("Si el servidor rechaza el historial, se vuelve a pedir después de un segundo, también la primera parte") {
  Sala sala(presentado());
  sala.elegir_texto(3);
  sala.aplicar(Negado{TipoMensaje::pedir_historial, MotivoNegado::demasiado_rapido});
  CHECK(sala.chat().hay_mas);
  CHECK_FALSE(sala.pedir_mas_viejos());
  std::this_thread::sleep_for(ventrik::cliente::kEsperaTrasRechazo + std::chrono::milliseconds(50));
  const auto pedido = sala.pedir_mas_viejos();
  REQUIRE(pedido);
  CHECK(como<PedirHistorial>(*pedido).canal == 3);
  CHECK(como<PedirHistorial>(*pedido).desde == 0);
  CHECK(como<PedirHistorial>(*pedido).cantidad == ventrik::cliente::kMensajesAlEntrar);
}

TEST_CASE("La sala no guarda más personas ni canales que los topes del protocolo") {
  Sala sala(presentado());
  for (std::uint32_t id = 100; id < 100 + kMaximoUsuariosEnLista + 50; ++id) {
    sala.aplicar(UsuarioEntro{{id, "Alguien", 1}});
    sala.aplicar(CanalCreado{{id, "canal", TipoCanal::voz, false, 0}});
  }
  CHECK(sala.usuarios().size() == kMaximoUsuariosEnLista);
  CHECK(sala.canales().size() == kMaximoCanalesEnLista);
  sala.aplicar(UsuarioEntro{{7, "Ana", 2}});
  CHECK(sala.usuarios().at(7).canal == 2);
}

TEST_CASE("Un rechazo con un motivo que no se conoce se explica como falta de permiso") {
  const auto idioma = ventrik::elegir_idioma("es");
  CHECK(ventrik::cliente::explicar(idioma, static_cast<MotivoNegado>(0)) ==
        ventrik::cliente::explicar(idioma, MotivoNegado::sin_permiso));
  CHECK(ventrik::cliente::explicar(idioma, static_cast<MotivoNegado>(200)) ==
        ventrik::cliente::explicar(idioma, MotivoNegado::sin_permiso));
  CHECK(ventrik::cliente::explicar(idioma, MotivoNegado::tipo_de_canal) !=
        ventrik::cliente::explicar(idioma, MotivoNegado::sin_permiso));
}

TEST_CASE("El chat de un canal tiene tope de mensajes y de bytes") {
  Sala sala(presentado());
  for (std::uint64_t id = 1; id <= ventrik::cliente::kVentanaChat + 5; ++id) {
    sala.aplicar(mensaje(id, 1, static_cast<std::uint32_t>(id)));
  }
  CHECK(sala.chat().mensajes.size() == ventrik::cliente::kVentanaChat);
  CHECK(sala.chat().mensajes.front().id == 6);
  CHECK(sala.chat().hay_mas);
  CHECK_FALSE(sala.chat().hay_nuevos);
  const auto pedido = sala.pedir_mas_viejos();
  REQUIRE(pedido);
  CHECK(como<PedirHistorial>(*pedido).desde == 6);
  Sala grande(presentado());
  for (std::uint64_t id = 1; id <= 600; ++id) {
    grande.aplicar(mensaje(id, 1, static_cast<std::uint32_t>(id), std::string(4000, 'x')));
  }
  CHECK(grande.chat().bytes <= ventrik::cliente::kTopeBytesPorChat);
  CHECK(grande.chat().mensajes.back().id == 600);
}

TEST_CASE("Con un servidor de verdad, la sala vuelve a pedir lo que soltó hacia los dos lados") {
  ventrik::servidor::Opciones opciones;
  opciones.rafaga_chat = opciones.chat_por_segundo = 1000;
  opciones.rafaga_pedidos = opciones.pedidos_por_segundo = 1000;
  opciones.rafaga_historial = opciones.historial_por_segundo = 1000;
  opciones.proteccion.rafaga_chat_por_ip = opciones.proteccion.chat_por_segundo_por_ip = 1000;
  prueba::ServidorDePrueba servidor(opciones);
  ventrik::cliente::Sesion sesion(servidor.direccion(), servidor.huella(), prueba::presentacion("Ana"));
  std::optional<Sala> sala;
  const auto ordenar = [&](std::vector<Orden> ordenes) {
    for (auto& orden : ordenes) {
      sesion.ordenar(std::move(orden));
    }
  };
  const auto hasta = [&](const std::function<bool()>& listo) {
    const auto limite = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!listo() && std::chrono::steady_clock::now() < limite) {
      for (const auto& novedad : sesion.esperar_novedades(std::chrono::milliseconds(20))) {
        if (const auto* intento = std::get_if<ventrik::cliente::IntentoDeConexion>(&novedad)) {
          REQUIRE(intento->presentado);
          sala.emplace(*intento->presentado);
          ordenar(sala->empezar());
        } else if (sala) {
          ordenar(sala->aplicar(novedad));
        }
      }
    }
    return listo();
  };
  const auto pedir = [&](std::optional<Orden> orden) {
    REQUIRE(orden);
    ordenar({*orden});
    REQUIRE(hasta([&] { return !sala->chat().pidiendo; }));
  };
  REQUIRE(hasta([&] { return sala.has_value(); }));
  ordenar(sala->elegir_voz(1));
  REQUIRE(hasta([&] { return sala->tu_canal() == 1 && !sala->chat().pidiendo; }));
  for (int i = 1; i <= 200; ++i) {
    sesion.ordenar(EnviarChat{1, "m " + std::to_string(i)});
  }
  REQUIRE(hasta([&] { return sala->chat().mensajes.size() == 200; }));
  CHECK(sala->chat().mensajes.back().id == 200);

  sala->soltar_lejanos();
  REQUIRE(sala->chat().mensajes.size() == ventrik::cliente::kMensajesAlEntrar);
  CHECK(sala->chat().mensajes.front().id == 137);
  sala->a_la_vista(137);
  pedir(sala->pedir_mas_viejos());
  CHECK(sala->chat().mensajes.front().id == 9);
  CHECK(sala->chat().hay_mas);
  pedir(sala->pedir_mas_viejos());
  CHECK(sala->chat().mensajes.size() == 200);
  CHECK_FALSE(sala->chat().hay_mas);

  sala->a_la_vista(1);
  sala->soltar_lejanos();
  REQUIRE(sala->chat().hay_nuevos);
  CHECK(sala->chat().mensajes.back().id == 64);
  sala->a_la_vista(50);
  pedir(sala->pedir_mas_nuevos());
  CHECK(sala->chat().mensajes.back().id == 192);
  CHECK(sala->chat().hay_nuevos);
  sala->a_la_vista(180);
  pedir(sala->pedir_mas_nuevos());
  CHECK(sala->chat().mensajes.back().id == 200);
  CHECK_FALSE(sala->chat().hay_nuevos);
  CHECK(sala->chat().mensajes.size() == 200);
  CHECK(sala->chat().autores.size() == 1);
  sesion.ordenar(EnviarChat{1, "otro"});
  REQUIRE(hasta([&] { return sala->chat().mensajes.back().id == 201; }));
}
