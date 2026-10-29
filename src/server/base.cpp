#include "server/base.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <limits>
#include <set>
#include <utility>

#include <windows.h>

#include <gsl/gsl>
#include <sqlite3.h>

#include "common/texto.h"
#include "common/utf.h"

namespace ventrik::servidor {
namespace {
constexpr std::size_t kMaximoPedidosEnCola = 4096;
constexpr std::size_t kPedidosPorTanda = 64;
constexpr std::chrono::hours kCadaCuantoLimpiar{1};
constexpr std::chrono::minutes kCadaCuantoAsentar{1};
constexpr int kVersionEsquema = 4;
constexpr int kMaximoFila = 64 * 1024;

const char kAjustes[] =
    "PRAGMA journal_mode=WAL;"
    "PRAGMA synchronous=NORMAL;"
    "PRAGMA cache_size=-2048;";

constexpr const char* kSentencias[] = {
    "BEGIN",
    "COMMIT",
    "ROLLBACK",
    "SAVEPOINT paso",
    "RELEASE paso",
    "ROLLBACK TO paso",
    "UPDATE canales SET contador = MAX(contador, ?2) WHERE id = ?1",
    "INSERT INTO mensajes(canal, numero, autor, hora, texto) VALUES (?1, ?2, ?3, ?4, ?5)",
    "SELECT id, numero, autor, hora, texto FROM mensajes WHERE canal = ?1"
    " AND id < ?2 AND numero BETWEEN 0 AND 4294967295 ORDER BY id DESC LIMIT ?3",
    "SELECT id, numero, autor, hora, texto FROM mensajes WHERE canal = ?1"
    " AND id > ?2 AND numero BETWEEN 0 AND 4294967295 ORDER BY id ASC LIMIT ?3",
};

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
    "CREATE TABLE IF NOT EXISTS config(clave TEXT PRIMARY KEY, valor INTEGER NOT NULL);"
    "CREATE TABLE IF NOT EXISTS usuarios(clave BLOB PRIMARY KEY, nombre TEXT NOT NULL,"
    " comparable TEXT NOT NULL UNIQUE, dueno INTEGER NOT NULL DEFAULT 0, admin INTEGER NOT NULL DEFAULT 0,"
    " creado INTEGER NOT NULL, visto INTEGER NOT NULL, entradas INTEGER NOT NULL DEFAULT 1) WITHOUT ROWID;"
    "CREATE TABLE IF NOT EXISTS equipos(clave BLOB NOT NULL, equipo BLOB NOT NULL, primera INTEGER NOT NULL,"
    " ultima INTEGER NOT NULL, PRIMARY KEY(clave, equipo)) WITHOUT ROWID;"
    "CREATE TABLE IF NOT EXISTS textos(clave TEXT PRIMARY KEY, valor TEXT NOT NULL);"
    "CREATE TABLE IF NOT EXISTS avisos(id INTEGER PRIMARY KEY, clave BLOB NOT NULL, salvo BLOB, tipo INTEGER NOT NULL,"
    " hora INTEGER NOT NULL, quien TEXT NOT NULL, detalle TEXT NOT NULL);"
    "CREATE INDEX IF NOT EXISTS avisos_por_clave ON avisos(clave, id);"
    "CREATE TABLE IF NOT EXISTS seguridad(id INTEGER PRIMARY KEY, tipo INTEGER NOT NULL, hora INTEGER NOT NULL,"
    " quien TEXT NOT NULL, detalle TEXT NOT NULL);"
    "CREATE INDEX IF NOT EXISTS seguridad_por_tipo ON seguridad(tipo, id);";

const char kHuerfanos[] =
    "DELETE FROM equipos WHERE clave NOT IN (SELECT clave FROM usuarios);"
    "DELETE FROM avisos WHERE clave NOT IN (SELECT clave FROM usuarios);";

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
    "PRAGMA user_version=4;"
    "COMMIT;";

class Consulta {
 public:
  Consulta(sqlite3* base, const char* sql) : propia_(true) {
    if (sqlite3_prepare_v2(base, sql, -1, &consulta_, nullptr) != SQLITE_OK) {
      sqlite3_finalize(consulta_);
      consulta_ = nullptr;
    }
  }
  explicit Consulta(sqlite3_stmt* preparada) : consulta_(preparada) {}
  ~Consulta() { propia_ ? sqlite3_finalize(consulta_) : sqlite3_reset(consulta_); }
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

  Consulta& bytes(int lugar, gsl::span<const std::uint8_t> valor) {
    if (consulta_ != nullptr && sqlite3_bind_blob(consulta_, lugar, valor.data(), static_cast<int>(valor.size()),
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

  bool columna_bytes(int columna, gsl::span<std::uint8_t> destino) {
    const void* datos = sqlite3_column_blob(consulta_, columna);
    const int largo = sqlite3_column_bytes(consulta_, columna);
    if (datos == nullptr || largo < 0 || static_cast<std::size_t>(largo) != destino.size()) {
      return false;
    }
    const gsl::span<const std::uint8_t> bytes(static_cast<const std::uint8_t*>(datos), destino.size());
    std::copy(bytes.begin(), bytes.end(), destino.begin());
    return true;
  }

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
  bool propia_ = false;
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

std::uint64_t hace_dias(std::uint64_t ahora, std::uint64_t dias) {
  return std::min<std::uint64_t>(ahora > dias * kMsPorDia ? ahora - dias * kMsPorDia : 0,
                                 std::numeric_limits<std::int64_t>::max());
}

constexpr auto kMaximoMs = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());

bool es_nombre_o_nada(const std::string& nombre) {
  return nombre.empty() || es_nombre_valido(nombre);
}

bool es_aviso_valido(const protocolo::AvisoDeSeguridad& aviso) {
  return aviso.tipo >= protocolo::Seguridad::entrada_nueva && aviso.tipo <= protocolo::Seguridad::ip_bloqueada &&
         aviso.hora_ms <= kMaximoMs && es_nombre_o_nada(aviso.quien) && es_nombre_o_nada(aviso.detalle);
}

void leer_avisos(Consulta& consulta, std::vector<protocolo::AvisoDeSeguridad>& lista) {
  while (consulta.paso() == SQLITE_ROW) {
    const std::int64_t tipo = consulta.columna_entero(0);
    const std::int64_t hora = consulta.columna_entero(1);
    protocolo::AvisoDeSeguridad aviso{static_cast<protocolo::Seguridad>(tipo & 0xFF),
                                      static_cast<std::uint64_t>(hora), consulta.columna_texto(2),
                                      consulta.columna_texto(3)};
    if (tipo >= 0 && tipo <= 0xFF && hora >= 0 && es_aviso_valido(aviso)) {
      lista.push_back(std::move(aviso));
    }
  }
}

}

bool esta_vencido(const UsuarioGuardado& usuario, std::uint64_t ahora) {
  return !usuario.dueno && !usuario.admin &&
         (usuario.visto_ms < hace_dias(ahora, kDiasSinVolver) ||
          (usuario.entradas <= 1 && usuario.visto_ms < hace_dias(ahora, kDiasDeUnaSolaVez)));
}

bool se_puede_desalojar(const UsuarioGuardado& usuario, std::uint64_t ahora) {
  return !usuario.dueno && !usuario.admin && usuario.entradas <= 1 &&
         usuario.visto_ms < hace_dias(ahora, kDiasParaDesalojar);
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
      (guardada != 1 && !ejecutar(abierta, "PRAGMA user_version=4")) ||
      (guardada == 1 && !ejecutar(abierta, kMigrarDe1))) {
    return nullptr;
  }
  Consulta general(abierta,
                   "INSERT OR IGNORE INTO canales(id, nombre, comparable, orden) VALUES (?1, ?2, ?3, 0)");
  general.entero(1, kCanalGeneral).texto(2, nombre_general).texto(3, comparable(nombre_general));
  if (!es_nombre_valido(nombre_general) || general.paso() != SQLITE_DONE || !ejecutar(abierta, kAjustes)) {
    return nullptr;
  }
  base->cambios_asentados_ = sqlite3_total_changes64(abierta);
  return base;
}

Base::Base(Llave, sqlite3* base) : base_(base), hilo_(GetCurrentThreadId()) {}

Base::~Base() {
  for (sqlite3_stmt* preparada : preparadas_) {
    sqlite3_finalize(preparada);
  }
  sqlite3_close(base_);
}

bool Base::es_su_hilo() const {
  return GetCurrentThreadId() == hilo_;
}

sqlite3_stmt* Base::preparada(Sentencia cual) {
  static_assert(std::size(kSentencias) == static_cast<std::size_t>(Sentencia::total), "Falta una sentencia");
  sqlite3_stmt*& lugar = gsl::at(preparadas_, static_cast<std::size_t>(cual));
  if (lugar == nullptr && sqlite3_prepare_v3(base_, gsl::at(kSentencias, static_cast<std::size_t>(cual)), -1,
                                             SQLITE_PREPARE_PERSISTENT, &lugar, nullptr) != SQLITE_OK) {
    sqlite3_finalize(lugar);
    lugar = nullptr;
  }
  return lugar;
}

bool Base::correr(Sentencia cual) {
  return Consulta(preparada(cual)).paso() == SQLITE_DONE;
}

bool Base::empezar_tanda() {
  return es_su_hilo() && correr(Sentencia::empezar);
}

bool Base::en_tanda() const {
  return sqlite3_get_autocommit(base_) == 0;
}

bool Base::confirmar_tanda() {
  if (en_tanda() && correr(Sentencia::confirmar)) {
    return true;
  }
  deshacer_lo_abierto();
  return false;
}

void Base::deshacer_lo_abierto() {
  if (en_tanda()) {
    correr(Sentencia::deshacer);
  }
}

bool Base::asentar() {
  if (!es_su_hilo() || !ejecutar(base_, "PRAGMA wal_checkpoint(TRUNCATE)")) {
    return false;
  }
  cambios_asentados_ = sqlite3_total_changes64(base_);
  return true;
}

bool Base::hay_sin_asentar() const {
  return sqlite3_total_changes64(base_) != cambios_asentados_;
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
  if (!es_su_hilo() || id == kCanalGeneral || !correr(Sentencia::marcar)) {
    return false;
  }
  Consulta mensajes(base_, "DELETE FROM mensajes WHERE canal = ?1");
  Consulta canal(base_, "DELETE FROM canales WHERE id = ?1");
  mensajes.entero(1, id);
  canal.entero(1, id);
  if (mensajes.paso() != SQLITE_DONE || canal.paso() != SQLITE_DONE ||
      sqlite3_changes(base_) != 1 || !correr(Sentencia::soltar)) {
    correr(Sentencia::volver);
    correr(Sentencia::soltar);
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
      !correr(Sentencia::marcar)) {
    return std::nullopt;
  }
  Consulta contador(preparada(Sentencia::contador));
  contador.entero(1, canal).entero(2, numero);
  bool hecho = contador.paso() == SQLITE_DONE && sqlite3_changes(base_) == 1;
  std::optional<std::uint64_t> id;
  if (hecho && guardar) {
    Consulta consulta(preparada(Sentencia::insertar));
    consulta.entero(1, canal)
        .entero(2, numero)
        .texto(3, autor)
        .entero(4, static_cast<std::int64_t>(hora_ms))
        .texto(5, texto);
    hecho = consulta.paso() == SQLITE_DONE;
    id = static_cast<std::uint64_t>(sqlite3_last_insert_rowid(base_));
  }
  if (!hecho || !correr(Sentencia::soltar)) {
    correr(Sentencia::volver);
    correr(Sentencia::soltar);
    return std::nullopt;
  }
  return id;
}

protocolo::Historial Base::historial(const protocolo::PedirHistorial& pedido, std::size_t maximo_bytes) {
  protocolo::Historial historial{pedido.canal, pedido.desde, pedido.sentido};
  const bool nuevos = pedido.sentido == protocolo::Sentido::nuevos;
  if (!es_su_hilo() || (!nuevos && pedido.sentido != protocolo::Sentido::viejos)) {
    return historial;
  }
  constexpr auto kMaximoId = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  const auto tope = static_cast<std::int64_t>(!nuevos && pedido.desde == 0 ? kMaximoId
                                                                           : std::min(pedido.desde, kMaximoId));
  const std::size_t cantidad =
      std::clamp<std::size_t>(pedido.cantidad, 1, protocolo::kMaximoMensajesEnHistorial);
  Consulta consulta(preparada(nuevos ? Sentencia::nuevos : Sentencia::viejos));
  consulta.entero(1, pedido.canal).entero(2, tope).entero(3, static_cast<std::int64_t>(cantidad) + 1);
  std::size_t bytes = 4 + 8 + 1 + 1 + 2;
  auto anterior = static_cast<std::uint64_t>(tope);
  while (consulta.paso() == SQLITE_ROW) {
    protocolo::MensajeGuardado guardado;
    guardado.id = static_cast<std::uint64_t>(consulta.columna_entero(0));
    guardado.numero = static_cast<std::uint32_t>(consulta.columna_entero(1));
    guardado.autor = consulta.columna_texto(2);
    guardado.hora_ms = static_cast<std::uint64_t>(consulta.columna_entero(3));
    guardado.texto = consulta.columna_texto(4);
    if (guardado.id == 0 || (nuevos ? guardado.id <= anterior : guardado.id >= anterior) ||
        !es_nombre_valido(guardado.autor) || !es_chat_valido(guardado.texto)) {
      continue;
    }
    anterior = guardado.id;
    const std::size_t ocupa = 8 + 4 + 2 + guardado.autor.size() + 8 + 2 + guardado.texto.size();
    if (historial.mensajes.size() == cantidad || bytes + ocupa > maximo_bytes) {
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

std::optional<std::vector<UsuarioGuardado>> Base::usuarios(std::uint64_t ahora) {
  if (!es_su_hilo()) {
    return std::nullopt;
  }
  Consulta ultimo(base_, "SELECT IFNULL(MAX(visto), 0) FROM usuarios");
  if (ultimo.paso() != SQLITE_ROW) {
    return std::nullopt;
  }
  if (ultimo.columna_entero(0) <= static_cast<std::int64_t>(std::min(ahora, kMaximoMs))) {
    Consulta vencidos(base_,
                      "DELETE FROM usuarios WHERE dueno = 0 AND admin = 0"
                      " AND (visto < ?1 OR (entradas <= 1 AND visto < ?2))");
    vencidos.entero(1, static_cast<std::int64_t>(hace_dias(ahora, kDiasSinVolver)))
        .entero(2, static_cast<std::int64_t>(hace_dias(ahora, kDiasDeUnaSolaVez)));
    if (vencidos.paso() != SQLITE_DONE ||
        !ejecutar(base_, kHuerfanos)) {
      return std::nullopt;
    }
  }
  Consulta consulta(base_,
                    "SELECT clave, nombre, dueno, admin, creado, visto, entradas FROM usuarios WHERE dueno IN (0, 1)"
                    " AND admin IN (0, 1) AND creado >= 0 AND visto >= 0 AND entradas BETWEEN 0 AND 4294967295"
                    " ORDER BY dueno DESC, admin DESC, visto DESC LIMIT ?1");
  consulta.entero(1, static_cast<std::int64_t>(kMaximoUsuariosGuardados));
  std::vector<UsuarioGuardado> lista;
  std::set<std::wstring> comparables;
  int paso = SQLITE_ROW;
  while ((paso = consulta.paso()) == SQLITE_ROW) {
    UsuarioGuardado usuario;
    usuario.nombre = consulta.columna_texto(1);
    usuario.dueno = consulta.columna_entero(2) == 1;
    usuario.admin = consulta.columna_entero(3) == 1;
    usuario.creado_ms = static_cast<std::uint64_t>(consulta.columna_entero(4));
    usuario.visto_ms = static_cast<std::uint64_t>(consulta.columna_entero(5));
    usuario.entradas = static_cast<std::uint32_t>(consulta.columna_entero(6));
    if (consulta.columna_bytes(0, usuario.clave) && usuario.clave[0] == 0x04 && es_nombre_valido(usuario.nombre) &&
        comparables.insert(nombre_para_comparar(usuario.nombre)).second) {
      lista.push_back(std::move(usuario));
    }
  }
  if (paso != SQLITE_DONE) {
    return std::nullopt;
  }
  return lista;
}

bool Base::guardar_usuario(const UsuarioGuardado& usuario) {
  if (!es_su_hilo() || !es_nombre_valido(usuario.nombre) || usuario.visto_ms > kMaximoMs ||
      usuario.creado_ms > usuario.visto_ms) {
    return false;
  }
  Consulta consulta(base_,
                    "INSERT INTO usuarios(clave, nombre, comparable, dueno, admin, creado, visto, entradas)"
                    " VALUES (?1, ?2, ?3, ?4, ?5, ?7, ?6, ?8) ON CONFLICT(clave) DO UPDATE SET nombre = ?2,"
                    " comparable = ?3, dueno = ?4, admin = ?5, visto = MAX(visto, ?6), entradas = MAX(entradas, ?8)");
  consulta.bytes(1, usuario.clave)
      .texto(2, usuario.nombre)
      .texto(3, comparable(usuario.nombre))
      .entero(4, usuario.dueno)
      .entero(5, usuario.admin)
      .entero(6, static_cast<std::int64_t>(usuario.visto_ms))
      .entero(7, static_cast<std::int64_t>(usuario.creado_ms))
      .entero(8, usuario.entradas);
  return consulta.paso() == SQLITE_DONE;
}

bool Base::borrar_usuarios(const std::vector<ClaveDeUsuario>& claves) {
  if (!es_su_hilo()) {
    return false;
  }
  bool bien = true;
  for (const ClaveDeUsuario& clave : claves) {
    Consulta usuario(base_, "DELETE FROM usuarios WHERE clave = ?1 AND dueno = 0 AND admin = 0");
    Consulta equipos(base_, "DELETE FROM equipos WHERE clave = ?1 AND clave NOT IN (SELECT clave FROM usuarios)");
    Consulta avisos(base_, "DELETE FROM avisos WHERE clave = ?1 AND clave NOT IN (SELECT clave FROM usuarios)");
    usuario.bytes(1, clave);
    equipos.bytes(1, clave);
    avisos.bytes(1, clave);
    bien = usuario.paso() == SQLITE_DONE && equipos.paso() == SQLITE_DONE && avisos.paso() == SQLITE_DONE && bien;
  }
  return bien;
}

bool Base::liberar_nombre(const std::string& nombre) {
  if (!es_su_hilo() || !es_nombre_valido(nombre)) {
    return false;
  }
  Consulta consulta(base_, "DELETE FROM usuarios WHERE comparable = ?1 AND dueno = 0");
  consulta.texto(1, comparable(nombre));
  return consulta.paso() == SQLITE_DONE && ejecutar(base_, kHuerfanos);
}

bool Base::anotar_equipo(const ClaveDeUsuario& clave, const Equipo& equipo, std::uint64_t ahora) {
  if (!es_su_hilo() || ahora > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return false;
  }
  Consulta nuevo(base_,
                 "INSERT INTO equipos(clave, equipo, primera, ultima) VALUES (?1, ?2, ?3, ?3)"
                 " ON CONFLICT(clave, equipo) DO UPDATE SET ultima = MAX(ultima, ?3)");
  nuevo.bytes(1, clave).bytes(2, equipo).entero(3, static_cast<std::int64_t>(ahora));
  Consulta sobrantes(base_,
                     "DELETE FROM equipos WHERE clave = ?1 AND equipo NOT IN"
                     " (SELECT equipo FROM equipos WHERE clave = ?1 ORDER BY ultima - primera >= ?3 DESC,"
                     " ultima DESC LIMIT ?2)");
  sobrantes.bytes(1, clave)
      .entero(2, static_cast<std::int64_t>(kMaximoEquiposPorUsuario))
      .entero(3, static_cast<std::int64_t>(kMsPorDia));
  return nuevo.paso() == SQLITE_DONE && sobrantes.paso() == SQLITE_DONE;
}

std::optional<bool> Base::conoce_equipo(const ClaveDeUsuario& clave, const Equipo& equipo) {
  if (!es_su_hilo()) {
    return std::nullopt;
  }
  Consulta consulta(base_, "SELECT 1 FROM equipos WHERE clave = ?1 AND equipo = ?2");
  consulta.bytes(1, clave).bytes(2, equipo);
  const int paso = consulta.paso();
  return paso == SQLITE_ROW || paso == SQLITE_DONE ? std::optional<bool>(paso == SQLITE_ROW) : std::nullopt;
}

bool Base::anotar(const protocolo::AvisoDeSeguridad& aviso) {
  if (!es_su_hilo() || !es_aviso_valido(aviso)) {
    return false;
  }
  Consulta nuevo(base_, "INSERT INTO seguridad(tipo, hora, quien, detalle) VALUES (?1, ?2, ?3, ?4)");
  nuevo.entero(1, static_cast<std::int64_t>(aviso.tipo))
      .entero(2, static_cast<std::int64_t>(aviso.hora_ms))
      .texto(3, aviso.quien)
      .texto(4, aviso.detalle);
  Consulta viejos(base_,
                  "DELETE FROM seguridad WHERE tipo = ?1 AND id <= (SELECT id FROM seguridad WHERE tipo = ?1"
                  " ORDER BY id DESC LIMIT 1 OFFSET ?2)");
  viejos.entero(1, static_cast<std::int64_t>(aviso.tipo)).entero(2, static_cast<std::int64_t>(kMaximoRegistroPorTipo));
  return nuevo.paso() == SQLITE_DONE && viejos.paso() == SQLITE_DONE;
}

std::vector<protocolo::AvisoDeSeguridad> Base::registro(std::size_t maximo) {
  std::vector<protocolo::AvisoDeSeguridad> lista;
  if (!es_su_hilo()) {
    return lista;
  }
  Consulta consulta(base_, "SELECT tipo, hora, quien, detalle FROM seguridad ORDER BY id DESC LIMIT ?1");
  consulta.entero(1, static_cast<std::int64_t>(std::min<std::size_t>(maximo, kMaximoRegistroPorTipo * 8)));
  leer_avisos(consulta, lista);
  return lista;
}

bool Base::guardar_aviso(const ClaveDeUsuario& clave, const std::optional<Equipo>& salvo,
                         const protocolo::AvisoDeSeguridad& aviso) {
  if (!es_su_hilo() || !es_aviso_valido(aviso)) {
    return false;
  }
  Consulta nuevo(base_,
                 "INSERT INTO avisos(clave, salvo, tipo, hora, quien, detalle) SELECT ?1, ?2, ?3, ?4, ?5, ?6"
                 " WHERE EXISTS (SELECT 1 FROM usuarios WHERE clave = ?1)");
  nuevo.bytes(1, clave);
  if (salvo) {
    nuevo.bytes(2, *salvo);
  }
  nuevo.entero(3, static_cast<std::int64_t>(aviso.tipo))
      .entero(4, static_cast<std::int64_t>(aviso.hora_ms))
      .texto(5, aviso.quien)
      .texto(6, aviso.detalle);
  Consulta viejos(base_,
                  "DELETE FROM avisos WHERE clave = ?1 AND id <= (SELECT id FROM avisos WHERE clave = ?1"
                  " ORDER BY id DESC LIMIT 1 OFFSET ?2)");
  viejos.bytes(1, clave).entero(2, static_cast<std::int64_t>(kMaximoAvisosPorUsuario));
  return nuevo.paso() == SQLITE_DONE && viejos.paso() == SQLITE_DONE;
}

std::vector<protocolo::AvisoDeSeguridad> Base::tomar_avisos(const ClaveDeUsuario& clave, const Equipo& equipo) {
  std::vector<protocolo::AvisoDeSeguridad> lista;
  if (!es_su_hilo()) {
    return lista;
  }
  Consulta consulta(base_,
                    "SELECT tipo, hora, quien, detalle FROM avisos WHERE clave = ?1 AND (salvo IS NULL OR salvo <> ?2)"
                    " ORDER BY id LIMIT ?3");
  consulta.bytes(1, clave).bytes(2, equipo).entero(3, static_cast<std::int64_t>(kMaximoAvisosPorUsuario));
  leer_avisos(consulta, lista);
  Consulta tomados(base_, "DELETE FROM avisos WHERE clave = ?1 AND (salvo IS NULL OR salvo <> ?2)");
  tomados.bytes(1, clave).bytes(2, equipo);
  if (tomados.paso() != SQLITE_DONE) {
    lista.clear();
  }
  return lista;
}

bool Base::quitar_dueno() {
  return es_su_hilo() && ejecutar(base_, "UPDATE usuarios SET dueno = 0");
}

std::string Base::texto_guardado(const std::string& clave) {
  if (!es_su_hilo()) {
    return {};
  }
  Consulta consulta(base_, "SELECT valor FROM textos WHERE clave = ?1");
  consulta.texto(1, clave);
  return consulta.paso() == SQLITE_ROW ? consulta.columna_texto(0) : std::string();
}

bool Base::guardar_texto(const std::string& clave, const std::string& valor) {
  if (!es_su_hilo()) {
    return false;
  }
  Consulta consulta(base_, "INSERT OR REPLACE INTO textos(clave, valor) VALUES (?1, ?2)");
  consulta.texto(1, clave).texto(2, valor);
  return consulta.paso() == SQLITE_DONE;
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
  using Reloj = std::chrono::steady_clock;
  auto proxima_limpieza = Reloj::now();
  auto asentar_en = Reloj::time_point::max();
  std::vector<Trabajo> tanda;
  std::vector<std::function<void()>> listas;
  std::unique_lock<std::mutex> candado(candado_);
  for (;;) {
    const auto ahora = Reloj::now();
    if (ahora >= std::min(proxima_limpieza, asentar_en)) {
      candado.unlock();
      if (ahora >= proxima_limpieza) {
        base->borrar_viejos(ahora_ms());
        proxima_limpieza = ahora + kCadaCuantoLimpiar;
      }
      base->asentar();
      asentar_en = Reloj::time_point::max();
      candado.lock();
    }
    hay_pedidos_.wait_until(candado, std::min(proxima_limpieza, asentar_en),
                            [this] { return terminar_ || !pedidos_.empty(); });
    if (pedidos_.empty()) {
      if (terminar_) {
        return;
      }
      continue;
    }
    tanda.clear();
    while (!pedidos_.empty() && tanda.size() < kPedidosPorTanda) {
      tanda.push_back(std::move(pedidos_.front()));
      pedidos_.pop_front();
    }
    candado.unlock();
    listas.clear();
    const bool junta = tanda.size() > 1 && base->empezar_tanda();
    for (auto& trabajo : tanda) {
      listas.push_back(trabajo(*base));
      if (junta && !base->en_tanda()) {
        break;
      }
    }
    if (junta && !base->confirmar_tanda()) {
      listas.clear();
      for (auto& trabajo : tanda) {
        listas.push_back(trabajo(*base));
        base->deshacer_lo_abierto();
      }
    }
    base->deshacer_lo_abierto();
    if (asentar_en == Reloj::time_point::max() && base->hay_sin_asentar()) {
      asentar_en = Reloj::now() + kCadaCuantoAsentar;
    }
    candado.lock();
    bool avisar = false;
    for (auto& respuesta : listas) {
      if (respuesta) {
        respuestas_.push_back(std::move(respuesta));
        avisar = true;
      }
    }
    if (avisar) {
      despertador_->despertar();
    }
  }
}

}
