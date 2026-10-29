#pragma once

#include <string>

#include "common/niveles.h"
#include "common/protocol/hostear.h"
#include "server/base.h"

namespace ventrik::servidor {

enum class Creado { hecho, carpeta_ocupada, datos_rotos, error_escritura, error_cifrado, error_base };

bool guardar_seguridad(Base& base, const Seguridad& seguridad);
Creado crear_servidor(const std::wstring& carpeta, const protocolo::ServidorNuevo& nuevo,
                      const std::string& nombre_general);

}
