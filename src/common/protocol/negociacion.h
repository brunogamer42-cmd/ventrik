#pragma once

#include <cstdint>
#include <variant>

#include "common/protocol/mensajes.h"
#include "common/version.h"

namespace ventrik::protocolo {

struct ConfigServidor {
  std::uint16_t protocolo_minimo = kProtocoloMinimo;
  std::uint16_t protocolo_maximo = kProtocoloMaximo;
  VersionPrograma programa_minimo;
  Funciones funciones = 0;
};

std::variant<Bienvenida, Rechazo> negociar(const Hola& hola, const ConfigServidor& servidor);

// El programa no confía en el servidor: revisa que la respuesta sea una que pudo pedir.
bool aceptar_bienvenida(const Hola& enviado, const Bienvenida& recibida);
bool aceptar_rechazo(const Hola& enviado, const Rechazo& recibido);

}  // namespace ventrik::protocolo
