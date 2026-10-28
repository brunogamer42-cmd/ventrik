#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <windows.h>

#include "client/sesion.h"
#include "common/cifrado/codigo_dueno.h"
#include "cliente_de_prueba.h"
#include <doctest/doctest.h>

using namespace prueba;
using namespace ventrik::protocolo;
using ventrik::cliente::EstadoConexion;
using ventrik::cliente::Fin;
using ventrik::cliente::IntentoDeConexion;
using ventrik::cliente::MotivoFin;
using ventrik::cliente::Novedad;
using ventrik::cliente::PingMedido;
using ventrik::servidor::Evento;
using ventrik::servidor::kCanalGeneral;

namespace {
struct ConDueno {
  ventrik::cifrado::CodigoDueno codigo = *ventrik::cifrado::CodigoDueno::generar();
  ventrik::servidor::Opciones opciones() const {
    ventrik::servidor::Opciones opciones;
    opciones.codigo_dueno = codigo;
    return opciones;
  }
};
}

TEST_CASE("Dos programas se presentan y se ven") {
  ServidorDePrueba servidor;
  Cliente ana(servidor, "Ana");
  const auto presentada = ana.entrar();
  CHECK_FALSE(presentada.dueno);
  REQUIRE(presentada.canales.size() == 1);
  CHECK(presentada.canales[0].nombre == "General");
  REQUIRE(presentada.usuarios.size() == 1);
  CHECK(presentada.usuarios[0].nombre == "Ana");
  CHECK(presentada.usuarios[0].canal == 0);
  CHECK(ana.no_llega<UsuarioCambioCanal>());
  Cliente beto(servidor, "Beto");
  const auto presentado = beto.entrar();
  CHECK(presentado.usuarios.size() == 2);
  const auto entro = ana.esperar<UsuarioEntro>();
  REQUIRE(entro);
  CHECK(entro->usuario.nombre == "Beto");
  CHECK(entro->usuario.id == presentado.tu_id);
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::presentada) == 2; }));
}

TEST_CASE("No se repite un nombre, aunque cambien las mayúsculas") {
  ServidorDePrueba servidor;
  Cliente ana(servidor, "Ana");
  ana.entrar();
  Cliente otra(servidor, "ANA");
  const auto intento = otra.esperar<IntentoDeConexion>();
  REQUIRE(intento);
  CHECK(intento->estado == EstadoConexion::nombre_en_uso);
}

TEST_CASE("Un código de dueño equivocado no entra y cuenta como falla") {
  ConDueno dueno;
  ServidorDePrueba servidor(dueno.opciones());
  Cliente intruso(servidor, "Ana", ventrik::cifrado::CodigoDueno::generar());
  const auto intento = intruso.esperar<IntentoDeConexion>();
  REQUIRE(intento);
  CHECK(intento->estado == EstadoConexion::codigo_incorrecto);
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::codigo_incorrecto) == 1; }));
  Cliente verdadero(servidor, "Ana", dueno.codigo);
  CHECK(verdadero.entrar().dueno);
}

TEST_CASE("El chat llega a los del mismo canal, se guarda y otro canal no lo ve") {
  ConDueno dueno;
  ServidorDePrueba servidor(dueno.opciones());
  Cliente ana(servidor, "Ana", dueno.codigo);
  Cliente beto(servidor, "Beto");
  Cliente carla(servidor, "Carla");
  ana.entrar_con_voz();
  beto.entrar_con_voz();
  const auto carla_id = carla.entrar().tu_id;
  ana.ordenar(CrearCanal{"Juegos"});
  const auto creado = carla.esperar<CanalCreado>();
  REQUIRE(creado);
  carla.ordenar(EntrarACanal{creado->canal.id});
  REQUIRE(beto.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == carla_id; }));
  ana.ordenar(EnviarChat{kCanalGeneral, "hola 😀"});
  const auto mensaje = beto.esperar<MensajeChat>();
  REQUIRE(mensaje);
  CHECK(mensaje->texto == "hola 😀");
  CHECK(mensaje->autor == "Ana");
  CHECK(mensaje->canal == ventrik::servidor::kCanalGeneral);
  CHECK(mensaje->numero == 1);
  CHECK(ana.esperar<MensajeChat>());
  CHECK(carla.no_llega<MensajeChat>());
  carla.ordenar(EnviarChat{kCanalGeneral, "intrusa"});
  const auto intrusa = carla.esperar<Negado>();
  REQUIRE(intrusa);
  CHECK(intrusa->pedido == TipoMensaje::enviar_chat);
  CHECK(intrusa->motivo == MotivoNegado::sin_permiso);
  CHECK(beto.no_llega<MensajeChat>());
  CHECK(carla.no_llega<MensajesNuevos>());

  carla.ordenar(PedirHistorial{ventrik::servidor::kCanalGeneral, 0});
  const auto negado = carla.esperar<Negado>();
  REQUIRE(negado);
  CHECK(negado->pedido == TipoMensaje::pedir_historial);
  CHECK(negado->motivo == MotivoNegado::sin_permiso);
  beto.ordenar(PedirHistorial{ventrik::servidor::kCanalGeneral, 0});
  const auto historial = beto.esperar<Historial>();
  REQUIRE(historial);
  REQUIRE(historial->mensajes.size() == 1);
  CHECK(historial->mensajes[0].texto == "hola 😀");
  CHECK_FALSE(historial->hay_mas);
}

TEST_CASE("Quien no es dueño no toca los canales") {
  ServidorDePrueba servidor;
  Cliente beto(servidor, "Beto");
  beto.entrar();
  beto.ordenar(CrearCanal{"Juegos"});
  beto.ordenar(CambiarCanal{ventrik::servidor::kCanalGeneral, "Mío", false});
  beto.ordenar(BorrarCanal{ventrik::servidor::kCanalGeneral});
  for (const auto pedido : {TipoMensaje::crear_canal, TipoMensaje::cambiar_canal,
                            TipoMensaje::borrar_canal}) {
    const auto negado = beto.esperar<Negado>([pedido](const Negado& n) { return n.pedido == pedido; });
    REQUIRE(negado);
    CHECK(negado->motivo == MotivoNegado::sin_permiso);
  }
  CHECK(beto.no_llega<CanalCreado>());
  beto.ordenar(EntrarACanal{99});
  const auto no_existe = beto.esperar<Negado>();
  REQUIRE(no_existe);
  CHECK(no_existe->motivo == MotivoNegado::no_existe);
}

TEST_CASE("El dueño crea, renombra y borra, y al borrar la gente pasa a General") {
  ConDueno dueno;
  ServidorDePrueba servidor(dueno.opciones());
  Cliente ana(servidor, "Ana", dueno.codigo);
  Cliente beto(servidor, "Beto");
  ana.entrar_con_voz();
  const auto beto_id = beto.entrar().tu_id;
  ana.ordenar(CrearCanal{"Juegos"});
  const auto creado = beto.esperar<CanalCreado>();
  REQUIRE(creado);
  CHECK(creado->canal.nombre == "Juegos");
  ana.ordenar(CrearCanal{"JUEGOS"});
  const auto repetido = ana.esperar<Negado>();
  REQUIRE(repetido);
  CHECK(repetido->motivo == MotivoNegado::nombre_en_uso);
  beto.ordenar(EntrarACanal{creado->canal.id});
  REQUIRE(ana.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == beto_id; }));
  ana.ordenar(CambiarCanal{creado->canal.id, "Música", false});
  const auto renombrado = beto.esperar<CanalCambiado>();
  REQUIRE(renombrado);
  CHECK(renombrado->canal.nombre == "Música");
  ana.ordenar(BorrarCanal{creado->canal.id});
  const auto borrado = beto.esperar<CanalBorrado>();
  REQUIRE(borrado);
  CHECK(borrado->id == creado->canal.id);
  CHECK(borrado->movidos_a == ventrik::servidor::kCanalGeneral);
  beto.ordenar(EnviarChat{kCanalGeneral, "volví"});
  CHECK(ana.esperar<MensajeChat>());
  ana.ordenar(BorrarCanal{ventrik::servidor::kCanalGeneral});
  const auto general = ana.esperar<Negado>();
  REQUIRE(general);
  CHECK(general->motivo == MotivoNegado::sin_permiso);
}

TEST_CASE("Los canales de texto se leen y se escriben sin entrar, y a los demás les llega solo el aviso") {
  ConDueno dueno;
  ServidorDePrueba servidor(dueno.opciones());
  Cliente ana(servidor, "Ana", dueno.codigo);
  Cliente beto(servidor, "Beto");
  Cliente carla(servidor, "Carla");
  ana.entrar();
  beto.entrar();
  carla.entrar();
  ana.ordenar(CrearCanal{"Charla", TipoCanal::texto, false});
  const auto charla = carla.esperar<CanalCreado>();
  REQUIRE(charla);
  CHECK(charla->canal.tipo == TipoCanal::texto);
  ana.ordenar(CrearCanal{"Anuncios", TipoCanal::texto, true});
  const auto anuncios = carla.esperar<CanalCreado>();
  REQUIRE(anuncios);
  CHECK(anuncios->canal.solo_admins);
  const std::uint32_t id = charla->canal.id;

  carla.ordenar(MirarCanal{id});
  carla.ordenar(PedirHistorial{id, 0});
  REQUIRE(carla.esperar<Historial>());
  beto.ordenar(EnviarChat{id, "hola"});
  const auto en_carla = carla.esperar<MensajeChat>();
  REQUIRE(en_carla);
  CHECK(en_carla->canal == id);
  CHECK(en_carla->numero == 1);
  CHECK(beto.esperar<MensajeChat>());
  const auto aviso = ana.esperar<MensajesNuevos>();
  const auto primero = std::chrono::steady_clock::now();
  REQUIRE(aviso);
  REQUIRE(aviso->canales.size() == 1);
  CHECK(aviso->canales[0].canal == id);
  CHECK(aviso->canales[0].contador == 1);
  for (int i = 0; i < 3; ++i) {
    beto.ordenar(EnviarChat{id, "otro"});
  }
  const auto juntos = ana.esperar<MensajesNuevos>();
  REQUIRE(juntos);
  CHECK(std::chrono::steady_clock::now() - primero >= 800ms);
  REQUIRE(juntos->canales.size() == 1);
  CHECK(juntos->canales[0].contador == 4);
  CHECK(carla.esperar<MensajeChat>([](const MensajeChat& m) { return m.numero == 4; }));
  CHECK(ana.no_llega<MensajeChat>());
  CHECK(ana.no_llega<MensajesNuevos>());
  CHECK(carla.no_llega<MensajesNuevos>());
  CHECK(beto.no_llega<MensajesNuevos>());
  beto.ordenar(PedirHistorial{id, 0});
  const auto historial = beto.esperar<Historial>();
  REQUIRE(historial);
  REQUIRE(historial->mensajes.size() == 4);
  CHECK(historial->mensajes[0].numero == 4);

  beto.ordenar(EnviarChat{anuncios->canal.id, "spam"});
  const auto sin_permiso = beto.esperar<Negado>();
  REQUIRE(sin_permiso);
  CHECK(sin_permiso->pedido == TipoMensaje::enviar_chat);
  CHECK(sin_permiso->motivo == MotivoNegado::sin_permiso);
  ana.ordenar(EnviarChat{anuncios->canal.id, "aviso"});
  CHECK(ana.esperar<MensajeChat>([&](const MensajeChat& m) { return m.canal == anuncios->canal.id; }));
  ana.ordenar(CambiarCanal{anuncios->canal.id, "Anuncios", false});
  const auto cambiado = beto.esperar<CanalCambiado>();
  REQUIRE(cambiado);
  CHECK(cambiado->canal.tipo == TipoCanal::texto);
  CHECK_FALSE(cambiado->canal.solo_admins);
  CHECK(cambiado->canal.contador == 1);
  beto.ordenar(EnviarChat{anuncios->canal.id, "ahora sí"});
  CHECK(beto.esperar<MensajeChat>([&](const MensajeChat& m) { return m.canal == anuncios->canal.id; }));

  const auto negado_a = [](Cliente& cliente, TipoMensaje pedido) {
    const auto negado = cliente.esperar<Negado>([pedido](const Negado& n) { return n.pedido == pedido; });
    return negado ? std::optional<MotivoNegado>(negado->motivo) : std::nullopt;
  };
  carla.ordenar(EntrarACanal{id});
  CHECK(negado_a(carla, TipoMensaje::entrar_a_canal) == MotivoNegado::tipo_de_canal);
  carla.ordenar(MirarCanal{kCanalGeneral});
  CHECK(negado_a(carla, TipoMensaje::mirar_canal) == MotivoNegado::tipo_de_canal);
  ana.ordenar(CrearCanal{"GENERAL", TipoCanal::texto, false});
  CHECK(negado_a(ana, TipoMensaje::crear_canal) == MotivoNegado::nombre_en_uso);
  ana.ordenar(CambiarCanal{kCanalGeneral, "General", true});
  CHECK(negado_a(ana, TipoMensaje::cambiar_canal) == MotivoNegado::tipo_de_canal);

  ana.ordenar(BorrarCanal{id});
  REQUIRE(carla.esperar<CanalBorrado>());
  beto.ordenar(EnviarChat{id, "tarde"});
  CHECK(negado_a(beto, TipoMensaje::enviar_chat) == MotivoNegado::no_existe);
}

TEST_CASE("Sin guardar, el chat sale una sola vez y el contador queda en la base al cerrar") {
  wchar_t carpeta[MAX_PATH] = {};
  REQUIRE(GetTempPathW(MAX_PATH, carpeta) > 0);
  const std::wstring archivo = std::wstring(carpeta) + L"ventrik_sin_guardar_" +
                               std::to_wstring(GetCurrentProcessId()) + L".db";
  DeleteFileW(archivo.c_str());
  {
    auto base = ventrik::servidor::Base::abrir(archivo, "General");
    REQUIRE(base);
    REQUIRE(base->cambiar_retencion(ventrik::servidor::Retencion::no_guardar));
  }
  ventrik::servidor::Opciones opciones;
  opciones.archivo_base = archivo;
  {
    ServidorDePrueba servidor(opciones);
    Cliente ana(servidor, "Ana");
    ana.entrar_con_voz();
    ana.ordenar(EnviarChat{kCanalGeneral, "uno"});
    ana.ordenar(EnviarChat{kCanalGeneral, "dos"});
    REQUIRE(ana.esperar<MensajeChat>([](const MensajeChat& m) { return m.numero == 2; }));
    CHECK(ana.esperar<MensajeChat>([](const MensajeChat& m) { return m.numero == 1; }));
    CHECK(ana.no_llega<MensajeChat>());
    ana.ordenar(PedirHistorial{kCanalGeneral, 0});
    const auto historial = ana.esperar<Historial>();
    REQUIRE(historial);
    CHECK(historial->mensajes.empty());
  }
  {
    auto base = ventrik::servidor::Base::abrir(archivo, "General");
    REQUIRE(base);
    CHECK(base->canales()[0].contador == 2);
  }
  CHECK(DeleteFileW(archivo.c_str()));
}

TEST_CASE("Entrar a la voz y salir muy seguido tiene tope") {
  ServidorDePrueba servidor;
  Cliente ana(servidor, "Ana");
  const auto ana_id = ana.entrar().tu_id;
  for (int vez = 0; vez < 5; ++vez) {
    ana.ordenar(EntrarACanal{kCanalGeneral});
    ana.ordenar(EntrarACanal{0});
  }
  ana.ordenar(EntrarACanal{kCanalGeneral});
  const auto negado = ana.esperar<Negado>();
  REQUIRE(negado);
  CHECK(negado->pedido == TipoMensaje::entrar_a_canal);
  CHECK(negado->motivo == MotivoNegado::demasiado_rapido);
  for (int vez = 0; vez < 10; ++vez) {
    REQUIRE(ana.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == ana_id; }));
  }
  CHECK(ana.no_llega<UsuarioCambioCanal>());
}

TEST_CASE("Hay un tope de mensajes de chat por persona") {
  ServidorDePrueba servidor;
  Cliente ana(servidor, "Ana");
  ana.entrar_con_voz();
  for (int i = 0; i < 15; ++i) {
    ana.ordenar(EnviarChat{kCanalGeneral, "mensaje " + std::to_string(i)});
  }
  const auto negado = ana.esperar<Negado>();
  REQUIRE(negado);
  CHECK(negado->pedido == TipoMensaje::enviar_chat);
  CHECK(negado->motivo == MotivoNegado::demasiado_rapido);
}

TEST_CASE("El ping vuelve") {
  ServidorDePrueba servidor;
  Cliente ana(servidor, "Ana");
  ana.entrar();
  const auto ping = ana.esperar<PingMedido>();
  REQUIRE(ping);
  CHECK(ping->milisegundos < 1000);
}

TEST_CASE("Varias órdenes juntas salen en orden") {
  ServidorDePrueba servidor;
  Cliente ana(servidor, "Ana");
  ana.entrar_con_voz();
  ana.ordenar_juntas({EnviarChat{kCanalGeneral, "uno"}, EnviarChat{kCanalGeneral, "dos"},
                      EnviarChat{kCanalGeneral, "tres"}});
  for (std::uint32_t numero = 1; numero <= 3; ++numero) {
    const auto mensaje = ana.esperar<MensajeChat>([numero](const MensajeChat& m) { return m.numero == numero; });
    REQUIRE(mensaje);
    CHECK(mensaje->texto == (numero == 1 ? "uno" : numero == 2 ? "dos" : "tres"));
  }
  ana.ordenar_juntas({});
  CHECK(ana.no_llega<MensajeChat>());
}

TEST_CASE("Ahorrando, el ping cambia de ritmo enseguida, en las dos direcciones") {
  ServidorDePrueba servidor;
  ventrik::cliente::OpcionesSesion opciones;
  opciones.cada_cuanto_ping = std::chrono::hours(1);
  opciones.cada_cuanto_ping_ahorrando = 100ms;
  Cliente ana(servidor, "Ana", {}, opciones);
  ana.entrar();
  REQUIRE(ana.esperar<PingMedido>());
  CHECK(ana.no_llega<PingMedido>());
  ana.ahorrar(true);
  for (int i = 0; i < 3; ++i) {
    CHECK(ana.esperar<PingMedido>({}, 1s));
  }
  ana.ahorrar(false);
  std::this_thread::sleep_for(100ms);
  while (ana.esperar<PingMedido>({}, 50ms)) {
  }
  CHECK(ana.no_llega<PingMedido>());
}

TEST_CASE("Una conexión que no manda nada se corta") {
  ventrik::servidor::Opciones opciones;
  opciones.inactividad = 300ms;
  ServidorDePrueba servidor(opciones);
  ventrik::cliente::OpcionesSesion sin_ping;
  sin_ping.cada_cuanto_ping = std::chrono::hours(1);
  Cliente callado(servidor, "Callado", {}, sin_ping);
  callado.entrar();
  const auto fin = callado.esperar<Fin>();
  REQUIRE(fin);
  CHECK(fin->motivo == MotivoFin::el_servidor_corto);
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::sin_respuesta) == 1; }));
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::salio) == 1; }));
}

TEST_CASE("Un programa que no lee lo que recibe es desconectado") {
  ventrik::servidor::Opciones opciones;
  opciones.maximo_salida = 256 * 1024;
  opciones.rafaga_chat = 1e6;
  opciones.chat_por_segundo = 1e6;
  opciones.proteccion.rafaga_chat_por_ip = 1e6;
  opciones.proteccion.chat_por_segundo_por_ip = 1e6;
  ServidorDePrueba servidor(opciones);
  const auto mudo = ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), presentacion("Mudo"));
  REQUIRE(mudo.estado == EstadoConexion::conectado);
  Cliente ana(servidor, "Ana");
  ana.entrar_con_voz();
  std::vector<std::uint8_t> a_general;
  REQUIRE(armar_mensaje(EntrarACanal{kCanalGeneral}, a_general));
  std::size_t escritos = 0;
  REQUIRE(mudo.abierta->tls().escribir(a_general, escritos) == ventrik::cifrado::Paso::listo);
  REQUIRE(escritos == a_general.size());
  REQUIRE(ana.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == mudo.presentado->tu_id; }));
  const std::string largo(4000, 'x');
  for (int ronda = 0; ronda < 500 && servidor.cuantos(Evento::no_lee) == 0; ++ronda) {
    for (int i = 0; i < 8; ++i) {
      ana.ordenar(EnviarChat{kCanalGeneral, largo});
    }
    for (int i = 0; i < 8; ++i) {
      REQUIRE(ana.esperar<MensajeChat>());
    }
  }
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::no_lee) == 1; }));
  const auto salio = ana.esperar<UsuarioSalio>();
  REQUIRE(salio);
  CHECK(salio->id == mudo.presentado->tu_id);
}

TEST_CASE("La voz va por UDP solo a los del mismo canal, y sigue andando al rotar la clave") {
  using ventrik::cliente::EstadoVoz;
  using ventrik::cliente::PaqueteVoz;
  using ventrik::cliente::PuenteVoz;
  ConDueno dueno;
  auto opciones = dueno.opciones();
  opciones.voz.rotacion = std::chrono::seconds(1);
  ServidorDePrueba servidor(opciones);
  Cliente ana(servidor, "Ana", dueno.codigo);
  Cliente beto(servidor, "Beto");
  Cliente carla(servidor, "Carla");
  Cliente dani(servidor, "Dani");
  const auto ana_id = ana.entrar_con_voz().tu_id;
  const auto beto_id = beto.entrar_con_voz().tu_id;
  const auto carla_id = carla.entrar().tu_id;
  dani.entrar();
  ana.ordenar(CrearCanal{"Juegos"});
  const auto creado = carla.esperar<CanalCreado>();
  REQUIRE(creado);
  carla.ordenar(EntrarACanal{creado->canal.id});
  REQUIRE(carla.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == carla_id; }));
  const auto pa = std::make_shared<PuenteVoz>();
  const auto pb = std::make_shared<PuenteVoz>();
  const auto pc = std::make_shared<PuenteVoz>();
  const auto pd = std::make_shared<PuenteVoz>();
  ana.voz(pa);
  beto.voz(pb);
  carla.voz(pc);
  dani.voz(pd);
  for (const auto* puente : {pa.get(), pb.get(), pc.get()}) {
    REQUIRE(esperar_que([puente] { return puente->estado == EstadoVoz::lista; }));
  }
  CHECK(pd->estado == EstadoVoz::sin_clave);
  const std::uint16_t enviado = hablar(*pa);
  const auto en_beto = escuchar(*pb, 5s);
  REQUIRE(en_beto);
  CHECK(en_beto->hablante == ana_id);
  CHECK(en_beto->numero == enviado);
  CHECK(en_beto->tiempo == static_cast<std::uint16_t>(enviado * 20));
  CHECK(en_beto->nivel == 90);
  CHECK(en_beto->largo == 3);
  CHECK(en_beto->opus[2] == 3);
  CHECK_FALSE(escuchar(*pc, 300ms));
  CHECK_FALSE(escuchar(*pa, 300ms));

  REQUIRE(esperar_que([&] { return servidor.cuantos(Evento::voz_conectada) >= 6; }, 10s));
  bool llego = false;
  for (int intento = 0; intento < 50 && !llego; ++intento) {
    hablar(*pa);
    llego = escuchar(*pb, 100ms).has_value();
  }
  CHECK(llego);

  carla.ordenar(EntrarACanal{kCanalGeneral});
  REQUIRE(ana.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == carla_id && c.canal == kCanalGeneral; }));
  ana.ahorrar(true);
  carla.ahorrar(true);
  llego = false;
  for (int intento = 0; intento < 50 && !llego; ++intento) {
    hablar(*pa);
    llego = escuchar(*pc, 100ms).has_value();
  }
  CHECK(llego);
  ana.ahorrar(false);
  CHECK(pa->estado == EstadoVoz::lista);
  CHECK(pc->estado == EstadoVoz::lista);
  CHECK(pb->recibidos > 0);
  CHECK(pb->ping_ms < 1000);

  beto.ordenar(EntrarACanal{0});
  REQUIRE(ana.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == beto_id && c.canal == 0; }));
  beto.voz(nullptr);
  std::this_thread::sleep_for(200ms);
  PaqueteVoz viejo;
  while (pb->bajada.sacar(viejo)) {
  }
  for (int vez = 0; vez < 5; ++vez) {
    hablar(*pa);
  }
  CHECK_FALSE(escuchar(*pb, 500ms));
  CHECK(escuchar(*pc, 5s));

  beto.ordenar(EntrarACanal{kCanalGeneral});
  REQUIRE(beto.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == beto_id && c.canal == kCanalGeneral; }));
  const auto otra_vez = std::make_shared<PuenteVoz>();
  beto.voz(otra_vez);
  REQUIRE(esperar_que([&] { return otra_vez->estado == EstadoVoz::lista; }));
  llego = false;
  for (int intento = 0; intento < 50 && !llego; ++intento) {
    hablar(*pa);
    llego = escuchar(*otra_vez, 100ms).has_value();
  }
  CHECK(llego);
}

TEST_CASE("El susurro llega por UDP solo a sus destinos, el servidor valida el canal y lo olvida al borrarlo") {
  using ventrik::cliente::EstadoVoz;
  using ventrik::cliente::PuenteVoz;
  ConDueno dueno;
  ServidorDePrueba servidor(dueno.opciones());
  Cliente ana(servidor, "Ana", dueno.codigo);
  Cliente beto(servidor, "Beto");
  Cliente carla(servidor, "Carla");
  ana.entrar_con_voz();
  beto.entrar_con_voz();
  const auto carla_id = carla.entrar().tu_id;
  ana.ordenar(CrearCanal{"Juegos"});
  const auto creado = carla.esperar<CanalCreado>();
  REQUIRE(creado);
  carla.ordenar(EntrarACanal{creado->canal.id});
  REQUIRE(carla.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == carla_id; }));
  const auto pa = std::make_shared<PuenteVoz>();
  const auto pb = std::make_shared<PuenteVoz>();
  const auto pc = std::make_shared<PuenteVoz>();
  ana.voz(pa);
  beto.voz(pb);
  carla.voz(pc);
  for (const auto* puente : {pa.get(), pb.get(), pc.get()}) {
    REQUIRE(esperar_que([puente] { return puente->estado == EstadoVoz::lista; }));
  }
  const auto le_llega = [](PuenteVoz& habla, PuenteVoz& escucha, std::uint8_t flujo) {
    for (int intento = 0; intento < 50; ++intento) {
      hablar(habla, flujo);
      if (const auto paquete = escuchar(escucha, 100ms); paquete && paquete->flujo == flujo) {
        return true;
      }
    }
    return false;
  };
  ana.ordenar(DestinosDeSusurro{1, 0, {carla_id}});
  CHECK(le_llega(*pa, *pc, 1));
  CHECK_FALSE(escuchar(*pb, 300ms));

  ana.ordenar(DestinosDeSusurro{2, 999, {}});
  CHECK(ana.esperar<Negado>([](const Negado& n) {
    return n.pedido == TipoMensaje::destinos_de_susurro && n.motivo == MotivoNegado::no_existe;
  }));
  ana.ordenar(DestinosDeSusurro{2, creado->canal.id, {}});
  CHECK(le_llega(*pa, *pc, 2));
  CHECK_FALSE(escuchar(*pb, 300ms));

  ana.ordenar(BorrarCanal{creado->canal.id});
  REQUIRE(carla.esperar<CanalBorrado>());
  std::this_thread::sleep_for(200ms);
  ventrik::cliente::PaqueteVoz viejo;
  while (pc->bajada.sacar(viejo)) {
  }
  for (int vez = 0; vez < 5; ++vez) {
    hablar(*pa, 2);
  }
  CHECK_FALSE(escuchar(*pc, 500ms));
  CHECK(le_llega(*pa, *pc, 1));
}

TEST_CASE("Los destinos del susurro valen desde antes de entrar a la voz y al volver; un canal de texto o un rechazo los vacían") {
  using ventrik::cliente::EstadoVoz;
  using ventrik::cliente::PuenteVoz;
  ConDueno dueno;
  ServidorDePrueba servidor(dueno.opciones());
  Cliente ana(servidor, "Ana", dueno.codigo);
  Cliente beto(servidor, "Beto");
  const auto ana_id = ana.entrar().tu_id;
  const auto beto_id = beto.entrar_con_voz().tu_id;
  const auto llega = [](PuenteVoz& habla, PuenteVoz& escucha, std::uint8_t flujo) {
    for (int intento = 0; intento < 50; ++intento) {
      hablar(habla, flujo);
      if (const auto paquete = escuchar(escucha, 100ms); paquete && paquete->flujo == flujo) {
        return true;
      }
    }
    return false;
  };
  ana.ordenar(CrearCanal{"Charla", TipoCanal::texto, false});
  const auto charla = beto.esperar<CanalCreado>();
  REQUIRE(charla);
  ana.ordenar(DestinosDeSusurro{3, charla->canal.id, {}});
  CHECK(ana.esperar<Negado>([](const Negado& n) {
    return n.pedido == TipoMensaje::destinos_de_susurro && n.motivo == MotivoNegado::tipo_de_canal;
  }));
  ana.ordenar(DestinosDeSusurro{1, 0, {beto_id}});
  ana.ordenar(CrearCanal{"Juegos"});
  const auto juegos = beto.esperar<CanalCreado>([](const CanalCreado& c) { return c.canal.nombre == "Juegos"; });
  REQUIRE(juegos);
  ana.ordenar(EntrarACanal{juegos->canal.id});
  REQUIRE(beto.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == ana_id && c.canal == juegos->canal.id; }));
  const auto pa = std::make_shared<PuenteVoz>();
  const auto pb = std::make_shared<PuenteVoz>();
  ana.voz(pa);
  beto.voz(pb);
  REQUIRE(esperar_que([&] { return pa->estado == EstadoVoz::lista && pb->estado == EstadoVoz::lista; }));
  CHECK(llega(*pa, *pb, 1));

  ana.ordenar(EntrarACanal{0});
  REQUIRE(beto.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == ana_id && c.canal == 0; }));
  ana.voz(nullptr);
  ana.ordenar(EntrarACanal{juegos->canal.id});
  REQUIRE(beto.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == ana_id && c.canal == juegos->canal.id; }));
  const auto otra = std::make_shared<PuenteVoz>();
  ana.voz(otra);
  REQUIRE(esperar_que([&] { return otra->estado == EstadoVoz::lista; }));
  CHECK(llega(*otra, *pb, 1));

  ana.ordenar(DestinosDeSusurro{1, 999, {beto_id}});
  CHECK(ana.esperar<Negado>([](const Negado& n) {
    return n.pedido == TipoMensaje::destinos_de_susurro && n.motivo == MotivoNegado::no_existe;
  }));
  ventrik::cliente::PaqueteVoz viejo;
  while (pb->bajada.sacar(viejo)) {
  }
  for (int vez = 0; vez < 5; ++vez) {
    hablar(*otra, 1);
  }
  CHECK_FALSE(escuchar(*pb, 500ms));
}

TEST_CASE("El silencio llega a todos y al que entra después; sin audio no te llega voz y silenciado tu voz no sale") {
  using ventrik::cliente::EstadoVoz;
  using ventrik::cliente::PaqueteVoz;
  using ventrik::cliente::PuenteVoz;
  ServidorDePrueba servidor;
  Cliente ana(servidor, "Ana");
  const auto ana_id = ana.entrar().tu_id;
  ana.ordenar(Silenciar{1});
  REQUIRE(ana.esperar<UsuarioSilencio>([&](const UsuarioSilencio& s) { return s.id == ana_id && s.silencio == 1; }));
  Cliente beto(servidor, "Beto");
  const auto presentado = beto.entrar();
  const auto vista = std::find_if(presentado.usuarios.begin(), presentado.usuarios.end(),
                                  [&](const UsuarioInfo& usuario) { return usuario.id == ana_id; });
  REQUIRE(vista != presentado.usuarios.end());
  CHECK(vista->silencio == 1);
  for (Cliente* cliente : {&ana, &beto}) {
    cliente->ordenar(EntrarACanal{kCanalGeneral});
  }
  REQUIRE(beto.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id != ana_id; }));
  REQUIRE(beto.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == ana_id; }));
  const auto pa = std::make_shared<PuenteVoz>();
  const auto pb = std::make_shared<PuenteVoz>();
  ana.voz(pa);
  beto.voz(pb);
  REQUIRE(esperar_que([&] { return pa->estado == EstadoVoz::lista && pb->estado == EstadoVoz::lista; }));
  for (int vez = 0; vez < 5; ++vez) {
    hablar(*pa);
  }
  CHECK_FALSE(escuchar(*pb, 500ms));

  ana.ordenar(Silenciar{0});
  REQUIRE(beto.esperar<UsuarioSilencio>([&](const UsuarioSilencio& s) { return s.id == ana_id && s.silencio == 0; }));
  CHECK(llega_alguno(*pa, *pb));

  const auto beto_id = presentado.tu_id;
  beto.ordenar(Silenciar{3});
  REQUIRE(ana.esperar<UsuarioSilencio>([&](const UsuarioSilencio& s) { return s.id == beto_id && s.silencio == 3; }));
  std::this_thread::sleep_for(200ms);
  PaqueteVoz viejo;
  while (pb->bajada.sacar(viejo)) {
  }
  for (int vez = 0; vez < 5; ++vez) {
    hablar(*pa);
  }
  CHECK_FALSE(escuchar(*pb, 500ms));

  ana.ordenar(Silenciar{2});
  CHECK(ana.esperar<Fin>());
}

TEST_CASE("Silenciar para mí vale desde antes de entrar a la voz y sigue al salir y volver") {
  using ventrik::cliente::EstadoVoz;
  using ventrik::cliente::PuenteVoz;
  ServidorDePrueba servidor;
  Cliente ana(servidor, "Ana");
  Cliente beto(servidor, "Beto");
  const auto ana_id = ana.entrar().tu_id;
  const auto beto_id = beto.entrar().tu_id;
  beto.ordenar(SilenciarPersonas{{ana_id}});
  for (Cliente* cliente : {&ana, &beto}) {
    cliente->ordenar(EntrarACanal{kCanalGeneral});
  }
  REQUIRE(ana.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == beto_id; }));
  const auto pa = std::make_shared<PuenteVoz>();
  const auto pb = std::make_shared<PuenteVoz>();
  ana.voz(pa);
  beto.voz(pb);
  REQUIRE(esperar_que([&] { return pa->estado == EstadoVoz::lista && pb->estado == EstadoVoz::lista; }));
  for (int vez = 0; vez < 5; ++vez) {
    hablar(*pa);
  }
  CHECK_FALSE(escuchar(*pb, 500ms));
  beto.ordenar(EntrarACanal{0});
  beto.voz(nullptr);
  REQUIRE(ana.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == beto_id && c.canal == 0; }));
  beto.ordenar(EntrarACanal{kCanalGeneral});
  REQUIRE(ana.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == beto_id && c.canal == kCanalGeneral; }));
  const auto otra = std::make_shared<PuenteVoz>();
  beto.voz(otra);
  REQUIRE(esperar_que([&] { return otra->estado == EstadoVoz::lista; }));
  for (int vez = 0; vez < 5; ++vez) {
    hablar(*pa);
  }
  CHECK_FALSE(escuchar(*otra, 500ms));
  beto.ordenar(SilenciarPersonas{});
  CHECK(llega_alguno(*pa, *otra));
}

TEST_CASE("Cerrar una sesión que todavía está saludando no espera el plazo del saludo") {
  const auto escucha = ventrik::red::escuchar({0x7F000001, 0});
  REQUIRE(escucha.valido());
  const auto donde = ventrik::red::direccion_local(escucha);
  REQUIRE(donde);
  const auto antes = std::chrono::steady_clock::now();
  {
    ventrik::cliente::Sesion sesion(*donde, ventrik::cifrado::Huella(ventrik::cifrado::Huella::Bytes{}),
                                    {"Nadie", std::nullopt});
    std::this_thread::sleep_for(300ms);
  }
  CHECK(std::chrono::steady_clock::now() - antes < 2s);
}
