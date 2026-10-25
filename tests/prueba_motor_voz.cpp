#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include <windows.h>

#include "client/audio.h"
#include "client/buffer_voz.h"
#include "client/canal_voz.h"
#include "client/motor_voz.h"
#include <doctest/doctest.h>

using ventrik::cliente::BufferVoz;
using ventrik::cliente::Calidad;
using ventrik::cliente::EstadoVoz;
using ventrik::cliente::MotorVoz;
using ventrik::cliente::PaqueteVoz;
using ventrik::cliente::PuenteVoz;
using Salida = BufferVoz::Salida;

namespace {
bool avisado(const PuenteVoz& puente) {
  return WaitForSingleObject(puente.cambio.evento(), 0) == WAIT_OBJECT_0;
}

void tono(std::vector<std::int16_t>& muestras, std::size_t& muestra, unsigned frecuencia) {
  for (auto& valor : muestras) {
    const double segundos = static_cast<double>(muestra++) / frecuencia;
    valor = static_cast<std::int16_t>(8000 * std::sin(2 * 3.14159265 * 440 * segundos) *
                                      (0.55 + 0.45 * std::sin(2 * 3.14159265 * 4 * segundos)));
  }
}

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
  std::vector<std::int16_t> sonido(por_tanda_emisor);
  std::vector<std::int16_t> mudo(por_tanda_emisor);
  std::vector<std::int16_t> silencio(por_tanda_receptor);
  std::vector<std::int16_t> salida(por_tanda_receptor);
  std::vector<std::int16_t> escuchado;
  std::uint16_t esperado = 0;
  std::size_t muestra = 0;
  for (int tanda = 0; tanda < 100; ++tanda) {
    tono(sonido, muestra, habla.frecuencia);
    emisor.procesar(sonido, mudo);
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
  CHECK_FALSE(avisado(puente));
  for (std::uint16_t numero = 0; numero < 4; ++numero) {
    REQUIRE(puente.bajada.poner(paquete(numero, 5)));
    REQUIRE(puente.bajada.poner(paquete(numero, 6)));
  }
  for (int i = 0; i < 6; ++i) {
    motor.procesar(entrada, salida);
  }
  CHECK(puente.hablando == 5);
  CHECK(avisado(puente));
  CHECK_FALSE(avisado(puente));
}

TEST_CASE("El puente avisa solo cuando cambia quién habla o el estado de la voz") {
  PuenteVoz puente;
  REQUIRE(puente.cambio.evento() != nullptr);
  CHECK_FALSE(avisado(puente));
  puente.cambiar(puente.hablando, 5u);
  CHECK(avisado(puente));
  puente.cambiar(puente.hablando, 5u);
  CHECK_FALSE(avisado(puente));
  puente.cambiar(puente.estado, EstadoVoz::lista);
  puente.cambiar(puente.estado, EstadoVoz::lista);
  CHECK(avisado(puente));
  CHECK_FALSE(avisado(puente));
}

TEST_CASE("Solo en el canal, la tecla no comprime ni manda voz, pero el medidor sigue") {
  PuenteVoz puente;
  MotorVoz motor(puente, Calidad{});
  REQUIRE(motor.ok());
  motor.transmitir(true);
  motor.solo_en_el_canal(true);
  std::vector<std::int16_t> entrada(480);
  std::vector<std::int16_t> salida(480);
  std::size_t muestra = 0;
  PaqueteVoz sacado;
  for (int i = 0; i < 10; ++i) {
    tono(entrada, muestra, 48000);
    motor.procesar(entrada, salida);
  }
  CHECK_FALSE(puente.subida.sacar(sacado));
  CHECK(motor.nivel(10) > 0);
  motor.solo_en_el_canal(false);
  for (int i = 0; i < 10; ++i) {
    tono(entrada, muestra, 48000);
    motor.procesar(entrada, salida);
  }
  CHECK(puente.subida.sacar(sacado));
  CHECK(sacado.numero == 0);
}

TEST_CASE("Con la tecla apretada y en silencio no sale voz, y el que escucha no lo toma como pérdida") {
  for (const Calidad& calidad : {Calidad{}, Calidad{16000, 0, 20}}) {
    PuenteVoz de_ida;
    PuenteVoz de_vuelta;
    MotorVoz emisor(de_ida, calidad);
    MotorVoz receptor(de_vuelta, calidad);
    REQUIRE(emisor.ok());
    REQUIRE(receptor.ok());
    emisor.transmitir(true);
    const std::size_t por_tanda = calidad.frecuencia / 100;
    std::vector<std::int16_t> sonido(por_tanda);
    const std::vector<std::int16_t> mudo(por_tanda);
    std::vector<std::int16_t> salida(por_tanda);
    std::vector<std::int16_t> escuchado;
    std::size_t muestra = 0;
    std::uint32_t ruido = 1;
    std::uint16_t esperado = 0;
    int en_silencio = 0;
    int al_final_del_silencio = 0;
    for (int tanda = 0; tanda < 500; ++tanda) {
      const bool habla = tanda < 100 || tanda >= 400;
      tono(sonido, muestra, calidad.frecuencia);
      for (auto& valor : sonido) {
        ruido = ruido * 1103515245u + 12345u;
        valor = habla ? valor : static_cast<std::int16_t>(static_cast<int>((ruido >> 16) & 63) - 32);
      }
      emisor.procesar(sonido, salida);
      PaqueteVoz paquete_voz;
      while (de_ida.subida.sacar(paquete_voz)) {
        CHECK(paquete_voz.numero == esperado++);
        CHECK(paquete_voz.largo > 2);
        en_silencio += habla ? 0 : 1;
        al_final_del_silencio += !habla && tanda >= 250 ? 1 : 0;
        paquete_voz.hablante = 9;
        REQUIRE(de_vuelta.bajada.poner(paquete_voz));
      }
      receptor.procesar(mudo, salida);
      escuchado.insert(escuchado.end(), salida.begin(), salida.end());
    }
    MESSAGE("frecuencia " << calidad.frecuencia << ": paquetes " << esperado << ", en 3 s de silencio "
                          << en_silencio << " (sin DTX serian 150)");
    CHECK(esperado - en_silencio == 100);
    CHECK(en_silencio <= 80);
    CHECK(al_final_del_silencio == 0);
    CHECK(receptor.faltas() == 0);
    CHECK(receptor.recortes() == 0);
    CHECK(energia(escuchado, escuchado.size() - por_tanda * 40) > 1e6);
  }
}

TEST_CASE("Después de soltar la tecla más de un segundo, lo siguiente que dice se escucha") {
  PuenteVoz de_ida;
  PuenteVoz de_vuelta;
  MotorVoz emisor(de_ida, Calidad{});
  MotorVoz receptor(de_vuelta, Calidad{});
  REQUIRE(emisor.ok());
  REQUIRE(receptor.ok());
  std::vector<std::int16_t> sonido(480);
  const std::vector<std::int16_t> mudo(480);
  std::vector<std::int16_t> salida(480);
  std::vector<std::int16_t> escuchado;
  std::size_t muestra = 0;
  for (int tanda = 0; tanda < 400; ++tanda) {
    emisor.transmitir(tanda < 100 || tanda >= 300);
    tono(sonido, muestra, 48000);
    emisor.procesar(sonido, salida);
    PaqueteVoz paquete_voz;
    while (de_ida.subida.sacar(paquete_voz)) {
      paquete_voz.hablante = 9;
      REQUIRE(de_vuelta.bajada.poner(paquete_voz));
    }
    receptor.procesar(mudo, salida);
    escuchado.insert(escuchado.end(), salida.begin(), salida.end());
  }
  CHECK(energia(escuchado, escuchado.size() - 480 * 40) > 1e6);
}

TEST_CASE("BufferVoz: una pausa sin paquetes con números seguidos no es pérdida ni recorte") {
  BufferVoz buffer;
  std::uint16_t numero = 0;
  int sonaron = 0;
  const auto paso = [&](bool llega) {
    if (llega) {
      buffer.llegar(paquete(numero++));
    }
    PaqueteVoz sacado;
    sonaron += buffer.sacar(sacado) == Salida::paquete ? 1 : 0;
  };
  for (int tramo = 0; tramo < 3; ++tramo) {
    for (int i = 0; i < 25; ++i) {
      paso(true);
    }
    for (int i = 0; i < 40; ++i) {
      paso(false);
    }
  }
  CHECK(sonaron == 75);
  CHECK(buffer.faltas() == 0);
  CHECK(buffer.recortes() == 0);
}

TEST_CASE("El canal de voz no cuenta como perdido el silencio, sí los números que faltan") {
  PuenteVoz puente;
  ventrik::cliente::CanalVoz canal(ventrik::red::Direccion{}, puente);
  const auto llega = [&](std::uint16_t numero, ventrik::cliente::CanalVoz::Reloj::time_point cuando) {
    ventrik::protocolo::MedioBajada bajada;
    bajada.origen = 7;
    bajada.medio.numero = numero;
    bajada.medio.datos = {1, 2, 3};
    std::vector<std::uint8_t> datagrama;
    REQUIRE(ventrik::protocolo::armar_datagrama(bajada, datagrama));
    canal.recibir(datagrama, cuando);
  };
  const auto ahora = ventrik::cliente::CanalVoz::Reloj::now();
  for (std::uint16_t numero = 0; numero < 10; ++numero) {
    llega(numero, ahora);
  }
  for (std::uint16_t numero = 10; numero < 20; ++numero) {
    llega(numero, ahora + std::chrono::seconds(2));
  }
  CHECK(puente.recibidos == 20);
  CHECK(puente.perdidos == 0);
  llega(23, ahora + std::chrono::seconds(3));
  CHECK(puente.perdidos == 3);
}

TEST_CASE("La memoria de la voz queda fija en la RAM, sin pasar de 1 MB, y se suelta al cerrar") {
  SIZE_T minimo_antes = 0;
  SIZE_T maximo_antes = 0;
  REQUIRE(GetProcessWorkingSetSize(GetCurrentProcess(), &minimo_antes, &maximo_antes));
  for (const Calidad& calidad : {Calidad{}, Calidad{16000, 0, 20}}) {
    PuenteVoz puente;
    MotorVoz motor(puente, calidad);
    REQUIRE(motor.ok());
    MESSAGE("frecuencia " << calidad.frecuencia << ": memoria fija " << motor.memoria_fija() / 1024 << " KB");
    CHECK(motor.memoria_fija() < 1024 * 1024);
  }
  SIZE_T minimo = 0;
  SIZE_T maximo = 0;
  REQUIRE(GetProcessWorkingSetSize(GetCurrentProcess(), &minimo, &maximo));
  CHECK(minimo == minimo_antes);
  CHECK(maximo == maximo_antes);
}

TEST_CASE("Windows no frena los hilos de la voz") {
  MESSAGE("sin freno de energia: " << ventrik::cliente::no_frenar_este_hilo());
}

TEST_CASE("Hacia los parlantes se tira solo lo callado, salvo con mucha espera") {
  using ventrik::cliente::tirar_pedazo;
  const std::vector<std::int16_t> silencio(160, 0);
  const std::vector<std::int16_t> casi(160, -999);
  std::vector<std::int16_t> voz(160, 0);
  voz[80] = 1000;
  for (const auto* callado : {&silencio, &casi}) {
    CHECK_FALSE(tirar_pedazo(110, *callado));
    CHECK(tirar_pedazo(120, *callado));
  }
  CHECK_FALSE(tirar_pedazo(120, voz));
  CHECK_FALSE(tirar_pedazo(240, voz));
  CHECK(tirar_pedazo(250, voz));
}
