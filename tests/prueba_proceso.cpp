#include <chrono>
#include <thread>

#include <windows.h>

#include "client/diagnostico.h"

#include "common/proceso.h"
#include <doctest/doctest.h>

namespace ventrik::pruebas {
extern CargaDeDll carga_de_dll_al_arrancar;  // tests/main.cpp
}  // namespace ventrik::pruebas

TEST_CASE("DEP queda activo después de activarlo") {
  ventrik::activar_dep();
  CHECK(ventrik::dep_activo());
}

TEST_CASE("al arrancar, la búsqueda de DLL queda protegida") {
  const ventrik::CargaDeDll modo = ventrik::pruebas::carga_de_dll_al_arrancar;
  CHECK(modo != ventrik::CargaDeDll::sin_cambios);
#if defined(VENTRIK_MODERNA)
  // Windows 10 y 11 permiten siempre el modo más estricto.
  CHECK(modo == ventrik::CargaDeDll::solo_carpeta_de_sistema);
#endif
}

TEST_CASE("con la búsqueda de DLL protegida, las de Windows siguen cargando") {
  // version.dll no está cargada de antes: se busca y está en la carpeta de sistema.
  const HMODULE version = LoadLibraryW(L"version.dll");
  CHECK(version != nullptr);
  if (version != nullptr) {
    FreeLibrary(version);
  }
}

TEST_CASE("esta PC tiene un Windows que Ventrik admite") {
  CHECK(ventrik::windows_es_compatible());
}

TEST_CASE("el diagnóstico separa el procesador de la ventana del resto del programa") {
  ventrik::cliente::Diagnostico diagnostico;
  const auto gastar = [] {
    const auto hasta = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (std::chrono::steady_clock::now() < hasta) {
    }
  };
  std::thread otro(gastar);
  gastar();
  otro.join();
  const ventrik::cliente::Medicion medicion = diagnostico.medir();
  CHECK(medicion.ventana > 10);
  CHECK(medicion.procesador - medicion.ventana > 10);
  CHECK(medicion.sistema > 0);
}
