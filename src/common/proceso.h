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

}  // namespace ventrik
