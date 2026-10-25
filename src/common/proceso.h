#pragma once

namespace ventrik {

void activar_dep();

bool dep_activo();

enum class CargaDeDll {
  solo_carpeta_de_sistema,
  sin_carpeta_actual,
  sin_cambios,
};

CargaDeDll endurecer_carga_de_dll();

bool windows_es_compatible();

void proteger_el_monton();

bool queda_poca_memoria();

}
