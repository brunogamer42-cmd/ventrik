#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

#include "client/mi_identidad.h"
#include "client/sesion.h"
#include "cliente_de_prueba.h"
#include "common/archivos.h"
#include "common/cifrado/clave.h"
#include "common/cifrado/tls.h"
#include "common/protocol/marcos.h"
#include "common/version.h"
#include "server/contrasena.h"
#include <doctest/doctest.h>

using namespace prueba;
using namespace ventrik::protocolo;
using ventrik::cliente::EstadoConexion;
using ventrik::cliente::Fin;
using ventrik::cliente::IntentoDeConexion;
using ventrik::cliente::MotivoFin;
using ventrik::servidor::Evento;

namespace {

class Manual {
 public:
  explicit Manual(const ServidorDePrueba& servidor)
      : config_(ventrik::cifrado::ConfigTls::para_programa()), canal_(ventrik::red::conectar(servidor.direccion(), 2s)) {
    REQUIRE(config_);
    REQUIRE(canal_.valido());
    tls_ = std::make_unique<ventrik::cifrado::SesionTls>(*config_, canal_.get());
    tls_->exigir_huella(servidor.huella());
    const auto hasta = std::chrono::steady_clock::now() + 5s;
    for (auto paso = tls_->apreton(); paso != ventrik::cifrado::Paso::listo; paso = tls_->apreton()) {
      REQUIRE((paso == ventrik::cifrado::Paso::esperar_lectura || paso == ventrik::cifrado::Paso::esperar_escritura));
      REQUIRE(std::chrono::steady_clock::now() < hasta);
      ventrik::red::esperar(canal_, paso == ventrik::cifrado::Paso::esperar_lectura,
                            paso == ventrik::cifrado::Paso::esperar_escritura, 100ms);
    }
    Escritor hola;
    escribir(hola, Hola{ventrik::kProtocoloMinimo, ventrik::kProtocoloMaximo, ventrik::kVersionPrograma, 0});
    std::vector<std::uint8_t> marco;
    REQUIRE(armar_marco(static_cast<std::uint16_t>(TipoMensaje::hola), hola.datos(), marco));
    enviar(marco);
    const auto bienvenida = leer();
    REQUIRE(bienvenida);
    REQUIRE(bienvenida->tipo == static_cast<std::uint16_t>(TipoMensaje::bienvenida));
    const auto leida = leer_bienvenida(bienvenida->cuerpo);
    REQUIRE(leida);
    protocolo_ = leida->protocolo;
    const auto cadena = leer();
    REQUIRE(cadena);
    REQUIRE(cadena->tipo == static_cast<std::uint16_t>(TipoMensaje::cadena_de_claves));
    REQUIRE(tls_->valor_para_identidad(valor_));
    huella_ = servidor.huella().bytes();
  }

  Presentarse firmada(const ventrik::cifrado::Clave& firmante, const std::string& nombre) const {
    Presentarse presentarse{nombre, {}};
    presentarse.clave = firmante.publica();
    firmar(presentarse, firmante, huella_);
    return presentarse;
  }

  void firmar(Presentarse& presentarse, const ventrik::cifrado::Clave& firmante,
              const ventrik::cifrado::Huella::Bytes& huella) const {
    const auto firma = firmante.firmar(lo_que_se_firma(presentarse, protocolo_, huella, valor_));
    REQUIRE(firma);
    presentarse.firma = *firma;
  }

  bool cerrada() { return !leer(); }

  void mandar(const Presentarse& presentarse) {
    std::vector<std::uint8_t> marco;
    REQUIRE(armar_mensaje(presentarse, marco));
    enviar(marco);
  }

  std::optional<MotivoPresentacion> presentar(const Presentarse& presentarse) {
    mandar(presentarse);
    const auto respuesta = leer();
    REQUIRE(respuesta);
    if (respuesta->tipo == static_cast<std::uint16_t>(TipoMensaje::presentado)) {
      return std::nullopt;
    }
    REQUIRE(respuesta->tipo == static_cast<std::uint16_t>(TipoMensaje::presentacion_rechazada));
    const auto rechazo = leer_mensaje<PresentacionRechazada>(respuesta->cuerpo);
    REQUIRE(rechazo);
    return rechazo->motivo;
  }

 private:
  void enviar(gsl::span<const std::uint8_t> datos) {
    const auto hasta = std::chrono::steady_clock::now() + 5s;
    while (!datos.empty()) {
      std::size_t escritos = 0;
      const auto paso = tls_->escribir(datos, escritos);
      REQUIRE(paso != ventrik::cifrado::Paso::cerrado);
      REQUIRE(std::chrono::steady_clock::now() < hasta);
      datos = datos.subspan(escritos);
      if (!datos.empty()) {
        ventrik::red::esperar(canal_, paso == ventrik::cifrado::Paso::esperar_lectura,
                              paso != ventrik::cifrado::Paso::esperar_lectura, 100ms);
      }
    }
  }

  std::optional<Marco> leer() {
    const auto hasta = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < hasta) {
      gsl::span<const std::uint8_t> pendientes(guardado_);
      if (auto marco = ensamblador_.siguiente(pendientes)) {
        guardado_.erase(guardado_.begin(), guardado_.end() - static_cast<std::ptrdiff_t>(pendientes.size()));
        return marco;
      }
      guardado_.clear();
      std::array<std::uint8_t, 4096> buffer{};
      std::size_t leidos = 0;
      const auto paso = tls_->leer(buffer, leidos);
      if (paso == ventrik::cifrado::Paso::listo) {
        guardado_.insert(guardado_.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(leidos));
        continue;
      }
      if (paso == ventrik::cifrado::Paso::cerrado || paso == ventrik::cifrado::Paso::error) {
        return std::nullopt;
      }
      ventrik::red::esperar(canal_, true, paso == ventrik::cifrado::Paso::esperar_escritura, 100ms);
    }
    return std::nullopt;
  }

  std::unique_ptr<ventrik::cifrado::ConfigTls> config_;
  ventrik::red::Socket canal_;
  std::unique_ptr<ventrik::cifrado::SesionTls> tls_;
  Ensamblador ensamblador_{kMaximoCuerpo};
  std::vector<std::uint8_t> guardado_;
  std::uint16_t protocolo_ = 0;
  std::array<std::uint8_t, kBytesValorDeConexion> valor_{};
  ventrik::cifrado::Huella::Bytes huella_{};
};

ventrik::cliente::Presentacion otra_identidad(std::string nombre,
                                              std::optional<ventrik::cifrado::CodigoDueno> codigo = {}) {
  return {std::move(nombre), codigo, identidad_nueva_de_prueba(), nullptr};
}

EstadoConexion estado_de(Cliente& cliente) {
  const auto intento = cliente.esperar<IntentoDeConexion>();
  REQUIRE(intento);
  return intento->estado;
}

std::wstring base_temporal() {
  std::array<wchar_t, MAX_PATH> carpeta{};
  REQUIRE(GetTempPathW(static_cast<DWORD>(carpeta.size()), carpeta.data()) > 0);
  static int numero = 0;
  const std::wstring archivo = std::wstring(carpeta.data()) + L"ventrik_entrar_" +
                               std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(++numero) + L".db";
  DeleteFileW(archivo.c_str());
  return archivo;
}

void borrar_base(const std::wstring& archivo) {
  for (const auto* final : {L"", L"-wal", L"-shm"}) {
    DeleteFileW((archivo + final).c_str());
  }
}

}  // namespace

TEST_CASE("Una presentación bien firmada entra y el servidor dice su nombre") {
  ventrik::servidor::Opciones opciones;
  opciones.nombre_servidor = "Escuadrón";
  ServidorDePrueba servidor(opciones);
  Cliente ana(servidor, "Ana");
  CHECK(ana.entrar().servidor == "Escuadrón");
  Manual manual(servidor);
  CHECK_FALSE(manual.presentar(manual.firmada(*identidad_de("Beto"), "Beto")).has_value());
}

TEST_CASE("Una firma de otra conexión, de otro servidor o de otra identidad no entra y cuenta como falla") {
  ServidorDePrueba servidor;
  const auto ana = identidad_de("Ana");
  const auto beto = identidad_de("Beto");
  Manual primera(servidor);
  const Presentarse de_otra_conexion = primera.firmada(*ana, "Ana");
  Manual segunda(servidor);
  CHECK(segunda.presentar(de_otra_conexion) == MotivoPresentacion::firma_invalida);

  Manual tercera(servidor);
  Presentarse otro_servidor{"Ana", {}};
  otro_servidor.clave = ana->publica();
  ventrik::cifrado::Huella::Bytes otra_huella{};
  otra_huella.fill(1);
  tercera.firmar(otro_servidor, *ana, otra_huella);
  CHECK(tercera.presentar(otro_servidor) == MotivoPresentacion::firma_invalida);

  Manual cuarta(servidor);
  Presentarse ajena = cuarta.firmada(*beto, "Ana");
  ajena.clave = ana->publica();
  CHECK(cuarta.presentar(ajena) == MotivoPresentacion::firma_invalida);

  Manual quinta(servidor);
  Presentarse cambiada = quinta.firmada(*ana, "Ana");
  cambiada.nombre = "Anita";
  CHECK(quinta.presentar(cambiada) == MotivoPresentacion::firma_invalida);

  Manual sexta(servidor);
  Presentarse fuera = sexta.firmada(*ana, "Ana");
  fuera.clave[64] ^= 1;
  CHECK(sexta.presentar(fuera) == MotivoPresentacion::firma_invalida);
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::firma_invalida) == 5; }));
}

TEST_CASE("Un nombre queda reservado para su identidad aunque no esté conectada, y cambiarlo lo libera") {
  ServidorDePrueba servidor;
  {
    Cliente jose(servidor, "José");
    jose.entrar();
  }
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::salio) == 1; }));
  Cliente impostor(servidor, otra_identidad("José"));
  CHECK(estado_de(impostor) == EstadoConexion::nombre_en_uso);
  Cliente parecido(servidor, otra_identidad("JOSÉ"));
  CHECK(estado_de(parecido) == EstadoConexion::nombre_en_uso);
  {
    Cliente renombrado(servidor, {"Pepe", {}, identidad_de("José"), nullptr});
    renombrado.entrar();
  }
  Cliente nuevo(servidor, otra_identidad("José"));
  CHECK(estado_de(nuevo) == EstadoConexion::conectado);
}

TEST_CASE("El código de dueño sirve una sola vez y el dueño sigue siéndolo sin código") {
  const auto codigo = ventrik::cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  ventrik::servidor::Opciones opciones;
  opciones.codigo_dueno = codigo;
  ServidorDePrueba servidor(opciones);
  {
    Cliente ana(servidor, "Ana", codigo);
    CHECK(ana.entrar().rol == ventrik::protocolo::Rol::dueno);
  }
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::dueno_reclamado) == 1; }));
  Cliente beto(servidor, "Beto", codigo);
  CHECK(estado_de(beto) == EstadoConexion::codigo_incorrecto);
  Cliente ana_otra_vez(servidor, "Ana");
  CHECK(ana_otra_vez.entrar().rol == ventrik::protocolo::Rol::dueno);
}

TEST_CASE("Si la misma identidad entra dos veces, la nueva saca a la vieja y los demás lo ven") {
  ServidorDePrueba servidor;
  Cliente beto(servidor, "Beto");
  beto.entrar();
  Cliente vieja(servidor, "Ana");
  const auto primera = vieja.entrar();
  REQUIRE(beto.esperar<UsuarioEntro>());
  Cliente nueva(servidor, "Ana");
  const auto segunda = nueva.entrar();
  CHECK(segunda.tu_id != primera.tu_id);
  const auto fin = vieja.esperar<Fin>();
  REQUIRE(fin);
  CHECK(fin->motivo == MotivoFin::desplazada);
  CHECK(beto.esperar<UsuarioSalio>([&](const UsuarioSalio& salio) { return salio.id == primera.tu_id; }));
  CHECK(beto.esperar<UsuarioEntro>([&](const UsuarioEntro& entro) { return entro.usuario.id == segunda.tu_id; }));
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::desplazada) == 1; }));
  CHECK(beto.no_llega<UsuarioSalio>());
}

TEST_CASE("El dueño desplazado por su otra sesión sigue siendo dueño") {
  const auto codigo = ventrik::cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  ventrik::servidor::Opciones opciones;
  opciones.codigo_dueno = codigo;
  ServidorDePrueba servidor(opciones);
  Cliente vieja(servidor, "Ana", codigo);
  CHECK(vieja.entrar().rol == ventrik::protocolo::Rol::dueno);
  Cliente nueva(servidor, "Ana");
  CHECK(nueva.entrar().rol == ventrik::protocolo::Rol::dueno);
  const auto fin = vieja.esperar<Fin>();
  REQUIRE(fin);
  CHECK(fin->motivo == MotivoFin::desplazada);
}

TEST_CASE("El servidor cuenta las entradas en días distintos y anota la última vez al salir") {
  const std::wstring archivo = base_temporal();
  constexpr std::uint64_t kDia = 24ull * 60 * 60 * 1000;
  const std::uint64_t ahora = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  {
    auto base = ventrik::servidor::Base::abrir(archivo, "General");
    REQUIRE(base);
    ventrik::servidor::UsuarioGuardado ana;
    ana.clave = identidad_de("Ana")->publica();
    ana.nombre = "Ana";
    ana.entradas = 1;
    ana.creado_ms = ahora - 3 * kDia;
    ana.visto_ms = ahora - 2 * kDia;
    REQUIRE(base->guardar_usuario(ana));
  }
  {
    ventrik::servidor::Opciones opciones;
    opciones.archivo_base = archivo;
    ServidorDePrueba servidor(opciones);
    {
      Cliente ana(servidor, "Ana");
      ana.entrar();
    }
    CHECK(esperar_que([&] { return servidor.cuantos(Evento::salio) == 1; }));
    {
      Cliente ana(servidor, "Ana");
      ana.entrar();
    }
    CHECK(esperar_que([&] { return servidor.cuantos(Evento::salio) == 2; }));
    {
      Cliente beto(servidor, "Beto");
      beto.entrar();
    }
    CHECK(esperar_que([&] { return servidor.cuantos(Evento::salio) == 3; }));
    Cliente beto(servidor, "Beto");
    beto.entrar();
  }
  {
    auto base = ventrik::servidor::Base::abrir(archivo, "General");
    REQUIRE(base);
    const auto usuarios = base->usuarios(ahora);
    REQUIRE(usuarios);
    REQUIRE(usuarios->size() == 2);
    for (const auto& usuario : *usuarios) {
      CHECK(usuario.entradas == (usuario.nombre == "Ana" ? 2u : 1u));
      CHECK(usuario.visto_ms >= ahora);
    }
  }
  borrar_base(archivo);
}

TEST_CASE("Los reemplazos seguidos de una identidad tienen tope") {
  ventrik::servidor::Opciones opciones;
  opciones.reemplazos_por_identidad = 2;
  ServidorDePrueba servidor(opciones);
  std::vector<std::unique_ptr<Cliente>> sesiones;
  for (int vez = 0; vez < 3; ++vez) {
    sesiones.push_back(std::make_unique<Cliente>(servidor, "Ana"));
    sesiones.back()->entrar();
  }
  Cliente de_mas(servidor, "Ana");
  CHECK(estado_de(de_mas) == EstadoConexion::demasiadas);
}

TEST_CASE("Las identidades nuevas tienen tope por IP y en total") {
  ventrik::servidor::Opciones por_ip;
  por_ip.identidades_nuevas_por_ip = 2;
  ServidorDePrueba servidor(por_ip);
  Cliente ana(servidor, "Ana");
  Cliente beto(servidor, "Beto");
  ana.entrar();
  beto.entrar();
  Cliente carla(servidor, "Carla");
  CHECK(estado_de(carla) == EstadoConexion::demasiadas);
  Cliente ana_otra_vez(servidor, "Ana");
  CHECK(estado_de(ana_otra_vez) == EstadoConexion::conectado);

  ventrik::servidor::Opciones total;
  total.maximo_identidades = 1;
  ServidorDePrueba chico(total);
  Cliente primera(chico, "Ana");
  primera.entrar();
  Cliente segunda(chico, "Beto");
  CHECK(estado_de(segunda) == EstadoConexion::servidor_lleno);
}

TEST_CASE("El servidor recuerda identidades, dueño y nombre al volver a abrir, y puede perder al dueño") {
  const std::wstring archivo = base_temporal();
  const auto codigo = ventrik::cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  {
    ventrik::servidor::Opciones opciones;
    opciones.archivo_base = archivo;
    opciones.codigo_dueno = codigo;
    opciones.nombre_servidor = "Taberna";
    ServidorDePrueba servidor(opciones);
    Cliente ana(servidor, "Ana", codigo);
    CHECK(ana.entrar().rol == ventrik::protocolo::Rol::dueno);
  }
  {
    ventrik::servidor::Opciones opciones;
    opciones.archivo_base = archivo;
    ServidorDePrueba servidor(opciones);
    Cliente impostor(servidor, otra_identidad("Ana"));
    CHECK(estado_de(impostor) == EstadoConexion::nombre_en_uso);
    Cliente ana(servidor, "Ana");
    const auto presentado = ana.entrar();
    CHECK(presentado.rol == ventrik::protocolo::Rol::dueno);
    CHECK(presentado.servidor == "Taberna");
  }
  {
    ventrik::servidor::Opciones opciones;
    opciones.archivo_base = archivo;
    opciones.quitar_dueno = true;
    opciones.codigo_dueno = codigo;
    ServidorDePrueba servidor(opciones);
    Cliente ana(servidor, "Ana");
    CHECK_FALSE(ana.entrar().rol == ventrik::protocolo::Rol::dueno);
    Cliente beto(servidor, "Beto", codigo);
    CHECK(beto.entrar().rol == ventrik::protocolo::Rol::dueno);
  }
  borrar_base(archivo);
}

TEST_CASE("Sin identidad no se intenta entrar") {
  ServidorDePrueba servidor;
  const auto resultado = ventrik::cliente::conectar(servidor.direccion(), servidor.huella(), {"Ana", {}});
  CHECK(resultado.estado == EstadoConexion::sin_identidad);
}

TEST_CASE("El código del equipo es distinto en cada servidor y siempre igual en el mismo") {
  ventrik::cifrado::Secreto secreto(ventrik::cliente::kBytesSecretoDeEquipo);
  secreto.bytes()[0] = 1;
  ventrik::cifrado::Huella::Bytes una{};
  ventrik::cifrado::Huella::Bytes otra{};
  otra.fill(2);
  const auto primero = ventrik::cliente::codigo_de_equipo(secreto, ventrik::cifrado::Huella(una));
  CHECK(primero == ventrik::cliente::codigo_de_equipo(secreto, ventrik::cifrado::Huella(una)));
  CHECK(primero != ventrik::cliente::codigo_de_equipo(secreto, ventrik::cifrado::Huella(otra)));
  CHECK(primero != decltype(primero){});
}

TEST_CASE("Una sola presentación por conexión, y una firma mala no revela si el nombre está ocupado") {
  ServidorDePrueba servidor;
  Cliente ana(servidor, "Ana");
  ana.entrar();
  Manual manual(servidor);
  CHECK(manual.presentar(manual.firmada(*identidad_de("Beto"), "Ana")) == MotivoPresentacion::nombre_en_uso);
  CHECK(manual.cerrada());
  Manual tanteo(servidor);
  Presentarse ajena = tanteo.firmada(*identidad_de("Beto"), "Ana");
  ajena.clave = identidad_de("Ana")->publica();
  CHECK(tanteo.presentar(ajena) == MotivoPresentacion::firma_invalida);
  CHECK(tanteo.cerrada());
}

TEST_CASE("Un reclamo de dueño rechazado por el nombre no gasta el código") {
  const auto codigo = ventrik::cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  ventrik::servidor::Opciones opciones;
  opciones.codigo_dueno = codigo;
  ServidorDePrueba servidor(opciones);
  {
    Cliente ana(servidor, "Ana");
    CHECK_FALSE(ana.entrar().rol == ventrik::protocolo::Rol::dueno);
  }
  Cliente impostor(servidor, otra_identidad("Ana", codigo));
  CHECK(estado_de(impostor) == EstadoConexion::nombre_en_uso);
  Cliente beto(servidor, "Beto", codigo);
  CHECK(beto.entrar().rol == ventrik::protocolo::Rol::dueno);
}

TEST_CASE("Las identidades nuevas también tienen tope por hora entre todas las IP") {
  ventrik::servidor::Opciones opciones;
  opciones.identidades_nuevas_por_hora = 2;
  opciones.maximo_identidades = 4;
  ServidorDePrueba servidor(opciones);
  Cliente ana(servidor, "Ana");
  Cliente beto(servidor, "Beto");
  ana.entrar();
  beto.entrar();
  Cliente carla(servidor, "Carla");
  CHECK(estado_de(carla) == EstadoConexion::demasiadas);
}

TEST_CASE("Con el servidor lleno se desaloja una identidad que entró una sola vez hace más de una semana") {
  const std::wstring archivo = base_temporal();
  constexpr std::uint64_t kDia = 24ull * 60 * 60 * 1000;
  const std::uint64_t ahora = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  {
    auto base = ventrik::servidor::Base::abrir(archivo, "General");
    REQUIRE(base);
    ventrik::servidor::UsuarioGuardado vieja;
    vieja.clave = identidad_nueva_de_prueba()->publica();
    vieja.nombre = "Vieja";
    vieja.entradas = 1;
    vieja.creado_ms = vieja.visto_ms = ahora - 8 * kDia;
    REQUIRE(base->guardar_usuario(vieja));
    ventrik::servidor::UsuarioGuardado volvio = vieja;
    volvio.clave = identidad_nueva_de_prueba()->publica();
    volvio.nombre = "Volvió";
    volvio.entradas = 2;
    volvio.creado_ms = ahora - 20 * kDia;
    REQUIRE(base->guardar_usuario(volvio));
  }
  {
    ventrik::servidor::Opciones opciones;
    opciones.archivo_base = archivo;
    opciones.maximo_identidades = 2;
    ServidorDePrueba servidor(opciones);
    Cliente ana(servidor, "Ana");
    ana.entrar();
    Cliente beto(servidor, "Beto");
    CHECK(estado_de(beto) == EstadoConexion::servidor_lleno);
    Cliente otra_vieja(servidor, otra_identidad("Vieja"));
    CHECK(estado_de(otra_vieja) == EstadoConexion::servidor_lleno);
  }
  borrar_base(archivo);
}

TEST_CASE("--liberar-nombre suelta el nombre de alguien que no es el dueño") {
  const std::wstring archivo = base_temporal();
  {
    ventrik::servidor::Opciones opciones;
    opciones.archivo_base = archivo;
    ServidorDePrueba servidor(opciones);
    Cliente perdida(servidor, otra_identidad("Perdida"));
    perdida.entrar();
  }
  {
    ventrik::servidor::Opciones opciones;
    opciones.archivo_base = archivo;
    opciones.liberar_nombre = "perdida";
    ServidorDePrueba servidor(opciones);
    Cliente nueva(servidor, otra_identidad("Perdida"));
    CHECK(estado_de(nueva) == EstadoConexion::conectado);
  }
  borrar_base(archivo);
}

TEST_CASE("El dueño nombra admins: manejan canales, todos ven su rol, no nombran a otros y el rol queda guardado") {
  const std::wstring archivo = base_temporal();
  const auto codigo = ventrik::cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  const auto negado = [](TipoMensaje pedido, MotivoNegado motivo) {
    return [pedido, motivo](const Negado& recibido) { return recibido.pedido == pedido && recibido.motivo == motivo; };
  };
  {
    ventrik::servidor::Opciones opciones;
    opciones.archivo_base = archivo;
    opciones.codigo_dueno = codigo;
    ServidorDePrueba servidor(opciones);
    Cliente ana(servidor, "Ana", codigo);
    const auto de_ana = ana.entrar();
    Cliente beto(servidor, "Beto");
    const auto de_beto = beto.entrar();
    beto.ordenar(CrearCanal{"Partidas"});
    CHECK(beto.esperar<Negado>(negado(TipoMensaje::crear_canal, MotivoNegado::sin_permiso)));
    beto.ordenar(NombrarAdmin{de_beto.tu_id, true});
    CHECK(beto.esperar<Negado>(negado(TipoMensaje::nombrar_admin, MotivoNegado::sin_permiso)));
    ana.ordenar(NombrarAdmin{de_ana.tu_id, false});
    CHECK(ana.esperar<Negado>(negado(TipoMensaje::nombrar_admin, MotivoNegado::sin_permiso)));
    ana.ordenar(NombrarAdmin{de_beto.tu_id, true});
    const auto es_admin = [&](const UsuarioRol& rol) { return rol.id == de_beto.tu_id && rol.rol == Rol::admin; };
    CHECK(ana.esperar<UsuarioRol>(es_admin));
    CHECK(beto.esperar<UsuarioRol>(es_admin));
    beto.ordenar(CrearCanal{"Partidas"});
    CHECK(ana.esperar<CanalCreado>([](const CanalCreado& creado) { return creado.canal.nombre == "Partidas"; }));
    beto.ordenar(NombrarAdmin{de_ana.tu_id, false});
    CHECK(beto.esperar<Negado>(negado(TipoMensaje::nombrar_admin, MotivoNegado::sin_permiso)));
    beto.ordenar(LiberarNombre{"Ana"});
    CHECK(beto.esperar<Negado>(negado(TipoMensaje::liberar_nombre, MotivoNegado::sin_permiso)));
    CHECK(esperar_que([&] { return servidor.cuantos(Evento::admin_nombrado) == 1; }));
  }
  {
    ventrik::servidor::Opciones opciones;
    opciones.archivo_base = archivo;
    ServidorDePrueba servidor(opciones);
    Cliente beto(servidor, "Beto");
    const auto de_beto = beto.entrar();
    CHECK(de_beto.rol == Rol::admin);
    Cliente ana(servidor, "Ana");
    const auto de_ana = ana.entrar();
    CHECK(std::any_of(de_ana.usuarios.begin(), de_ana.usuarios.end(), [&](const UsuarioInfo& usuario) {
      return usuario.id == de_beto.tu_id && usuario.rol == Rol::admin;
    }));
    ana.ordenar(NombrarAdmin{de_beto.tu_id, false});
    CHECK(beto.esperar<UsuarioRol>([&](const UsuarioRol& rol) { return rol.id == de_beto.tu_id && rol.rol == Rol::ninguno; }));
    beto.ordenar(CrearCanal{"Otra"});
    CHECK(beto.esperar<Negado>(negado(TipoMensaje::crear_canal, MotivoNegado::sin_permiso)));
    CHECK(esperar_que([&] { return servidor.cuantos(Evento::admin_quitado) == 1; }));
  }
  borrar_base(archivo);
}

TEST_CASE("Si un admin borra un canal, al dueño le llega el aviso al momento o al volver a entrar") {
  const auto codigo = ventrik::cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  ventrik::servidor::Opciones opciones;
  opciones.codigo_dueno = codigo;
  ServidorDePrueba servidor(opciones);
  std::optional<Cliente> ana;
  ana.emplace(servidor, "Ana", codigo);
  const auto de_ana = ana->entrar();
  Cliente beto(servidor, "Beto");
  const auto de_beto = beto.entrar();
  ana->ordenar(NombrarAdmin{de_beto.tu_id, true});
  REQUIRE(beto.esperar<UsuarioRol>());
  const auto crear = [&](Cliente& quien, const std::string& nombre) {
    quien.ordenar(CrearCanal{nombre, TipoCanal::texto});
    const auto creado =
        quien.esperar<CanalCreado>([&](const CanalCreado& canal) { return canal.canal.nombre == nombre; });
    REQUIRE(creado);
    return creado->canal.id;
  };
  beto.ordenar(BorrarCanal{crear(beto, "Uno")});
  const auto aviso = ana->esperar<AvisoDeSeguridad>();
  REQUIRE(aviso);
  CHECK(aviso->tipo == Seguridad::canal_borrado);
  CHECK(aviso->quien == "Beto");
  CHECK(aviso->detalle == "Uno");
  CHECK(beto.no_llega<AvisoDeSeguridad>());
  const std::uint32_t dos = crear(beto, "Dos");
  ana.reset();
  REQUIRE(beto.esperar<UsuarioSalio>([&](const UsuarioSalio& salio) { return salio.id == de_ana.tu_id; }));
  beto.ordenar(BorrarCanal{dos});
  REQUIRE(beto.esperar<CanalBorrado>([&](const CanalBorrado& borrado) { return borrado.id == dos; }));
  Cliente vuelve(servidor, "Ana");
  vuelve.entrar();
  const auto pendiente = vuelve.esperar<AvisoDeSeguridad>();
  REQUIRE(pendiente);
  CHECK(pendiente->detalle == "Dos");
  vuelve.ordenar(BorrarCanal{crear(vuelve, "Tres")});
  CHECK(vuelve.no_llega<AvisoDeSeguridad>());
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::canal_borrado) == 3; }));
}

TEST_CASE("Entrar desde un equipo nuevo queda anotado y avisa al volver desde el de antes, no al equipo nuevo") {
  ServidorDePrueba servidor;
  const auto identidad = identidad_nueva_de_prueba();
  const auto desde = [&](std::string_view equipo) {
    return ventrik::cliente::Presentacion{"Ana", std::nullopt, identidad, secreto_de(equipo), nullptr};
  };
  {
    Cliente casa(servidor, desde("casa"));
    casa.entrar();
    CHECK(casa.no_llega<AvisoDeSeguridad>());
  }
  Cliente otra(servidor, desde("otra"));
  otra.entrar();
  CHECK(otra.no_llega<AvisoDeSeguridad>());
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::equipo_nuevo) == 1; }));
  Cliente casa(servidor, desde("casa"));
  casa.entrar();
  const auto aviso = casa.esperar<AvisoDeSeguridad>();
  REQUIRE(aviso);
  CHECK(aviso->tipo == Seguridad::equipo_nuevo);
  CHECK(aviso->quien == "Ana");
  CHECK(servidor.cuantos(Evento::equipo_nuevo) == 1);
}

TEST_CASE("El dueño libera el nombre de alguien desconectado, pero no el suyo ni el de alguien conectado") {
  const auto codigo = ventrik::cifrado::CodigoDueno::generar();
  REQUIRE(codigo);
  ventrik::servidor::Opciones opciones;
  opciones.codigo_dueno = codigo;
  ServidorDePrueba servidor(opciones);
  {
    Cliente perdida(servidor, otra_identidad("Perdida"));
    perdida.entrar();
  }
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::salio) == 1; }));
  Cliente ana(servidor, "Ana", codigo);
  ana.entrar();
  Cliente carla(servidor, "Carla");
  carla.entrar();
  const auto negado = [&](const std::string& nombre, MotivoNegado motivo) {
    ana.ordenar(LiberarNombre{nombre});
    return ana.esperar<Negado>([motivo](const Negado& recibido) {
      return recibido.pedido == TipoMensaje::liberar_nombre && recibido.motivo == motivo;
    });
  };
  CHECK(negado("ana", MotivoNegado::sin_permiso));
  CHECK(negado("Carla", MotivoNegado::sin_permiso));
  CHECK(negado("Nadie", MotivoNegado::nombre_libre));
  ana.ordenar(LiberarNombre{"perdida"});
  const auto aviso = ana.esperar<AvisoDeSeguridad>();
  REQUIRE(aviso);
  CHECK(aviso->tipo == Seguridad::nombre_liberado);
  CHECK(aviso->detalle == "Perdida");
  Cliente nueva(servidor, otra_identidad("Perdida"));
  CHECK(estado_de(nueva) == EstadoConexion::conectado);
  CHECK(servidor.cuantos(Evento::nombre_liberado) == 1);
}

TEST_CASE("Cortar la conexión mientras se calcula la contraseña no saltea la espera") {
  ventrik::servidor::Opciones opciones;
  opciones.poner_contrasena = ventrik::servidor::guardar_contrasena("sésamo", ventrik::cifrado::Argon2{32 * 1024, 3});
  REQUIRE(opciones.poner_contrasena);
  ServidorDePrueba servidor(opciones);
  const auto intruso = identidad_nueva_de_prueba();
  {
    Manual manual(servidor);
    Presentarse presentarse{"Intruso", {}};
    presentarse.clave = intruso->publica();
    presentarse.contrasena = "mala";
    manual.firmar(presentarse, *intruso, servidor.huella().bytes());
    manual.mandar(presentarse);
  }
  CHECK(esperar_que([&] { return servidor.cuantos(Evento::contrasena_incorrecta) == 1; }));
  const auto resultado = ventrik::cliente::conectar(servidor.direccion(), servidor.huella(),
                                                    {"Intruso", {}, intruso, nullptr, secreto_de("sésamo")});
  CHECK(resultado.estado == EstadoConexion::esperar);
  CHECK(resultado.espera >= 1);
}
