#include "common/archivos.h"

#include <algorithm>
#include <atomic>
#include <string>
#include <utility>
#include <vector>

struct IUnknown;

#include <windows.h>
#include <shlobj.h>

namespace ventrik {

namespace {

constexpr std::size_t kPedazo = 1 << 20;

std::atomic<unsigned long> temporales_creados{0};

class Manejador {
 public:
  explicit Manejador(HANDLE manejador) : manejador_(manejador) {}
  ~Manejador() {
    if (valido()) {
      CloseHandle(manejador_);
    }
  }
  Manejador(const Manejador&) = delete;
  Manejador& operator=(const Manejador&) = delete;

  bool valido() const { return manejador_ != INVALID_HANDLE_VALUE && manejador_ != nullptr; }
  HANDLE get() const { return manejador_; }

 private:
  HANDLE manejador_;
};

bool escribir_todo(HANDLE archivo, gsl::span<const std::uint8_t> contenido) {
  while (!contenido.empty()) {
    DWORD escritos = 0;
    const auto pedir = static_cast<DWORD>(std::min(contenido.size(), kPedazo));
    if (!WriteFile(archivo, contenido.data(), pedir, &escritos, nullptr) || escritos == 0) {
      return false;
    }
    contenido = contenido.subspan(escritos);
  }
  return FlushFileBuffers(archivo) != 0;
}

bool es_carpeta(const std::wstring& ruta) {
  const DWORD atributos = GetFileAttributesW(ruta.c_str());
  return atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

}

LecturaArchivo leer_archivo(const std::wstring& ruta, std::size_t maximo,
                            cifrado::Secreto& contenido) {
  const Manejador archivo(CreateFileW(ruta.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!archivo.valido()) {
    const DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
               ? LecturaArchivo::no_existe
               : LecturaArchivo::error;
  }
  LARGE_INTEGER tamano = {};
  if (!GetFileSizeEx(archivo.get(), &tamano) || tamano.QuadPart < 0) {
    return LecturaArchivo::error;
  }
  if (static_cast<unsigned long long>(tamano.QuadPart) > maximo) {
    return LecturaArchivo::demasiado_grande;
  }
  cifrado::Secreto leido(static_cast<std::size_t>(tamano.QuadPart));
  auto resto = leido.bytes();
  while (!resto.empty()) {
    DWORD obtenidos = 0;
    const auto pedir = static_cast<DWORD>(std::min(resto.size(), kPedazo));
    if (!ReadFile(archivo.get(), resto.data(), pedir, &obtenidos, nullptr) || obtenidos == 0) {
      return LecturaArchivo::error;
    }
    resto = resto.subspan(obtenidos);
  }
  contenido = std::move(leido);
  return LecturaArchivo::leido;
}

namespace {

EscrituraNueva escribir_y_renombrar(const std::wstring& ruta, gsl::span<const std::uint8_t> contenido,
                                    bool reemplazar) {
  const std::wstring temporal = ruta + L"." + std::to_wstring(GetCurrentProcessId()) + L"." +
                                std::to_wstring(++temporales_creados) + L".tmp";
  bool escrito = false;
  {
    const Manejador archivo(CreateFileW(temporal.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL, nullptr));
    escrito = archivo.valido() && escribir_todo(archivo.get(), contenido);
  }
  const DWORD opciones = MOVEFILE_WRITE_THROUGH | (reemplazar ? MOVEFILE_REPLACE_EXISTING : 0);
  if (escrito && MoveFileExW(temporal.c_str(), ruta.c_str(), opciones) != 0) {
    return EscrituraNueva::escrito;
  }
  const DWORD error = GetLastError();
  DeleteFileW(temporal.c_str());
  return escrito && (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS)
             ? EscrituraNueva::ya_existe
             : EscrituraNueva::error;
}

}

bool escribir_archivo_completo(const std::wstring& ruta, gsl::span<const std::uint8_t> contenido) {
  return escribir_y_renombrar(ruta, contenido, true) == EscrituraNueva::escrito;
}

EscrituraNueva escribir_archivo_nuevo(const std::wstring& ruta,
                                      gsl::span<const std::uint8_t> contenido) {
  return escribir_y_renombrar(ruta, contenido, false);
}

void borrar_temporales_de(const std::wstring& ruta) {
  const std::size_t barra = ruta.find_last_of(L"\\/");
  const std::wstring carpeta = barra == std::wstring::npos ? std::wstring() : ruta.substr(0, barra + 1);
  WIN32_FIND_DATAW encontrado = {};
  const HANDLE busqueda = FindFirstFileW((ruta + L".*.tmp").c_str(), &encontrado);
  if (busqueda == INVALID_HANDLE_VALUE) {
    return;
  }
  do {
    if ((encontrado.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      DeleteFileW((carpeta + encontrado.cFileName).c_str());
    }
  } while (FindNextFileW(busqueda, &encontrado) != 0);
  FindClose(busqueda);
}

bool borrar_con_ceros(const std::wstring& ruta) {
  {
    const Manejador archivo(
        CreateFileW(ruta.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    LARGE_INTEGER tamano = {};
    if (archivo.valido() && GetFileSizeEx(archivo.get(), &tamano) && tamano.QuadPart > 0 &&
        static_cast<unsigned long long>(tamano.QuadPart) <= kPedazo) {
      escribir_todo(archivo.get(), std::vector<std::uint8_t>(static_cast<std::size_t>(tamano.QuadPart)));
    }
  }
  return DeleteFileW(ruta.c_str()) != 0;
}

bool crear_carpetas(const std::wstring& ruta) {
  for (std::size_t i = 0; i < ruta.size(); ++i) {
    if ((ruta[i] == L'\\' || ruta[i] == L'/') && i > 0 && ruta[i - 1] != L':' &&
        ruta[i - 1] != L'\\') {
      CreateDirectoryW(ruta.substr(0, i).c_str(), nullptr);
    }
  }
  CreateDirectoryW(ruta.c_str(), nullptr);
  return es_carpeta(ruta);
}

std::wstring carpeta_de_datos(const std::wstring& nombre) {
  wchar_t carpeta[MAX_PATH] = {};
  if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT,
                       carpeta) != S_OK) {
    return {};
  }
  return std::wstring(carpeta) + L"\\Ventrik\\" + nombre;
}

std::wstring ruta_completa(const std::wstring& ruta) {
  const DWORD necesario = GetFullPathNameW(ruta.c_str(), 0, nullptr, nullptr);
  std::vector<wchar_t> completa(necesario);
  const DWORD largo =
      necesario == 0 ? 0 : GetFullPathNameW(ruta.c_str(), necesario, completa.data(), nullptr);
  return largo == 0 || largo >= necesario ? ruta : std::wstring(completa.data(), largo);
}

}
