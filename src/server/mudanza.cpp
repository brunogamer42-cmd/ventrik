#include "server/mudanza.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

#include <windows.h>

#include <gsl/gsl>

#include "common/archivos.h"
#include "common/protocol/pedidos.h"
#include "common/utf.h"
#include "server/base.h"
#include "server/contrasena.h"
#include "server/respaldo.h"

namespace ventrik::servidor {
namespace {

constexpr wchar_t kArchivoExportando[] = L"\\exportando.db";
constexpr wchar_t kArchivoImportando[] = L"\\importando.db";
constexpr const wchar_t* kArchivosDelServidor[] = {cifrado::kArchivoBoveda,      cifrado::kArchivoClave,
                                                   cifrado::kArchivoClaveVieja,  cifrado::kArchivoClaveNueva,
                                                   kArchivoBase,                 kArchivoCadena};

bool existe(const std::wstring& archivo) {
  return GetFileAttributesW(archivo.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool borrar_si_existe(const std::wstring& archivo) {
  return !existe(archivo) || borrar_con_ceros(archivo);
}

bool termina_en(const cifrado::Cadena& cadena, const cifrado::Huella& huella) {
  const auto primera = cadena.empty() ? std::nullopt : cifrado::huella_de_publica(cadena.front().vieja);
  return primera && cifrado::seguir_cadena(cadena, *primera, huella, {});
}

std::vector<std::uint8_t> bytes_de(const cifrado::Cadena& cadena) {
  return cadena.empty() ? std::vector<std::uint8_t>() : cifrado::escribir_cadena(cadena);
}

std::uint64_t ahora_ms() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count());
}

Mudanza de_apertura(cifrado::AperturaSobre resultado) {
  switch (resultado) {
    case cifrado::AperturaSobre::contrasena: return Mudanza::contrasena;
    case cifrado::AperturaSobre::sin_memoria: return Mudanza::sin_memoria;
    case cifrado::AperturaSobre::abierto:
    case cifrado::AperturaSobre::danado: break;
  }
  return Mudanza::respaldo_danado;
}

Mudanza de_revision(RevisionBase revision) {
  switch (revision) {
    case RevisionBase::buena: return Mudanza::hecha;
    case RevisionBase::otra_version: return Mudanza::base_nueva;
    case RevisionBase::otro_esquema: return Mudanza::otro_esquema;
    case RevisionBase::error: return Mudanza::error_base;
    case RevisionBase::danada:
    case RevisionBase::version_vieja: break;
  }
  return Mudanza::base_danada;
}

}

bool hay_servidor(const std::wstring& carpeta) {
  return existe(carpeta + cifrado::kArchivoBoveda) || existe(carpeta + cifrado::kArchivoClaveVieja);
}

bool limpiar_restos(const std::wstring& carpeta) {
  borrar_base(carpeta + kArchivoExportando);
  borrar_base(carpeta + kArchivoImportando);
  return borrar_si_existe(carpeta + cifrado::kArchivoClaveAnterior);
}

CadenaGuardada leer_cadena_guardada(const std::wstring& carpeta, const cifrado::Huella& huella) {
  const std::wstring archivo = carpeta + kArchivoCadena;
  borrar_temporales_de(archivo);
  cifrado::Secreto leido;
  const LecturaArchivo lectura = leer_archivo(archivo, protocolo::kMaximoCuerpoCadena, leido);
  if (lectura == LecturaArchivo::no_existe) {
    return {};
  }
  auto cadena = lectura == LecturaArchivo::leido ? cifrado::leer_cadena(leido.bytes()) : std::nullopt;
  if (!cadena) {
    return {Mudanza::cadena_danada, {}};
  }
  const auto ultima_vieja = cifrado::huella_de_publica(cadena->back().vieja);
  if (ultima_vieja && *ultima_vieja == huella && existe(carpeta + cifrado::kArchivoClaveNueva)) {
    cadena->pop_back();
  }
  if (cadena->empty()) {
    return {};
  }
  if (!termina_en(*cadena, huella)) {
    return {Mudanza::cadena_de_otra_clave, {}};
  }
  return {Mudanza::hecha, std::move(*cadena)};
}

Mudanza revisar_base_importada(const std::wstring& archivo, const std::string& nombre_general,
                               cifrado::Secreto clave_historial) {
  RevisionBase revision = revisar_base(archivo);
  if (revision == RevisionBase::version_vieja) {
    const bool migrada = Base::abrir(archivo, nombre_general) != nullptr;
    revision = migrada ? revisar_base(archivo) : RevisionBase::danada;
  }
  if (revision != RevisionBase::buena) {
    return de_revision(revision);
  }
  const auto base = Base::abrir(archivo, nombre_general, std::move(clave_historial));
  const std::string contrasena = base ? base->texto_guardado("contrasena") : std::string();
  const bool bien = base && !base->canales().empty() && base->usuarios(ahora_ms()) &&
                    (contrasena.empty() || (leer_contrasena(contrasena) && base->guardar_texto("rehacer_contrasena", "1")));
  return bien ? Mudanza::hecha : Mudanza::base_danada;
}

Mudanza exportar_servidor(const std::wstring& carpeta, const cifrado::IdentidadServidor& identidad,
                          const cifrado::Boveda& boveda, const std::wstring& archivo, std::string_view contrasena,
                          std::optional<cifrado::Argon2> fijos) {
  if (cantidad_de_letras(contrasena) < cifrado::kMinimoContrasena) {
    return Mudanza::contrasena_corta;
  }
  if (existe(archivo)) {
    return Mudanza::archivo_existe;
  }
  const auto clave = cifrado::cerrar_clave(identidad, boveda);
  if (!clave) {
    return Mudanza::error_cifrado;
  }
  const std::wstring copia = carpeta + kArchivoExportando;
  borrar_base(copia);
  const auto borrar = gsl::finally([&copia] { borrar_base(copia); });
  cifrado::Secreto base;
  const LecturaArchivo lectura = copiar_base(carpeta + kArchivoBase, copia)
                                     ? leer_archivo(copia, kMaximoBaseRespaldo, base)
                                     : LecturaArchivo::error;
  if (lectura != LecturaArchivo::leido) {
    return lectura == LecturaArchivo::demasiado_grande ? Mudanza::base_grande : Mudanza::error_base;
  }
  const auto cadena = bytes_de(leer_cadena_guardada(carpeta, identidad.huella()).cadena);
  const auto respaldo = armar_respaldo({*clave, boveda.bytes(), base.bytes(), cadena}, contrasena, fijos);
  if (!respaldo) {
    return Mudanza::error_cifrado;
  }
  const EscrituraNueva escrita = escribir_archivo_nuevo(archivo, *respaldo);
  return escrita == EscrituraNueva::escrito     ? Mudanza::hecha
         : escrita == EscrituraNueva::ya_existe ? Mudanza::archivo_existe
                                                : Mudanza::error_escritura;
}

bool se_puede_importar(const std::wstring& carpeta) {
  return std::none_of(std::begin(kArchivosDelServidor), std::end(kArchivosDelServidor),
                      [&carpeta](const wchar_t* nombre) { return existe(carpeta + nombre); });
}

ResultadoMudanza importar_servidor(const std::wstring& carpeta, const std::wstring& archivo,
                                   std::string_view contrasena, const std::string& nombre_general,
                                   std::optional<std::string_view> arranque) {
  const std::wstring base_final = carpeta + kArchivoBase;
  const std::wstring ruta_boveda = carpeta + cifrado::kArchivoBoveda;
  const std::wstring ruta_clave = carpeta + cifrado::kArchivoClave;
  const std::wstring ruta_cadena = carpeta + kArchivoCadena;
  if (!se_puede_importar(carpeta)) {
    return {Mudanza::ya_hay_servidor};
  }
  cifrado::Secreto leido;
  const LecturaArchivo lectura = leer_archivo(archivo, kMaximoArchivoRespaldo, leido);
  if (lectura != LecturaArchivo::leido) {
    return {lectura == LecturaArchivo::demasiado_grande ? Mudanza::respaldo_danado : Mudanza::error_lectura};
  }
  const RespaldoAbierto abierto = abrir_respaldo(leido.bytes(), contrasena);
  if (!abierto.contenido) {
    return {de_apertura(abierto.resultado)};
  }
  const PartesRespaldo& partes = abierto.partes;
  const cifrado::Boveda boveda(cifrado::copiar(partes.boveda));
  const auto identidad = cifrado::abrir_clave(partes.clave, boveda);
  const auto cadena = partes.cadena.empty() ? std::optional<cifrado::Cadena>(cifrado::Cadena())
                                            : cifrado::leer_cadena(partes.cadena);
  auto clave_historial = boveda.derivar(cifrado::kEtiquetaHistorial);
  const auto archivo_boveda = boveda.armar_archivo(arranque);
  const auto archivo_clave = identidad ? cifrado::cerrar_clave(*identidad, boveda) : std::nullopt;
  if (!identidad || !cadena || (!cadena->empty() && !termina_en(*cadena, identidad->huella())) || !clave_historial) {
    return {Mudanza::respaldo_danado};
  }
  if (!archivo_boveda || !archivo_clave) {
    return {Mudanza::error_cifrado};
  }
  const std::wstring temporal = carpeta + kArchivoImportando;
  borrar_base(temporal);
  const auto limpiar = gsl::finally([&temporal] { borrar_base(temporal); });
  if (escribir_archivo_nuevo(temporal, partes.base) != EscrituraNueva::escrito) {
    return {Mudanza::error_escritura};
  }
  const Mudanza revisada = revisar_base_importada(temporal, nombre_general, std::move(*clave_historial));
  if (revisada != Mudanza::hecha) {
    return {revisada};
  }
  std::vector<std::wstring> escritos;
  const auto deshacer = gsl::finally([&escritos] {
    for (auto ruta = escritos.rbegin(); ruta != escritos.rend(); ++ruta) {
      borrar_con_ceros(*ruta);
    }
  });
  const auto escribir = [&escritos](const std::wstring& ruta, gsl::span<const std::uint8_t> bytes) {
    const bool bien = !bytes.empty() && escribir_archivo_nuevo(ruta, bytes) == EscrituraNueva::escrito;
    if (bien) {
      escritos.push_back(ruta);
    }
    return bien;
  };
  for (const wchar_t* sufijo : {L"-wal", L"-shm", L"-journal"}) {
    DeleteFileW((base_final + sufijo).c_str());
  }
  if (existe(temporal + L"-wal") || MoveFileExW(temporal.c_str(), base_final.c_str(), MOVEFILE_WRITE_THROUGH) == 0) {
    return {Mudanza::error_escritura};
  }
  escritos.push_back(base_final);
  if ((!cadena->empty() && !escribir(ruta_cadena, bytes_de(*cadena))) || !escribir(ruta_clave, *archivo_clave) ||
      !escribir(ruta_boveda, *archivo_boveda)) {
    return {Mudanza::error_escritura};
  }
  escritos.clear();
  return {Mudanza::hecha, identidad->huella()};
}

ResultadoMudanza cambiar_clave(const std::wstring& carpeta, const cifrado::IdentidadServidor& actual,
                               const cifrado::Boveda& boveda) {
  const Candado solo_uno(carpeta + kArchivoCambioDeClave);
  if (!solo_uno.tomado()) {
    return {Mudanza::otro_cambio};
  }
  if (existe(carpeta + cifrado::kArchivoClaveVieja)) {
    return {Mudanza::clave_vieja_sin_borrar};
  }
  CadenaGuardada guardada = leer_cadena_guardada(carpeta, actual.huella());
  if (guardada.resultado != Mudanza::hecha) {
    return {guardada.resultado};
  }
  const auto anterior = bytes_de(guardada.cadena);
  const std::uint32_t numero = guardada.cadena.empty() ? 1 : guardada.cadena.back().numero + 1;
  auto nueva = cifrado::IdentidadServidor::crear_nueva();
  auto eslabon = nueva && numero != 0 ? cifrado::firmar_eslabon(actual.firmante(), nueva->firmante().publica(), numero)
                                      : std::nullopt;
  const auto archivo_clave = nueva ? cifrado::cerrar_clave(*nueva, boveda) : std::nullopt;
  if (!eslabon || !archivo_clave || !cifrado::agregar_eslabon(guardada.cadena, std::move(*eslabon))) {
    return {Mudanza::error_cifrado};
  }
  const auto cadena = bytes_de(guardada.cadena);
  const std::wstring ruta_cadena = carpeta + kArchivoCadena;
  const std::wstring ruta_clave = carpeta + cifrado::kArchivoClave;
  const std::wstring ruta_nueva = carpeta + cifrado::kArchivoClaveNueva;
  const std::wstring apartada = carpeta + cifrado::kArchivoClaveAnterior;
  if (cadena.empty() || !borrar_si_existe(apartada) || !escribir_archivo_completo(ruta_nueva, *archivo_clave)) {
    return {Mudanza::error_escritura};
  }
  if (!escribir_archivo_completo(ruta_cadena, cadena)) {
    borrar_con_ceros(ruta_nueva);
    return {Mudanza::error_escritura};
  }
  if (MoveFileExW(ruta_clave.c_str(), apartada.c_str(), MOVEFILE_WRITE_THROUGH) == 0 ||
      MoveFileExW(ruta_nueva.c_str(), ruta_clave.c_str(), MOVEFILE_WRITE_THROUGH) == 0) {
    MoveFileExW(apartada.c_str(), ruta_clave.c_str(), MOVEFILE_WRITE_THROUGH);
    borrar_con_ceros(ruta_nueva);
    if (anterior.empty()) {
      DeleteFileW(ruta_cadena.c_str());
    } else {
      escribir_archivo_completo(ruta_cadena, anterior);
    }
    return {Mudanza::error_escritura};
  }
  return {Mudanza::hecha, nueva->huella(), !borrar_con_ceros(apartada)};
}

}
