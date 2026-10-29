#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <windows.h>

#include "client/conexion.h"
#include "client/mis_servidores.h"
#include "common/archivos.h"
#include "common/cifrado/boveda.h"
#include "common/cifrado/cadena.h"
#include "common/cifrado/identidad.h"
#include "common/cifrado/sobre.h"
#include "common/protocol/hostear.h"
#include "common/protocol/pedidos.h"
#include "server/base.h"
#include "server/contrasena.h"
#include "server/crear.h"
#include "server/mudanza.h"
#include "server/servidor.h"
#include "cliente_de_prueba.h"
#include "identidad_de_prueba.h"
#include "servidor_de_prueba.h"
#include "servidor_externo.h"
#include <doctest/doctest.h>

using namespace std::chrono_literals;
using namespace prueba;
using ventrik::cliente::EstadoConexion;
using ventrik::protocolo::TipoMensaje;
using ventrik::servidor::Creado;

namespace {

using Bytes = std::vector<std::uint8_t>;
constexpr const char* kArranque = "arranque largo";

class Raiz {
 public:
  Raiz() {
    std::array<wchar_t, MAX_PATH> base{};
    REQUIRE(GetTempPathW(static_cast<DWORD>(base.size()), base.data()) > 0);
    static std::atomic<int> creadas{0};
    ruta_ = std::wstring(base.data()) + L"ventrik_hostear_410_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
            std::to_wstring(++creadas) + L"_ñ";
    REQUIRE(ventrik::crear_carpetas(ruta_));
  }
  ~Raiz() { borrar(ruta_); }
  Raiz(const Raiz&) = delete;
  Raiz& operator=(const Raiz&) = delete;

  std::wstring de(const std::wstring& nombre) const { return ruta_ + L"\\" + nombre; }
  const std::wstring& ruta() const { return ruta_; }

 private:
  static void borrar(const std::wstring& carpeta) {
    WIN32_FIND_DATAW encontrado = {};
    const HANDLE busqueda = FindFirstFileW((carpeta + L"\\*").c_str(), &encontrado);
    if (busqueda != INVALID_HANDLE_VALUE) {
      do {
        const std::wstring nombre = encontrado.cFileName;
        if (nombre == L"." || nombre == L"..") {
          continue;
        }
        if ((encontrado.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
          borrar(carpeta + L"\\" + nombre);
        } else {
          DeleteFileW((carpeta + L"\\" + nombre).c_str());
        }
      } while (FindNextFileW(busqueda, &encontrado) != 0);
      FindClose(busqueda);
    }
    RemoveDirectoryW(carpeta.c_str());
  }

  std::wstring ruta_;
};

bool existe(const std::wstring& ruta) { return GetFileAttributesW(ruta.c_str()) != INVALID_FILE_ATTRIBUTES; }

std::size_t cuantos_archivos(const std::wstring& carpeta) {
  std::size_t cuantos = 0;
  WIN32_FIND_DATAW encontrado = {};
  const HANDLE busqueda = FindFirstFileW((carpeta + L"\\*").c_str(), &encontrado);
  if (busqueda == INVALID_HANDLE_VALUE) {
    return 0;
  }
  do {
    cuantos += std::wstring_view(encontrado.cFileName) != L"." && std::wstring_view(encontrado.cFileName) != L"..";
  } while (FindNextFileW(busqueda, &encontrado) != 0);
  FindClose(busqueda);
  return cuantos;
}

ventrik::protocolo::ServidorNuevo servidor_nuevo(const std::string& nombre, std::string_view contrasena = {},
                                                 std::string_view arranque = {},
                                                 ventrik::Nivel nivel = ventrik::Nivel::basico) {
  ventrik::protocolo::ServidorNuevo nuevo;
  nuevo.nombre = nombre;
  ventrik::elegir_nivel(nuevo.seguridad, nivel);
  nuevo.dueno = identidad_de("Dueño")->publica();
  nuevo.nombre_dueno = "Dueño";
  nuevo.contrasena = ventrik::cifrado::secreto_de(contrasena);
  nuevo.arranque = ventrik::cifrado::secreto_de(arranque);
  return nuevo;
}

ventrik::cifrado::Secreto clave_del_historial(const ventrik::cifrado::Boveda& boveda) {
  auto clave = boveda.derivar(ventrik::cifrado::kEtiquetaHistorial);
  REQUIRE(clave);
  return std::move(*clave);
}

std::unique_ptr<ventrik::servidor::Base> abrir_base(const std::wstring& carpeta,
                                                    std::optional<std::string_view> arranque = std::nullopt) {
  auto cargada = ventrik::cifrado::cargar_o_crear_identidad(carpeta, arranque, true);
  REQUIRE(cargada.boveda);
  auto base = ventrik::servidor::Base::abrir(carpeta + ventrik::servidor::kArchivoBase, "General",
                                             clave_del_historial(*cargada.boveda));
  REQUIRE(base);
  return base;
}

std::optional<ventrik::protocolo::DatosPublicos> datos_publicos(const std::wstring& carpeta) {
  ventrik::cifrado::Secreto leido;
  return ventrik::leer_archivo(carpeta + ventrik::protocolo::kArchivoPublico, ventrik::protocolo::kMaximoDatosPublicos,
                               leido) == ventrik::LecturaArchivo::leido
             ? ventrik::protocolo::leer_datos_publicos(leido.bytes())
             : std::nullopt;
}

class ServidorDeCarpeta {
 public:
  using Evento = ventrik::servidor::Evento;

  explicit ServidorDeCarpeta(const std::wstring& carpeta) {
    auto cargada = ventrik::cifrado::cargar_o_crear_identidad(carpeta, std::nullopt, true);
    REQUIRE(cargada.identidad);
    REQUIRE(cargada.boveda);
    identidad_ = std::make_unique<ventrik::cifrado::IdentidadServidor>(std::move(*cargada.identidad));
    ventrik::servidor::Opciones opciones;
    opciones.escuchar = {kLocal, 0};
    opciones.nombre_general = "General";
    opciones.archivo_base = carpeta + ventrik::servidor::kArchivoBase;
    opciones.archivo_publico = carpeta + ventrik::protocolo::kArchivoPublico;
    opciones.identidades_nuevas_por_ip = opciones.identidades_nuevas_por_hora = 1000;
    servidor_ = std::make_unique<ventrik::servidor::Servidor>(
        *identidad_, opciones, [this](Evento evento, const ventrik::red::Direccion&, const std::string&) {
          ++eventos_[static_cast<std::size_t>(evento)];
        });
    servidor_->clave_del_historial(clave_del_historial(*cargada.boveda));
    REQUIRE(servidor_->escuchar());
    hilo_ = std::thread([this] { servidor_->correr(seguir_); });
  }
  ~ServidorDeCarpeta() {
    seguir_ = false;
    hilo_.join();
  }
  ServidorDeCarpeta(const ServidorDeCarpeta&) = delete;
  ServidorDeCarpeta& operator=(const ServidorDeCarpeta&) = delete;

  ventrik::red::Direccion direccion() const { return {kLocal, servidor_->puerto()}; }
  const ventrik::cifrado::Huella& huella() const { return identidad_->huella(); }
  bool tiene_dueno() const { return servidor_->tiene_dueno(); }
  int cuantos(Evento evento) const { return eventos_[static_cast<std::size_t>(evento)].load(); }

 private:
  std::unique_ptr<ventrik::cifrado::IdentidadServidor> identidad_;
  std::array<std::atomic<int>, 48> eventos_{};
  std::atomic<bool> seguir_{true};
  std::unique_ptr<ventrik::servidor::Servidor> servidor_;
  std::thread hilo_;
};

ventrik::cliente::ResultadoConexion entrar_directo(const ventrik::red::Direccion& destino,
                                                   const ventrik::cifrado::Huella& huella, const std::string& nombre,
                                                   std::string_view contrasena = {}) {
  auto quien = presentacion(nombre);
  if (!contrasena.empty()) {
    quien.contrasena = secreto_de(contrasena);
  }
  ventrik::cliente::OpcionesConexion opciones;
  opciones.tiempo_para_saludar = 30s;
  return ventrik::cliente::conectar(destino, huella, quien, opciones);
}

bool negado(Cliente& cliente, TipoMensaje pedido, ventrik::protocolo::MotivoNegado motivo) {
  return cliente
      .esperar<ventrik::protocolo::Negado>(
          [&](const ventrik::protocolo::Negado& cual) { return cual.pedido == pedido && cual.motivo == motivo; })
      .has_value();
}

std::optional<ventrik::protocolo::RegistroDeSeguridad> registro_de(Cliente& cliente) {
  return cliente.esperar<ventrik::protocolo::RegistroDeSeguridad>({}, 30s);
}

}

TEST_CASE("El servidor nuevo, las contraseñas por el caño y los datos públicos van y vuelven y se rechazan rotos") {
  using namespace ventrik::protocolo;
  const auto nuevo = servidor_nuevo("Casa de Tano", "secreta", kArranque, ventrik::Nivel::reforzado);
  const auto armado = armar_servidor_nuevo(nuevo);
  REQUIRE(armado.size() > 0);
  const auto leido = leer_servidor_nuevo(armado.bytes());
  REQUIRE(leido);
  CHECK(leido->nombre == "Casa de Tano");
  CHECK(leido->nombre_dueno == "Dueño");
  CHECK(leido->dueno == nuevo.dueno);
  CHECK(leido->seguridad.nivel == ventrik::Nivel::reforzado);
  CHECK(leido->seguridad.opciones == nuevo.seguridad.opciones);
  CHECK(std::string(ventrik::cifrado::texto_de(leido->contrasena)) == "secreta");
  CHECK(std::string(ventrik::cifrado::texto_de(leido->arranque)) == kArranque);
  CHECK_FALSE(leer_servidor_nuevo(armado.bytes().first(armado.size() - 1)));
  Bytes de_mas(armado.bytes().begin(), armado.bytes().end());
  de_mas.push_back(0);
  CHECK_FALSE(leer_servidor_nuevo(de_mas));
  Bytes otra_version(armado.bytes().begin(), armado.bytes().end());
  otra_version[0] = 2;
  CHECK_FALSE(leer_servidor_nuevo(otra_version));

  auto con_problema = [](auto&& romper) {
    auto roto = servidor_nuevo("Casa");
    romper(roto);
    return armar_servidor_nuevo(roto).size() == 0;
  };
  CHECK(con_problema([](ServidorNuevo& roto) { roto.nombre = " Casa"; }));
  CHECK(con_problema([](ServidorNuevo& roto) { roto.nombre_dueno.clear(); }));
  CHECK(con_problema([](ServidorNuevo& roto) { roto.seguridad.opciones.valores[0] = 9; }));
  CHECK(con_problema([](ServidorNuevo& roto) { roto.seguridad.nivel = ventrik::Nivel::maximo; }));
  CHECK(con_problema([](ServidorNuevo& roto) { roto.arranque = ventrik::cifrado::secreto_de("corta"); }));
  CHECK(con_problema([](ServidorNuevo& roto) { roto.contrasena = ventrik::cifrado::secreto_de("con\ttab"); }));
  CHECK(con_problema(
      [](ServidorNuevo& roto) { roto.contrasena = ventrik::cifrado::secreto_de(std::string(128, 'a')); }));
  auto sin_punto = servidor_nuevo("Casa");
  sin_punto.dueno[0] = 0x02;
  CHECK_FALSE(leer_servidor_nuevo(armar_servidor_nuevo(sin_punto).bytes()));

  const auto entrada = armar_entrada(kArranque, "del archivo");
  const auto entrada_leida = leer_entrada(entrada.bytes());
  REQUIRE(entrada_leida);
  CHECK(std::string(ventrik::cifrado::texto_de(entrada_leida->arranque)) == kArranque);
  CHECK(std::string(ventrik::cifrado::texto_de(entrada_leida->contrasena)) == "del archivo");
  const auto vacia = leer_entrada(armar_entrada({}, {}).bytes());
  REQUIRE(vacia);
  CHECK(vacia->arranque.size() == 0);
  CHECK(armar_entrada("mal\x01", {}).size() == 0);
  CHECK_FALSE(leer_entrada(entrada.bytes().first(entrada.size() - 1)));

  const DatosPublicos publicos{identidad_nueva().huella().bytes(), 7700, "Casa de Tano"};
  const auto bytes = armar_datos_publicos(publicos);
  const auto publicos_leidos = leer_datos_publicos(bytes);
  REQUIRE(publicos_leidos);
  CHECK(publicos_leidos->huella == publicos.huella);
  CHECK(publicos_leidos->puerto == 7700);
  CHECK(publicos_leidos->nombre == "Casa de Tano");
  CHECK(armar_datos_publicos({publicos.huella, 0, "Casa"}).empty());
  CHECK_FALSE(leer_datos_publicos(gsl::span<const std::uint8_t>(bytes).first(bytes.size() - 1)));
}

TEST_CASE("Los pedidos de la configuración del servidor van y vuelven por el protocolo") {
  using namespace ventrik::protocolo;
  const auto ida_y_vuelta = [](const auto& mensaje) {
    Bytes marco;
    REQUIRE(armar_mensaje(mensaje, marco));
    const auto cuerpo = gsl::span<const std::uint8_t>(marco).subspan(kCabeceraMarco);
    auto leido = leer_mensaje<std::decay_t<decltype(mensaje)>>(cuerpo);
    REQUIRE(leido);
    return *leido;
  };
  CHECK(ida_y_vuelta(CambiarNombreServidor{"Nombre nuevo"}).nombre == "Nombre nuevo");
  CHECK(ida_y_vuelta(CambiarContrasenaServidor{"otra clave"}).contrasena == "otra clave");
  CHECK(ida_y_vuelta(CambiarContrasenaServidor{}).contrasena.empty());
  ida_y_vuelta(PedirRegistro{});
  CHECK(ida_y_vuelta(SacarAdminGuardado{"Sofi"}).nombre == "Sofi");
  CHECK(ida_y_vuelta(NombreServidor{"Los pibes"}).nombre == "Los pibes");
  RegistroDeSeguridad registro{true, {{"Sofi", identidad_de("Sofi")->publica()}},
                               {{Seguridad::admin_nombrado, 1234, "Dueño", "Sofi"}}};
  const auto leido = ida_y_vuelta(registro);
  CHECK(leido.con_contrasena);
  REQUIRE(leido.admins.size() == 1);
  CHECK(leido.admins[0].nombre == "Sofi");
  CHECK(leido.admins[0].clave == registro.admins[0].clave);
  REQUIRE(leido.entradas.size() == 1);
  CHECK(leido.entradas[0].tipo == Seguridad::admin_nombrado);
  Bytes marco;
  CHECK_FALSE(armar_mensaje(CambiarNombreServidor{" mal"}, marco));
  registro.entradas.resize(kMaximoFilasDelRegistro + 1, registro.entradas[0]);
  CHECK_FALSE(armar_mensaje(registro, marco));
  for (const auto tipo : {TipoMensaje::cambiar_nombre_servidor, TipoMensaje::cambiar_contrasena_servidor,
                          TipoMensaje::pedir_registro, TipoMensaje::sacar_admin_guardado}) {
    CHECK(ida_y_vuelta(Negado{tipo, MotivoNegado::sin_permiso}).pedido == tipo);
  }
}

TEST_CASE("La cadena de claves vacía vale en el mensaje y no en el archivo") {
  Bytes marco;
  REQUIRE(ventrik::protocolo::armar_mensaje(ventrik::protocolo::CadenaDeClaves{}, marco));
  const auto cuerpo = gsl::span<const std::uint8_t>(marco).subspan(ventrik::protocolo::kCabeceraMarco);
  CHECK(cuerpo.size() == 1);
  const auto leida = ventrik::protocolo::leer_mensaje<ventrik::protocolo::CadenaDeClaves>(cuerpo);
  REQUIRE(leida);
  CHECK(leida->cadena.empty());
  CHECK_FALSE(ventrik::cifrado::leer_cadena(cuerpo));
  CHECK(ventrik::cifrado::escribir_cadena(ventrik::cifrado::Cadena{}).empty());
}

TEST_CASE("Con la cadena vacía, una huella sin confirmar da el aviso al instante y no a los 8 s") {
  ServidorDePrueba servidor;
  ventrik::cliente::OpcionesConexion opciones;
  opciones.tiempo_para_saludar = 8s;
  const auto desde = std::chrono::steady_clock::now();
  const auto resultado =
      ventrik::cliente::conectar(servidor.direccion(), identidad_nueva().huella(), presentacion("Apurado"), opciones);
  CHECK(resultado.estado == EstadoConexion::huella_distinta);
  CHECK(std::chrono::steady_clock::now() - desde < 3s);
  const auto bien = entrar_directo(servidor.direccion(), servidor.huella(), "Apurado");
  CHECK(bien.estado == EstadoConexion::conectado);
}

TEST_CASE("El evento para detener es uno por carpeta, como sea que esté escrita") {
  using ventrik::protocolo::evento_de;
  const std::wstring nombre = evento_de(L"C:\\Servidores\\Casa");
  CHECK(nombre.rfind(L"Local\\VentrikServidor-", 0) == 0);
  CHECK(evento_de(L"c:\\servidores\\casa\\") == nombre);
  CHECK(evento_de(L"C:\\Servidores\\.\\Casa") == nombre);
  CHECK(evento_de(L"C:\\Servidores\\Otra") != nombre);
}

TEST_CASE("Crear arma un servidor real con el dueño ya asignado, y el dueño entra sin código") {
  Raiz raiz;
  const std::wstring carpeta = raiz.de(L"Casa");
  auto nuevo = servidor_nuevo("Casa de Tano");
  REQUIRE(ventrik::cambiar_opcion(nuevo.seguridad, ventrik::Opcion::historial_cifrado, 1));
  REQUIRE(ventrik::servidor::crear_servidor(carpeta, nuevo, "General") == Creado::hecho);
  CHECK(existe(carpeta + ventrik::cifrado::kArchivoBoveda));
  CHECK(existe(carpeta + ventrik::cifrado::kArchivoClave));
  CHECK(existe(carpeta + ventrik::servidor::kArchivoBase));
  CHECK_FALSE(existe(carpeta + L"\\codigo_dueno.txt"));
  CHECK_FALSE(ventrik::cliente::pide_arranque(carpeta));
  {
    auto base = abrir_base(carpeta);
    CHECK(base->texto_guardado("nombre") == "Casa de Tano");
    CHECK(base->texto_guardado("contrasena").empty());
    std::optional<ventrik::Seguridad> seguridad;
    REQUIRE(base->leer_seguridad(seguridad));
    REQUIRE(seguridad);
    CHECK(seguridad->nivel == ventrik::Nivel::personalizado);
    CHECK(seguridad->opciones == nuevo.seguridad.opciones);
    CHECK(base->historial_cifrado());
    const auto usuarios = base->usuarios(1000);
    REQUIRE(usuarios);
    REQUIRE(usuarios->size() == 1);
    CHECK(usuarios->front().dueno);
    CHECK(usuarios->front().clave == identidad_de("Dueño")->publica());
  }
  ServidorDeCarpeta servidor(carpeta);
  CHECK(servidor.tiene_dueno());
  const auto publicos = datos_publicos(carpeta);
  REQUIRE(publicos);
  CHECK(publicos->huella == servidor.huella().bytes());
  CHECK(publicos->puerto == servidor.direccion().puerto);
  CHECK(publicos->nombre == "Casa de Tano");
  Cliente dueno(servidor.direccion(), servidor.huella(), "Dueño");
  const auto presentado = dueno.entrar();
  CHECK(presentado.rol == ventrik::protocolo::Rol::dueno);
  CHECK(presentado.servidor == "Casa de Tano");
  Cliente invitado(servidor.direccion(), servidor.huella(), "Invitado");
  CHECK(invitado.entrar().rol == ventrik::protocolo::Rol::ninguno);
  dueno.ordenar(ventrik::protocolo::PedirRegistro{});
  const auto registro = registro_de(dueno);
  REQUIRE(registro);
  CHECK(std::none_of(registro->entradas.begin(), registro->entradas.end(), [](const auto& entrada) {
    return entrada.tipo == ventrik::protocolo::Seguridad::equipo_nuevo;
  }));
  CHECK(servidor.cuantos(ServidorDeCarpeta::Evento::equipo_nuevo) == 0);
}

TEST_CASE("El servidor creado arranca con la seguridad que eligió el asistente: en Reforzado el dueño configura el segundo paso") {
  Raiz raiz;
  const std::wstring carpeta = raiz.de(L"Reforzado");
  REQUIRE(ventrik::servidor::crear_servidor(carpeta, servidor_nuevo("Casa", {}, {}, ventrik::Nivel::reforzado),
                                            "General") == Creado::hecho);
  {
    auto base = abrir_base(carpeta);
    std::optional<ventrik::Seguridad> seguridad;
    REQUIRE(base->leer_seguridad(seguridad));
    REQUIRE(seguridad);
    CHECK(seguridad->nivel == ventrik::Nivel::reforzado);
    CHECK(base->historial_cifrado());
  }
  ServidorDeCarpeta servidor(carpeta);
  Cliente dueno(servidor.direccion(), servidor.huella(), "Dueño");
  CHECK(dueno.entrar().rol == ventrik::protocolo::Rol::ninguno);
  CHECK(dueno.esperar<ventrik::protocolo::ConfigurarSegundoPaso>());
  dueno.ordenar(ventrik::protocolo::CambiarNombreServidor{"Sin segundo paso"});
  CHECK(negado(dueno, TipoMensaje::cambiar_nombre_servidor, ventrik::protocolo::MotivoNegado::sin_permiso));
  dueno.ordenar(ventrik::protocolo::PedirRegistro{});
  CHECK(negado(dueno, TipoMensaje::pedir_registro, ventrik::protocolo::MotivoNegado::sin_permiso));
}

TEST_CASE("Crear con datos rotos o en una carpeta ocupada no crea nada") {
  Raiz raiz;
  auto sin_curva = servidor_nuevo("Casa");
  sin_curva.dueno.fill(0x04);
  CHECK(ventrik::servidor::crear_servidor(raiz.de(L"rota"), sin_curva, "General") == Creado::datos_rotos);
  CHECK_FALSE(existe(raiz.de(L"rota")));
  REQUIRE(ventrik::crear_carpetas(raiz.de(L"ocupada")));
  REQUIRE(ventrik::escribir_archivo_completo(raiz.de(L"ocupada\\algo.txt"), Bytes{1, 2, 3}));
  CHECK(ventrik::servidor::crear_servidor(raiz.de(L"ocupada"), servidor_nuevo("Casa"), "General") ==
        Creado::carpeta_ocupada);
  CHECK(cuantos_archivos(raiz.de(L"ocupada")) == 1);
  REQUIRE(ventrik::crear_carpetas(raiz.de(L"vacia")));
  CHECK(ventrik::servidor::crear_servidor(raiz.de(L"vacia"), servidor_nuevo("Casa"), "General") == Creado::hecho);
  CHECK(ventrik::servidor::crear_servidor(raiz.de(L"vacia"), servidor_nuevo("Casa"), "General") ==
        Creado::carpeta_ocupada);
}

TEST_CASE("Con contraseña de arranque, la bóveda la pide; y la contraseña para entrar queda guardada") {
  Raiz raiz;
  const std::wstring carpeta = raiz.de(L"Con arranque");
  REQUIRE(ventrik::servidor::crear_servidor(carpeta, servidor_nuevo("Casa", "para entrar", kArranque), "General") ==
          Creado::hecho);
  CHECK(ventrik::cliente::pide_arranque(carpeta));
  CHECK(ventrik::cifrado::cargar_o_crear_identidad(carpeta, std::nullopt, true).resultado ==
        ventrik::cifrado::ResultadoIdentidad::falta_contrasena);
  CHECK(ventrik::cifrado::cargar_o_crear_identidad(carpeta, std::string_view("otra cosa"), true).resultado ==
        ventrik::cifrado::ResultadoIdentidad::contrasena_incorrecta);
  auto base = abrir_base(carpeta, std::string_view(kArranque));
  const auto guardada = ventrik::servidor::leer_contrasena(base->texto_guardado("contrasena"));
  REQUIRE(guardada);
  CHECK(ventrik::servidor::comprobar(*guardada, "para entrar") == ventrik::servidor::Comprobado::coincide);
}

TEST_CASE("El dueño cambia el nombre y la contraseña en vivo, y otro no puede") {
  Raiz raiz;
  const std::wstring carpeta = raiz.de(L"En vivo");
  REQUIRE(ventrik::servidor::crear_servidor(carpeta, servidor_nuevo("Casa"), "General") == Creado::hecho);
  ServidorDeCarpeta servidor(carpeta);
  Cliente dueno(servidor.direccion(), servidor.huella(), "Dueño");
  dueno.entrar();
  Cliente otro(servidor.direccion(), servidor.huella(), "Otro");
  otro.entrar();
  using ventrik::protocolo::MotivoNegado;
  otro.ordenar(ventrik::protocolo::CambiarNombreServidor{"Pirata"});
  CHECK(negado(otro, TipoMensaje::cambiar_nombre_servidor, MotivoNegado::sin_permiso));
  otro.ordenar(ventrik::protocolo::CambiarContrasenaServidor{"robada1"});
  CHECK(negado(otro, TipoMensaje::cambiar_contrasena_servidor, MotivoNegado::sin_permiso));
  otro.ordenar(ventrik::protocolo::PedirRegistro{});
  CHECK(negado(otro, TipoMensaje::pedir_registro, MotivoNegado::sin_permiso));
  otro.ordenar(ventrik::protocolo::SacarAdminGuardado{"Dueño"});
  CHECK(negado(otro, TipoMensaje::sacar_admin_guardado, MotivoNegado::sin_permiso));

  dueno.ordenar(ventrik::protocolo::CambiarNombreServidor{"Nombre nuevo"});
  const auto nombre = [](const ventrik::protocolo::NombreServidor& cual) { return cual.nombre == "Nombre nuevo"; };
  CHECK(dueno.esperar<ventrik::protocolo::NombreServidor>(nombre));
  CHECK(otro.esperar<ventrik::protocolo::NombreServidor>(nombre));
  CHECK(esperar_que([&] {
    const auto publicos = datos_publicos(carpeta);
    return publicos && publicos->nombre == "Nombre nuevo";
  }));

  dueno.ordenar(ventrik::protocolo::CambiarContrasenaServidor{"con\x01 control"});
  CHECK(negado(dueno, TipoMensaje::cambiar_contrasena_servidor, MotivoNegado::texto_invalido));
  dueno.ordenar(ventrik::protocolo::CambiarContrasenaServidor{"clave nueva"});
  const auto con = registro_de(dueno);
  REQUIRE(con);
  CHECK(con->con_contrasena);
  CHECK(entrar_directo(servidor.direccion(), servidor.huella(), "Nuevo").estado == EstadoConexion::falta_contrasena);
  CHECK(entrar_directo(servidor.direccion(), servidor.huella(), "Nuevo", "clave nueva").estado ==
        EstadoConexion::conectado);
  CHECK(entrar_directo(servidor.direccion(), servidor.huella(), "Otro").estado == EstadoConexion::conectado);

  dueno.ordenar(ventrik::protocolo::CambiarContrasenaServidor{});
  const auto sin = registro_de(dueno);
  REQUIRE(sin);
  CHECK_FALSE(sin->con_contrasena);
  CHECK(entrar_directo(servidor.direccion(), servidor.huella(), "Sin clave").estado == EstadoConexion::conectado);

  dueno.ordenar(ventrik::protocolo::CambiarContrasenaServidor{"la que llega tarde"});
  dueno.ordenar(ventrik::protocolo::CambiarContrasenaServidor{});
  const auto ultima = registro_de(dueno);
  REQUIRE(ultima);
  CHECK_FALSE(ultima->con_contrasena);
  CHECK_FALSE(dueno.esperar<ventrik::protocolo::RegistroDeSeguridad>({}, 6s));
  CHECK(entrar_directo(servidor.direccion(), servidor.huella(), "Después de la carrera").estado ==
        EstadoConexion::conectado);
}

TEST_CASE("El registro llega solo al dueño, con los admins desconectados, y sacarle el admin a uno queda anotado") {
  Raiz raiz;
  const std::wstring carpeta = raiz.de(L"Registro");
  REQUIRE(ventrik::servidor::crear_servidor(carpeta, servidor_nuevo("Casa"), "General") == Creado::hecho);
  ServidorDeCarpeta servidor(carpeta);
  Cliente dueno(servidor.direccion(), servidor.huella(), "Dueño");
  dueno.entrar();
  {
    Cliente sofi(servidor.direccion(), servidor.huella(), "Sofi");
    const auto presentada = sofi.entrar();
    dueno.ordenar(ventrik::protocolo::NombrarAdmin{presentada.tu_id, true});
    REQUIRE(sofi.esperar<ventrik::protocolo::UsuarioRol>(
        [&](const ventrik::protocolo::UsuarioRol& rol) { return rol.id == presentada.tu_id; }));
  }
  REQUIRE(dueno.esperar<ventrik::protocolo::UsuarioSalio>());
  dueno.ordenar(ventrik::protocolo::PedirRegistro{});
  const auto con_sofi = registro_de(dueno);
  REQUIRE(con_sofi);
  REQUIRE(con_sofi->admins.size() == 1);
  CHECK(con_sofi->admins[0].nombre == "Sofi");
  CHECK(con_sofi->admins[0].clave == identidad_de("Sofi")->publica());
  CHECK(con_sofi->entradas.size() <= ventrik::protocolo::kMaximoFilasDelRegistro);
  CHECK(std::any_of(con_sofi->entradas.begin(), con_sofi->entradas.end(), [](const auto& entrada) {
    return entrada.tipo == ventrik::protocolo::Seguridad::admin_nombrado && entrada.detalle == "Sofi";
  }));

  dueno.ordenar(ventrik::protocolo::SacarAdminGuardado{"Nadie"});
  CHECK(negado(dueno, TipoMensaje::sacar_admin_guardado, ventrik::protocolo::MotivoNegado::no_existe));
  dueno.ordenar(ventrik::protocolo::SacarAdminGuardado{"Sofi"});
  const auto sin_sofi = registro_de(dueno);
  REQUIRE(sin_sofi);
  CHECK(sin_sofi->admins.empty());
  REQUIRE_FALSE(sin_sofi->entradas.empty());
  CHECK(sin_sofi->entradas.front().tipo == ventrik::protocolo::Seguridad::admin_quitado);
  CHECK(sin_sofi->entradas.front().quien == "Dueño");
  CHECK(sin_sofi->entradas.front().detalle == "Sofi");
  Cliente sofi(servidor.direccion(), servidor.huella(), "Sofi");
  CHECK(sofi.entrar().rol == ventrik::protocolo::Rol::ninguno);
}

TEST_CASE("Después de mudar el servidor, el primer acierto rehace el hash de la contraseña con los valores de esta PC") {
  Raiz raiz;
  const std::wstring origen = raiz.de(L"Origen");
  REQUIRE(ventrik::servidor::crear_servidor(origen, servidor_nuevo("Casa", "para entrar"), "General") ==
          Creado::hecho);
  const ventrik::cifrado::Argon2 de_otra_pc{ventrik::cifrado::kMinimoMemoriaArgon2, 1};
  const auto chico = ventrik::servidor::guardar_contrasena("para entrar", de_otra_pc);
  REQUIRE(chico);
  abrir_base(origen)->guardar_texto("contrasena", *chico);
  const std::wstring archivo = raiz.de(L"casa.vkservidor");
  {
    auto cargada = ventrik::cifrado::cargar_o_crear_identidad(origen, std::nullopt, true);
    REQUIRE(cargada.identidad);
    REQUIRE(ventrik::servidor::exportar_servidor(origen, *cargada.identidad, *cargada.boveda, archivo,
                                                 "contraseña del respaldo", de_otra_pc) ==
            ventrik::servidor::Mudanza::hecha);
  }
  const std::wstring destino = raiz.de(L"Destino");
  REQUIRE(ventrik::crear_carpetas(destino));
  REQUIRE(ventrik::servidor::importar_servidor(destino, archivo, "contraseña del respaldo", "General").resultado ==
          ventrik::servidor::Mudanza::hecha);
  CHECK(abrir_base(destino)->texto_guardado("rehacer_contrasena") == "1");
  {
    ServidorDeCarpeta servidor(destino);
    CHECK(entrar_directo(servidor.direccion(), servidor.huella(), "Llega", "otra").estado ==
          EstadoConexion::contrasena_incorrecta);
    CHECK(servidor.cuantos(ServidorDeCarpeta::Evento::contrasena_rehecha) == 0);
    CHECK(esperar_que(
        [&] {
          return entrar_directo(servidor.direccion(), servidor.huella(), "Llega 2", "para entrar").estado ==
                 EstadoConexion::conectado;
        },
        10s));
    CHECK(esperar_que([&] { return servidor.cuantos(ServidorDeCarpeta::Evento::contrasena_rehecha) == 1; }, 30s));
  }
  auto base = abrir_base(destino);
  CHECK(base->texto_guardado("rehacer_contrasena").empty());
  const std::string rehecho = base->texto_guardado("contrasena");
  CHECK(rehecho != *chico);
  const auto guardada = ventrik::servidor::leer_contrasena(rehecho);
  REQUIRE(guardada);
  CHECK(ventrik::servidor::comprobar(*guardada, "para entrar") == ventrik::servidor::Comprobado::coincide);
}

TEST_CASE("Mis servidores: carpetas nuevas sin nombres prohibidos y la lista lee los datos públicos y si se exportó") {
  using ventrik::cliente::carpeta_nueva;
  Raiz raiz;
  const std::wstring primera = carpeta_nueva(raiz.ruta(), "Casa: de/Tano?");
  CHECK(primera == raiz.de(L"Casa_ de_Tano_"));
  CHECK(carpeta_nueva(raiz.ruta(), "Casa: de/Tano?") == raiz.de(L"Casa_ de_Tano_ (2)"));
  CHECK(carpeta_nueva(raiz.ruta(), "con") == raiz.de(L"_con"));
  CHECK(carpeta_nueva(raiz.ruta(), "Puntos...") == raiz.de(L"Puntos"));
  CHECK(ventrik::cliente::leer_mis_servidores(raiz.ruta()).empty());

  const auto huella = identidad_nueva().huella().bytes();
  REQUIRE(ventrik::escribir_archivo_completo(primera + ventrik::cifrado::kArchivoBoveda, Bytes{1}));
  REQUIRE(ventrik::escribir_archivo_completo(primera + ventrik::protocolo::kArchivoPublico,
                                             ventrik::protocolo::armar_datos_publicos({huella, 7701, "Casa de Tano"})));
  const std::wstring otra = carpeta_nueva(raiz.ruta(), "Otra");
  REQUIRE(ventrik::escribir_archivo_completo(otra + ventrik::cifrado::kArchivoBoveda, Bytes{1}));
  auto lista = ventrik::cliente::leer_mis_servidores(raiz.ruta());
  REQUIRE(lista.size() == 2);
  CHECK(lista[0].nombre == "Casa de Tano");
  CHECK(lista[0].puerto == 7701);
  CHECK(lista[0].huella == huella);
  CHECK_FALSE(lista[0].exportada);
  CHECK(lista[1].nombre == "Otra");
  CHECK(lista[1].puerto == ventrik::red::kPuertoPredeterminado);
  CHECK_FALSE(lista[1].huella);
  CHECK_FALSE(ventrik::cliente::esta_andando(primera));
  REQUIRE(ventrik::escribir_archivo_completo(primera + ventrik::protocolo::kArchivoExportada, huella));
  CHECK(ventrik::cliente::leer_mis_servidores(raiz.ruta())[0].exportada);
  ventrik::cliente::borrar_carpeta_vacia(raiz.de(L"_con"));
  CHECK_FALSE(existe(raiz.de(L"_con")));
}

TEST_CASE("ventrik_servidor.exe crea por el caño, arranca sin consola con y sin contraseña de arranque y se detiene con el evento") {
  const std::wstring exe = ventrik::cliente::exe_del_servidor();
  if (exe.empty()) {
    MESSAGE("No está ventrik_servidor.exe junto a las pruebas: se saltea");
    return;
  }
  using ventrik::cliente::ProcesoDelServidor;
  using ventrik::protocolo::SalidaServidor;
  const auto esperar_salida = [](const ProcesoDelServidor& proceso) {
    std::optional<SalidaServidor> salida;
    esperar_que([&] { return (salida = proceso.salida()).has_value(); }, 60s);
    return salida;
  };
  Raiz raiz;
  const std::wstring carpeta = raiz.de(L"Desde el exe");
  const Bytes basura{1, 2, 3};
  auto rotos = ProcesoDelServidor::lanzar(exe, {L"--crear", L"--carpeta", carpeta}, basura);
  REQUIRE(rotos);
  CHECK(esperar_salida(*rotos) == SalidaServidor::datos_rotos);
  CHECK_FALSE(existe(carpeta));

  const auto nuevo = ventrik::protocolo::armar_servidor_nuevo(servidor_nuevo("Desde el exe", {}, kArranque));
  auto creado = ProcesoDelServidor::lanzar(exe, {L"--crear", L"--carpeta", carpeta}, nuevo.bytes());
  REQUIRE(creado);
  REQUIRE(esperar_salida(*creado) == SalidaServidor::bien);
  CHECK(existe(carpeta + ventrik::cifrado::kArchivoClave));
  CHECK_FALSE(existe(carpeta + L"\\codigo_dueno.txt"));
  auto otra_vez = ProcesoDelServidor::lanzar(exe, {L"--crear", L"--carpeta", carpeta}, nuevo.bytes());
  REQUIRE(otra_vez);
  CHECK(esperar_salida(*otra_vez) == SalidaServidor::carpeta_ocupada);

  const std::uint16_t puerto = puerto_libre();
  const std::vector<std::wstring> argumentos = {L"--carpeta", carpeta, L"--sin-consola", L"--local", L"--puerto",
                                                std::to_wstring(puerto)};
  auto sin = ProcesoDelServidor::lanzar(exe, argumentos, ventrik::protocolo::armar_entrada({}, {}).bytes());
  REQUIRE(sin);
  CHECK(esperar_salida(*sin) == SalidaServidor::arranque);
  auto mal = ProcesoDelServidor::lanzar(exe, argumentos, ventrik::protocolo::armar_entrada("otra cosa", {}).bytes());
  REQUIRE(mal);
  CHECK(esperar_salida(*mal) == SalidaServidor::arranque);
  CHECK_FALSE(ventrik::cliente::esta_andando(carpeta));

  auto andando =
      ProcesoDelServidor::lanzar(exe, argumentos, ventrik::protocolo::armar_entrada(kArranque, {}).bytes(), carpeta);
  REQUIRE(andando);
  REQUIRE(esperar_que([&] { return ventrik::cliente::esta_andando(carpeta); }, 60s));
  const auto publicos = datos_publicos(carpeta);
  REQUIRE(publicos);
  CHECK(publicos->puerto == puerto);
  CHECK(publicos->nombre == "Desde el exe");
  const auto resultado =
      entrar_directo({kLocal, puerto}, ventrik::cifrado::Huella(publicos->huella), "Dueño");
  REQUIRE(resultado.estado == EstadoConexion::conectado);
  CHECK(resultado.presentado->rol == ventrik::protocolo::Rol::dueno);
  auto segundo = ProcesoDelServidor::lanzar(exe, argumentos, ventrik::protocolo::armar_entrada(kArranque, {}).bytes());
  REQUIRE(segundo);
  CHECK(esperar_salida(*segundo) == SalidaServidor::andando);
  CHECK(ventrik::cliente::esta_andando(carpeta));

  REQUIRE(ventrik::cliente::detener_servidor(carpeta));
  CHECK(esperar_salida(*andando) == SalidaServidor::bien);
  CHECK_FALSE(ventrik::cliente::esta_andando(carpeta));
  const std::string registro = leer_todo(carpeta + ventrik::protocolo::kArchivoRegistro);
  CHECK_FALSE(registro.empty());
  CHECK(registro.find(kArranque) == std::string::npos);
  CHECK(registro.find("Desde el exe") != std::string::npos);
}
