#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include "client/limpieza.h"
#include "client/motor_voz.h"
#include "sonido_de_prueba.h"

using ventrik::cliente::Calidad;
using ventrik::cliente::Limpieza;
using ventrik::cliente::Limpiar;
using ventrik::cliente::MotorVoz;
using ventrik::cliente::PaqueteVoz;
using ventrik::cliente::PuenteVoz;
using ventrik::cliente::Ruido;

namespace {

std::vector<std::int16_t> ruido_blanco(std::size_t cuantas, double amplitud, unsigned semilla) {
  std::mt19937 azar(semilla);
  std::normal_distribution<double> normal(0, amplitud);
  std::vector<std::int16_t> muestras(cuantas);
  for (auto& muestra : muestras) {
    muestra = static_cast<std::int16_t>(std::clamp(normal(azar), -32768.0, 32767.0));
  }
  return muestras;
}

double decibeles(double antes, double despues) { return 10 * std::log10(despues / antes); }

double energia_de(const std::vector<std::int16_t>& muestras, std::size_t desde, std::size_t hasta) {
  double suma = 0;
  for (std::size_t i = desde; i < hasta; ++i) {
    suma += static_cast<double>(muestras[i]) * muestras[i];
  }
  return suma / static_cast<double>(hasta - desde);
}

std::vector<std::int16_t> limpiar_todo(Limpieza& limpieza, std::vector<std::int16_t> muestras) {
  const std::size_t bloque = limpieza.bloque();
  for (std::size_t desde = 0; desde + bloque <= muestras.size(); desde += bloque) {
    limpieza.limpiar(gsl::make_span(muestras).subspan(desde, bloque));
  }
  return muestras;
}

}

TEST_CASE("La cancelación de eco borra lo que sale por los parlantes y vuelve a entrar por el micrófono") {
  constexpr unsigned kFrecuencia = 48000;
  constexpr std::size_t kDemora = kFrecuencia * 40 / 1000;
  Limpieza limpieza(kFrecuencia);
  limpieza.usar({Ruido::apagado, true, false});
  const std::size_t bloque = limpieza.bloque();
  const auto parlantes = ruido_blanco(kFrecuencia * 5, 3000, 7);
  std::vector<std::int16_t> microfono(parlantes.size());
  for (std::size_t i = kDemora; i < microfono.size(); ++i) {
    microfono[i] = static_cast<std::int16_t>(parlantes[i - kDemora] / 2);
  }
  std::vector<std::int16_t> limpio = microfono;
  const auto tramo = [&](std::size_t desde, std::size_t hasta) {
    for (; desde + bloque <= hasta; desde += bloque) {
      limpieza.escuchar(gsl::make_span(parlantes).subspan(desde, bloque));
      limpieza.limpiar(gsl::make_span(limpio).subspan(desde, bloque));
    }
  };
  const auto baja_entre = [&](std::size_t desde, std::size_t hasta) {
    return decibeles(energia_de(microfono, desde, hasta), energia_de(limpio, desde, hasta));
  };
  tramo(0, kFrecuencia * 3);
  const double aprendido = baja_entre(kFrecuencia * 2, kFrecuencia * 3);
  tramo(kFrecuencia * 4, kFrecuencia * 5);
  const double al_volver = baja_entre(kFrecuencia * 4 + kFrecuencia * 3 / 10, kFrecuencia * 5);
  MESSAGE("eco: " << aprendido << " dB; al volver a hablar tras 1 s sin micrófono: " << al_volver << " dB");
  CHECK(aprendido <= -15);
  CHECK(al_volver <= -15);
}

TEST_CASE("Quitar ruido baja un ventilador y deja pasar la voz, a 48 y a 16 kHz") {
  for (const auto& [frecuencia, ruido, minimo] : {std::make_tuple(48000u, Ruido::media, 10.0),
                                                  std::make_tuple(48000u, Ruido::alta, 10.0),
                                                  std::make_tuple(16000u, Ruido::media, 6.0),
                                                  std::make_tuple(48000u, Ruido::baja, 6.0)}) {
    Limpieza limpieza(frecuencia);
    limpieza.usar({ruido, false, false});
    const auto ventilador = ruido_blanco(frecuencia * 2, 800, 3);
    const auto limpio = limpiar_todo(limpieza, ventilador);
    const double baja = decibeles(energia_de(ventilador, frecuencia, ventilador.size()),
                                  energia_de(limpio, frecuencia, limpio.size()));
    MESSAGE(frecuencia << " Hz, nivel " << static_cast<int>(ruido) << ": " << baja << " dB");
    CHECK(baja <= -minimo);
  }
}

TEST_CASE("Sin limpieza la voz pasa igual que entró") {
  Limpieza limpieza(48000);
  std::vector<std::int16_t> voz(48000);
  std::size_t muestra = 0;
  prueba::tono(voz, muestra, 48000);
  CHECK(limpiar_todo(limpieza, voz) == voz);
}

TEST_CASE("El volumen automático sube una voz que llega baja") {
  Limpieza limpieza(48000);
  limpieza.usar({Ruido::apagado, false, true});
  std::vector<std::int16_t> voz(48000 * 4);
  std::size_t muestra = 0;
  prueba::tono(voz, muestra, 48000, 300, 600);
  const auto ajustada = limpiar_todo(limpieza, voz);
  const double sube = decibeles(energia_de(voz, 48000 * 3, voz.size()), energia_de(ajustada, 48000 * 3, voz.size()));
  MESSAGE("volumen automático: " << sube << " dB");
  CHECK(sube >= 3);
}

TEST_CASE("Por voz no se abre con un ventilador si se quita el ruido, y el volumen automático no lo abre") {
  for (const auto& [limpiar, fuerza, se_abre] : {std::make_tuple(Limpiar{Ruido::apagado, false, false}, 1500.0, true),
                                                 std::make_tuple(Limpiar{Ruido::media, false, false}, 1500.0, false),
                                                 std::make_tuple(Limpiar{Ruido::media, false, true}, 1500.0, false),
                                                 std::make_tuple(Limpiar{Ruido::apagado, false, true}, 300.0, false)}) {
    PuenteVoz puente;
    MotorVoz motor(puente, Calidad{});
    REQUIRE(motor.ok());
    motor.sensibilidad(50);
    motor.por_voz(true);
    motor.limpiar(limpiar);
    const auto ventilador = ruido_blanco(48000 * 3, fuerza, 11);
    std::vector<std::int16_t> salida(480);
    int paquetes_al_final = 0;
    for (std::size_t desde = 0; desde + 480 <= ventilador.size(); desde += 480) {
      motor.procesar(gsl::make_span(ventilador).subspan(desde, 480), salida);
      PaqueteVoz sacado;
      while (puente.subida.sacar(sacado)) {
        paquetes_al_final += desde >= 48000 * 2 ? 1 : 0;
      }
    }
    CHECK((paquetes_al_final > 0) == se_abre);
  }
}

TEST_CASE("banco de la limpieza" * doctest::skip()) {
  constexpr int kBloques = 2000;
  for (const unsigned frecuencia : {48000u, 16000u}) {
    for (const auto& [nombre, limpiar] :
         {std::make_pair("eco", Limpiar{Ruido::apagado, true, false}),
          std::make_pair("ruido bajo", Limpiar{Ruido::baja, false, false}),
          std::make_pair("ruido medio", Limpiar{Ruido::media, false, false}),
          std::make_pair("volumen", Limpiar{Ruido::apagado, false, true}),
          std::make_pair("todo", Limpiar{Ruido::media, true, true})}) {
      Limpieza limpieza(frecuencia);
      limpieza.usar(limpiar);
      const auto voz = ruido_blanco(limpieza.bloque() * kBloques, 2000, 5);
      auto copia = voz;
      const auto desde = std::chrono::steady_clock::now();
      for (std::size_t i = 0; i < kBloques; ++i) {
        const auto bloque = gsl::make_span(copia).subspan(i * limpieza.bloque(), limpieza.bloque());
        limpieza.escuchar(gsl::make_span(voz).subspan(i * limpieza.bloque(), limpieza.bloque()));
        limpieza.limpiar(bloque);
      }
      const double us =
          std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - desde).count() / kBloques;
      MESSAGE("limpieza " << std::string(nombre) << " a " << frecuencia << " Hz: " << us << " us cada 10 ms ("
                          << us / 100 << " % de un núcleo)");
    }
  }
}
