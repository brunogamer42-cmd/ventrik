#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "client/buffer_voz.h"
#include "client/motor_voz.h"
#include <doctest/doctest.h>

using ventrik::cliente::BufferVoz;
using ventrik::cliente::Calidad;
using ventrik::cliente::MotorVoz;
using ventrik::cliente::PaqueteVoz;
using ventrik::cliente::PuenteVoz;
using Salida = BufferVoz::Salida;

namespace {
PaqueteVoz paquete(std::uint16_t numero, std::uint32_t hablante = 5) {
  PaqueteVoz armado;
  armado.hablante = hablante;
  armado.numero = numero;
  armado.largo = 1;
  armado.opus[0] = static_cast<std::uint8_t>(numero);
  return armado;
}

std::vector<int> sacar_todo(BufferVoz& buffer, int veces) {
  std::vector<int> salidas;
  for (int i = 0; i < veces; ++i) {
    PaqueteVoz sacado;
    const auto salida = buffer.sacar(sacado);
    salidas.push_back(salida == Salida::paquete   ? sacado.numero
                      : salida == Salida::perdido ? -1
                                                  : -2);
  }
  return salidas;
}

double energia(const std::vector<std::int16_t>& muestras, std::size_t desde) {
  double suma = 0;
  for (std::size_t i = desde; i < muestras.size(); ++i) {
    suma += static_cast<double>(muestras[i]) * muestras[i];
  }
  return suma / static_cast<double>(muestras.size() - desde);
}

std::vector<std::int16_t> hablar_y_escuchar(const Calidad& habla, const Calidad& escucha,
                                            std::uint32_t& hablando) {
  PuenteVoz de_ida;
  PuenteVoz de_vuelta;
  MotorVoz emisor(de_ida, habla);
  MotorVoz receptor(de_vuelta, escucha);
  REQUIRE(emisor.ok());
  REQUIRE(receptor.ok());
  emisor.transmitir(true);
  const std::size_t por_tanda_emisor = habla.frecuencia / 100;
  const std::size_t por_tanda_receptor = escucha.frecuencia / 100;
  std::vector<std::int16_t> tono(por_tanda_emisor);
  std::vector<std::int16_t> mudo(por_tanda_emisor);
  std::vector<std::int16_t> silencio(por_tanda_receptor);
  std::vector<std::int16_t> salida(por_tanda_receptor);
  std::vector<std::int16_t> escuchado;
  std::uint16_t esperado = 0;
  std::size_t muestra = 0;
  for (int tanda = 0; tanda < 100; ++tanda) {
    for (auto& valor : tono) {
      valor = static_cast<std::int16_t>(8000 * std::sin(2 * 3.14159265 * 440 * muestra++ /
                                                        habla.frecuencia));
    }
    emisor.procesar(tono, mudo);
    PaqueteVoz paquete_voz;
    while (de_ida.subida.sacar(paquete_voz)) {
      CHECK(paquete_voz.numero == esperado++);
      paquete_voz.hablante = 9;
      REQUIRE(de_vuelta.bajada.poner(paquete_voz));
    }
    receptor.procesar(silencio, salida);
    escuchado.insert(escuchado.end(), salida.begin(), salida.end());
    hablando = std::max(hablando, de_vuelta.hablando.load());
  }
  CHECK(esperado == 50);
  return escuchado;
}

}

TEST_CASE("BufferVoz espera dos paquetes y los da en orden") {
  BufferVoz buffer;
  buffer.llegar(paquete(0));
  CHECK(sacar_todo(buffer, 1) == std::vector<int>{-2});
  buffer.llegar(paquete(1));
  CHECK(sacar_todo(buffer, 3) == std::vector<int>{0, 1, -2});
  CHECK(buffer.recortes() == 0);
}

TEST_CASE("BufferVoz ordena, descarta duplicados y tardíos, y marca los perdidos") {
  BufferVoz buffer;
  for (const std::uint16_t numero : std::initializer_list<std::uint16_t>{0, 2, 1, 1, 4}) {
    buffer.llegar(paquete(numero));
  }
  CHECK(sacar_todo(buffer, 3) == std::vector<int>{0, 1, 2});
  buffer.llegar(paquete(1));
  buffer.llegar(paquete(2));
  CHECK(sacar_todo(buffer, 3) == std::vector<int>{-1, 4, -2});
  CHECK(buffer.faltas() == 1);
  CHECK(buffer.vaciados() == 1);
}

TEST_CASE("BufferVoz sigue la vuelta del número y se reinicia del todo") {
  BufferVoz buffer;
  for (const std::uint16_t numero : std::initializer_list<std::uint16_t>{65534, 65535, 0, 1}) {
    buffer.llegar(paquete(numero));
  }
  CHECK(sacar_todo(buffer, 4) == std::vector<int>{65534, 65535, 0, 1});
  buffer.llegar(paquete(0));
  CHECK(sacar_todo(buffer, 1) == std::vector<int>{-2});
  buffer.reiniciar();
  buffer.llegar(paquete(0));
  buffer.llegar(paquete(1));
  CHECK(sacar_todo(buffer, 2) == std::vector<int>{0, 1});
}

TEST_CASE("BufferVoz no junta retraso con las trabas de la red ni con relojes distintos") {
  BufferVoz buffer;
  std::uint16_t numero = 0;
  const auto paso = [&](int llegan) {
    for (int i = 0; i < llegan; ++i) {
      buffer.llegar(paquete(numero++));
    }
    PaqueteVoz sacado;
    return buffer.sacar(sacado);
  };
  std::size_t mayor = 0;
  for (int traba = 1; traba <= 10; ++traba) {
    for (int i = 0; i < 50; ++i) {
      paso(1);
    }
    for (int i = 0; i < traba * 3; ++i) {
      paso(0);
    }
    paso(traba * 3);
    mayor = std::max(mayor, buffer.cantidad());
  }
  CHECK(mayor <= 6);
  CHECK(buffer.recortes() > 0);
  int sonaron = 0;
  for (int i = 1; i <= 10000; ++i) {
    sonaron += paso(i % 100 == 0 ? 2 : 1) == Salida::paquete ? 1 : 0;
  }
  CHECK(buffer.cantidad() <= 6);
  CHECK(sonaron >= 9900);
}

TEST_CASE("La voz comprimida se escucha del otro lado, también con otra calidad") {
  for (const auto& [habla, escucha] : {std::make_pair(Calidad{}, Calidad{}),
                                       std::make_pair(Calidad{}, Calidad{16000, 0, 20}),
                                       std::make_pair(Calidad{16000, 0, 20}, Calidad{})}) {
    std::uint32_t hablando = 0;
    const auto escuchado = hablar_y_escuchar(habla, escucha, hablando);
    CHECK(energia(escuchado, escuchado.size() / 2) > 1e6);
    CHECK(hablando == 9);
  }
}

TEST_CASE("Sin transmitir no sale nada, y un segundo hablante espera su turno") {
  PuenteVoz puente;
  MotorVoz motor(puente, Calidad{});
  REQUIRE(motor.ok());
  std::vector<std::int16_t> entrada(480, 1000);
  std::vector<std::int16_t> salida(480);
  for (int i = 0; i < 10; ++i) {
    motor.procesar(entrada, salida);
  }
  PaqueteVoz sacado;
  CHECK_FALSE(puente.subida.sacar(sacado));
  for (std::uint16_t numero = 0; numero < 4; ++numero) {
    REQUIRE(puente.bajada.poner(paquete(numero, 5)));
    REQUIRE(puente.bajada.poner(paquete(numero, 6)));
  }
  for (int i = 0; i < 6; ++i) {
    motor.procesar(entrada, salida);
  }
  CHECK(puente.hablando == 5);
}
