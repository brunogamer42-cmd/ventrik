#include "common/protocol/negociacion.h"

#include <algorithm>

#include <gsl/gsl>

namespace ventrik::protocolo {

namespace {

VersionPrograma primera_version_del_protocolo(std::uint16_t protocolo) {
  const std::size_t conocidos = std::size(kPrimeraVersionDelProtocolo);
  return gsl::at(kPrimeraVersionDelProtocolo,
                 static_cast<gsl::index>(std::min<std::size_t>(protocolo, conocidos)) - 1);
}

}  // namespace

std::variant<Bienvenida, Rechazo> negociar(const Hola& hola, const ConfigServidor& servidor) {
  Expects(servidor.protocolo_minimo > 0 && servidor.protocolo_minimo <= servidor.protocolo_maximo);

  if (hola.protocolo_minimo == 0 || hola.protocolo_minimo > hola.protocolo_maximo) {
    return Rechazo{MotivoRechazo::saludo_invalido, {}};
  }
  const VersionPrograma pedida =
      std::max(servidor.programa_minimo, primera_version_del_protocolo(servidor.protocolo_minimo));
  if (hola.protocolo_maximo < servidor.protocolo_minimo || hola.programa < pedida) {
    return Rechazo{MotivoRechazo::programa_viejo, pedida};
  }
  if (hola.protocolo_minimo > servidor.protocolo_maximo) {
    return Rechazo{MotivoRechazo::servidor_viejo, {}};
  }
  return Bienvenida{std::min(hola.protocolo_maximo, servidor.protocolo_maximo),
                    hola.funciones & servidor.funciones};
}

bool aceptar_bienvenida(const Hola& enviado, const Bienvenida& recibida) {
  return recibida.protocolo >= enviado.protocolo_minimo &&
         recibida.protocolo <= enviado.protocolo_maximo &&
         (recibida.funciones & ~enviado.funciones) == 0;
}

bool aceptar_rechazo(const Hola& enviado, const Rechazo& recibido) {
  return recibido.motivo != MotivoRechazo::programa_viejo ||
         enviado.programa < recibido.version_minima;
}

}  // namespace ventrik::protocolo
