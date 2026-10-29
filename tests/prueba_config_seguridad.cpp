#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include <windows.h>

#include "cliente_de_prueba.h"
#include "common/cifrado/tls.h"
#include "server/base.h"
#include <doctest/doctest.h>

using namespace prueba;
using namespace ventrik::protocolo;
using ventrik::Algoritmo;
using ventrik::Nivel;
using ventrik::Opcion;
using ventrik::servidor::Evento;

namespace {
std::wstring base_temporal() {
  std::array<wchar_t, MAX_PATH> carpeta{};
  REQUIRE(GetTempPathW(static_cast<DWORD>(carpeta.size()), carpeta.data()) > 0);
  static int numero = 0;
  const std::wstring archivo = std::wstring(carpeta.data()) + L"ventrik_seguridad_" +
                               std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(++numero) + L".db";
  DeleteFileW(archivo.c_str());
  return archivo;
}

void borrar_base(const std::wstring& archivo) {
  for (const auto* final : {L"", L"-wal", L"-shm"}) {
    DeleteFileW((archivo + final).c_str());
  }
}

ventrik::Seguridad nivel(Nivel cual) {
  ventrik::Seguridad seguridad;
  ventrik::elegir_nivel(seguridad, cual);
  return seguridad;
}

ventrik::Seguridad con(Opcion opcion, std::uint8_t valor, ventrik::Seguridad seguridad = {}) {
  REQUIRE(ventrik::cambiar_opcion(seguridad, opcion, valor));
  return seguridad;
}

bool saluda(const ServidorDePrueba& servidor, Algoritmo ofrece, std::string* cifrado = nullptr) {
  const auto config = ventrik::cifrado::ConfigTls::para_programa(ofrece);
  REQUIRE(config);
  const auto canal = ventrik::red::conectar(servidor.direccion(), 2s);
  REQUIRE(canal.valido());
  ventrik::cifrado::SesionTls tls(*config, canal.get());
  tls.exigir_huella(servidor.huella());
  const auto hasta = std::chrono::steady_clock::now() + 5s;
  for (auto paso = tls.apreton(); paso != ventrik::cifrado::Paso::listo; paso = tls.apreton()) {
    if ((paso != ventrik::cifrado::Paso::esperar_lectura && paso != ventrik::cifrado::Paso::esperar_escritura) ||
        std::chrono::steady_clock::now() >= hasta) {
      return false;
    }
    ventrik::red::esperar(canal, paso == ventrik::cifrado::Paso::esperar_lectura,
                          paso == ventrik::cifrado::Paso::esperar_escritura, 100ms);
  }
  if (cifrado != nullptr) {
    *cifrado = tls.cifrado();
  }
  return true;
}

ConfigSeguridad cambiar(Cliente& dueno, const ventrik::Seguridad& nueva) {
  dueno.ordenar(CambiarConfigSeguridad{nueva});
  const auto respuesta = dueno.esperar<ConfigSeguridad>();
  REQUIRE(respuesta);
  return *respuesta;
}

struct ConDueno {
  std::optional<ventrik::cifrado::CodigoDueno> codigo = ventrik::cifrado::CodigoDueno::generar();
  ventrik::servidor::Opciones opciones() const {
    ventrik::servidor::Opciones armadas;
    armadas.codigo_dueno = codigo;
    return armadas;
  }
};
}

TEST_CASE("Cada nivel prende en el servidor lo que dice la tabla de niveles") {
  for (const Nivel cual : {Nivel::basico, Nivel::reforzado, Nivel::maximo}) {
    CAPTURE(static_cast<int>(cual));
    const auto esperado = nivel(cual);
    const std::wstring archivo = base_temporal();
    {
      ventrik::servidor::Opciones opciones;
      opciones.archivo_base = archivo;
      opciones.seguridad = esperado;
      ServidorDePrueba servidor(opciones);
      Cliente ana(servidor, "Ana");
      ana.entrar_con_voz();
      const auto puente = std::make_shared<ventrik::cliente::PuenteVoz>();
      ana.voz(puente);
      REQUIRE(esperar_que([&] { return puente->estado == ventrik::cliente::EstadoVoz::lista; }));
      CHECK(puente->voz_fija == (esperado.opciones[Opcion::voz_fija] != 0));
      const bool pide_a_todos = esperado.opciones[Opcion::segundo_paso] == static_cast<std::uint8_t>(ventrik::SegundoPaso::todos);
      CHECK(ana.esperar<ConfigurarSegundoPaso>({}, pide_a_todos ? 5000ms : 300ms).has_value() == pide_a_todos);
    }
    auto base = ventrik::servidor::Base::abrir(archivo, "General");
    REQUIRE(base);
    std::optional<ventrik::Seguridad> guardada;
    REQUIRE(base->leer_seguridad(guardada));
    REQUIRE(guardada);
    CHECK(guardada->nivel == cual);
    CHECK(guardada->opciones == esperado.opciones);
    CHECK(base->historial_cifrado() == (esperado.opciones[Opcion::historial_cifrado] != 0));
    CHECK((base->retencion() == ventrik::servidor::Retencion::no_guardar) ==
          (esperado.opciones[Opcion::sin_historial] != 0));
    base.reset();
    borrar_base(archivo);
  }
}

TEST_CASE("El dueño pide y cambia la configuración: una opción de otro nivel pasa a Personalizado y queda anotado") {
  const std::wstring archivo = base_temporal();
  ConDueno dueno;
  auto opciones = dueno.opciones();
  opciones.archivo_base = archivo;
  {
    ServidorDePrueba servidor(opciones);
    Cliente ana(servidor, "Ana", dueno.codigo);
    ana.entrar();
    ana.ordenar(PedirConfigSeguridad{});
    const auto actual = ana.esperar<ConfigSeguridad>();
    REQUIRE(actual);
    CHECK(actual->seguridad.nivel == Nivel::basico);
    CHECK_FALSE(actual->xp);
    const auto cambiada = cambiar(ana, con(Opcion::avisos_de_admins, 1));
    CHECK(cambiada.seguridad.nivel == Nivel::personalizado);
    CHECK(cambiada.seguridad.opciones[Opcion::avisos_de_admins] == 1);
    CHECK(cambiar(ana, con(Opcion::historial_cifrado, 1, nivel(Nivel::basico))).seguridad.nivel == Nivel::personalizado);
    CHECK(esperar_que([&] { return servidor.cuantos(Evento::config_cambiada) == 2; }));
  }
  {
    ventrik::servidor::Opciones sin_nada;
    sin_nada.archivo_base = archivo;
    ServidorDePrueba servidor(sin_nada);
    Cliente beto(servidor, "Beto");
    beto.entrar();
  }
  auto base = ventrik::servidor::Base::abrir(archivo, "General");
  REQUIRE(base);
  std::optional<ventrik::Seguridad> guardada;
  REQUIRE(base->leer_seguridad(guardada));
  REQUIRE(guardada);
  CHECK(guardada->nivel == Nivel::personalizado);
  CHECK(guardada->opciones[Opcion::historial_cifrado] == 1);
  int anotadas = 0;
  for (const auto& entrada : base->registro(100)) {
    if (entrada.tipo == Seguridad::config_cambiada) {
      CHECK(entrada.quien == "Ana");
      ++anotadas;
    }
  }
  CHECK(anotadas == 2);
  base.reset();
  borrar_base(archivo);
}

TEST_CASE("Nadie más que el dueño con su segundo paso pide o cambia la configuración") {
  ConDueno dueno;
  auto opciones = dueno.opciones();
  opciones.segundo_paso = ventrik::SegundoPaso::admins;
  ServidorDePrueba servidor(opciones);
  Cliente ana(servidor, "Ana", dueno.codigo);
  CHECK(ana.entrar().rol == Rol::ninguno);
  Cliente beto(servidor, "Beto");
  beto.entrar();
  for (Cliente* quien : {&ana, &beto}) {
    quien->ordenar(PedirConfigSeguridad{});
    quien->ordenar(CambiarConfigSeguridad{nivel(Nivel::basico)});
    for (const TipoMensaje pedido : {TipoMensaje::pedir_config_seguridad, TipoMensaje::cambiar_config_seguridad}) {
      CHECK(quien->esperar<Negado>([pedido](const Negado& negado) {
        return negado.pedido == pedido && negado.motivo == MotivoNegado::sin_permiso;
      }));
    }
    CHECK(quien->no_llega<ConfigSeguridad>());
  }
  CHECK(servidor.cuantos(Evento::config_cambiada) == 0);
}

TEST_CASE("Con solo ChaCha20 una conexión que solo ofrece AES no entra, y al revés, también al cambiarlo en vivo") {
  ConDueno dueno;
  auto opciones = dueno.opciones();
  opciones.seguridad = con(Opcion::algoritmo, static_cast<std::uint8_t>(Algoritmo::solo_chacha20));
  ServidorDePrueba servidor(opciones);
  std::string cifrado;
  CHECK_FALSE(saluda(servidor, Algoritmo::solo_aes_gcm));
  REQUIRE(saluda(servidor, Algoritmo::solo_chacha20, &cifrado));
  CHECK(cifrado.find("CHACHA20") != std::string::npos);
  Cliente ana(servidor, "Ana", dueno.codigo);
  const auto intento = ana.esperar<ventrik::cliente::IntentoDeConexion>();
  REQUIRE(intento);
  REQUIRE(intento->estado == ventrik::cliente::EstadoConexion::conectado);
  CHECK(intento->cifrado.find("CHACHA20") != std::string::npos);
  cambiar(ana, con(Opcion::algoritmo, static_cast<std::uint8_t>(Algoritmo::solo_aes_gcm)));
  CHECK_FALSE(saluda(servidor, Algoritmo::solo_chacha20));
  REQUIRE(saluda(servidor, Algoritmo::solo_aes_gcm, &cifrado));
  CHECK(cifrado.find("AES") != std::string::npos);
  Cliente beto(servidor, "Beto");
  CHECK(beto.esperar<ventrik::cliente::IntentoDeConexion>()->cifrado.find("AES") != std::string::npos);
}

TEST_CASE("La voz de tamaño fijo se avisa al entrar a la voz y cuando cambia") {
  ConDueno dueno;
  ServidorDePrueba servidor(dueno.opciones());
  Cliente ana(servidor, "Ana", dueno.codigo);
  Cliente beto(servidor, "Beto");
  ana.entrar_con_voz();
  beto.entrar_con_voz();
  const auto puente_ana = std::make_shared<ventrik::cliente::PuenteVoz>();
  const auto puente_beto = std::make_shared<ventrik::cliente::PuenteVoz>();
  ana.voz(puente_ana);
  beto.voz(puente_beto);
  REQUIRE(llega_alguno(*puente_ana, *puente_beto));
  CHECK_FALSE(puente_beto->voz_fija);
  cambiar(ana, con(Opcion::voz_fija, 1));
  CHECK(esperar_que([&] { return puente_ana->voz_fija && puente_beto->voz_fija; }));
  CHECK(llega_alguno(*puente_ana, *puente_beto));
  cambiar(ana, ventrik::Seguridad{});
  CHECK(esperar_que([&] { return !puente_ana->voz_fija && !puente_beto->voz_fija; }));
}

TEST_CASE("Sin avisos de acciones de admins el dueño no se entera de un canal borrado, pero queda anotado") {
  const std::wstring archivo = base_temporal();
  ConDueno dueno;
  auto opciones = dueno.opciones();
  opciones.archivo_base = archivo;
  {
    ServidorDePrueba servidor(opciones);
    Cliente ana(servidor, "Ana", dueno.codigo);
    ana.entrar();
    Cliente beto(servidor, "Beto");
    const auto de_beto = beto.entrar();
    ana.ordenar(NombrarAdmin{de_beto.tu_id, true});
    REQUIRE(beto.esperar<UsuarioRol>());
    const auto borrar = [&](const std::string& nombre) {
      beto.ordenar(CrearCanal{nombre, TipoCanal::texto});
      const auto creado = beto.esperar<CanalCreado>([&](const CanalCreado& canal) { return canal.canal.nombre == nombre; });
      REQUIRE(creado);
      beto.ordenar(BorrarCanal{creado->canal.id});
      REQUIRE(ana.esperar<CanalBorrado>([&](const CanalBorrado& borrado) { return borrado.id == creado->canal.id; }));
    };
    borrar("Uno");
    CHECK(ana.no_llega<AvisoDeSeguridad>());
    cambiar(ana, con(Opcion::avisos_de_admins, 1));
    borrar("Dos");
    const auto aviso = ana.esperar<AvisoDeSeguridad>([](const AvisoDeSeguridad& recibido) {
      return recibido.tipo == Seguridad::canal_borrado;
    });
    REQUIRE(aviso);
    CHECK(aviso->detalle == "Dos");
  }
  auto base = ventrik::servidor::Base::abrir(archivo, "General");
  REQUIRE(base);
  int borrados = 0;
  for (const auto& entrada : base->registro(100)) {
    borrados += entrada.tipo == Seguridad::canal_borrado ? 1 : 0;
  }
  CHECK(borrados == 2);
  base.reset();
  borrar_base(archivo);
}

TEST_CASE("Sin historial los mensajes llegan pero no se guardan, y al volver a guardar se guardan otra vez") {
  ConDueno dueno;
  ServidorDePrueba servidor(dueno.opciones());
  Cliente ana(servidor, "Ana", dueno.codigo);
  Cliente beto(servidor, "Beto");
  ana.entrar_con_voz();
  beto.entrar_con_voz();
  const auto charlar = [&](const std::string& texto) {
    ana.ordenar(EnviarChat{kCanalGeneral, texto});
    REQUIRE(beto.esperar<MensajeChat>([&](const MensajeChat& mensaje) { return mensaje.texto == texto; }));
  };
  const auto guardados = [&] {
    beto.ordenar(PedirHistorial{kCanalGeneral, 0});
    const auto historial = beto.esperar<Historial>();
    REQUIRE(historial);
    return historial->mensajes.size();
  };
  charlar("antes");
  CHECK(guardados() == 1);
  cambiar(ana, con(Opcion::sin_historial, 1));
  CHECK(guardados() == 0);
  charlar("sin guardar");
  CHECK(guardados() == 0);
  cambiar(ana, ventrik::Seguridad{});
  charlar("de nuevo");
  std::this_thread::sleep_for(1100ms);
  CHECK(guardados() == 1);
}

TEST_CASE("Prender el historial cifrado en vivo convierte lo guardado y el chat sigue igual") {
  const std::wstring archivo = base_temporal();
  ConDueno dueno;
  auto opciones = dueno.opciones();
  opciones.archivo_base = archivo;
  {
    ServidorDePrueba servidor(opciones);
    Cliente ana(servidor, "Ana", dueno.codigo);
    ana.entrar_con_voz();
    ana.ordenar(EnviarChat{kCanalGeneral, "una frase en claro"});
    REQUIRE(ana.esperar<MensajeChat>());
    CHECK(cambiar(ana, con(Opcion::historial_cifrado, 1)).seguridad.opciones[Opcion::historial_cifrado] == 1);
    ana.ordenar(PedirHistorial{kCanalGeneral, 0});
    const auto historial = ana.esperar<Historial>();
    REQUIRE(historial);
    REQUIRE(historial->mensajes.size() == 1);
    CHECK(historial->mensajes[0].texto == "una frase en claro");
  }
  auto base = ventrik::servidor::Base::abrir(archivo, "General");
  REQUIRE(base);
  CHECK(base->historial_cifrado());
  CHECK(base->historial({kCanalGeneral, 0}, 65536).mensajes.empty());
  base.reset();
  borrar_base(archivo);
}

TEST_CASE("Máximo en un servidor con Windows XP se avisa en la consola y en la configuración") {
  ConDueno dueno;
  auto opciones = dueno.opciones();
  opciones.en_xp = true;
  ServidorDePrueba servidor(opciones);
  Cliente ana(servidor, "Ana", dueno.codigo);
  ana.entrar();
  ventrik::Seguridad maximo = nivel(Nivel::maximo);
  REQUIRE(ventrik::cambiar_opcion(maximo, Opcion::segundo_paso, 0));
  const auto respuesta = cambiar(ana, maximo);
  CHECK(respuesta.xp);
  CHECK(respuesta.seguridad.nivel == Nivel::personalizado);
  CHECK(servidor.cuantos(Evento::maximo_en_xp) == 0);
  ventrik::elegir_nivel(maximo, Nivel::maximo);
  ana.ordenar(CambiarConfigSeguridad{maximo});
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::maximo_en_xp) == 1; }));
}

TEST_CASE("Un mensaje que llega mientras se pasa a no guardar el historial no queda guardado") {
  ConDueno dueno;
  ServidorDePrueba servidor(dueno.opciones());
  Cliente ana(servidor, "Ana", dueno.codigo);
  ana.entrar_con_voz();
  ana.ordenar_juntas({CambiarConfigSeguridad{con(Opcion::sin_historial, 1)}, EnviarChat{kCanalGeneral, "en el medio"}});
  REQUIRE(ana.esperar<ConfigSeguridad>());
  REQUIRE(ana.esperar<MensajeChat>());
  ana.ordenar(PedirHistorial{kCanalGeneral, 0});
  const auto historial = ana.esperar<Historial>();
  REQUIRE(historial);
  CHECK(historial->mensajes.empty());
}

TEST_CASE("El dueño no puede pedirse un segundo paso que no podría activar desde ese equipo") {
  ConDueno dueno;
  ServidorDePrueba servidor(dueno.opciones());
  const auto identidad = identidad_nueva_de_prueba();
  {
    Cliente casa(servidor, {"Ana", dueno.codigo, identidad, secreto_de("casa"), nullptr});
    casa.entrar();
  }
  Cliente otra(servidor, {"Ana", std::nullopt, identidad, secreto_de("otra"), nullptr});
  CHECK(otra.entrar().rol == Rol::dueno);
  otra.ordenar(CambiarConfigSeguridad{con(Opcion::segundo_paso, 1)});
  CHECK(otra.esperar<Negado>([](const Negado& negado) {
    return negado.pedido == TipoMensaje::cambiar_config_seguridad && negado.motivo == MotivoNegado::sin_permiso;
  }));
  CHECK(otra.no_llega<ConfigSeguridad>());
  Cliente vuelve(servidor, {"Ana", std::nullopt, identidad, secreto_de("casa"), nullptr});
  vuelve.entrar();
  REQUIRE(vuelve.esperar<AvisoDeSeguridad>());
  vuelve.ordenar(CambiarConfigSeguridad{con(Opcion::segundo_paso, 1)});
  CHECK(vuelve.esperar<ConfigurarSegundoPaso>());
  CHECK(vuelve.no_llega<Negado>());
}