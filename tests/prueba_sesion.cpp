#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "client/sesion.h"
#include "common/cifrado/codigo_dueno.h"
#include "servidor_de_prueba.h"
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

namespace {
class Cliente {
 public:
  Cliente(const ServidorDePrueba& servidor, std::string nombre,
          std::optional<ventrik::cifrado::CodigoDueno> codigo = {},
          ventrik::cliente::OpcionesSesion opciones = {})
      : sesion_(servidor.direccion(), servidor.huella(), {std::move(nombre), codigo}, opciones) {}

  template <typename T>
  std::optional<T> esperar(const std::function<bool(const T&)>& condicion = {},
                           std::chrono::milliseconds limite = 5s) {
    const auto hasta = std::chrono::steady_clock::now() + limite;
    for (;;) {
      for (auto i = recibidas_.begin(); i != recibidas_.end(); ++i) {
        if (const T* cual = std::get_if<T>(&*i); cual && (!condicion || condicion(*cual))) {
          T encontrado = *cual;
          recibidas_.erase(i);
          return encontrado;
        }
      }
      if (std::chrono::steady_clock::now() >= hasta) {
        return std::nullopt;
      }
      for (auto& novedad : sesion_.esperar_novedades(50ms)) {
        recibidas_.push_back(std::move(novedad));
      }
    }
  }

  template <typename T>
  bool no_llega(std::chrono::milliseconds tiempo = 300ms) {
    return !esperar<T>({}, tiempo);
  }

  Presentado entrar() {
    const auto intento = esperar<IntentoDeConexion>();
    REQUIRE(intento);
    REQUIRE(intento->estado == EstadoConexion::conectado);
    REQUIRE(intento->presentado);
    return *intento->presentado;
  }

  void ordenar(ventrik::cliente::Orden orden) { sesion_.ordenar(std::move(orden)); }

 private:
  ventrik::cliente::Sesion sesion_;
  std::vector<Novedad> recibidas_;
};

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
  ana.entrar();
  beto.entrar();
  const auto carla_id = carla.entrar().tu_id;
  ana.ordenar(CrearCanal{"Juegos"});
  const auto creado = carla.esperar<CanalCreado>();
  REQUIRE(creado);
  carla.ordenar(EntrarACanal{creado->canal.id});
  REQUIRE(beto.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == carla_id; }));
  ana.ordenar(EnviarChat{"hola 😀"});
  const auto mensaje = beto.esperar<MensajeChat>();
  REQUIRE(mensaje);
  CHECK(mensaje->texto == "hola 😀");
  CHECK(mensaje->autor == "Ana");
  CHECK(mensaje->canal == ventrik::servidor::kCanalGeneral);
  CHECK(ana.esperar<MensajeChat>());
  CHECK(carla.no_llega<MensajeChat>());

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
  beto.ordenar(RenombrarCanal{ventrik::servidor::kCanalGeneral, "Mío"});
  beto.ordenar(BorrarCanal{ventrik::servidor::kCanalGeneral});
  for (const auto pedido : {TipoMensaje::crear_canal, TipoMensaje::renombrar_canal,
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
  ana.entrar();
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
  ana.ordenar(RenombrarCanal{creado->canal.id, "Música"});
  const auto renombrado = beto.esperar<CanalRenombrado>();
  REQUIRE(renombrado);
  CHECK(renombrado->canal.nombre == "Música");
  ana.ordenar(BorrarCanal{creado->canal.id});
  const auto borrado = beto.esperar<CanalBorrado>();
  REQUIRE(borrado);
  CHECK(borrado->id == creado->canal.id);
  CHECK(borrado->movidos_a == ventrik::servidor::kCanalGeneral);
  beto.ordenar(EnviarChat{"volví"});
  CHECK(ana.esperar<MensajeChat>());
  ana.ordenar(BorrarCanal{ventrik::servidor::kCanalGeneral});
  const auto general = ana.esperar<Negado>();
  REQUIRE(general);
  CHECK(general->motivo == MotivoNegado::sin_permiso);
}

TEST_CASE("Hay un tope de mensajes de chat por persona") {
  ServidorDePrueba servidor;
  Cliente ana(servidor, "Ana");
  ana.entrar();
  for (int i = 0; i < 15; ++i) {
    ana.ordenar(EnviarChat{"mensaje " + std::to_string(i)});
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
  const auto mudo = ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), {"Mudo", {}});
  REQUIRE(mudo.estado == EstadoConexion::conectado);
  Cliente ana(servidor, "Ana");
  ana.entrar();
  const std::string largo(4000, 'x');
  for (int ronda = 0; ronda < 500 && servidor.cuantos(Evento::no_lee) == 0; ++ronda) {
    for (int i = 0; i < 8; ++i) {
      ana.ordenar(EnviarChat{largo});
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
  const auto con_voz = [] {
    ventrik::cliente::OpcionesSesion sesion;
    sesion.voz = std::make_shared<PuenteVoz>();
    return sesion;
  };
  const auto oa = con_voz();
  const auto ob = con_voz();
  const auto oc = con_voz();
  Cliente ana(servidor, "Ana", dueno.codigo, oa);
  Cliente beto(servidor, "Beto", {}, ob);
  Cliente carla(servidor, "Carla", {}, oc);
  const auto ana_id = ana.entrar().tu_id;
  beto.entrar();
  const auto carla_id = carla.entrar().tu_id;
  ana.ordenar(CrearCanal{"Juegos"});
  const auto creado = carla.esperar<CanalCreado>();
  REQUIRE(creado);
  carla.ordenar(EntrarACanal{creado->canal.id});
  REQUIRE(ana.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == carla_id; }));
  for (const auto* puente : {oa.voz.get(), ob.voz.get(), oc.voz.get()}) {
    REQUIRE(esperar_que([puente] { return puente->estado == EstadoVoz::lista; }));
  }
  std::uint16_t numero = 7;
  const auto hablar = [&numero](PuenteVoz& puente) {
    PaqueteVoz paquete;
    paquete.numero = numero++;
    paquete.largo = 3;
    paquete.opus[0] = 1;
    paquete.opus[1] = 2;
    paquete.opus[2] = 3;
    REQUIRE(puente.subida.poner(paquete));
    puente.despertador.despertar();
  };
  const auto escuchar = [](PuenteVoz& puente, std::chrono::milliseconds limite) {
    PaqueteVoz paquete;
    return esperar_que([&] { return puente.bajada.sacar(paquete); }, limite)
               ? std::optional<PaqueteVoz>(paquete)
               : std::nullopt;
  };
  hablar(*oa.voz);
  const auto en_beto = escuchar(*ob.voz, 5s);
  REQUIRE(en_beto);
  CHECK(en_beto->hablante == ana_id);
  CHECK(en_beto->numero == 7);
  CHECK(en_beto->largo == 3);
  CHECK(en_beto->opus[2] == 3);
  CHECK_FALSE(escuchar(*oc.voz, 300ms));
  CHECK_FALSE(escuchar(*oa.voz, 300ms));

  REQUIRE(esperar_que([&] { return servidor.cuantos(Evento::voz_conectada) >= 6; }, 10s));
  bool llego = false;
  for (int intento = 0; intento < 50 && !llego; ++intento) {
    hablar(*oa.voz);
    llego = escuchar(*ob.voz, 100ms).has_value();
  }
  CHECK(llego);

  carla.ordenar(EntrarACanal{ventrik::servidor::kCanalGeneral});
  REQUIRE(ana.esperar<UsuarioCambioCanal>([&](const auto& c) { return c.id == carla_id; }));
  llego = false;
  for (int intento = 0; intento < 50 && !llego; ++intento) {
    hablar(*oa.voz);
    llego = escuchar(*oc.voz, 100ms).has_value();
  }
  CHECK(llego);
  CHECK(ob.voz->recibidos > 0);
  CHECK(ob.voz->ping_ms < 1000);
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
