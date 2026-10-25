#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "common/cifrado/inicio.h"
#include "common/proceso.h"
#include "common/red/red.h"

namespace ventrik::pruebas {

CargaDeDll carga_de_dll_al_arrancar = CargaDeDll::sin_cambios;

}

int main(int cantidad, char** argumentos) {
  ventrik::pruebas::carga_de_dll_al_arrancar = ventrik::endurecer_carga_de_dll();
  ventrik::activar_dep();
  ventrik::proteger_el_monton();
  const ventrik::cifrado::Inicio cifrado;
  const ventrik::red::Inicio red;
  if (!cifrado.ok() || !red.ok()) {
    return 1;
  }

  doctest::Context contexto;
  contexto.applyCommandLine(cantidad, argumentos);
  return contexto.run();
}
