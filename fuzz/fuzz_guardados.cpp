#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <gsl/gsl>

#include "client/guardados.h"

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const gsl::span<const std::uint8_t> entrada(datos, largo);
  ventrik::cliente::Guardados guardados;
  if (ventrik::cliente::leer_guardados(entrada, guardados) != ventrik::cliente::LecturaGuardados::leidos) {
    exigir(guardados.servidores.empty() && guardados.nombre.empty() && guardados.tecla_hablar == 0 &&
           !guardados.por_voz && guardados.sensibilidad == ventrik::cliente::kSensibilidadPorDefecto &&
           guardados.silencio == 0 && guardados.volumen_microfono == ventrik::cliente::kVolumenPorDefecto &&
           guardados.volumen_salida == ventrik::cliente::kVolumenPorDefecto && guardados.ajustes.empty());
    return 0;
  }
  const std::vector<std::uint8_t> armado = ventrik::cliente::armar_guardados(guardados);
  if (entrada[4] != 8) {
    ventrik::cliente::Guardados de_nuevo;
    exigir(ventrik::cliente::leer_guardados(armado, de_nuevo) == ventrik::cliente::LecturaGuardados::leidos &&
           ventrik::cliente::armar_guardados(de_nuevo) == armado);
  } else {
    exigir(armado.size() == largo && std::equal(armado.begin(), armado.end(), entrada.begin()));
  }
  for (std::size_t lugar = 0; lugar < ventrik::protocolo::kLugaresDeSusurro; ++lugar) {
    ventrik::cliente::Guardados copia = guardados;
    ventrik::cliente::poner_susurro(copia, lugar, ventrik::cliente::susurro_de(guardados, lugar));
    exigir(ventrik::cliente::armar_guardados(copia) == armado);
  }
  for (auto& servidor : guardados.servidores) {
    ventrik::cliente::cambiar_huella(
        servidor, {ventrik::cifrado::Huella(servidor.huella), ventrik::cliente::huellas_atras(servidor)});
    exigir(servidor.atras.size() <= ventrik::cliente::kMaximoHuellasAtras);
  }
  exigir(!ventrik::cliente::armar_guardados(guardados).empty());
  ventrik::cliente::recordar(guardados, ventrik::cliente::ServidorGuardado{{1, 1}, {}, "x", 0, {}, 0});
  exigir(!ventrik::cliente::armar_guardados(guardados).empty());
  return 0;
}
