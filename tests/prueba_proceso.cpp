#include <chrono>
#include <cstdint>
#include <thread>

#include <malloc.h>
#include <windows.h>

#include "client/diagnostico.h"

#include "common/proceso.h"
#include <doctest/doctest.h>

namespace ventrik::pruebas {
extern CargaDeDll carga_de_dll_al_arrancar;
}

TEST_CASE("DEP queda activo después de activarlo") {
  ventrik::activar_dep();
  CHECK(ventrik::dep_activo());
}

TEST_CASE("al arrancar, la búsqueda de DLL queda protegida") {
  const ventrik::CargaDeDll modo = ventrik::pruebas::carga_de_dll_al_arrancar;
  CHECK(modo != ventrik::CargaDeDll::sin_cambios);
#if defined(VENTRIK_MODERNA)
  CHECK(modo == ventrik::CargaDeDll::solo_carpeta_de_sistema);
#endif
}

TEST_CASE("con la búsqueda de DLL protegida, las de Windows siguen cargando") {
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

TEST_CASE("los montones de memoria del proceso quedan con baja fragmentación") {
  ventrik::proteger_el_monton();
  for (const HANDLE monton : {GetProcessHeap(), reinterpret_cast<HANDLE>(_get_heap_handle())}) {
    ULONG modo = 0;
    CHECK(HeapQueryInformation(monton, HeapCompatibilityInformation, &modo, sizeof(modo), nullptr));
    CHECK(modo == 2);
  }
}

TEST_CASE("con memoria de sobra, Windows no avisa que queda poca") {
  MEMORYSTATUSEX estado = {};
  estado.dwLength = sizeof(estado);
  REQUIRE(GlobalMemoryStatusEx(&estado));
  if (estado.ullAvailPhys > (std::uint64_t{256} << 20)) {
    CHECK_FALSE(ventrik::queda_poca_memoria());
    CHECK_FALSE(ventrik::queda_poca_memoria());
  }
}
