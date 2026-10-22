// Punto de entrada de las pruebas automáticas.
//
// Arranca con las mismas protecciones que el programa, así todas las
// pruebas (y las bibliotecas que prueban) corren con ellas puestas.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "common/cifrado/inicio.h"
#include "common/proceso.h"
#include "common/red/red.h"

namespace ventrik::pruebas {

// Lo que devolvió endurecer_carga_de_dll() al arrancar (ver prueba_proceso.cpp).
CargaDeDll carga_de_dll_al_arrancar = CargaDeDll::sin_cambios;

}  // namespace ventrik::pruebas

int main(int cantidad, char** argumentos) {
  ventrik::pruebas::carga_de_dll_al_arrancar = ventrik::endurecer_carga_de_dll();
  ventrik::activar_dep();
  const ventrik::cifrado::Inicio cifrado;
  const ventrik::red::Inicio red;
  if (!cifrado.ok() || !red.ok()) {
    return 1;
  }

  doctest::Context contexto;
  contexto.applyCommandLine(cantidad, argumentos);
  return contexto.run();
}
