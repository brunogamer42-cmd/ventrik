#include "common/niveles.h"

#include <algorithm>

namespace ventrik {
Opciones opciones_de(Nivel nivel) {
  const auto cual = static_cast<std::size_t>(nivel);
  return {cual < kValoresDeNivel.size() ? kValoresDeNivel[cual] : kValoresDeNivel[0]};
}

Nivel nivel_de(const Opciones& opciones) {
  const auto igual = std::find(kValoresDeNivel.begin(), kValoresDeNivel.end(), opciones.valores);
  return igual == kValoresDeNivel.end() ? Nivel::personalizado
                                        : static_cast<Nivel>(igual - kValoresDeNivel.begin());
}

int etapa_de(Opcion opcion) { return kEtapaDeOpcion[static_cast<std::size_t>(opcion)]; }

std::uint8_t valores_de(Opcion opcion) { return kValoresPorOpcion[static_cast<std::size_t>(opcion)]; }

bool es_valida(const Seguridad& seguridad) {
  for (std::size_t i = 0; i < kOpciones; ++i) {
    if (seguridad.opciones.valores[i] >= kValoresPorOpcion[i]) {
      return false;
    }
  }
  return static_cast<std::size_t>(seguridad.nivel) < kNiveles &&
         (seguridad.nivel == Nivel::personalizado || seguridad.opciones == opciones_de(seguridad.nivel));
}

void elegir_nivel(Seguridad& seguridad, Nivel nivel) {
  if (nivel != Nivel::personalizado) {
    seguridad.opciones = opciones_de(nivel);
  }
  seguridad.nivel = nivel;
}

bool cambiar_opcion(Seguridad& seguridad, Opcion opcion, std::uint8_t valor) {
  if (etapa_de(opcion) != 0 || valor >= valores_de(opcion) || seguridad.opciones[opcion] == valor) {
    return false;
  }
  seguridad.opciones[opcion] = valor;
  seguridad.nivel = nivel_de(seguridad.opciones);
  return true;
}

VersionPrograma version_minima(const Opciones& opciones) {
  return opciones[Opcion::programa_minimo] == static_cast<std::uint8_t>(Minimo::con_arreglos) ? kUltimaConArreglos
                                                                                              : VersionPrograma{};
}

std::uint32_t segundos_de_renovacion(const Opciones& opciones) {
  return kRenovarVozCada[std::min<std::size_t>(opciones[Opcion::renovar_voz], kRenovarVozCada.size() - 1)];
}

}
