#include "server/crear.h"

#include <chrono>
#include <vector>

#include <windows.h>

#include <gsl/gsl>

#include "common/archivos.h"
#include "common/cifrado/boveda.h"
#include "common/cifrado/identidad.h"
#include "server/contrasena.h"
#include "server/mudanza.h"
#include "server/respaldo.h"
#include "server/servidor.h"

namespace ventrik::servidor {
namespace {

bool vacia(const std::wstring& carpeta) {
  WIN32_FIND_DATAW encontrado = {};
  const HANDLE busqueda = FindFirstFileW((carpeta + L"\\*").c_str(), &encontrado);
  if (busqueda == INVALID_HANDLE_VALUE) {
    return GetLastError() == ERROR_FILE_NOT_FOUND;
  }
  bool sin_nada = true;
  do {
    const std::wstring_view nombre = encontrado.cFileName;
    sin_nada = sin_nada && (nombre == L"." || nombre == L"..");
  } while (sin_nada && FindNextFileW(busqueda, &encontrado) != 0);
  FindClose(busqueda);
  return sin_nada;
}

}

bool guardar_seguridad(Base& base, const Seguridad& seguridad) {
  return base.guardar_seguridad(seguridad) && aplicar_en_la_base(base, seguridad);
}

Creado crear_servidor(const std::wstring& carpeta, const protocolo::ServidorNuevo& nuevo,
                      const std::string& nombre_general) {
  const bool existia = GetFileAttributesW(carpeta.c_str()) != INVALID_FILE_ATTRIBUTES;
  if (existia && !vacia(carpeta)) {
    return Creado::carpeta_ocupada;
  }
  if (!es_valida(nuevo.seguridad) || !cifrado::huella_de_publica(nuevo.dueno)) {
    return Creado::datos_rotos;
  }
  const auto boveda = cifrado::Boveda::crear_nueva();
  const auto identidad = cifrado::IdentidadServidor::crear_nueva();
  const std::string_view arranque = cifrado::texto_de(nuevo.arranque);
  const auto archivo_boveda =
      boveda ? boveda->armar_archivo(arranque.empty() ? std::nullopt : std::optional<std::string_view>(arranque))
             : std::nullopt;
  const auto archivo_clave = boveda && identidad ? cifrado::cerrar_clave(*identidad, *boveda) : std::nullopt;
  auto clave_historial = boveda ? boveda->derivar(cifrado::kEtiquetaHistorial) : std::nullopt;
  const std::string_view contrasena = cifrado::texto_de(nuevo.contrasena);
  const auto hash = contrasena.empty() ? std::optional<std::string>(std::string()) : guardar_contrasena(contrasena);
  if (!archivo_boveda || !archivo_clave || !clave_historial || !hash) {
    return Creado::error_cifrado;
  }
  if (!crear_carpetas(carpeta)) {
    return Creado::error_escritura;
  }
  const std::wstring archivo_base = carpeta + kArchivoBase;
  bool base_creada = false;
  std::vector<std::wstring> escritos;
  const auto deshacer = gsl::finally([&] {
    for (const std::wstring& escrito : escritos) {
      borrar_con_ceros(escrito);
    }
    if (base_creada) {
      borrar_base(archivo_base);
    }
    if (!existia && (base_creada || !escritos.empty())) {
      RemoveDirectoryW(carpeta.c_str());
    }
  });
  if (GetFileAttributesW(archivo_base.c_str()) != INVALID_FILE_ATTRIBUTES) {
    return Creado::carpeta_ocupada;
  }
  base_creada = true;
  {
    const auto base = Base::abrir(archivo_base, nombre_general, std::move(*clave_historial));
    if (!base || !base->guardar_texto("nombre", nuevo.nombre) || !base->guardar_texto("contrasena", *hash) ||
        !guardar_seguridad(*base, nuevo.seguridad) ||
        !base->guardar_usuario({nuevo.dueno, nuevo.nombre_dueno, true, false, 0, 0, 0})) {
      return Creado::error_base;
    }
  }
  for (const auto& [sufijo, contenido] :
       {std::make_pair(cifrado::kArchivoClave, &*archivo_clave), std::make_pair(cifrado::kArchivoBoveda, &*archivo_boveda)}) {
    const EscrituraNueva escrita = escribir_archivo_nuevo(carpeta + sufijo, *contenido);
    if (escrita != EscrituraNueva::escrito) {
      return escrita == EscrituraNueva::ya_existe ? Creado::carpeta_ocupada : Creado::error_escritura;
    }
    escritos.push_back(carpeta + sufijo);
  }
  escritos.clear();
  base_creada = false;
  return Creado::hecho;
}

}
