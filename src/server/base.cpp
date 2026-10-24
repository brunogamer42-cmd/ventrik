#include "server/base.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>

#include <windows.h>

#include <gsl/gsl>
#include <sqlite3.h>

#include "common/texto.h"
#include "common/utf.h"

namespace ventrik::servidor {
namespace {
constexpr std::size_t kMaximoPedidosEnCola = 4096;
constexpr std::chrono::hours kCadaCuantoLimpiar{1};
constexpr std::uint64_t kMsPorDia = 24ull * 60 * 60 * 1000;
constexpr int kVersionEsquema = 2;
constexpr int kMaximoFila = 64 * 1024;

const char kEsquema[] =
    "PRAGMA secure_delete=ON;"
    "PRAGMA trusted_schema=OFF;"
    "CREATE TABLE IF NOT EXISTS canales(id INTEGER PRIMARY KEY AUTOINCREMENT, nombre TEXT NOT NULL,"
    " comparable TEXT NOT NULL UNIQUE, orden INTEGER NOT NULL, tipo INTEGER NOT NULL DEFAULT 1,"
    " solo_admins INTEGER NOT NULL DEFAULT 0, contador INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE IF NOT EXISTS mensajes(id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " canal INTEGER NOT NULL, autor TEXT NOT NULL, hora INTEGER NOT NULL, texto TEXT NOT NULL,"
    " numero INTEGER NOT NULL DEFAULT 0);"
    "CREATE INDEX IF NOT EXISTS mensajes_por_canal ON mensajes(canal, id);"
    "CREATE INDEX IF NOT EXISTS mensajes_por_hora ON mensajes(hora);"
    "CREATE TABLE IF NOT EXISTS config(clave TEXT PRIMARY KEY, valor INTEGER NOT NULL);";

const char kMigrarDe1[] =
    "BEGIN;"
    "ALTER TABLE canales ADD COLUMN tipo INTEGER NOT NULL DEFAULT 1;"
    "ALTER TABLE canales ADD COLUMN solo_admins INTEGER NOT NULL DEFAULT 0;"
    "ALTER TABLE canales ADD COLUMN contador INTEGER NOT NULL DEFAULT 0;"
    "ALTER TABLE mensajes ADD COLUMN numero INTEGER NOT NULL DEFAULT 0;"
    "UPDATE mensajes SET numero = orden.n FROM (SELECT id, ROW_NUMBER() OVER"
    " (PARTITION BY canal ORDER BY id) AS n FROM mensajes) AS orden WHERE mensajes.id = orden.id;"
    "UPDATE canales SET contador = (SELECT IFNULL(MAX(numero), 0) FROM mensajes"
    " WHERE mensajes.canal = canales.id);"
    "PRAGMA user_version=2;"
    "COMMIT;";

class Consulta {
 public:
  Consulta(sqlite3* base, const char* sql) {
    if (sqlite3_prepare_v2(base, sql, -1, &consulta_, nullptr) != SQLITE_OK) {
      sqlite3_finalize(consulta_);
      consulta_ = nullptr;
    }
  }
  ~Consulta() { sqlite3_finalize(consulta_); }
  Consulta(const Consulta&) = delete;
  Consulta& operator=(const Consulta&) = delete;

  Consulta& texto(int lugar, const std::string& valor) {
    if (consulta_ != nullptr &&
        sqlite3_bind_text(consulta_, lugar, valor.data(), static_cast<int>(valor.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
      bien_ = false;
    }
    return *this;
  }

  Consulta& entero(int lugar, std::int64_t valor) {
    if (consulta_ != nullptr && sqlite3_bind_int64(consulta_, lugar, valor) != SQLITE_OK) {
      bien_ = false;
    }
    return *this;
  }

  int paso() { return consulta_ != nullptr && bien_ ? sqlite3_step(consulta_) : SQLITE_ERROR; }

  std::int64_t columna_entero(int columna) { return sqlite3_column_int64(consulta_, columna); }

  std::string columna_texto(int columna) {
    const unsigned char* datos = sqlite3_column_text(consulta_, columna);
    const int largo = sqlite3_column_bytes(consulta_, columna);
    if (datos == nullptr || largo <= 0) {
      return {};
    }
    const gsl::span<const unsigned char> bytes(datos, static_cast<std::size_t>(largo));
    return std::string(bytes.begin(), bytes.end());
  }

 private:
  sqlite3_stmt* consulta_ = nullptr;
  bool bien_ = true;
};

bool ejecutar(sqlite3* base, const char* sql) {
  return sqlite3_exec(base, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

std::string comparable(const std::string& nombre) {
  return a_utf8(nombre_para_comparar(nombre));
}

std::uint64_t ahora_ms() {
  const auto desde_1970 = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(desde_1970).count());
}

}

std::unique_ptr<Base> Base::abrir(const std::wstring& archivo, const std::string& nombre_general) {
  const std::string ruta = archivo.empty() ? std::string(":memory:") : a_utf8(archivo);
  sqlite3* abierta = nullptr;
  if (ruta.empty() ||
      sqlite3_open_v2(ruta.c_str(), &abierta, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                      nullptr) != SQLITE_OK) {
    sqlite3_close(abierta);
    return nullptr;
  }
  auto base = std::make_unique<Base>(Llave{}, abierta);
  sqlite3_limit(abierta, SQLITE_LIMIT_LENGTH, kMaximoFila);
  std::int64_t guardada = -1;
  {
    Consulta version(abierta, "PRAGMA user_version");
    if (version.paso() == SQLITE_ROW) {
      guardada = version.columna_entero(0);
    }
  }
  if (sqlite3_db_config(abierta, SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr) != SQLITE_OK ||
      guardada < 0 || guardada > kVersionEsquema || !ejecutar(abierta, kEsquema) ||
      (guardada == 0 && !ejecutar(abierta, "PRAGMA user_version=2")) ||
      (guardada == 1 && !ejecutar(abierta, kMigrarDe1))) {
    return nullptr;
  }
  Consulta general(abierta,
                   "INSERT OR IGNORE INTO canales(id, nombre, comparable, orden) VALUES (?1, ?2, ?3, 0)");
  general.entero(1, kCanalGeneral).texto(2, nombre_general).texto(3, comparable(nombre_general));
  if (!es_nombre_valido(nombre_general) || general.paso() != SQLITE_DONE) {
    return nullptr;
  }
  return base;
}

Base::Base(Llave, sqlite3* base) : base_(base), hilo_(GetCurrentThreadId()) {}

Base::~Base() {
  sqlite3_close(base_);
}

bool Base::es_su_hilo() const {
  return GetCurrentThreadId() == hilo_;
}

std::vector<protocolo::CanalInfo> Base::canales() {
  std::vector<protocolo::CanalInfo> lista;
  if (!es_su_hilo()) {
    return lista;
  }
  Consulta consulta(base_,
                    "SELECT id, nombre, tipo, solo_admins, contador FROM canales"
                    " WHERE id BETWEEN 1 AND 4294967295 AND tipo IN (1, 2) AND solo_admins IN (0, 1)"
                    " AND contador BETWEEN 0 AND 4294967294 ORDER BY orden, id LIMIT ?1");
  consulta.entero(1, static_cast<std::int64_t>(protocolo::kMaximoCanalesEnLista));
  while (consulta.paso() == SQLITE_ROW) {
    protocolo::CanalInfo canal;
    canal.id = static_cast<std::uint32_t>(consulta.columna_entero(0));
    canal.nombre = consulta.columna_texto(1);
    canal.tipo = static_cast<protocolo::TipoCanal>(consulta.columna_entero(2));
    canal.solo_admins = consulta.columna_entero(3) == 1;
    canal.contador = static_cast<std::uint32_t>(consulta.columna_entero(4));
    if (es_nombre_valido(canal.nombre) && protocolo::es_tipo_valido(canal.tipo, canal.solo_admins)) {
      lista.push_back(std::move(canal));
    }
  }
  return lista;
}

std::optional<std::uint32_t> Base::crear_canal(const std::string& nombre,
                                               protocolo::TipoCanal tipo, bool solo_admins) {
  if (!es_su_hilo() || !es_nombre_valido(nombre) || !protocolo::es_tipo_valido(tipo, solo_admins)) {
    return std::nullopt;
  }
  Consulta consulta(base_,
                    "INSERT INTO canales(nombre, comparable, orden, tipo, solo_admins)"
                    " VALUES (?1, ?2, (SELECT IFNULL(MAX(orden), 0) + 1 FROM canales), ?3, ?4)");
  consulta.texto(1, nombre)
      .texto(2, comparable(nombre))
      .entero(3, static_cast<std::int64_t>(tipo))
      .entero(4, solo_admins);
  if (consulta.paso() != SQLITE_DONE) {
    return std::nullopt;
  }
  const auto id = sqlite3_last_insert_rowid(base_);
  if (id <= 0 || id > std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(id);
}

bool Base::cambiar_canal(std::uint32_t id, const std::string& nombre, bool solo_admins) {
  if (!es_su_hilo() || !es_nombre_valido(nombre)) {
    return false;
  }
  Consulta consulta(base_, "UPDATE canales SET nombre = ?2, comparable = ?3, solo_admins = ?4"
                           " WHERE id = ?1 AND (tipo = 2 OR ?4 = 0)");
  consulta.entero(1, id).texto(2, nombre).texto(3, comparable(nombre)).entero(4, solo_admins);
  return consulta.paso() == SQLITE_DONE && sqlite3_changes(base_) == 1;
}

bool Base::borrar_canal(std::uint32_t id) {
  if (!es_su_hilo() || id == kCanalGeneral || !ejecutar(base_, "BEGIN")) {
    return false;
  }
  Consulta mensajes(base_, "DELETE FROM mensajes WHERE canal = ?1");
  Consulta canal(base_, "DELETE FROM canales WHERE id = ?1");
  mensajes.entero(1, id);
  canal.entero(1, id);
  if (mensajes.paso() != SQLITE_DONE || canal.paso() != SQLITE_DONE ||
      sqlite3_changes(base_) != 1 || !ejecutar(base_, "COMMIT")) {
    ejecutar(base_, "ROLLBACK");
    return false;
  }
  return true;
}

std::optional<std::uint64_t> Base::guardar_mensaje(std::uint32_t canal, std::uint32_t numero,
                                                   const std::string& autor,
                                                   std::uint64_t hora_ms,
                                                   const std::string& texto, bool guardar) {
  if (!es_su_hilo() || numero == 0 || !es_nombre_valido(autor) || !es_chat_valido(texto) ||
      hora_ms > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
      !ejecutar(base_, "BEGIN")) {
    return std::nullopt;
  }
  Consulta contador(base_, "UPDATE canales SET contador = MAX(contador, ?2) WHERE id = ?1");
  contador.entero(1, canal).entero(2, numero);
  bool hecho = contador.paso() == SQLITE_DONE && sqlite3_changes(base_) == 1;
  std::optional<std::uint64_t> id;
  if (hecho && guardar) {
    Consulta consulta(base_, "INSERT INTO mensajes(canal, numero, autor, hora, texto)"
                             " VALUES (?1, ?2, ?3, ?4, ?5)");
    consulta.entero(1, canal)
        .entero(2, numero)
        .texto(3, autor)
        .entero(4, static_cast<std::int64_t>(hora_ms))
        .texto(5, texto);
    hecho = consulta.paso() == SQLITE_DONE;
    id = static_cast<std::uint64_t>(sqlite3_last_insert_rowid(base_));
  }
  if (!hecho || !ejecutar(base_, "COMMIT")) {
    ejecutar(base_, "ROLLBACK");
    return std::nullopt;
  }
  return id;
}

protocolo::Historial Base::historial(std::uint32_t canal, std::uint64_t antes_de,
                                     std::size_t maximo_bytes) {
  protocolo::Historial historial;
  historial.canal = canal;
  if (!es_su_hilo()) {
    return historial;
  }
  const auto tope = antes_de == 0 || antes_de > static_cast<std::uint64_t>(
                                                     std::numeric_limits<std::int64_t>::max())
                        ? std::numeric_limits<std::int64_t>::max()
                        : static_cast<std::int64_t>(antes_de);
  Consulta consulta(base_,
                    "SELECT id, numero, autor, hora, texto FROM mensajes WHERE canal = ?1"
                    " AND id < ?2 AND numero BETWEEN 0 AND 4294967295 ORDER BY id DESC LIMIT ?3");
  consulta.entero(1, canal)
      .entero(2, tope)
      .entero(3, static_cast<std::int64_t>(protocolo::kMaximoMensajesEnHistorial) + 1);
  std::size_t bytes = 4 + 1 + 2;
  auto anterior = static_cast<std::uint64_t>(tope);
  while (consulta.paso() == SQLITE_ROW) {
    protocolo::MensajeGuardado guardado;
    guardado.id = static_cast<std::uint64_t>(consulta.columna_entero(0));
    guardado.numero = static_cast<std::uint32_t>(consulta.columna_entero(1));
    guardado.autor = consulta.columna_texto(2);
    guardado.hora_ms = static_cast<std::uint64_t>(consulta.columna_entero(3));
    guardado.texto = consulta.columna_texto(4);
    if (guardado.id == 0 || guardado.id >= anterior || !es_nombre_valido(guardado.autor) ||
        !es_chat_valido(guardado.texto)) {
      continue;
    }
    anterior = guardado.id;
    const std::size_t ocupa = 8 + 4 + 2 + guardado.autor.size() + 8 + 2 + guardado.texto.size();
    if (historial.mensajes.size() == protocolo::kMaximoMensajesEnHistorial ||
        bytes + ocupa > maximo_bytes) {
      historial.hay_mas = true;
      break;
    }
    bytes += ocupa;
    historial.mensajes.push_back(std::move(guardado));
  }
  return historial;
}

Retencion Base::retencion() {
  if (!es_su_hilo()) {
    return Retencion::siempre;
  }
  Consulta consulta(base_, "SELECT valor FROM config WHERE clave = 'retencion'");
  if (consulta.paso() != SQLITE_ROW) {
    return Retencion::siempre;
  }
  const auto valor = consulta.columna_entero(0);
  return valor >= 0 && valor <= static_cast<std::int64_t>(Retencion::no_guardar)
             ? static_cast<Retencion>(valor)
             : Retencion::siempre;
}

bool Base::cambiar_retencion(Retencion retencion) {
  if (!es_su_hilo()) {
    return false;
  }
  Consulta consulta(base_, "INSERT OR REPLACE INTO config(clave, valor) VALUES ('retencion', ?1)");
  consulta.entero(1, static_cast<std::int64_t>(retencion));
  return consulta.paso() == SQLITE_DONE;
}

void Base::borrar_viejos(std::uint64_t ahora) {
  if (!es_su_hilo()) {
    return;
  }
  std::uint64_t dias = 0;
  switch (retencion()) {
    case Retencion::siempre:
      return;
    case Retencion::dias_30:
      dias = 30;
      break;
    case Retencion::dias_7:
      dias = 7;
      break;
    case Retencion::no_guardar:
      dias = 0;
      break;
  }
  const std::uint64_t limite = ahora > dias * kMsPorDia ? ahora - dias * kMsPorDia : 0;
  Consulta consulta(base_, "DELETE FROM mensajes WHERE hora < ?1");
  consulta.entero(1, static_cast<std::int64_t>(std::min<std::uint64_t>(
                         limite, std::numeric_limits<std::int64_t>::max())));
  consulta.paso();
}

ServicioBase::~ServicioBase() {
  {
    const std::lock_guard<std::mutex> candado(candado_);
    terminar_ = true;
  }
  hay_pedidos_.notify_all();
  if (hilo_.joinable()) {
    hilo_.join();
  }
}

bool ServicioBase::arrancar(std::wstring archivo, std::string nombre_general,
                            const red::Despertador& despertador) {
  despertador_ = &despertador;
  hilo_ = std::thread([this, archivo = std::move(archivo),
                       nombre_general = std::move(nombre_general)]() mutable {
    trabajar(std::move(archivo), std::move(nombre_general));
  });
  std::unique_lock<std::mutex> candado(candado_);
  abierta_.wait(candado, [this] { return pudo_abrir_.has_value(); });
  return *pudo_abrir_;
}

bool ServicioBase::pedir(Trabajo trabajo) {
  {
    const std::lock_guard<std::mutex> candado(candado_);
    if (pedidos_.size() >= kMaximoPedidosEnCola) {
      return false;
    }
    pedidos_.push_back(std::move(trabajo));
  }
  hay_pedidos_.notify_one();
  return true;
}

void ServicioBase::atender_respuestas() {
  std::deque<std::function<void()>> listas;
  {
    const std::lock_guard<std::mutex> candado(candado_);
    listas.swap(respuestas_);
  }
  for (auto& respuesta : listas) {
    respuesta();
  }
}

void ServicioBase::trabajar(std::wstring archivo, std::string nombre_general) {
  auto base = Base::abrir(archivo, nombre_general);
  {
    const std::lock_guard<std::mutex> candado(candado_);
    pudo_abrir_ = base != nullptr;
  }
  abierta_.notify_all();
  if (!base) {
    return;
  }
  auto proxima_limpieza = std::chrono::steady_clock::now();
  std::unique_lock<std::mutex> candado(candado_);
  for (;;) {
    if (std::chrono::steady_clock::now() >= proxima_limpieza) {
      candado.unlock();
      base->borrar_viejos(ahora_ms());
      candado.lock();
      proxima_limpieza = std::chrono::steady_clock::now() + kCadaCuantoLimpiar;
    }
    hay_pedidos_.wait_until(candado, proxima_limpieza,
                            [this] { return terminar_ || !pedidos_.empty(); });
    if (pedidos_.empty()) {
      if (terminar_) {
        return;
      }
      continue;
    }
    Trabajo trabajo = std::move(pedidos_.front());
    pedidos_.pop_front();
    candado.unlock();
    auto respuesta = trabajo(*base);
    candado.lock();
    if (respuesta) {
      respuestas_.push_back(std::move(respuesta));
      despertador_->despertar();
    }
  }
}

}
