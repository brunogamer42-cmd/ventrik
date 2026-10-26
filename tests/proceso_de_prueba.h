#pragma once

#include <chrono>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <windows.h>

#include "servidor_de_prueba.h"
#include <doctest/doctest.h>

namespace prueba {

using Manija = std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)>;

inline std::wstring variable(const wchar_t* nombre) {
  std::vector<wchar_t> valor(32768);
  const DWORD largo = GetEnvironmentVariableW(nombre, valor.data(), static_cast<DWORD>(valor.size()));
  return largo < valor.size() ? std::wstring(valor.data(), largo) : std::wstring();
}

inline std::string leer_todo(const std::wstring& archivo) {
  std::ifstream entrada(archivo.c_str(), std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(entrada), std::istreambuf_iterator<char>());
}

class Proceso {
 public:
  Proceso(const std::wstring& exe, const std::vector<std::wstring>& argumentos, const std::wstring& salida = {})
      : trabajo_(CreateJobObjectW(nullptr, nullptr), &CloseHandle), proceso_(nullptr, &CloseHandle) {
    REQUIRE(trabajo_);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limites{};
    limites.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(trabajo_.get(), JobObjectExtendedLimitInformation, &limites, sizeof(limites));
    std::wstring linea = L"\"" + exe + L"\"";
    for (const auto& argumento : argumentos) {
      linea += L" \"" + argumento + L"\"";
    }
    STARTUPINFOW inicio{};
    inicio.cb = sizeof(inicio);
    SECURITY_ATTRIBUTES heredable{sizeof(heredable), nullptr, TRUE};
    Manija archivo(nullptr, &CloseHandle);
    if (!salida.empty()) {
      const HANDLE abierto = CreateFileW(salida.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         &heredable, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      REQUIRE(abierto != INVALID_HANDLE_VALUE);
      archivo.reset(abierto);
      inicio.dwFlags = STARTF_USESTDHANDLES;
      inicio.hStdOutput = abierto;
      inicio.hStdError = abierto;
    }
    PROCESS_INFORMATION info{};
    REQUIRE(CreateProcessW(nullptr, linea.data(), nullptr, nullptr, archivo ? TRUE : FALSE,
                           CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED, nullptr, nullptr, &inicio, &info));
    proceso_.reset(info.hProcess);
    AssignProcessToJobObject(trabajo_.get(), info.hProcess);
    ResumeThread(info.hThread);
    CloseHandle(info.hThread);
    id_ = info.dwProcessId;
  }
  ~Proceso() {
    if (vivo()) {
      TerminateProcess(proceso_.get(), 1);
      WaitForSingleObject(proceso_.get(), 5000);
    }
  }
  Proceso(const Proceso&) = delete;
  Proceso& operator=(const Proceso&) = delete;

  bool vivo() const { return WaitForSingleObject(proceso_.get(), 0) == WAIT_TIMEOUT; }
  bool cortar() const { return GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, id_) != 0; }

  std::optional<DWORD> esperar_salida(std::chrono::milliseconds limite) const {
    DWORD codigo = 0;
    if (WaitForSingleObject(proceso_.get(), static_cast<DWORD>(limite.count())) != WAIT_OBJECT_0 ||
        !GetExitCodeProcess(proceso_.get(), &codigo)) {
      return std::nullopt;
    }
    return codigo;
  }

  HWND ventana(std::chrono::milliseconds limite) const {
    struct Busqueda {
      DWORD id;
      HWND encontrada;
    } busqueda{id_, nullptr};
    esperar_que(
        [&] {
          EnumWindows(
              [](HWND ventana, LPARAM dato) -> BOOL {
                auto& buscada = *reinterpret_cast<Busqueda*>(dato);
                DWORD de = 0;
                GetWindowThreadProcessId(ventana, &de);
                if (de != buscada.id || !IsWindowVisible(ventana) || GetWindow(ventana, GW_OWNER)) {
                  return TRUE;
                }
                buscada.encontrada = ventana;
                return FALSE;
              },
              reinterpret_cast<LPARAM>(&busqueda));
          return busqueda.encontrada != nullptr;
        },
        limite);
    return busqueda.encontrada;
  }

 private:
  Manija trabajo_;
  Manija proceso_;
  DWORD id_ = 0;
};

inline void escribir(HWND ventana, const std::wstring& texto) {
  for (const wchar_t letra : texto) {
    PostMessageW(ventana, WM_CHAR, letra, 1);
  }
}

inline void enter(HWND ventana) {
  PostMessageW(ventana, WM_KEYDOWN, VK_RETURN, 0x001C0001);
  PostMessageW(ventana, WM_KEYUP, VK_RETURN, static_cast<LPARAM>(0xC01C0001u));
}

}
