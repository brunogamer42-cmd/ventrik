#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>

#include <gsl/gsl>

#include "client/buffer_voz.h"
#include "client/canal_voz.h"
#include "client/motor_voz.h"
#include "common/red/red.h"

using ventrik::cliente::BufferVoz;
using ventrik::cliente::PaqueteVoz;

namespace {
void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

std::unique_ptr<ventrik::red::Inicio> red;
}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  red = std::make_unique<ventrik::red::Inicio>();
  exigir(red->ok());
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  auto entrada = gsl::make_span(datos, largo);
  if (entrada.empty()) {
    return 0;
  }
  ventrik::cliente::PuenteVoz puente;
  ventrik::cliente::MotorVoz motor(
      puente, (entrada[0] & 1) != 0 ? ventrik::cliente::Calidad{} : ventrik::cliente::Calidad{16000, 0, 20});
  exigir(motor.ok());
  motor.transmitir((entrada[0] & 2) != 0);
  entrada = entrada.subspan(1);
  BufferVoz buffer;
  ventrik::cliente::CanalVoz canal(ventrik::red::Direccion{}, puente);
  bool hay_anterior = false;
  std::uint16_t anterior = 0;
  std::array<std::int16_t, 480> sonido{};
  std::array<std::int16_t, 480> salida{};
  while (entrada.size() >= 5) {
    PaqueteVoz paquete;
    paquete.hablante = entrada[0] & 3u;
    paquete.numero = static_cast<std::uint16_t>(entrada[1] | (entrada[2] << 8));
    const std::size_t cuanto = std::min<std::size_t>(entrada[3], entrada.size() - 5);
    paquete.largo = static_cast<std::uint16_t>(cuanto);
    const auto opus = entrada.subspan(5, cuanto);
    std::copy(opus.begin(), opus.end(), paquete.opus.begin());
    if ((entrada[4] & 4) != 0) {
      canal.recibir(opus, ventrik::cliente::CanalVoz::Reloj::now());
      exigir(puente.perdidos <= 50u * puente.recibidos);
    } else if ((entrada[4] & 1) != 0) {
      puente.bajada.poner(paquete);
      std::transform(opus.begin(), opus.begin() + std::min(opus.size(), sonido.size()),
                     sonido.begin(), [](std::uint8_t valor) { return static_cast<std::int16_t>(valor << 7); });
      motor.procesar(sonido, salida);
    } else {
      buffer.llegar(paquete);
      PaqueteVoz sacado;
      if (buffer.sacar(sacado) == BufferVoz::Salida::paquete) {
        exigir(sacado.largo > 0 && sacado.largo <= sacado.opus.size());
        exigir(!hay_anterior || sacado.numero != anterior);
        hay_anterior = true;
        anterior = sacado.numero;
      }
      exigir(buffer.cantidad() <= 6);
      if ((entrada[4] & 2) != 0) {
        buffer.reiniciar();
        hay_anterior = false;
      }
    }
    entrada = entrada.subspan(5 + cuanto);
  }
  PaqueteVoz sobrante;
  while (puente.subida.sacar(sobrante)) {
    exigir(sobrante.largo > 0 && sobrante.largo <= sobrante.opus.size());
  }
  return 0;
}
