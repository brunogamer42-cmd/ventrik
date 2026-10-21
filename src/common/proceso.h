#pragma once

// Protecciones del proceso que se activan al arrancar.

namespace ventrik {

// Activa DEP (protección contra ejecución de datos) sin depender de la
// configuración de Windows. Solo hace algo en 32 bits: en 64 bits Windows
// lo tiene siempre activo. SetProcessDEPPolicy existe desde XP SP3 y se
// busca con GetProcAddress por si falta.
void activar_dep();

// true si DEP está activo en este proceso.
bool dep_activo();

// Cuánto se pudo proteger la búsqueda de DLL.
enum class CargaDeDll {
  solo_carpeta_de_sistema,  // Windows 8 en adelante, o 7 con KB2533623
  sin_carpeta_actual,       // XP SP1 a 7: sale la carpeta actual, no la del .exe
  sin_cambios,
};

// Hace que las DLL que se carguen por nombre se busquen solo en la carpeta
// de sistema de Windows, nunca al lado del .exe ni en la carpeta actual,
// para que nadie pueda poner una DLL falsa ahí. Donde Windows no lo
// permite, al menos saca la carpeta actual de la búsqueda.
CargaDeDll endurecer_carga_de_dll();

// false si Windows es más viejo que lo que admite esta versión de Ventrik.
// La clásica pide XP con Service Pack 3: antes, el generador de números
// aleatorios de Windows tenía una falla conocida.
bool windows_es_compatible();

}  // namespace ventrik
