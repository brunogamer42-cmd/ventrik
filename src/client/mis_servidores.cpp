#include "client/mis_servidores.h"

#include <algorithm>
#include <cwchar>

#include "common/archivos.h"
#include "common/cifrado/boveda.h"
#include "common/cifrado/identidad.h"
#include "common/red/red.h"
#include "common/utf.h"

namespace ventrik::cliente {
namespace {
constexpr std::size_t kMaximoIntentosDeCarpeta = 99;
constexpr DWORD kInicioExtendido = 0x00080000;
constexpr DWORD_PTR kListaDeManijas = 0x00020002;

using Inicializar = BOOL(WINAPI*)(void*, DWORD, DWORD, SIZE_T*);
using Actualizar = BOOL(WINAPI*)(void*, DWORD, DWORD_PTR, void*, SIZE_T, void*, SIZE_T*);
using Soltar = void(WINAPI*)(void*);

struct InicioExtendido {
  STARTUPINFOW inicio;
  void* atributos;
};

bool existe(const std::wstring& ruta) { return GetFileAttributesW(ruta.c_str()) != INVALID_FILE_ATTRIBUTES; }

std::wstring entre_comillas(const std::wstring& texto) { return L"\"" + texto + L"\""; }

bool es_nombre_reservado(const std::wstring& nombre) {
  const std::wstring base = nombre.substr(0, nombre.find(L'.'));
  for (const wchar_t* reservado : {L"CON", L"PRN", L"AUX", L"NUL"}) {
    if (_wcsicmp(base.c_str(), reservado) == 0) {
      return true;
    }
  }
  return base.size() == 4 && (_wcsnicmp(base.c_str(), L"COM", 3) == 0 || _wcsnicmp(base.c_str(), L"LPT", 3) == 0) &&
         base[3] >= L'1' && base[3] <= L'9';
}

std::wstring nombre_de_carpeta(std::string_view nombre) {
  std::wstring limpio = a_utf16(nombre);
  std::replace_if(
      limpio.begin(), limpio.end(),
      [](wchar_t letra) { return letra < 0x20 || std::wstring_view(L"<>:\"/\\|?*").find(letra) != std::wstring_view::npos; },
      L'_');
  while (!limpio.empty() && (limpio.back() == L'.' || limpio.back() == L' ')) {
    limpio.pop_back();
  }
  if (limpio.empty()) {
    limpio = L"Ventrik";
  }
  return es_nombre_reservado(limpio) ? L"_" + limpio : limpio;
}

}

std::optional<ProcesoDelServidor> ProcesoDelServidor::lanzar(const std::wstring& exe,
                                                             const std::vector<std::wstring>& argumentos,
                                                             gsl::span<const std::uint8_t> entrada,
                                                             const std::wstring& carpeta) {
  HANDLE leer = nullptr;
  HANDLE escribir = nullptr;
  if (entrada.size() > protocolo::kMaximoEntradaPorCano || CreatePipe(&leer, &escribir, nullptr, 65536) == 0) {
    return std::nullopt;
  }
  const auto cerrar = [](HANDLE manija) {
    if (manija != nullptr) {
      CloseHandle(manija);
    }
  };
  DWORD escritos = 0;
  const bool bien = SetHandleInformation(leer, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) != 0 &&
                    (entrada.empty() || (WriteFile(escribir, entrada.data(), static_cast<DWORD>(entrada.size()),
                                                   &escritos, nullptr) != 0 &&
                                         escritos == entrada.size()));
  cerrar(escribir);
  std::wstring linea = entre_comillas(exe);
  for (const std::wstring& argumento : argumentos) {
    linea += L" " + entre_comillas(argumento);
  }
  InicioExtendido inicio = {};
  inicio.inicio.cb = sizeof(inicio.inicio);
  inicio.inicio.dwFlags = STARTF_USESTDHANDLES;
  inicio.inicio.hStdInput = leer;
  const HMODULE nucleo = GetModuleHandleW(L"kernel32.dll");
  const auto inicializar = reinterpret_cast<Inicializar>(GetProcAddress(nucleo, "InitializeProcThreadAttributeList"));
  const auto actualizar = reinterpret_cast<Actualizar>(GetProcAddress(nucleo, "UpdateProcThreadAttribute"));
  const auto soltar = reinterpret_cast<Soltar>(GetProcAddress(nucleo, "DeleteProcThreadAttributeList"));
  SIZE_T tamano = 0;
  std::vector<std::uint8_t> lista;
  if (inicializar != nullptr && actualizar != nullptr && soltar != nullptr) {
    inicializar(nullptr, 1, 0, &tamano);
    lista.resize(tamano);
    if (tamano > 0 && inicializar(lista.data(), 1, 0, &tamano) != 0) {
      inicio.atributos = lista.data();
      if (actualizar(lista.data(), 0, kListaDeManijas, &leer, sizeof(leer), nullptr, nullptr) != 0) {
        inicio.inicio.cb = sizeof(inicio);
      }
    }
  }
  PROCESS_INFORMATION info = {};
  const bool extendido = inicio.inicio.cb == sizeof(inicio);
  const bool lanzado = bien && CreateProcessW(exe.c_str(), &linea[0], nullptr, nullptr, TRUE,
                                              CREATE_NO_WINDOW | (extendido ? kInicioExtendido : 0), nullptr,
                                              carpeta.empty() ? nullptr : carpeta.c_str(), &inicio.inicio,
                                              &info) != 0;
  if (inicio.atributos != nullptr) {
    soltar(inicio.atributos);
  }
  cerrar(leer);
  if (!lanzado) {
    return std::nullopt;
  }
  CloseHandle(info.hThread);
  return ProcesoDelServidor(info.hProcess);
}

std::optional<protocolo::SalidaServidor> ProcesoDelServidor::salida() const {
  DWORD codigo = 0;
  if (WaitForSingleObject(proceso_.get(), 0) != WAIT_OBJECT_0 || GetExitCodeProcess(proceso_.get(), &codigo) == 0) {
    return std::nullopt;
  }
  return codigo <= static_cast<DWORD>(protocolo::SalidaServidor::contrasena) ? static_cast<protocolo::SalidaServidor>(codigo)
                                                                              : protocolo::SalidaServidor::error;
}

std::wstring exe_del_servidor() {
  std::vector<wchar_t> ruta(MAX_PATH);
  DWORD largo = 0;
  while ((largo = GetModuleFileNameW(nullptr, ruta.data(), static_cast<DWORD>(ruta.size()))) == ruta.size() &&
         ruta.size() < 32768) {
    ruta.resize(ruta.size() * 2);
  }
  const std::wstring propio(ruta.data(), largo);
  const std::size_t barra = propio.find_last_of(L"\\/");
  const std::wstring exe =
      (barra == std::wstring::npos ? std::wstring() : propio.substr(0, barra + 1)) + L"ventrik_servidor.exe";
  return largo > 0 && existe(exe) ? exe : std::wstring();
}

std::wstring carpeta_de_mis_servidores(const std::wstring& carpeta_del_programa, bool propia) {
  return ruta_completa(propia ? carpeta_de_datos(L"Servidores") : carpeta_del_programa + L"\\Servidores");
}

std::vector<MiServidorEnDisco> leer_mis_servidores(const std::wstring& raiz) {
  std::vector<MiServidorEnDisco> lista;
  WIN32_FIND_DATAW encontrado = {};
  const HANDLE busqueda = FindFirstFileW((raiz + L"\\*").c_str(), &encontrado);
  if (busqueda == INVALID_HANDLE_VALUE) {
    return lista;
  }
  do {
    const std::wstring nombre = encontrado.cFileName;
    const std::wstring carpeta = raiz + L"\\" + nombre;
    if ((encontrado.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || nombre == L"." || nombre == L".." ||
        !existe(carpeta + cifrado::kArchivoBoveda)) {
      continue;
    }
    MiServidorEnDisco mio{carpeta, a_utf8(nombre), red::kPuertoPredeterminado};
    cifrado::Secreto leido;
    const auto publicos = leer_archivo(carpeta + protocolo::kArchivoPublico, protocolo::kMaximoDatosPublicos, leido) ==
                                  LecturaArchivo::leido
                              ? protocolo::leer_datos_publicos(leido.bytes())
                              : std::nullopt;
    if (publicos) {
      mio.nombre = publicos->nombre;
      mio.puerto = publicos->puerto;
      mio.huella = publicos->huella;
      cifrado::Secreto exportada;
      mio.exportada =
          leer_archivo(carpeta + protocolo::kArchivoExportada, publicos->huella.size(), exportada) ==
              LecturaArchivo::leido &&
          std::equal(exportada.bytes().begin(), exportada.bytes().end(), publicos->huella.begin(), publicos->huella.end());
    }
    lista.push_back(std::move(mio));
  } while (lista.size() < kMaximoMisServidores && FindNextFileW(busqueda, &encontrado) != 0);
  FindClose(busqueda);
  std::sort(lista.begin(), lista.end(),
            [](const MiServidorEnDisco& a, const MiServidorEnDisco& b) { return a.carpeta < b.carpeta; });
  return lista;
}

std::wstring carpeta_nueva(const std::wstring& raiz, std::string_view nombre) {
  const std::wstring base = raiz + L"\\" + nombre_de_carpeta(nombre);
  if (!crear_carpetas(raiz)) {
    return {};
  }
  for (std::size_t intento = 1; intento <= kMaximoIntentosDeCarpeta; ++intento) {
    const std::wstring carpeta = intento == 1 ? base : base + L" (" + std::to_wstring(intento) + L")";
    if (CreateDirectoryW(carpeta.c_str(), nullptr) != 0) {
      return carpeta;
    }
  }
  return {};
}

void borrar_carpeta_vacia(const std::wstring& carpeta) {
  DeleteFileW((carpeta + protocolo::kArchivoRegistro).c_str());
  DeleteFileW((carpeta + protocolo::kArchivoRegistro + L".anterior").c_str());
  RemoveDirectoryW(carpeta.c_str());
}

bool esta_andando(const std::wstring& carpeta) {
  const HANDLE evento = OpenEventW(SYNCHRONIZE, FALSE, protocolo::evento_de(carpeta).c_str());
  if (evento == nullptr) {
    return GetLastError() == ERROR_ACCESS_DENIED;
  }
  CloseHandle(evento);
  return true;
}

bool detener_servidor(const std::wstring& carpeta) {
  const HANDLE evento = OpenEventW(EVENT_MODIFY_STATE, FALSE, protocolo::evento_de(carpeta).c_str());
  if (evento == nullptr) {
    return false;
  }
  const bool hecho = SetEvent(evento) != 0;
  CloseHandle(evento);
  return hecho;
}

bool pide_arranque(const std::wstring& carpeta) {
  cifrado::Secreto leido;
  return leer_archivo(carpeta + cifrado::kArchivoBoveda, cifrado::kMaximoArchivoBoveda, leido) ==
             LecturaArchivo::leido &&
         cifrado::pide_contrasena_de_arranque(leido.bytes());
}

}
