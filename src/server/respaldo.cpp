#include "server/respaldo.h"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

#include <windows.h>

#include <mbedtls/platform_util.h>
#include <sqlite3.h>

#include "common/archivos.h"
#include "common/protocol/bytes.h"
#include "common/utf.h"
#include "server/base.h"

namespace ventrik::servidor {
namespace {

constexpr std::uint8_t kVersionRespaldo = 1;
constexpr std::size_t kPartes = 3;
constexpr std::size_t kBytesCabeceraParte = 1 + 4;
constexpr std::array<std::size_t, kPartes> kTopes{kMaximoClaveRespaldo, kMaximoBovedaRespaldo, kMaximoBaseRespaldo};
constexpr int kEsperaMs = 5000;
constexpr std::size_t kMaximoFilas = 1000;
constexpr std::array<std::array<int, 2>, 4> kDesconfiar{{{SQLITE_DBCONFIG_DEFENSIVE, 1},
                                                       {SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0},
                                                       {SQLITE_DBCONFIG_ENABLE_TRIGGER, 0},
                                                       {SQLITE_DBCONFIG_ENABLE_VIEW, 0}}};

using Conexion = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Firma = std::array<std::vector<std::string>, 2>;

std::array<gsl::span<const std::uint8_t>, kPartes> en_orden(const PartesRespaldo& partes) {
  return {partes.clave, partes.boveda, partes.base};
}

Conexion conectar(const std::wstring& archivo, int banderas) {
  sqlite3* abierta = nullptr;
  const int resultado =
      archivo.empty() ? SQLITE_CANTOPEN : sqlite3_open_v2(a_utf8(archivo).c_str(), &abierta, banderas, nullptr);
  Conexion conexion(abierta, &sqlite3_close);
  if (resultado != SQLITE_OK) {
    conexion.reset();
  }
  return conexion;
}

std::optional<std::vector<std::string>> filas(sqlite3* base, const char* sql) {
  sqlite3_stmt* preparada = nullptr;
  const int preparar = sqlite3_prepare_v2(base, sql, -1, &preparada, nullptr);
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> consulta(preparada, &sqlite3_finalize);
  std::vector<std::string> leidas;
  int paso = preparar == SQLITE_OK ? sqlite3_step(preparada) : SQLITE_ERROR;
  for (; paso == SQLITE_ROW && leidas.size() < kMaximoFilas; paso = sqlite3_step(preparada)) {
    const unsigned char* texto = sqlite3_column_text(preparada, 0);
    const gsl::span<const unsigned char> bytes(
        texto, texto == nullptr ? 0 : static_cast<std::size_t>(sqlite3_column_bytes(preparada, 0)));
    leidas.emplace_back(bytes.begin(), bytes.end());
  }
  return paso == SQLITE_DONE ? std::optional<std::vector<std::string>>(std::move(leidas)) : std::nullopt;
}

bool desconfiar(sqlite3* base) {
  return std::all_of(kDesconfiar.begin(), kDesconfiar.end(),
                     [base](const std::array<int, 2>& opcion) {
                       return sqlite3_db_config(base, opcion[0], opcion[1], nullptr) == SQLITE_OK;
                     }) &&
         sqlite3_exec(base, "PRAGMA cell_size_check=ON", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool se_puede_escribir(sqlite3* base) {
  sqlite3_file* archivo = nullptr;
  std::array<unsigned char, 2> versiones{};
  return sqlite3_file_control(base, "main", SQLITE_FCNTL_FILE_POINTER, &archivo) == SQLITE_OK && archivo != nullptr &&
         archivo->pMethods != nullptr && archivo->pMethods->xRead(archivo, versiones.data(), 2, 18) == SQLITE_OK &&
         std::all_of(versiones.begin(), versiones.end(), [](unsigned char version) { return version == 1 || version == 2; });
}

std::optional<Firma> firma(sqlite3* base) {
  auto version = filas(base, "PRAGMA user_version");
  auto esquema = version ? filas(base,
                                 "SELECT quote(type) || quote(name) || quote(tbl_name) || quote(sql)"
                                 " FROM sqlite_master ORDER BY 1")
                         : std::nullopt;
  if (!esquema) {
    return std::nullopt;
  }
  return Firma{std::move(*version), std::move(*esquema)};
}

std::optional<Firma> firma_de_referencia() {
  std::array<wchar_t, MAX_PATH + 1> carpeta{};
  std::array<wchar_t, MAX_PATH + 1> nombre{};
  const DWORD largo = GetTempPathW(MAX_PATH, carpeta.data());
  if (largo == 0 || largo >= MAX_PATH || GetTempFileNameW(carpeta.data(), L"vkr", 0, nombre.data()) == 0) {
    return std::nullopt;
  }
  const std::wstring archivo(nombre.data());
  const auto limpiar = gsl::finally([&archivo] { borrar_base(archivo); });
  if (!Base::abrir(archivo, "General")) {
    return std::nullopt;
  }
  const auto conexion = conectar(archivo, SQLITE_OPEN_READONLY);
  return conexion ? firma(conexion.get()) : std::nullopt;
}

}

std::optional<PartesRespaldo> leer_contenido(gsl::span<const std::uint8_t> contenido) {
  protocolo::Lector lector(contenido);
  std::array<gsl::span<const std::uint8_t>, kPartes> leidas{};
  if (lector.leer_u8() != kVersionRespaldo) {
    return std::nullopt;
  }
  for (std::size_t i = 0; i < kPartes; ++i) {
    const std::size_t tipo = lector.leer_u8();
    const std::size_t largo = lector.leer_u32();
    if (tipo != i + 1 || largo == 0 || largo > gsl::at(kTopes, i)) {
      return std::nullopt;
    }
    gsl::at(leidas, i) = lector.leer_bytes(largo);
  }
  if (!lector.terminado()) {
    return std::nullopt;
  }
  return PartesRespaldo{leidas[0], leidas[1], leidas[2]};
}

std::optional<std::vector<std::uint8_t>> armar_respaldo(const PartesRespaldo& partes, std::string_view contrasena,
                                                        std::optional<cifrado::Argon2> fijos) {
  const auto lista = en_orden(partes);
  std::size_t total = 1;
  for (std::size_t i = 0; i < kPartes; ++i) {
    if (gsl::at(lista, i).empty() || gsl::at(lista, i).size() > gsl::at(kTopes, i)) {
      return std::nullopt;
    }
    total += kBytesCabeceraParte + gsl::at(lista, i).size();
  }
  if (cantidad_de_letras(contrasena) < cifrado::kMinimoContrasena) {
    return std::nullopt;
  }
  std::vector<std::uint8_t> lugar;
  lugar.reserve(total);
  protocolo::Escritor escritor(std::move(lugar));
  escritor.escribir_u8(kVersionRespaldo);
  for (std::size_t i = 0; i < kPartes; ++i) {
    escritor.escribir_u8(static_cast<std::uint8_t>(i + 1));
    escritor.escribir_u32(static_cast<std::uint32_t>(gsl::at(lista, i).size()));
    escritor.escribir_bytes(gsl::at(lista, i));
  }
  auto contenido = std::move(escritor).soltar();
  const auto borrar = gsl::finally([&contenido] { mbedtls_platform_zeroize(contenido.data(), contenido.size()); });
  return cifrado::cerrar_sobre(kMarcaRespaldo, contenido, contrasena, fijos);
}

RespaldoAbierto abrir_respaldo(gsl::span<const std::uint8_t> archivo, std::string_view contrasena) {
  if (archivo.size() > kMaximoArchivoRespaldo) {
    return {};
  }
  cifrado::SobreAbierto sobre = cifrado::abrir_sobre(kMarcaRespaldo, archivo, contrasena);
  RespaldoAbierto abierto{sobre.resultado, std::move(sobre.contenido), {}};
  const auto partes = abierto.contenido ? leer_contenido(abierto.contenido->bytes()) : std::nullopt;
  if (partes) {
    abierto.partes = *partes;
  } else if (abierto.contenido) {
    abierto = RespaldoAbierto{};
  }
  return abierto;
}

bool copiar_base(const std::wstring& origen, const std::wstring& destino) {
  if (destino.empty() || escribir_archivo_nuevo(destino, {}) != EscrituraNueva::escrito) {
    return false;
  }
  bool copiada = false;
  {
    const auto fuente = conectar(origen, SQLITE_OPEN_READONLY);
    const auto copia =
        fuente ? conectar(destino, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE) : Conexion(nullptr, &sqlite3_close);
    sqlite3_backup* respaldo = copia && sqlite3_busy_timeout(fuente.get(), kEsperaMs) == SQLITE_OK
                                   ? sqlite3_backup_init(copia.get(), "main", fuente.get(), "main")
                                   : nullptr;
    copiada = respaldo != nullptr && sqlite3_backup_step(respaldo, -1) == SQLITE_DONE;
    copiada = sqlite3_backup_finish(respaldo) == SQLITE_OK && copiada;
  }
  if (!copiada) {
    borrar_base(destino);
  }
  return copiada;
}

void borrar_base(const std::wstring& archivo) {
  for (const wchar_t* sufijo : {L"", L"-journal", L"-wal", L"-shm"}) {
    DeleteFileW((archivo + sufijo).c_str());
  }
}

RevisionBase revisar_base(const std::wstring& archivo) {
  const auto conexion = conectar(archivo, SQLITE_OPEN_READONLY);
  if (!conexion || !desconfiar(conexion.get())) {
    return RevisionBase::error;
  }
  const auto leida = se_puede_escribir(conexion.get()) ? firma(conexion.get()) : std::nullopt;
  if (!leida) {
    return RevisionBase::danada;
  }
  const auto referencia = firma_de_referencia();
  if (!referencia) {
    return RevisionBase::error;
  }
  if ((*leida)[0] != (*referencia)[0]) {
    return RevisionBase::otra_version;
  }
  if ((*leida)[1] != (*referencia)[1]) {
    return RevisionBase::otro_esquema;
  }
  return filas(conexion.get(), "PRAGMA integrity_check") == std::vector<std::string>{"ok"} ? RevisionBase::buena
                                                                                           : RevisionBase::danada;
}

}
