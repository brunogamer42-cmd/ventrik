#include <chrono>
#include <string>
#include <variant>

#include "client/avisos.h"
#include "common/utf.h"
#include <doctest/doctest.h>

using namespace ventrik::cliente;
using namespace ventrik::protocolo;
using namespace std::chrono_literals;

namespace {
const std::string kPunto = "\xC2\xB7";
const Avisos::Reloj::time_point kAhora = Avisos::Reloj::now();

MensajeChat chat(std::uint32_t canal, std::uint32_t autor_id, const std::string& autor, const std::string& texto) {
  static std::uint32_t numero = 1;
  return MensajeChat{numero, canal, numero++, autor_id, autor, 1000, texto};
}

MensajeGuardado guardado(std::uint32_t numero, const std::string& autor, const std::string& texto) {
  return MensajeGuardado{numero, numero, autor, 1000, texto};
}

Historial respuesta(std::uint32_t canal, std::vector<MensajeGuardado> mensajes) {
  return Historial{canal, kDesdeAviso, Sentido::viejos, false, std::move(mensajes)};
}

AvisosDelServidor de_todo() {
  AvisosDelServidor ajustes;
  ajustes.servidor.avisar = Avisar::todos;
  return ajustes;
}

Avisos conectados(OpcionesAvisos opciones = {}, AvisosDelServidor ajustes = de_todo()) {
  Avisos avisos(ventrik::elegir_idioma("es"), opciones);
  avisos.conectado("1.2.3.4:7700", 1, "Yo");
  avisos.ajustes(std::move(ajustes));
  avisos.fuera(true);
  return avisos;
}

std::string cuerpo(const Avisos& avisos) {
  const auto lista = avisos.lista();
  REQUIRE(lista.size() == 1);
  return lista.front().cuerpo;
}

PedirHistorial pedido(Avisos& avisos, Avisos::Reloj::time_point ahora = kAhora) {
  const auto orden = avisos.pedir(ahora, false, false);
  REQUIRE(orden);
  return std::get<PedirHistorial>(*orden);
}
}

TEST_CASE("Un mensaje del chat de voz con Ventrik fuera avisa con quién y el texto") {
  Avisos avisos = conectados();
  avisos.mensaje(chat(5, 2, "Tano", "Arrancamos?"), "Partida", true);
  const auto lista = avisos.lista();
  REQUIRE(lista.size() == 1);
  CHECK(lista[0].canal == 5);
  CHECK(lista[0].voz);
  CHECK(lista[0].titulo == "1.2.3.4:7700 " + kPunto + " Partida");
  CHECK(lista[0].cuerpo == "Tano: Arrancamos?");
}

TEST_CASE("Varios mensajes del mismo canal se juntan en un aviso que dice de quiénes") {
  Avisos avisos = conectados();
  avisos.mensaje(chat(6, 2, "Tano", "a"), "general", false);
  avisos.mensaje(chat(6, 2, "Tano", "b"), "general", false);
  avisos.mensaje(chat(6, 2, "Tano", "c"), "general", false);
  CHECK(cuerpo(avisos) == "3 mensajes de Tano");
  CHECK(avisos.lista()[0].titulo == "1.2.3.4:7700 " + kPunto + " #general");
  avisos.mensaje(chat(6, 3, "Sofi", "d"), "general", false);
  CHECK(cuerpo(avisos) == "4 mensajes de Tano y Sofi");
  avisos.mensaje(chat(6, 4, "Ana", "e"), "general", false);
  avisos.mensaje(chat(6, 5, "Beto", "f"), "general", false);
  CHECK(cuerpo(avisos) == "6 mensajes de Tano, Sofi y 2 m\xC3\xA1s");
}

TEST_CASE("Los mensajes propios no avisan, con Ventrik al frente tampoco, y al volver se borra todo") {
  Avisos avisos(ventrik::elegir_idioma("es"));
  avisos.conectado("srv", 1, "Yo");
  avisos.ajustes(de_todo());
  avisos.mensaje(chat(5, 2, "Tano", "hola"), "Partida", true);
  CHECK(avisos.lista().empty());
  avisos.fuera(true);
  avisos.mensaje(chat(5, 1, "Yo", "hola"), "Partida", true);
  CHECK(avisos.lista().empty());
  avisos.mensaje(chat(5, 2, "Tano", "hola"), "Partida", true);
  CHECK(avisos.lista().size() == 1);
  const auto antes = avisos.version();
  avisos.fuera(false);
  CHECK(avisos.lista().empty());
  CHECK(avisos.version() > antes);
}

TEST_CASE("Con privacidad solo dice el canal y la cantidad, y no pide el texto al servidor") {
  OpcionesAvisos opciones;
  opciones.privado = true;
  Avisos avisos = conectados(opciones);
  avisos.mensaje(chat(5, 2, "Tano", "secreto"), "Partida", true);
  CHECK(cuerpo(avisos) == "1 mensaje nuevo");
  avisos.mensaje(chat(5, 3, "Sofi", "otro"), "Partida", true);
  CHECK(cuerpo(avisos) == "2 mensajes nuevos");
  avisos.nuevos(7, "charla", 4);
  CHECK_FALSE(avisos.pedir(kAhora, false, false));
}

TEST_CASE("Un canal no mirado pide sus mensajes y arma el aviso con lo que llega") {
  Avisos avisos = conectados();
  avisos.nuevos(7, "charla", 1);
  CHECK(cuerpo(avisos) == "1 mensaje nuevo");
  const PedirHistorial primero = pedido(avisos);
  CHECK(primero.canal == 7);
  CHECK(primero.desde == kDesdeAviso);
  CHECK(primero.sentido == Sentido::viejos);
  CHECK(primero.cantidad == 1);
  CHECK(avisos.espera(7));
  CHECK_FALSE(avisos.pedir(kAhora + 5s, false, false));
  avisos.historial(respuesta(7, {guardado(5, "Tano", "hola")}), 4);
  CHECK_FALSE(avisos.espera(7));
  CHECK(cuerpo(avisos) == "Tano: hola");
  avisos.nuevos(7, "charla", 3);
  CHECK_FALSE(avisos.pedir(kAhora + 1s, false, false));
  CHECK(pedido(avisos, kAhora + 2s).cantidad == 3);
  avisos.historial(respuesta(7, {guardado(12, "Yo", "mio"), guardado(11, "Sofi", "chau"),
                                 guardado(10, "Tano", "hola"), guardado(9, "Viejo", "ya leido")}),
                   9);
  CHECK(cuerpo(avisos) == "3 mensajes de Tano y Sofi");
}

TEST_CASE("Varios canales a la vez: un solo pedido en vuelo, cada 2 s y por turnos") {
  Avisos avisos = conectados();
  for (std::uint32_t canal = 7; canal <= 10; ++canal) {
    avisos.nuevos(canal, "c", 1);
  }
  const std::uint32_t uno = pedido(avisos).canal;
  CHECK_FALSE(avisos.pedir(kAhora + 3s, false, false));
  avisos.historial(respuesta(uno, {}), 0);
  const std::uint32_t dos = pedido(avisos, kAhora + 3s).canal;
  avisos.historial(respuesta(dos, {}), 0);
  const std::uint32_t tres = pedido(avisos, kAhora + 6s).canal;
  avisos.historial(respuesta(tres, {}), 0);
  const std::uint32_t cuatro = pedido(avisos, kAhora + 9s).canal;
  avisos.historial(respuesta(cuatro, {}), 0);
  CHECK(uno + dos + tres + cuatro == 7 + 8 + 9 + 10);
  CHECK_FALSE(avisos.pedir(kAhora + 12s, false, false));
}

TEST_CASE("No pide con un juego adelante, mientras la pantalla espera su historial, ni con Ventrik al frente") {
  Avisos avisos = conectados();
  avisos.nuevos(7, "charla", 1);
  CHECK_FALSE(avisos.pedir(kAhora, false, true));
  CHECK_FALSE(avisos.pedir(kAhora, true, false));
  avisos.fuera(false);
  CHECK_FALSE(avisos.pedir(kAhora, false, false));
}

TEST_CASE("Una respuesta que el aviso no pidió no lo toca") {
  Avisos avisos = conectados();
  avisos.nuevos(7, "charla", 1);
  avisos.historial(respuesta(7, {guardado(5, "Tano", "hola")}), 0);
  CHECK(cuerpo(avisos) == "1 mensaje nuevo");
  pedido(avisos);
  avisos.historial(Historial{7, 0, Sentido::viejos, false, {guardado(5, "Tano", "hola")}}, 0);
  avisos.historial(Historial{7, kDesdeAviso, Sentido::nuevos, false, {guardado(5, "Tano", "hola")}}, 0);
  CHECK(avisos.espera(7));
  CHECK(cuerpo(avisos) == "1 mensaje nuevo");
}

TEST_CASE("Si llegan menos mensajes que los sin leer, o ninguno, dice solo la cantidad") {
  Avisos avisos = conectados();
  avisos.nuevos(7, "charla", 50);
  CHECK(pedido(avisos).cantidad == kMensajesPorAviso);
  avisos.historial(respuesta(7, {guardado(50, "Tano", "a"), guardado(49, "Tano", "b")}), 0);
  CHECK(cuerpo(avisos) == "50 mensajes nuevos");
  avisos.nuevos(7, "charla", 3);
  pedido(avisos, kAhora + 2s);
  avisos.historial(respuesta(7, {}), 0);
  CHECK(cuerpo(avisos) == "3 mensajes nuevos");
}

TEST_CASE("Un rechazo o un pedido sin respuesta dejan volver a pedir ese canal") {
  Avisos avisos = conectados();
  avisos.nuevos(7, "charla", 1);
  pedido(avisos);
  avisos.negado();
  CHECK_FALSE(avisos.pidiendo());
  CHECK(pedido(avisos, kAhora + 2s).canal == 7);
  CHECK_FALSE(avisos.pedir(kAhora + 5s, false, false));
  avisos.nuevos(7, "charla", 2);
  CHECK(pedido(avisos, kAhora + 12s).canal == 7);
}

TEST_CASE("La conexión cortada avisa solo con Ventrik fuera y queda después de desconectar") {
  Avisos avisos(ventrik::elegir_idioma("es"));
  avisos.conectado("srv", 1, "Yo");
  avisos.corte("El servidor cort\xC3\xB3 la conexi\xC3\xB3n.");
  CHECK(avisos.lista().empty());
  avisos.fuera(true);
  avisos.corte("El servidor cort\xC3\xB3 la conexi\xC3\xB3n.");
  avisos.mensaje(chat(5, 2, "Tano", "hola"), "Partida", true);
  avisos.desconectado();
  const auto lista = avisos.lista();
  REQUIRE(lista.size() == 1);
  CHECK(lista[0].canal == 0);
  CHECK(lista[0].titulo == "srv");
  CHECK(lista[0].cuerpo == "El servidor cort\xC3\xB3 la conexi\xC3\xB3n.");
  avisos.fuera(false);
  CHECK(avisos.lista().empty());
}

TEST_CASE("Cada tipo de aviso y No molestar se apagan") {
  OpcionesAvisos sin_voz;
  sin_voz.voz = false;
  Avisos a = conectados(sin_voz);
  a.mensaje(chat(5, 2, "Tano", "hola"), "Partida", true);
  a.mensaje(chat(6, 2, "Tano", "hola"), "general", false);
  CHECK(a.lista().size() == 1);
  OpcionesAvisos callado;
  callado.no_molestar = true;
  Avisos b = conectados(callado);
  b.mensaje(chat(5, 2, "Tano", "hola"), "Partida", true);
  b.nuevos(7, "charla", 1);
  CHECK_FALSE(b.pedir(kAhora, false, false));
  b.corte("corte");
  CHECK(b.lista().empty());
}

TEST_CASE("Ver un canal borra su aviso y la lista sale en el orden en que cambiaron") {
  Avisos avisos = conectados();
  avisos.mensaje(chat(5, 2, "Tano", "a"), "Partida", true);
  avisos.mensaje(chat(6, 2, "Tano", "b"), "general", false);
  avisos.mensaje(chat(5, 3, "Sofi", "c"), "Partida", true);
  auto lista = avisos.lista();
  REQUIRE(lista.size() == 2);
  CHECK(lista[0].canal == 6);
  CHECK(lista[1].canal == 5);
  avisos.visto(5);
  lista = avisos.lista();
  REQUIRE(lista.size() == 1);
  CHECK(lista[0].canal == 6);
}

TEST_CASE("Los datos de un clic en una notificación se leen estrictos y vuelven iguales") {
  Notificacion aviso;
  aviso.servidor = "192.168.0.10:7700";
  aviso.canal = 42;
  aviso.voz = true;
  const std::string argumentos = argumentos_de_aviso(aviso);
  CHECK(argumentos == "servidor=192.168.0.10:7700;canal=42;voz=1");
  const auto leido = leer_argumentos_de_aviso(argumentos);
  REQUIRE(leido);
  CHECK(leido->servidor == aviso.servidor);
  CHECK(leido->canal == 42);
  CHECK(leido->voz);
  CHECK(leer_argumentos_de_aviso("servidor=1.2.3.4;canal=0;voz=0")->servidor == "1.2.3.4:7700");
  for (const char* malo : {"", "servidor=1.2.3.4:7700;canal=42", "servidor=1.2.3.4:7700;canal=42;voz=1;",
                           "servidor=1.2.3.4:7700;canal=042;voz=1", "servidor=1.2.3.4:7700;canal=;voz=1",
                           "servidor=1.2.3.4:7700;canal=4294967296;voz=1", "servidor=1.2.3.4:7700;canal=4a;voz=1",
                           "servidor=1.2.3.4:7700;canal=4;voz=2", "servidor=host:7700;canal=4;voz=1",
                           "canal=4;servidor=1.2.3.4:7700;voz=1", "servidor=1.2.3.4:7700;canal=4;voz=1;x=1"}) {
    CHECK_FALSE(leer_argumentos_de_aviso(malo));
  }
  CHECK(leer_argumentos_de_aviso("servidor=1.2.3.4:7700;canal=4294967295;voz=0"));
  CHECK_FALSE(leer_argumentos_de_aviso("servidor=1.2.3.4:7700;canal=1;voz=0" + std::string(100, ' ')));
}

TEST_CASE("Los avisos tienen tope de canales y de texto") {
  Avisos avisos = conectados();
  for (std::uint32_t canal = 1; canal <= 200; ++canal) {
    avisos.mensaje(chat(canal, 2, "Tano", "hola"), "c", false);
  }
  CHECK(avisos.pendientes() == kMaximoCanales);
  Avisos largo = conectados();
  std::string acentos;
  for (int i = 0; i < 300; ++i) {
    acentos += "\xC3\xA1";
  }
  largo.mensaje(chat(5, 2, "Tano", std::string(150, 'a') + "\n" + acentos), "Partida", true);
  const std::string texto = cuerpo(largo);
  CHECK(texto.size() <= std::string("Tano: ").size() + kTopeTextoAviso + 3);
  CHECK(texto.find('\n') == std::string::npos);
  CHECK(texto.substr(texto.size() - 3) == "\xE2\x80\xA6");
  CHECK(ventrik::es_utf8_valido(texto));
}

TEST_CASE("Una mención es @ y tu nombre, sin importar mayúsculas, con sus acentos y con borde") {
  CHECK(mencion_en("hola @Tano", "Tano") == Mencion::directa);
  CHECK(mencion_en("@tano, vení", "Tano") == Mencion::directa);
  CHECK(mencion_en("che @REN\xC3\x89!", "Ren\xC3\xA9") == Mencion::directa);
  CHECK(mencion_en("@Jos\xC3\xA9 hola", "Jose") == Mencion::ninguna);
  CHECK(mencion_en("@Juan Perez llegaste?", "Juan Perez") == Mencion::directa);
  CHECK(mencion_en("@Tanoo hola", "Tano") == Mencion::ninguna);
  CHECK(mencion_en("@Tano_2", "Tano") == Mencion::ninguna);
  CHECK(mencion_en("Tano sin arroba", "Tano") == Mencion::ninguna);
  CHECK(mencion_en("@todos a jugar", "Tano") == Mencion::todos);
  CHECK(mencion_en("@aqu\xC3\xAD y @aqui", "Tano") == Mencion::todos);
  CHECK(mencion_en("@everyone and @Here", "Tano") == Mencion::todos);
  CHECK(mencion_en("@todos y @Tano", "Tano") == Mencion::directa);
  CHECK(mencion_en("@\xC3", "\xC3\xA9") == Mencion::ninguna);
  CHECK(mencion_en(std::string(1000, '@') + "Tano", "Tano") == Mencion::ninguna);
  CHECK(mencion_en(std::string(31, '@') + "Tano", "Tano") == Mencion::directa);
}

TEST_CASE("El canal manda sobre el servidor, y silenciado solo avisa una mención si se pidió") {
  AvisosDelServidor avisos;
  constexpr std::uint64_t kHora = 1'000'000;
  CHECK_FALSE(avisa(avisos, 5, Mencion::ninguna, kHora));
  CHECK(avisa(avisos, 5, Mencion::directa, kHora));
  CHECK_FALSE(avisa(avisos, 5, Mencion::todos, kHora));
  avisos.contar_todos = true;
  CHECK(avisa(avisos, 5, Mencion::todos, kHora));
  avisos.servidor.silenciado_hasta = kHora + 1;
  CHECK(avisa(avisos, 5, Mencion::directa, kHora));
  avisos.servidor.aun_con_mencion = false;
  CHECK_FALSE(avisa(avisos, 5, Mencion::directa, kHora));
  CHECK(avisa(avisos, 5, Mencion::directa, kHora + 1));
  cambiar_canal(avisos, 5, {0, true, Avisar::todos});
  CHECK(avisa(avisos, 5, Mencion::ninguna, kHora));
  CHECK_FALSE(avisa(avisos, 6, Mencion::directa, kHora));
  cambiar_canal(avisos, 6, {kHastaQueLoActive, false, Avisar::todos});
  CHECK_FALSE(avisa(avisos, 6, Mencion::directa, kHora));
  cambiar_canal(avisos, 7, {0, true, Avisar::nada});
  CHECK_FALSE(avisa(avisos, 7, Mencion::directa, kHora + 5));
  REQUIRE(avisos.canales.size() == 3);
  cambiar_canal(avisos, 7, {});
  CHECK(avisos.canales.size() == 2);
  cambiar_canal(avisos, 5, {kHora + 10, true, Avisar::como_el_servidor});
  CHECK_FALSE(normalizar(avisos, kHora));
  CHECK(normalizar(avisos, kHora + 10));
  CHECK_FALSE(normalizar(avisos, kHora + 10));
  CHECK(avisos.canales.size() == 1);
  CHECK(avisos.servidor.silenciado_hasta == 0);
  CHECK(son_validos(avisos));
  for (std::uint32_t canal = 1; canal <= ventrik::protocolo::kMaximoCanales + 5; ++canal) {
    cambiar_canal(avisos, canal, {0, true, Avisar::todos});
  }
  CHECK(avisos.canales.size() == ventrik::protocolo::kMaximoCanales);
  CHECK(son_validos(avisos));
  avisos.servidor.avisar = Avisar::como_el_servidor;
  CHECK_FALSE(son_validos(avisos));
}

TEST_CASE("Por defecto avisa solo de las menciones, también en un canal no mirado") {
  Avisos avisos = conectados({}, {});
  avisos.mensaje(chat(6, 2, "Tano", "hola"), "general", false);
  CHECK(avisos.lista().empty());
  avisos.mensaje(chat(6, 2, "Tano", "@yo mir\xC3\xA1"), "general", false);
  CHECK(cuerpo(avisos) == "Tano: @yo mir\xC3\xA1");

  avisos.nuevos(7, "charla", 2);
  CHECK(avisos.lista().size() == 1);
  CHECK(pedido(avisos).canal == 7);
  avisos.historial(respuesta(7, {guardado(1, "Sofi", "hola"), guardado(2, "Sofi", "chau")}), 0);
  CHECK(avisos.lista().size() == 1);
  CHECK_FALSE(avisos.pedir(kAhora + 3s, false, false));
  avisos.nuevos(7, "charla", 3);
  CHECK(pedido(avisos, kAhora + 6s).canal == 7);
  avisos.historial(respuesta(7, {guardado(1, "Sofi", "hola"), guardado(2, "Sofi", "chau"), guardado(3, "Ana", "@Yo ven\xC3\xAD")}), 0);
  REQUIRE(avisos.lista().size() == 2);
  CHECK(avisos.lista()[1].cuerpo == "Ana: @Yo ven\xC3\xAD");

  const auto version = avisos.version();
  for (std::uint32_t total = 4; total <= 15; ++total) {
    avisos.nuevos(7, "charla", total);
    CHECK(pedido(avisos, kAhora + 6s + 3s * total).canal == 7);
    std::vector<MensajeGuardado> ultimos;
    for (std::uint32_t numero = total > 10 ? total - 9 : 1; numero <= total; ++numero) {
      ultimos.push_back(guardado(numero, "Sofi", "algo com\xC3\xBAn"));
    }
    avisos.historial(respuesta(7, ultimos), 0);
  }
  CHECK(avisos.version() == version);
  REQUIRE(avisos.lista().size() == 2);
  CHECK(avisos.lista()[1].cuerpo == "Ana: @Yo ven\xC3\xAD");
}

TEST_CASE("Con privacidad igual busca las menciones, pero solo muestra la cantidad") {
  OpcionesAvisos opciones;
  opciones.privado = true;
  Avisos avisos = conectados(opciones, {});
  avisos.nuevos(7, "charla", 1);
  CHECK(avisos.lista().empty());
  CHECK(pedido(avisos).canal == 7);
  avisos.historial(respuesta(7, {guardado(1, "Ana", "@Yo secreto")}), 0);
  CHECK(cuerpo(avisos) == "1 mensaje nuevo");
}

TEST_CASE("Un canal en Todos los mensajes avisa aunque el servidor esté silenciado") {
  AvisosDelServidor ajustes;
  ajustes.servidor.silenciado_hasta = kHastaQueLoActive;
  cambiar_canal(ajustes, 6, {0, true, Avisar::todos});
  cambiar_canal(ajustes, 9, {0, true, Avisar::nada});
  Avisos avisos = conectados({}, ajustes);
  avisos.mensaje(chat(5, 2, "Tano", "hola"), "otro", false);
  avisos.mensaje(chat(6, 2, "Tano", "hola"), "general", false);
  REQUIRE(avisos.lista().size() == 1);
  CHECK(avisos.lista()[0].canal == 6);
  avisos.nuevos(9, "silencioso", 4);
  CHECK_FALSE(avisos.pedir(kAhora, false, false));
}
