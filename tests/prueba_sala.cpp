#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "client/sala.h"
#include "client/vigia.h"
#include <doctest/doctest.h>

using namespace ventrik::protocolo;
using ventrik::cliente::Orden;
using ventrik::cliente::PingMedido;
using ventrik::cliente::Sala;

namespace {
Presentado presentado(bool dueno = false) {
  Presentado armado;
  armado.tu_id = 7;
  armado.dueno = dueno;
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

std::vector<std::uint64_t> ids(const Sala& sala) {
  std::vector<std::uint64_t> lista;
  for (const auto& guardado : sala.chat().mensajes) {
    lista.push_back(guardado.id);
  }
  return lista;
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
  CHECK(como<PedirHistorial>(ordenes[0]).antes_de == 0);
  CHECK(sala.chat().canal == 1);
  CHECK(sala.chat().pidiendo);
  sala.aplicar(PingMedido{20});
  CHECK(sala.ping() == 20u);
}

TEST_CASE("Con lo leído guardado, cada canal sigue desde donde quedó y uno nuevo arranca sin leer") {
  Sala sala(presentado(), {{1, 0}, {3, 2}, {4, 99}, {7, 5}});
  CHECK(sala.nuevos(3) == 3);
  CHECK(sala.nuevos(4) == 0);
  CHECK(sala.nuevos(2) == 0);
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
  sala.aplicar(UsuarioEntro{{10, "Dani", 2}});
  sala.aplicar(UsuarioCambioCanal{8, 2});
  sala.aplicar(UsuarioSalio{9});
  CHECK(sala.usuarios().size() == 3);
  CHECK(sala.usuarios().at(8).canal == 2);
  CHECK(sala.aplicar(UsuarioCambioCanal{99, 2}).empty());
  const auto ordenes = sala.aplicar(UsuarioCambioCanal{7, 2});
  REQUIRE(ordenes.size() == 1);
  CHECK(como<PedirHistorial>(ordenes[0]).canal == 2);
  CHECK(sala.tu_canal() == 2);
  CHECK(sala.chat().canal == 2);
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
  sala.aplicar(Historial{3, true, {{50, 6, "Beto", 1, "c"}, {40, 5, "Ana", 1, "b"}, {30, 4, "Beto", 1, "a"}}});
  CHECK(ids(sala) == std::vector<std::uint64_t>{30, 40, 50});
  CHECK(sala.chat().hay_mas);
  CHECK_FALSE(sala.chat().pidiendo);
  const auto pedido = sala.pedir_mas_viejos();
  REQUIRE(pedido);
  CHECK(como<PedirHistorial>(*pedido).canal == 3);
  CHECK(como<PedirHistorial>(*pedido).antes_de == 30);
  CHECK_FALSE(sala.pedir_mas_viejos());
  sala.aplicar(Historial{2, false, {{5, 1, "Ana", 1, "otro canal"}}});
  sala.aplicar(Historial{3, false, {{20, 3, "Ana", 1, "z"}}});
  CHECK(ids(sala) == std::vector<std::uint64_t>{20, 30, 40, 50});
  CHECK_FALSE(sala.pedir_mas_viejos());
  sala.aplicar(Historial{3, true, {{10, 2, "Ana", 1, "sin pedir"}}});
  CHECK(ids(sala) == std::vector<std::uint64_t>{20, 30, 40, 50});
  CHECK_FALSE(sala.chat().hay_mas);
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
  CHECK(como<PedirHistorial>(*pedido).antes_de == 0);
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
  CHECK(ventrik::cliente::explicar(idioma, static_cast<MotivoNegado>(8)) ==
        ventrik::cliente::explicar(idioma, MotivoNegado::sin_permiso));
  CHECK(ventrik::cliente::explicar(idioma, MotivoNegado::tipo_de_canal) !=
        ventrik::cliente::explicar(idioma, MotivoNegado::sin_permiso));
}

TEST_CASE("El chat de un canal tiene tope de mensajes y de bytes") {
  Sala sala(presentado());
  for (std::uint64_t id = 1; id <= ventrik::cliente::kTopeMensajesPorChat + 5; ++id) {
    sala.aplicar(mensaje(id, 1, static_cast<std::uint32_t>(id)));
  }
  CHECK(sala.chat().mensajes.size() == ventrik::cliente::kTopeMensajesPorChat);
  CHECK(sala.chat().mensajes.front().id == 6);
  CHECK(sala.chat().hay_mas);
  CHECK_FALSE(sala.pedir_mas_viejos());
  Sala grande(presentado());
  for (std::uint64_t id = 1; id <= 600; ++id) {
    grande.aplicar(mensaje(id, 1, static_cast<std::uint32_t>(id), std::string(4000, 'x')));
  }
  CHECK(grande.chat().bytes <= ventrik::cliente::kTopeBytesPorChat);
  CHECK(grande.chat().mensajes.back().id == 600);
}
