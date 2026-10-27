#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include <windows.h>

#include "client/audio.h"
#include "client/buffer_voz.h"
#include "client/canal_voz.h"
#include "client/motor_voz.h"
#include "sonido_de_prueba.h"
#include <doctest/doctest.h>

using prueba::energia;
using prueba::potencia_en;
using prueba::tono;
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

std::vector<std::int16_t> hablar_y_escuchar(const Calidad& habla, const Calidad& escucha, std::uint32_t& hablando,
                                            const std::function<void(MotorVoz&, MotorVoz&)>& ajustar = {}) {
  PuenteVoz de_ida;
  PuenteVoz de_vuelta;
  MotorVoz emisor(de_ida, habla);
  MotorVoz receptor(de_vuelta, escucha);
  REQUIRE(emisor.ok());
  REQUIRE(receptor.ok());
  emisor.transmitir(true);
  if (ajustar) {
    ajustar(emisor, receptor);
  }
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
    const auto ids = de_vuelta.quienes();
    hablando = std::max(hablando, *std::max_element(ids.begin(), ids.end()));
  }
  CHECK(esperado == 50);
  return escuchado;
}

struct Escucha {
  std::vector<std::int16_t> salida;
  std::set<std::uint32_t> hablaron;
  std::ptrdiff_t a_la_vez = 0;
  std::uint32_t sin_lugar = 0;
};

Escucha varios_hablan(const std::vector<std::pair<double, double>>& tonos) {
  PuenteVoz de_vuelta;
  MotorVoz receptor(de_vuelta, Calidad{});
  REQUIRE(receptor.ok());
  std::vector<std::unique_ptr<PuenteVoz>> puentes;
  std::vector<std::unique_ptr<MotorVoz>> emisores;
  for (std::size_t i = 0; i < tonos.size(); ++i) {
    puentes.push_back(std::make_unique<PuenteVoz>());
    emisores.push_back(std::make_unique<MotorVoz>(*puentes.back(), Calidad{}));
    REQUIRE(emisores.back()->ok());
    emisores.back()->transmitir(true);
  }
  std::vector<std::int16_t> sonido(480);
  std::vector<std::int16_t> descarte(480);
  std::vector<std::int16_t> salida(480);
  std::vector<std::size_t> muestras(tonos.size());
  Escucha escucha;
  for (int tanda = 0; tanda < 100; ++tanda) {
    for (std::size_t i = 0; i < tonos.size(); ++i) {
      tono(sonido, muestras[i], 48000, tonos[i].first, tonos[i].second);
      emisores[i]->procesar(sonido, descarte);
      PaqueteVoz paquete_voz;
      while (puentes[i]->subida.sacar(paquete_voz)) {
        paquete_voz.hablante = static_cast<std::uint32_t>(i + 1);
        REQUIRE(de_vuelta.bajada.poner(paquete_voz));
      }
    }
    receptor.procesar(descarte, salida);
    escucha.salida.insert(escucha.salida.end(), salida.begin(), salida.end());
    const auto ids = de_vuelta.quienes();
    escucha.hablaron.insert(ids.begin(), ids.end());
    escucha.a_la_vez = std::max(escucha.a_la_vez, std::count_if(ids.begin(), ids.end(), [](std::uint32_t id) {
                                  return id != 0;
                                }));
  }
  escucha.hablaron.erase(0);
  escucha.sin_lugar = receptor.sin_lugar();
  return escucha;
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

TEST_CASE("El volumen multiplica cada muestra con tope, y sin audio no suena pero se sabe quién habla") {
  CHECK(ventrik::cliente::con_ganancia(1000, 0) == 0);
  CHECK(ventrik::cliente::con_ganancia(1000, 100) == 1000);
  CHECK(ventrik::cliente::con_ganancia(1000, 200) == 2000);
  CHECK(ventrik::cliente::con_ganancia(-1000, 50) == -500);
  CHECK(ventrik::cliente::con_ganancia(30000, 200) == 32767);
  CHECK(ventrik::cliente::con_ganancia(-30000, 200) == -32768);
  std::uint32_t hablando = 0;
  const auto normal = hablar_y_escuchar(Calidad{}, Calidad{}, hablando);
  const auto a_la_mitad = hablar_y_escuchar(Calidad{}, Calidad{}, hablando,
                                            [](MotorVoz&, MotorVoz& receptor) { receptor.volumenes(100, 50); });
  const double proporcion = energia(a_la_mitad, a_la_mitad.size() / 2) / energia(normal, normal.size() / 2);
  CHECK(proporcion > 0.2);
  CHECK(proporcion < 0.3);
  hablando = 0;
  const auto sin_audio = hablar_y_escuchar(Calidad{}, Calidad{}, hablando,
                                           [](MotorVoz&, MotorVoz& receptor) { receptor.silenciar(true, true); });
  CHECK(energia(sin_audio, 0) == 0);
  CHECK(hablando == 9);
}

TEST_CASE("Con el micrófono silenciado no se comprime ni se manda nada, aunque se pida transmitir") {
  PuenteVoz puente;
  MotorVoz motor(puente, Calidad{});
  REQUIRE(motor.ok());
  motor.silenciar(true, false);
  motor.transmitir(true);
  CHECK_FALSE(motor.transmitiendo());
  std::vector<std::int16_t> entrada(480);
  std::vector<std::int16_t> salida(480);
  std::size_t muestra = 0;
  PaqueteVoz sacado;
  for (const bool por_voz : {false, true}) {
    motor.por_voz(por_voz);
    motor.sensibilidad(0);
    for (int i = 0; i < 10; ++i) {
      tono(entrada, muestra, 48000);
      motor.procesar(entrada, salida);
    }
    CHECK_FALSE(puente.subida.sacar(sacado));
    CHECK(motor.nivel(10) == 0);
    CHECK_FALSE(motor.transmitiendo());
  }
  motor.por_voz(false);
  motor.silenciar(false, false);
  CHECK_FALSE(motor.transmitiendo());
  std::vector<std::int16_t> callado(480);
  for (int i = 0; i < 10; ++i) {
    tono(entrada, muestra, 48000);
    motor.procesar(entrada, salida);
  }
  CHECK_FALSE(puente.subida.sacar(sacado));

  motor.por_voz(true);
  for (int i = 0; i < 10; ++i) {
    tono(entrada, muestra, 48000);
    motor.procesar(entrada, salida);
  }
  CHECK(motor.transmitiendo());
  while (puente.subida.sacar(sacado)) {
  }
  motor.silenciar(true, false);
  motor.procesar(entrada, salida);
  motor.silenciar(false, false);
  for (int i = 0; i < 10; ++i) {
    motor.procesar(callado, salida);
  }
  CHECK_FALSE(puente.subida.sacar(sacado));
  CHECK_FALSE(motor.transmitiendo());

  motor.por_voz(false);
  motor.transmitir(true);
  for (int i = 0; i < 10; ++i) {
    tono(entrada, muestra, 48000);
    motor.procesar(entrada, salida);
  }
  CHECK(puente.subida.sacar(sacado));
}

TEST_CASE("Dos que hablan a la vez se oyen los dos") {
  const Escucha escucha = varios_hablan({{440, 8000}, {660, 8000}});
  const auto mitad = escucha.salida.size() / 2;
  const double ruido = potencia_en(escucha.salida, mitad, 1000, 48000);
  CHECK(potencia_en(escucha.salida, mitad, 440, 48000) > 100 * ruido);
  CHECK(potencia_en(escucha.salida, mitad, 660, 48000) > 100 * ruido);
  CHECK(escucha.hablaron == std::set<std::uint32_t>{1, 2});
  CHECK(escucha.a_la_vez == 2);
  CHECK(escucha.sin_lugar == 0);
}

TEST_CASE("De cinco que hablan suenan cuatro, y uno más fuerte se hace lugar") {
  const Escucha bajito = varios_hablan({{440, 8000}, {550, 8000}, {660, 8000}, {770, 8000}, {880, 300}});
  CHECK(bajito.hablaron == std::set<std::uint32_t>{1, 2, 3, 4});
  CHECK(bajito.a_la_vez == 4);
  CHECK(bajito.sin_lugar > 0);
  const Escucha fuerte = varios_hablan({{440, 1000}, {550, 1000}, {660, 1000}, {770, 1000}, {880, 8000}});
  CHECK(fuerte.hablaron.count(5) == 1);
  CHECK(fuerte.a_la_vez == 4);
  CHECK(fuerte.sin_lugar > 0);
  const auto mitad = fuerte.salida.size() / 2;
  CHECK(potencia_en(fuerte.salida, mitad, 880, 48000) > 100 * potencia_en(fuerte.salida, mitad, 1000, 48000));
}

TEST_CASE("La mezcla nunca recorta: baja todo el pedazo parejo y vuelve de a poco") {
  using ventrik::cliente::limitar;
  std::vector<float> mezcla(480);
  std::vector<std::int16_t> salida(480);
  for (std::size_t i = 0; i < mezcla.size(); ++i) {
    mezcla[i] = static_cast<float>(4 * 2 * 32767 * std::sin(2 * 3.14159265 * 440 * static_cast<double>(i) / 48000));
  }
  float limite = 1;
  limitar(mezcla, salida, limite);
  float pico = 0;
  for (const float valor : mezcla) {
    pico = std::max(pico, std::abs(valor));
  }
  const float ganancia = 32767 / pico;
  for (std::size_t i = 0; i < mezcla.size(); ++i) {
    CHECK(std::abs(salida[i] - mezcla[i] * ganancia) < 1.5f);
  }
  CHECK(limite < 0.2f);
  std::fill(mezcla.begin(), mezcla.end(), 1000.0f);
  for (int tanda = 0; tanda < 100; ++tanda) {
    limitar(mezcla, salida, limite);
  }
  CHECK(salida[0] > 950);
  CHECK(limite > 0.95f);
}

TEST_CASE("El nivel de cada paquete va en dB sobre -127, de 0 a 127") {
  using ventrik::cliente::nivel_de;
  CHECK(nivel_de(std::vector<std::int16_t>(960, 0)) == 0);
  CHECK(nivel_de(std::vector<std::int16_t>(960, 32767)) == 127);
  std::vector<std::int16_t> seno(960);
  std::size_t muestra = 0;
  for (auto& valor : seno) {
    valor = static_cast<std::int16_t>(3277 * std::sin(2 * 3.14159265 * 1000 * static_cast<double>(muestra++) / 48000));
  }
  CHECK(nivel_de(seno) == 104);
}

TEST_CASE("Un paquete sin origen se descarta y no ocupa lugar") {
  PuenteVoz puente;
  MotorVoz motor(puente, Calidad{});
  REQUIRE(motor.ok());
  std::vector<std::int16_t> entrada(480);
  std::vector<std::int16_t> salida(480);
  for (std::uint16_t numero = 0; numero < 4; ++numero) {
    REQUIRE(puente.bajada.poner(paquete(numero, 0)));
  }
  motor.procesar(entrada, salida);
  CHECK(motor.sin_lugar() == 4);
  CHECK(motor.espera_ms() == 0);
}

TEST_CASE("Sin transmitir no sale nada, y dos que hablan entran los dos") {
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
  const auto ids = puente.quienes();
  CHECK(std::count(ids.begin(), ids.end(), 5u) == 1);
  CHECK(std::count(ids.begin(), ids.end(), 6u) == 1);
  CHECK(avisado(puente));
  CHECK_FALSE(avisado(puente));
}

TEST_CASE("El puente avisa solo cuando cambia quién habla o el estado de la voz") {
  PuenteVoz puente;
  REQUIRE(puente.cambio.evento() != nullptr);
  CHECK_FALSE(avisado(puente));
  puente.cambiar(puente.hablando[0], 5u);
  CHECK(avisado(puente));
  puente.cambiar(puente.hablando[0], 5u);
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

TEST_CASE("La marca del medidor va de -50 a 0 dB, como el medidor") {
  using ventrik::cliente::umbral_de;
  CHECK(umbral_de(0) == 104);
  CHECK(umbral_de(-5) == 104);
  CHECK(umbral_de(50) == 1843);
  CHECK(umbral_de(100) == 32768);
  CHECK(umbral_de(200) == 32768);
}

TEST_CASE("Por voz se transmite al pasar la marca, con el pedazo anterior, y se sigue 0,4 s al callarse") {
  PuenteVoz puente;
  MotorVoz motor(puente, Calidad{});
  REQUIRE(motor.ok());
  motor.sensibilidad(50);
  motor.por_voz(true);
  std::vector<std::int16_t> entrada(480);
  std::vector<std::int16_t> salida(480);
  std::size_t muestra = 0;
  int paquetes = 0;
  const auto pasar = [&](int tandas, bool fuerte) {
    for (int i = 0; i < tandas; ++i) {
      for (auto& valor : entrada) {
        const double segundos = static_cast<double>(muestra++) / 48000;
        valor = fuerte ? static_cast<std::int16_t>(5000 * std::sin(2 * 3.14159265 * 440 * segundos)) : std::int16_t{300};
      }
      motor.procesar(entrada, salida);
      PaqueteVoz sacado;
      while (puente.subida.sacar(sacado)) {
        ++paquetes;
      }
    }
  };
  pasar(20, false);
  CHECK(paquetes == 0);
  CHECK_FALSE(motor.transmitiendo());
  CHECK_FALSE(avisado(puente));
  CHECK(motor.nivel(20) > 0);
  pasar(20, true);
  CHECK(motor.transmitiendo());
  CHECK(avisado(puente));
  CHECK(paquetes == 11);
  pasar(2 * ventrik::cliente::kPedazosTrasCallarse, false);
  CHECK(motor.transmitiendo());
  CHECK_FALSE(avisado(puente));
  pasar(2, false);
  CHECK_FALSE(motor.transmitiendo());
  CHECK(avisado(puente));
  const int hasta_callarse = paquetes;
  pasar(20, false);
  CHECK(paquetes == hasta_callarse);
  motor.por_voz(false);
  motor.transmitir(false);
  pasar(20, true);
  CHECK(paquetes == hasta_callarse);
  CHECK_FALSE(motor.transmitiendo());
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
