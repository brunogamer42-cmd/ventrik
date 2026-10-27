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
  bool hay_ultimo = false;
  std::uint16_t ultimo = 0;
  bool seguidos = true;
  bool hay_llegado = false;
  std::uint16_t llegado = 0;
  std::array<std::int16_t, 480> sonido{};
  std::array<std::int16_t, 480> salida{};
  while (entrada.size() >= 5) {
    PaqueteVoz paquete;
    paquete.hablante = entrada[0] & 7u;
    paquete.numero = static_cast<std::uint16_t>(entrada[1] | (entrada[2] << 8));
    paquete.tiempo = static_cast<std::uint16_t>(paquete.numero * 20 + entrada[0]);
    paquete.nivel = static_cast<std::uint8_t>(entrada[3] % (ventrik::protocolo::kNivelMaximo + 1));
    const std::size_t cuanto = std::min<std::size_t>(entrada[3], entrada.size() - 5);
    paquete.largo = static_cast<std::uint16_t>(cuanto);
    const auto opus = entrada.subspan(5, cuanto);
    std::copy(opus.begin(), opus.end(), paquete.opus.begin());
    if ((entrada[4] & 4) != 0) {
      canal.recibir(opus, ventrik::cliente::CanalVoz::Reloj::now());
      exigir(puente.perdidos <= 50u * puente.recibidos);
    } else if ((entrada[4] & 1) != 0) {
      const bool solo = (entrada[4] & 16) != 0;
      motor.solo_en_el_canal(solo);
      motor.por_voz((entrada[4] & 32) != 0);
      motor.sensibilidad(entrada[0] >> 1);
      const bool sin_microfono = (entrada[4] & 64) != 0;
      motor.silenciar(sin_microfono, sin_microfono && (entrada[4] & 128) != 0);
      motor.volumenes(entrada[1] % 201, entrada[2] % 201);
      puente.bajada.poner(paquete);
      std::transform(opus.begin(), opus.begin() + std::min(opus.size(), sonido.size()),
                     sonido.begin(), [](std::uint8_t valor) { return static_cast<std::int16_t>(valor << 7); });
      motor.procesar(sonido, salida);
      exigir(!sin_microfono || !motor.transmitiendo());
      exigir(motor.objetivo_ms() >= 40 && motor.objetivo_ms() <= 200);
      auto ids = puente.quienes();
      std::sort(ids.begin(), ids.end());
      exigir(std::adjacent_find(ids.begin(), ids.end(), [](std::uint32_t uno, std::uint32_t otro) {
               return uno != 0 && uno == otro;
             }) == ids.end());
      PaqueteVoz salido;
      while (puente.subida.sacar(salido)) {
        exigir(!solo && !sin_microfono && salido.largo > 2 && salido.largo <= salido.opus.size() &&
               salido.nivel <= ventrik::protocolo::kNivelMaximo);
      }
    } else {
      if (hay_ultimo && static_cast<std::int16_t>(static_cast<std::uint16_t>(paquete.numero - ultimo)) <= -64) {
        hay_anterior = false;
      }
      buffer.esperar(entrada[0] % 12u);
      if ((entrada[4] & 8) == 0) {
        buffer.llegar(paquete);
      }
      if ((entrada[4] & 8) == 0 && cuanto > 0) {
        seguidos = seguidos && (!hay_llegado || paquete.numero == static_cast<std::uint16_t>(llegado + 1));
        hay_llegado = true;
        llegado = paquete.numero;
      }
      PaqueteVoz sacado;
      const auto resultado = buffer.sacar(sacado);
      if (resultado != BufferVoz::Salida::nada) {
        hay_ultimo = true;
        ultimo = sacado.numero;
      }
      if (resultado == BufferVoz::Salida::paquete) {
        exigir(sacado.largo > 0 && sacado.largo <= sacado.opus.size());
        exigir(!hay_anterior || sacado.numero != anterior);
        hay_anterior = true;
        anterior = sacado.numero;
      }
      exigir(buffer.cantidad() <= BufferVoz::kTope);
      exigir(!seguidos || (buffer.faltas() == 0 && buffer.recortes() == 0));
      if ((entrada[4] & 2) != 0) {
        buffer.reiniciar();
        hay_anterior = false;
        hay_ultimo = false;
      }
    }
    entrada = entrada.subspan(5 + cuanto);
  }
  return 0;
}
