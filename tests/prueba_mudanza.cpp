#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <windows.h>

#include <gsl/gsl>
#include <sqlite3.h>

#include "client/conexion.h"
#include "client/guardados.h"
#include "common/archivos.h"
#include "common/cifrado/cadena.h"
#include "common/cifrado/identidad.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/tls.h"
#include "common/protocol/marcos.h"
#include "common/protocol/mensajes.h"
#include "common/protocol/pedidos.h"
#include "common/red/red.h"
#include "common/utf.h"
#include "server/base.h"
#include "server/mudanza.h"
#include "server/respaldo.h"
#include "server/servidor.h"
#include "cliente_de_prueba.h"
#include "identidad_de_prueba.h"
#include "servidor_de_prueba.h"
#include <doctest/doctest.h>

using namespace std::chrono_literals;
using namespace prueba;
using ventrik::cifrado::Cadena;
using ventrik::cifrado::Huella;
using ventrik::cifrado::IdentidadServidor;
using ventrik::cliente::EstadoConexion;
using ventrik::servidor::Base;
using ventrik::servidor::kArchivoBase;
using ventrik::servidor::kArchivoCadena;
using ventrik::servidor::Mudanza;
using ventrik::protocolo::TipoMensaje;

namespace {

constexpr ventrik::cifrado::Argon2 kRapido{ventrik::cifrado::kMinimoMemoriaArgon2, 1};
constexpr const char* kContrasena = "contraseña del respaldo";
using Bytes = std::vector<std::uint8_t>;

class Carpeta {
 public:
  Carpeta() {
    std::array<wchar_t, MAX_PATH> base{};
    REQUIRE(GetTempPathW(static_cast<DWORD>(base.size()), base.data()) > 0);
    static std::atomic<int> creadas{0};
    ruta_ = std::wstring(base.data()) + L"ventrik_mudanza_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
            std::to_wstring(++creadas) + L"_ñ";
    REQUIRE(ventrik::crear_carpetas(ruta_));
  }
  ~Carpeta() {
    for (const auto& archivo : archivos()) {
      SetFileAttributesW((ruta_ + L"\\" + archivo).c_str(), FILE_ATTRIBUTE_NORMAL);
      DeleteFileW((ruta_ + L"\\" + archivo).c_str());
    }
    RemoveDirectoryW(ruta_.c_str());
  }
  Carpeta(const Carpeta&) = delete;
  Carpeta& operator=(const Carpeta&) = delete;

  const std::wstring& ruta() const { return ruta_; }
  std::wstring de(const wchar_t* nombre) const { return ruta_ + nombre; }
  std::vector<std::wstring> archivos() const {
    std::vector<std::wstring> lista;
    WIN32_FIND_DATAW encontrado = {};
    const HANDLE busqueda = FindFirstFileW((ruta_ + L"\\*").c_str(), &encontrado);
    if (busqueda == INVALID_HANDLE_VALUE) {
      return lista;
    }
    do {
      if ((encontrado.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        lista.emplace_back(encontrado.cFileName);
      }
    } while (FindNextFileW(busqueda, &encontrado) != 0);
    FindClose(busqueda);
    return lista;
  }

 private:
  std::wstring ruta_;
};

bool existe(const std::wstring& archivo) {
  return GetFileAttributesW(archivo.c_str()) != INVALID_FILE_ATTRIBUTES;
}

Bytes leer_todo(const std::wstring& archivo) {
  ventrik::cifrado::Secreto contenido;
  REQUIRE(ventrik::leer_archivo(archivo, ventrik::servidor::kMaximoArchivoRespaldo, contenido) ==
          ventrik::LecturaArchivo::leido);
  return Bytes(contenido.bytes().begin(), contenido.bytes().end());
}

bool contiene(const Bytes& todo, const std::string& parte) {
  return std::search(todo.begin(), todo.end(), parte.begin(), parte.end()) != todo.end();
}

void ejecutar(const std::wstring& archivo, const char* sql) {
  sqlite3* base = nullptr;
  REQUIRE(sqlite3_open_v2(ventrik::a_utf8(archivo).c_str(), &base, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
  CHECK(sqlite3_exec(base, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(base);
}

ventrik::cifrado::IdentidadCargada cargar(const Carpeta& carpeta, bool con_datos = false) {
  auto cargada = ventrik::cifrado::cargar_o_crear_identidad(carpeta.ruta(), std::nullopt, con_datos);
  REQUIRE(cargada.identidad);
  REQUIRE(cargada.boveda);
  return cargada;
}

ventrik::cifrado::Secreto clave_del_historial(const ventrik::cifrado::Boveda& boveda) {
  auto clave = boveda.derivar(ventrik::cifrado::kEtiquetaHistorial);
  REQUIRE(clave);
  return std::move(*clave);
}

std::unique_ptr<Base> abrir_base(const Carpeta& carpeta, const ventrik::cifrado::Boveda& boveda) {
  auto base = Base::abrir(carpeta.de(kArchivoBase), "General", clave_del_historial(boveda));
  REQUIRE(base);
  return base;
}

ventrik::servidor::UsuarioGuardado usuario(std::uint8_t clave, const std::string& nombre, bool dueno, bool admin) {
  ventrik::servidor::UsuarioGuardado guardado;
  guardado.clave.fill(clave);
  guardado.clave[0] = 0x04;
  guardado.nombre = nombre;
  guardado.dueno = dueno;
  guardado.admin = admin;
  guardado.creado_ms = guardado.visto_ms = 1000;
  guardado.entradas = 3;
  return guardado;
}

std::unique_ptr<Base> llenar(const Carpeta& carpeta, const ventrik::cifrado::Boveda& boveda) {
  auto base = abrir_base(carpeta, boveda);
  REQUIRE(base->crear_canal("Charla", ventrik::protocolo::TipoCanal::texto));
  REQUIRE(base->guardar_usuario(usuario(1, "Ana", true, false)));
  REQUIRE(base->guardar_usuario(usuario(2, "Beto", false, true)));
  REQUIRE(base->guardar_mensaje(ventrik::servidor::kCanalGeneral, 1, "Ana", 1000, "antes de cifrar"));
  REQUIRE(base->cifrar_historial(true));
  REQUIRE(base->guardar_mensaje(ventrik::servidor::kCanalGeneral, 2, "Beto", 2000, "ya cifrado"));
  return base;
}

void revisar_copia(const Carpeta& carpeta, const Huella& huella) {
  auto cargada = cargar(carpeta, true);
  CHECK(cargada.resultado == ventrik::cifrado::ResultadoIdentidad::cargada);
  CHECK(cargada.identidad->huella() == huella);
  auto base = abrir_base(carpeta, *cargada.boveda);
  const auto canales = base->canales();
  CHECK(std::any_of(canales.begin(), canales.end(), [](const auto& canal) { return canal.nombre == "Charla"; }));
  const auto usuarios = base->usuarios(1000);
  REQUIRE(usuarios);
  REQUIRE(usuarios->size() == 2);
  for (const auto& guardado : *usuarios) {
    CHECK(guardado.dueno == (guardado.nombre == "Ana"));
    CHECK(guardado.admin == (guardado.nombre == "Beto"));
  }
  CHECK(base->historial_cifrado());
  const auto mensajes = base->historial({ventrik::servidor::kCanalGeneral, 0}, 65536).mensajes;
  REQUIRE(mensajes.size() == 2);
  CHECK(mensajes[0].texto == "ya cifrado");
  CHECK(mensajes[1].texto == "antes de cifrar");
}

struct Exportado {
  Carpeta origen;
  std::optional<Huella> huella;
  std::wstring archivo;
};

void exportar(Exportado& exportado) {
  auto cargada = cargar(exportado.origen);
  exportado.huella = cargada.identidad->huella();
  const auto prendida = llenar(exportado.origen, *cargada.boveda);
  exportado.archivo = exportado.origen.de(L"\\servidor.vkservidor");
  REQUIRE(ventrik::servidor::exportar_servidor(exportado.origen.ruta(), *cargada.identidad, *cargada.boveda,
                                               exportado.archivo, kContrasena, kRapido) == Mudanza::hecha);
}

class ServidorConClave {
 public:
  ServidorConClave(IdentidadServidor identidad, Cadena cadena) : identidad_(std::move(identidad)) {
    ventrik::servidor::Opciones opciones;
    opciones.escuchar = {kLocal, 0};
    opciones.nombre_general = "General";
    opciones.cadena = std::move(cadena);
    servidor_ = std::make_unique<ventrik::servidor::Servidor>(identidad_, opciones);
    ventrik::cifrado::Secreto clave(ventrik::cifrado::kBytesClaveSimetrica);
    REQUIRE(ventrik::cifrado::aleatorio(nullptr, clave.bytes().data(), clave.size()) == 0);
    servidor_->clave_del_historial(std::move(clave));
    REQUIRE(servidor_->escuchar());
    hilo_ = std::thread([this] { servidor_->correr(seguir_); });
  }
  ~ServidorConClave() {
    seguir_ = false;
    hilo_.join();
  }
  ServidorConClave(const ServidorConClave&) = delete;
  ServidorConClave& operator=(const ServidorConClave&) = delete;

  ventrik::red::Direccion direccion() const { return {kLocal, servidor_->puerto()}; }

 private:
  IdentidadServidor identidad_;
  std::atomic<bool> seguir_{true};
  std::unique_ptr<ventrik::servidor::Servidor> servidor_;
  std::thread hilo_;
};

class ServidorQueAnota {
 public:
  ServidorQueAnota(IdentidadServidor& identidad, Bytes despues_de_la_bienvenida)
      : config_(ventrik::cifrado::ConfigTls::para_servidor(identidad)), extra_(std::move(despues_de_la_bienvenida)) {
    REQUIRE(config_);
    escucha_ = ventrik::red::escuchar({kLocal, 0});
    REQUIRE(escucha_.valido());
    puerto_ = ventrik::red::direccion_local(escucha_)->puerto;
    hilo_ = std::thread([this] { atender(); });
  }
  ~ServidorQueAnota() {
    if (hilo_.joinable()) {
      hilo_.join();
    }
  }
  ServidorQueAnota(const ServidorQueAnota&) = delete;
  ServidorQueAnota& operator=(const ServidorQueAnota&) = delete;

  ventrik::red::Direccion direccion() const { return {kLocal, puerto_}; }
  std::vector<std::uint16_t> recibidos() {
    hilo_.join();
    return tipos_;
  }

 private:
  void atender() {
    const auto hasta = std::chrono::steady_clock::now() + 8s;
    std::optional<std::pair<ventrik::red::Socket, ventrik::red::Direccion>> aceptada;
    while (!aceptada && std::chrono::steady_clock::now() < hasta) {
      bool fallo = false;
      ventrik::red::esperar(escucha_, true, false, 100ms);
      aceptada = ventrik::red::aceptar(escucha_, fallo);
    }
    if (!aceptada) {
      return;
    }
    const auto& canal = aceptada->first;
    ventrik::cifrado::SesionTls tls(*config_, canal.get());
    ventrik::protocolo::Ensamblador ensamblador;
    bool saludado = false;
    while (std::chrono::steady_clock::now() < hasta) {
      std::array<std::uint8_t, 4096> datos{};
      std::size_t leidos = 0;
      const auto paso = saludado ? tls.leer(datos, leidos) : tls.apreton();
      if (paso == ventrik::cifrado::Paso::listo) {
        saludado = true;
        gsl::span<const std::uint8_t> llegados = gsl::span<const std::uint8_t>(datos).first(leidos);
        while (auto marco = ensamblador.siguiente(llegados)) {
          tipos_.push_back(marco->tipo);
          if (marco->tipo == static_cast<std::uint16_t>(TipoMensaje::hola)) {
            responder(tls, canal);
          }
        }
        continue;
      }
      if (paso != ventrik::cifrado::Paso::esperar_lectura && paso != ventrik::cifrado::Paso::esperar_escritura) {
        return;
      }
      ventrik::red::esperar(canal, paso == ventrik::cifrado::Paso::esperar_lectura,
                            paso == ventrik::cifrado::Paso::esperar_escritura, 100ms);
    }
  }

  void responder(ventrik::cifrado::SesionTls& tls, const ventrik::red::Socket& canal) {
    ventrik::protocolo::Escritor cuerpo;
    ventrik::protocolo::escribir(cuerpo, ventrik::protocolo::Bienvenida{ventrik::kProtocoloMaximo, 0});
    Bytes salida;
    if (!ventrik::protocolo::armar_marco(static_cast<std::uint16_t>(TipoMensaje::bienvenida), cuerpo.datos(),
                                         salida)) {
      return;
    }
    salida.insert(salida.end(), extra_.begin(), extra_.end());
    gsl::span<const std::uint8_t> resto(salida);
    for (int intento = 0; !resto.empty() && intento < 100; ++intento) {
      std::size_t escritos = 0;
      const auto paso = tls.escribir(resto, escritos);
      resto = resto.subspan(paso == ventrik::cifrado::Paso::listo ? escritos : 0);
      if (paso != ventrik::cifrado::Paso::listo) {
        ventrik::red::esperar(canal, false, true, 50ms);
      }
    }
  }

  std::unique_ptr<ventrik::cifrado::ConfigTls> config_;
  Bytes extra_;
  ventrik::red::Socket escucha_;
  std::uint16_t puerto_ = 0;
  std::vector<std::uint16_t> tipos_;
  std::thread hilo_;
};

Bytes marco_de(const Cadena& cadena) {
  Bytes marco;
  REQUIRE(ventrik::protocolo::armar_mensaje(ventrik::protocolo::CadenaDeClaves{cadena}, marco));
  return marco;
}

ventrik::cifrado::Eslabon eslabon(const IdentidadServidor& vieja, const IdentidadServidor& nueva,
                                  std::uint32_t numero) {
  auto firmado = ventrik::cifrado::firmar_eslabon(vieja.firmante(), nueva.firmante().publica(), numero);
  REQUIRE(firmado);
  return std::move(*firmado);
}

ventrik::cliente::ResultadoConexion entrar(const ventrik::red::Direccion& destino, const Huella& huella,
                                           std::vector<Huella> atras = {}) {
  static std::atomic<int> numero{0};
  ventrik::cliente::OpcionesConexion opciones;
  opciones.tiempo_para_saludar = 2s;
  opciones.atras = std::move(atras);
  return ventrik::cliente::conectar(destino, huella, presentacion("Mudanza " + std::to_string(++numero)), opciones);
}

}

TEST_CASE("Exportar e importar por consola deja el mismo servidor: huella, canales, dueño, admins e historial cifrado") {
  Exportado exportado;
  exportar(exportado);
  const Bytes respaldo = leer_todo(exportado.archivo);
  CHECK_FALSE(contiene(respaldo, "Charla"));
  CHECK_FALSE(contiene(respaldo, "SQLite format"));
  CHECK_FALSE(existe(exportado.origen.de(L"\\exportando.db")));
  Carpeta destino;
  const auto importado =
      ventrik::servidor::importar_servidor(destino.ruta(), exportado.archivo, kContrasena, "General");
  REQUIRE(importado.resultado == Mudanza::hecha);
  REQUIRE(importado.huella);
  CHECK(*importado.huella == *exportado.huella);
  CHECK_FALSE(existe(destino.de(L"\\importando.db")));
  CHECK_FALSE(existe(destino.de(kArchivoCadena)));
  revisar_copia(destino, *exportado.huella);
}

TEST_CASE("Importar con la contraseña equivocada, un respaldo dañado o sobre un servidor que ya existe no toca nada") {
  Exportado exportado;
  exportar(exportado);
  Carpeta destino;
  const auto importar = [&destino](const std::wstring& archivo, const char* contrasena) {
    return ventrik::servidor::importar_servidor(destino.ruta(), archivo, contrasena, "General").resultado;
  };
  CHECK(importar(exportado.archivo, "otra contraseña larga") == Mudanza::contrasena);
  CHECK(destino.archivos().empty());
  Bytes alterado = leer_todo(exportado.archivo);
  alterado[alterado.size() / 2] ^= 0x20;
  const std::wstring archivo_alterado = exportado.origen.de(L"\\alterado.vkservidor");
  REQUIRE(ventrik::escribir_archivo_completo(archivo_alterado, alterado));
  CHECK(importar(archivo_alterado, kContrasena) != Mudanza::hecha);
  CHECK(destino.archivos().empty());
  CHECK(importar(exportado.origen.de(L"\\no_existe.vkservidor"), kContrasena) == Mudanza::error_lectura);
  CHECK(destino.archivos().empty());

  const Bytes boveda = leer_todo(exportado.origen.de(ventrik::cifrado::kArchivoBoveda));
  const Bytes clave = leer_todo(exportado.origen.de(ventrik::cifrado::kArchivoClave));
  CHECK(ventrik::servidor::importar_servidor(exportado.origen.ruta(), exportado.archivo, kContrasena, "General")
            .resultado == Mudanza::ya_hay_servidor);
  CHECK(leer_todo(exportado.origen.de(ventrik::cifrado::kArchivoBoveda)) == boveda);
  CHECK(leer_todo(exportado.origen.de(ventrik::cifrado::kArchivoClave)) == clave);
  for (const wchar_t* sobrante : {kArchivoBase, kArchivoCadena, ventrik::cifrado::kArchivoClaveVieja}) {
    Carpeta con_algo;
    REQUIRE(ventrik::escribir_archivo_completo(con_algo.de(sobrante), Bytes(10, 1)));
    CHECK(ventrik::servidor::importar_servidor(con_algo.ruta(), exportado.archivo, kContrasena, "General").resultado ==
          Mudanza::ya_hay_servidor);
    CHECK(con_algo.archivos().size() == 1);
  }
}

TEST_CASE("Importar borra los restos de otra base antes de poner la del respaldo") {
  Exportado exportado;
  exportar(exportado);
  Carpeta destino;
  REQUIRE(ventrik::escribir_archivo_completo(destino.de(L"\\ventrik.db-wal"), Bytes(4096, 7)));
  REQUIRE(ventrik::escribir_archivo_completo(destino.de(L"\\ventrik.db-journal"), Bytes(512, 7)));
  REQUIRE(ventrik::escribir_archivo_completo(destino.de(L"\\ventrik.db-shm"), Bytes(4096, 7)));
  REQUIRE(ventrik::servidor::importar_servidor(destino.ruta(), exportado.archivo, kContrasena, "General").resultado ==
          Mudanza::hecha);
  for (const wchar_t* resto : {L"\\ventrik.db-wal", L"\\ventrik.db-shm", L"\\ventrik.db-journal"}) {
    CHECK_FALSE(existe(destino.de(resto)));
  }
  revisar_copia(destino, *exportado.huella);
}

TEST_CASE("Al arrancar se borran los restos de una exportación, una importación o un cambio de clave cortados") {
  Carpeta carpeta;
  for (const wchar_t* resto : {L"\\exportando.db", L"\\importando.db", L"\\importando.db-wal",
                               ventrik::cifrado::kArchivoClaveAnterior}) {
    REQUIRE(ventrik::escribir_archivo_completo(carpeta.de(resto), Bytes(100, 1)));
  }
  CHECK(ventrik::servidor::limpiar_restos(carpeta.ruta()));
  CHECK(carpeta.archivos().empty());
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.de(ventrik::cifrado::kArchivoClaveAnterior), Bytes(10, 1)));
  REQUIRE(SetFileAttributesW(carpeta.de(ventrik::cifrado::kArchivoClaveAnterior).c_str(), FILE_ATTRIBUTE_READONLY));
  CHECK_FALSE(ventrik::servidor::limpiar_restos(carpeta.ruta()));
}

TEST_CASE("Un cambio de clave cortado a mitad de camino se completa o se deshace solo, sin perder la clave") {
  Carpeta carpeta;
  auto primera = cargar(carpeta);
  const Huella a = primera.identidad->huella();
  const Bytes clave_a = leer_todo(carpeta.de(ventrik::cifrado::kArchivoClave));
  const auto cambio = ventrik::servidor::cambiar_clave(carpeta.ruta(), *primera.identidad, *primera.boveda);
  REQUIRE(cambio.resultado == Mudanza::hecha);
  const Huella b = *cambio.huella;
  const Bytes clave_b = leer_todo(carpeta.de(ventrik::cifrado::kArchivoClave));
  const Bytes cadena_b = leer_todo(carpeta.de(kArchivoCadena));

  SUBCASE("cortado después de apartar la clave vieja: arranca con la nueva y borra la vieja") {
    REQUIRE(MoveFileExW(carpeta.de(ventrik::cifrado::kArchivoClave).c_str(),
                        carpeta.de(ventrik::cifrado::kArchivoClaveNueva).c_str(), 0));
    REQUIRE(ventrik::escribir_archivo_completo(carpeta.de(ventrik::cifrado::kArchivoClaveAnterior), clave_a));
    auto cargada = cargar(carpeta, true);
    CHECK(cargada.identidad->huella() == b);
    CHECK_FALSE(existe(carpeta.de(ventrik::cifrado::kArchivoClaveNueva)));
    CHECK(ventrik::servidor::limpiar_restos(carpeta.ruta()));
    CHECK_FALSE(existe(carpeta.de(ventrik::cifrado::kArchivoClaveAnterior)));
    CHECK(ventrik::servidor::leer_cadena_guardada(carpeta.ruta(), b).cadena.size() == 1);
  }
  SUBCASE("cortado después de guardar la cadena: sigue con la vieja, la cadena no se trabó y se puede volver a cambiar") {
    REQUIRE(ventrik::escribir_archivo_completo(carpeta.de(ventrik::cifrado::kArchivoClave), clave_a));
    REQUIRE(ventrik::escribir_archivo_completo(carpeta.de(ventrik::cifrado::kArchivoClaveNueva), clave_b));
    auto cargada = cargar(carpeta, true);
    CHECK(cargada.identidad->huella() == a);
    const auto guardada = ventrik::servidor::leer_cadena_guardada(carpeta.ruta(), a);
    CHECK(guardada.resultado == Mudanza::hecha);
    CHECK(guardada.cadena.empty());
    const auto otra = ventrik::servidor::cambiar_clave(carpeta.ruta(), *cargada.identidad, *cargada.boveda);
    REQUIRE(otra.resultado == Mudanza::hecha);
    CHECK(*otra.huella != b);
    const auto nueva = ventrik::servidor::leer_cadena_guardada(carpeta.ruta(), *otra.huella);
    REQUIRE(nueva.cadena.size() == 1);
    CHECK(ventrik::cifrado::seguir_cadena(nueva.cadena, a, *otra.huella, {}));
    CHECK_FALSE(existe(carpeta.de(ventrik::cifrado::kArchivoClaveNueva)));
  }
  SUBCASE("sin la marca de un cambio cortado, volver a la clave vieja sigue sin dejar cambiarla") {
    REQUIRE(ventrik::escribir_archivo_completo(carpeta.de(ventrik::cifrado::kArchivoClave), clave_a));
    auto cargada = cargar(carpeta, true);
    CHECK(ventrik::servidor::leer_cadena_guardada(carpeta.ruta(), a).resultado == Mudanza::cadena_de_otra_clave);
    CHECK(ventrik::servidor::cambiar_clave(carpeta.ruta(), *cargada.identidad, *cargada.boveda).resultado ==
          Mudanza::cadena_de_otra_clave);
    CHECK(leer_todo(carpeta.de(kArchivoCadena)) == cadena_b);
  }
  SUBCASE("con la clave vieja sin cifrar todavía en la carpeta no se cambia") {
    auto cargada = cargar(carpeta, true);
    REQUIRE(ventrik::escribir_archivo_completo(carpeta.de(ventrik::cifrado::kArchivoClaveVieja), Bytes(3, 1)));
    CHECK(ventrik::servidor::cambiar_clave(carpeta.ruta(), *cargada.identidad, *cargada.boveda).resultado ==
          Mudanza::clave_vieja_sin_borrar);
    CHECK(leer_todo(carpeta.de(ventrik::cifrado::kArchivoClave)) == clave_b);
  }
}

TEST_CASE("Exportar pide 8 letras, no pisa un archivo y necesita la base") {
  Carpeta origen;
  auto cargada = cargar(origen);
  const std::wstring archivo = origen.de(L"\\respaldo.vkservidor");
  CHECK(ventrik::servidor::exportar_servidor(origen.ruta(), *cargada.identidad, *cargada.boveda, archivo, "corta12",
                                             kRapido) == Mudanza::contrasena_corta);
  CHECK(ventrik::servidor::exportar_servidor(origen.ruta(), *cargada.identidad, *cargada.boveda, archivo, kContrasena,
                                             kRapido) == Mudanza::error_base);
  CHECK_FALSE(existe(archivo));
  abrir_base(origen, *cargada.boveda);
  REQUIRE(ventrik::escribir_archivo_completo(archivo, Bytes(3, 9)));
  CHECK(ventrik::servidor::exportar_servidor(origen.ruta(), *cargada.identidad, *cargada.boveda, archivo, kContrasena,
                                             kRapido) == Mudanza::archivo_existe);
  CHECK(leer_todo(archivo) == Bytes(3, 9));
  DeleteFileW(archivo.c_str());
  CHECK(ventrik::servidor::exportar_servidor(origen.ruta(), *cargada.identidad, *cargada.boveda, archivo, kContrasena,
                                             kRapido) == Mudanza::hecha);
}

TEST_CASE("Una base de una versión vieja se actualiza al importar; con un trigger de más no se importa") {
  Carpeta origen;
  auto cargada = cargar(origen);
  {
    auto base = abrir_base(origen, *cargada.boveda);
    REQUIRE(base->guardar_mensaje(ventrik::servidor::kCanalGeneral, 1, "Ana", 1, "de antes"));
  }
  ejecutar(origen.de(kArchivoBase), "ALTER TABLE mensajes DROP COLUMN sellado; PRAGMA user_version=4;");
  CHECK(ventrik::servidor::revisar_base(origen.de(kArchivoBase)) == ventrik::servidor::RevisionBase::version_vieja);
  const std::wstring archivo = origen.de(L"\\vieja.vkservidor");
  REQUIRE(ventrik::servidor::exportar_servidor(origen.ruta(), *cargada.identidad, *cargada.boveda, archivo, kContrasena,
                                               kRapido) == Mudanza::hecha);
  Carpeta destino;
  REQUIRE(ventrik::servidor::importar_servidor(destino.ruta(), archivo, kContrasena, "General").resultado ==
          Mudanza::hecha);
  CHECK(ventrik::servidor::revisar_base(destino.de(kArchivoBase)) == ventrik::servidor::RevisionBase::buena);
  auto importada = cargar(destino, true);
  CHECK(abrir_base(destino, *importada.boveda)->historial({ventrik::servidor::kCanalGeneral, 0}, 65536)
            .mensajes.at(0)
            .texto == "de antes");

  ejecutar(origen.de(kArchivoBase), "CREATE TRIGGER borrar AFTER INSERT ON mensajes BEGIN DELETE FROM canales; END;");
  const std::wstring con_trigger = origen.de(L"\\trigger.vkservidor");
  REQUIRE(ventrik::servidor::exportar_servidor(origen.ruta(), *cargada.identidad, *cargada.boveda, con_trigger,
                                               kContrasena, kRapido) == Mudanza::hecha);
  Carpeta otro;
  CHECK(ventrik::servidor::importar_servidor(otro.ruta(), con_trigger, kContrasena, "General").resultado ==
        Mudanza::otro_esquema);
  CHECK(otro.archivos().empty());
}

TEST_CASE("Cambiar la clave firma la nueva con la vieja, guarda la cadena y no sigue si la clave no es la última") {
  Carpeta carpeta;
  auto primera = cargar(carpeta);
  const Huella a = primera.identidad->huella();
  const auto cambio = ventrik::servidor::cambiar_clave(carpeta.ruta(), *primera.identidad, *primera.boveda);
  REQUIRE(cambio.resultado == Mudanza::hecha);
  CHECK_FALSE(cambio.anterior_sin_borrar);
  REQUIRE(cambio.huella);
  const Huella b = *cambio.huella;
  CHECK(b != a);
  CHECK_FALSE(existe(carpeta.de(L"\\clave_servidor.anterior")));
  auto segunda = cargar(carpeta, true);
  CHECK(segunda.identidad->huella() == b);
  const auto guardada = ventrik::servidor::leer_cadena_guardada(carpeta.ruta(), b);
  REQUIRE(guardada.resultado == Mudanza::hecha);
  REQUIRE(guardada.cadena.size() == 1);
  CHECK(guardada.cadena[0].numero == 1);
  CHECK(ventrik::cifrado::seguir_cadena(guardada.cadena, a, b, {}));
  CHECK(ventrik::servidor::leer_cadena_guardada(carpeta.ruta(), a).resultado == Mudanza::cadena_de_otra_clave);

  const Bytes clave_b = leer_todo(carpeta.de(ventrik::cifrado::kArchivoClave));
  const auto otro_cambio = ventrik::servidor::cambiar_clave(carpeta.ruta(), *segunda.identidad, *segunda.boveda);
  REQUIRE(otro_cambio.resultado == Mudanza::hecha);
  const Huella c = *otro_cambio.huella;
  const auto dos = ventrik::servidor::leer_cadena_guardada(carpeta.ruta(), c);
  REQUIRE(dos.cadena.size() == 2);
  CHECK(dos.cadena[1].numero == 2);
  CHECK(ventrik::cifrado::seguir_cadena(dos.cadena, a, c, {}));

  const Bytes cadena_c = leer_todo(carpeta.de(kArchivoCadena));
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.de(ventrik::cifrado::kArchivoClave), clave_b));
  auto vuelta_atras = cargar(carpeta, true);
  CHECK(vuelta_atras.identidad->huella() == b);
  CHECK(ventrik::servidor::cambiar_clave(carpeta.ruta(), *vuelta_atras.identidad, *vuelta_atras.boveda).resultado ==
        Mudanza::cadena_de_otra_clave);
  CHECK(leer_todo(carpeta.de(kArchivoCadena)) == cadena_c);
  CHECK(leer_todo(carpeta.de(ventrik::cifrado::kArchivoClave)) == clave_b);

  REQUIRE(ventrik::escribir_archivo_completo(carpeta.de(kArchivoCadena), Bytes(40, 3)));
  CHECK(ventrik::servidor::leer_cadena_guardada(carpeta.ruta(), b).resultado == Mudanza::cadena_danada);
  CHECK(ventrik::servidor::cambiar_clave(carpeta.ruta(), *vuelta_atras.identidad, *vuelta_atras.boveda).resultado ==
        Mudanza::cadena_danada);
  CHECK(leer_todo(carpeta.de(kArchivoCadena)) == Bytes(40, 3));
}

TEST_CASE("El respaldo lleva la cadena de claves y el servidor importado la sigue mandando") {
  Carpeta origen;
  auto primera = cargar(origen);
  const Huella a = primera.identidad->huella();
  abrir_base(origen, *primera.boveda);
  REQUIRE(ventrik::servidor::cambiar_clave(origen.ruta(), *primera.identidad, *primera.boveda).resultado ==
          Mudanza::hecha);
  auto segunda = cargar(origen, true);
  const std::wstring archivo = origen.de(L"\\con_cadena.vkservidor");
  REQUIRE(ventrik::servidor::exportar_servidor(origen.ruta(), *segunda.identidad, *segunda.boveda, archivo, kContrasena,
                                               kRapido) == Mudanza::hecha);
  Carpeta destino;
  const auto importado = ventrik::servidor::importar_servidor(destino.ruta(), archivo, kContrasena, "General");
  REQUIRE(importado.resultado == Mudanza::hecha);
  CHECK(*importado.huella == segunda.identidad->huella());
  CHECK(leer_todo(destino.de(kArchivoCadena)) == leer_todo(origen.de(kArchivoCadena)));
  auto importada = cargar(destino, true);
  auto cadena = ventrik::servidor::leer_cadena_guardada(destino.ruta(), importada.identidad->huella());
  REQUIRE(cadena.resultado == Mudanza::hecha);
  ServidorConClave servidor(std::move(*importada.identidad), std::move(cadena.cadena));
  const auto resultado = entrar(servidor.direccion(), a);
  REQUIRE(resultado.estado == EstadoConexion::conectado);
  REQUIRE(resultado.cambio);
  CHECK(resultado.cambio->nueva == segunda.identidad->huella());
}

TEST_CASE("Un programa con la huella vieja entra sin aviso, queda con la nueva y no vuelve atrás") {
  Carpeta carpeta;
  auto primera = cargar(carpeta);
  const Huella a = primera.identidad->huella();
  REQUIRE(ventrik::servidor::cambiar_clave(carpeta.ruta(), *primera.identidad, *primera.boveda).resultado ==
          Mudanza::hecha);
  auto segunda = cargar(carpeta, true);
  const Huella b = segunda.identidad->huella();
  auto cadena = ventrik::servidor::leer_cadena_guardada(carpeta.ruta(), b);
  REQUIRE(cadena.resultado == Mudanza::hecha);
  ServidorConClave servidor(std::move(*segunda.identidad), std::move(cadena.cadena));

  const auto resultado = entrar(servidor.direccion(), a);
  REQUIRE(resultado.estado == EstadoConexion::conectado);
  REQUIRE(resultado.cambio);
  CHECK(resultado.cambio->nueva == b);
  CHECK(resultado.cambio->atras == std::vector<Huella>{a});
  ventrik::cliente::ServidorGuardado guardado;
  guardado.direccion = servidor.direccion();
  guardado.huella = a.bytes();
  guardado.nombre = "Mi servidor";
  ventrik::cliente::cambiar_huella(guardado, *resultado.cambio);
  CHECK(guardado.huella == b.bytes());
  CHECK(ventrik::cliente::huellas_atras(guardado) == std::vector<Huella>{a});

  const auto con_la_nueva = entrar(servidor.direccion(), b, {a});
  CHECK(con_la_nueva.estado == EstadoConexion::conectado);
  CHECK_FALSE(con_la_nueva.cambio);

  Cliente sesion(servidor.direccion(), a, "Sesión vieja");
  const auto intento = sesion.esperar<ventrik::cliente::IntentoDeConexion>();
  REQUIRE(intento);
  CHECK(intento->estado == EstadoConexion::conectado);
  REQUIRE(intento->cambio);
  CHECK(intento->cambio->nueva == b);

  CHECK(entrar(servidor.direccion(), a, {b}).estado == EstadoConexion::huella_distinta);
  CHECK(entrar(servidor.direccion(), identidad_nueva().huella()).estado == EstadoConexion::huella_distinta);
}

TEST_CASE("Sin una cadena que sirva, el programa solo manda el saludo y da el aviso de huella") {
  auto a = identidad_nueva();
  auto b = identidad_nueva();
  auto falsa = identidad_nueva();
  auto ajena = identidad_nueva();
  const Cadena buena{eslabon(a, b, 1), eslabon(b, falsa, 2)};
  Bytes alterada = marco_de(buena);
  alterada.back() ^= 1;
  Bytes basura;
  REQUIRE(ventrik::protocolo::armar_marco(static_cast<std::uint16_t>(TipoMensaje::cadena_de_claves), Bytes{1, 2, 3},
                                          basura));
  const std::vector<std::pair<const char*, Bytes>> casos = {
      {"sin cadena", {}},
      {"alterada", alterada},
      {"cortada", marco_de({buena[0]})},
      {"de otra clave", marco_de({eslabon(ajena, falsa, 1)})},
      {"que no es una cadena", basura}};
  for (const auto& [nombre, extra] : casos) {
    CAPTURE(nombre);
    ServidorQueAnota falso(falsa, extra);
    CHECK(entrar(falso.direccion(), a.huella()).estado == EstadoConexion::huella_distinta);
    CHECK(falso.recibidos() == std::vector<std::uint16_t>{static_cast<std::uint16_t>(TipoMensaje::hola)});
  }
  ServidorQueAnota con_cadena(falsa, marco_de(buena));
  CHECK(entrar(con_cadena.direccion(), a.huella()).estado != EstadoConexion::huella_distinta);
  CHECK(con_cadena.recibidos() == std::vector<std::uint16_t>{static_cast<std::uint16_t>(TipoMensaje::hola),
                                                              static_cast<std::uint16_t>(TipoMensaje::presentarse)});
}

TEST_CASE("La cadena de claves va y vuelve igual por el protocolo y no se lee con bytes de más o de menos") {
  auto a = identidad_nueva();
  auto b = identidad_nueva();
  const Cadena cadena{eslabon(a, b, 7)};
  const Bytes marco = marco_de(cadena);
  REQUIRE(marco.size() > ventrik::protocolo::kCabeceraMarco);
  const auto cuerpo = gsl::span<const std::uint8_t>(marco).subspan(ventrik::protocolo::kCabeceraMarco);
  CHECK(cuerpo.size() <= ventrik::protocolo::kMaximoCuerpoCadena);
  const auto leida = ventrik::protocolo::leer_mensaje<ventrik::protocolo::CadenaDeClaves>(cuerpo);
  REQUIRE(leida);
  REQUIRE(leida->cadena.size() == 1);
  CHECK(leida->cadena[0].numero == 7);
  CHECK(leida->cadena[0].firma == cadena[0].firma);
  CHECK_FALSE(ventrik::protocolo::leer_mensaje<ventrik::protocolo::CadenaDeClaves>(cuerpo.first(cuerpo.size() - 1)));
  Bytes de_mas(cuerpo.begin(), cuerpo.end());
  de_mas.push_back(0);
  CHECK_FALSE(ventrik::protocolo::leer_mensaje<ventrik::protocolo::CadenaDeClaves>(de_mas));
}

TEST_CASE("Importar con contraseña de arranque deja la bóveda protegida con ella") {
  Exportado exportado;
  exportar(exportado);
  Carpeta destino;
  const auto importado = ventrik::servidor::importar_servidor(destino.ruta(), exportado.archivo, kContrasena, "General",
                                                              std::string_view("arranque largo"));
  REQUIRE(importado.resultado == Mudanza::hecha);
  CHECK(ventrik::cifrado::cargar_o_crear_identidad(destino.ruta(), std::nullopt, true).resultado ==
        ventrik::cifrado::ResultadoIdentidad::falta_contrasena);
  const auto con_arranque =
      ventrik::cifrado::cargar_o_crear_identidad(destino.ruta(), std::string_view("arranque largo"), true);
  REQUIRE(con_arranque.identidad);
  CHECK(con_arranque.identidad->huella() == *exportado.huella);
}

TEST_CASE("Dos cambios de clave a la vez en la misma carpeta no se pisan, y el candado se suelta solo") {
  Carpeta carpeta;
  auto cargada = cargar(carpeta);
  const Huella antes = cargada.identidad->huella();
  {
    const ventrik::Candado otro(carpeta.de(ventrik::servidor::kArchivoCambioDeClave));
    REQUIRE(otro.tomado());
    CHECK(ventrik::servidor::cambiar_clave(carpeta.ruta(), *cargada.identidad, *cargada.boveda).resultado ==
          Mudanza::otro_cambio);
    CHECK(cargar(carpeta, true).identidad->huella() == antes);
  }
  CHECK_FALSE(existe(carpeta.de(ventrik::servidor::kArchivoCambioDeClave)));
  CHECK(ventrik::servidor::cambiar_clave(carpeta.ruta(), *cargada.identidad, *cargada.boveda).resultado ==
        Mudanza::hecha);
  CHECK_FALSE(existe(carpeta.de(ventrik::servidor::kArchivoCambioDeClave)));
}

TEST_CASE("Después de cambiar la clave, el código del equipo sale de la huella original: por la tarjeta, por Mi servidor y sin tarjeta") {
  Carpeta carpeta;
  auto primera = cargar(carpeta);
  const Huella a = primera.identidad->huella();
  ventrik::servidor::Opciones opciones;
  opciones.archivo_base = carpeta.de(kArchivoBase);
  opciones.segundo_paso = ventrik::SegundoPaso::todos;
  using EventoDePrueba = ServidorDePrueba::Evento;
  const auto entra_de_confianza = [](const ServidorDePrueba& servidor, const Huella& huella, std::vector<Huella> atras,
                                     std::optional<Huella> original) {
    const int antes = servidor.cuantos(EventoDePrueba::salio);
    std::optional<Huella> usada;
    {
      ventrik::cliente::OpcionesSesion sesion;
      sesion.conexion.atras = std::move(atras);
      sesion.conexion.original = original;
      Cliente ana(servidor.direccion(), huella, "Ana del equipo", {}, sesion);
      const auto intento = ana.esperar<ventrik::cliente::IntentoDeConexion>();
      REQUIRE(intento);
      REQUIRE(intento->estado == EstadoConexion::conectado);
      CHECK(ana.esperar<ventrik::protocolo::ConfigurarSegundoPaso>());
      usada = intento->original;
    }
    CHECK(esperar_que([&] { return servidor.cuantos(EventoDePrueba::salio) > antes; }));
    return usada;
  };
  {
    ServidorDePrueba servidor(opciones, &*primera.identidad);
    CHECK(entra_de_confianza(servidor, a, {}, std::nullopt) == a);
  }
  REQUIRE(ventrik::servidor::cambiar_clave(carpeta.ruta(), *primera.identidad, *primera.boveda).resultado ==
          Mudanza::hecha);
  auto segunda = cargar(carpeta, true);
  const Huella b = segunda.identidad->huella();
  auto cadena = ventrik::servidor::leer_cadena_guardada(carpeta.ruta(), b);
  REQUIRE(cadena.resultado == Mudanza::hecha);
  opciones.cadena = cadena.cadena;
  for (int vez = 0; vez < 2; ++vez) {
    CAPTURE(vez);
    ServidorDePrueba servidor(opciones, &*segunda.identidad);
    CHECK(entra_de_confianza(servidor, a, {}, a) == a);
    CHECK(entra_de_confianza(servidor, b, {a}, a) == a);
    CHECK(entra_de_confianza(servidor, b, {}, std::nullopt) == a);
    CHECK(servidor.cuantos(EventoDePrueba::equipo_nuevo) == 0);
  }
}
