#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

#include <miniz.h>

#include "client/cuando_suena.h"
#include "client/paquete_de_sonidos.h"
#include "client/sonidos.h"
#include "common/protocol/bytes.h"
#include <doctest/doctest.h>

using namespace ventrik::cliente;
using Bytes = std::vector<std::uint8_t>;

namespace {
Muestras tono(std::size_t cuantas, double fuerza = 8000) {
  Muestras muestras(cuantas);
  for (std::size_t i = 0; i < cuantas; ++i) {
    muestras[i] = static_cast<std::int16_t>(fuerza * std::sin(2 * 3.14159265358979 * 440 * static_cast<double>(i) / 48000));
  }
  return muestras;
}

double energia(const Muestras& muestras) {
  double suma = 0;
  for (const std::int16_t muestra : muestras) {
    suma += static_cast<double>(muestra) * muestra;
  }
  return suma / static_cast<double>(std::max<std::size_t>(muestras.size(), 1));
}

Bytes wav(std::uint32_t frecuencia, std::uint16_t canales, double segundos) {
  const auto cuadros = static_cast<std::uint32_t>(frecuencia * segundos);
  ventrik::protocolo::Escritor escritor;
  const auto marca = [&escritor](const char* texto) {
    escritor.escribir_bytes(gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(texto), 4));
  };
  marca("RIFF");
  escritor.escribir_u32(36 + cuadros * canales * 2);
  marca("WAVE");
  marca("fmt ");
  escritor.escribir_u32(16);
  escritor.escribir_u16(1);
  escritor.escribir_u16(canales);
  escritor.escribir_u32(frecuencia);
  escritor.escribir_u32(frecuencia * canales * 2);
  escritor.escribir_u16(static_cast<std::uint16_t>(canales * 2));
  escritor.escribir_u16(16);
  marca("data");
  escritor.escribir_u32(cuadros * canales * 2);
  for (std::uint32_t i = 0; i < cuadros * canales; ++i) {
    escritor.escribir_u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(
        6000 * std::sin(2 * 3.14159265358979 * 440 * static_cast<double>(i / canales) / frecuencia))));
  }
  return std::move(escritor).soltar();
}

Bytes zip(const std::vector<std::pair<std::string, Bytes>>& archivos) {
  mz_zip_archive armado{};
  REQUIRE(mz_zip_writer_init_heap(&armado, 0, 0));
  for (const auto& [nombre, contenido] : archivos) {
    REQUIRE(mz_zip_writer_add_mem(&armado, nombre.c_str(), contenido.data(), contenido.size(), MZ_DEFAULT_LEVEL));
  }
  void* datos = nullptr;
  std::size_t largo = 0;
  REQUIRE(mz_zip_writer_finalize_heap_archive(&armado, &datos, &largo));
  const auto* bytes = static_cast<const std::uint8_t*>(datos);
  Bytes resultado(bytes, bytes + largo);
  mz_free(datos);
  mz_zip_writer_end(&armado);
  return resultado;
}

Bytes texto(const std::string& contenido) { return Bytes(contenido.begin(), contenido.end()); }

Bytes recurso(int id) {
  const HMODULE modulo = GetModuleHandleW(nullptr);
  const HRSRC encontrado = FindResourceW(modulo, MAKEINTRESOURCEW(id), RT_RCDATA);
  REQUIRE(encontrado != nullptr);
  const auto* datos = static_cast<const std::uint8_t*>(LockResource(LoadResource(modulo, encontrado)));
  REQUIRE(datos != nullptr);
  return Bytes(datos, datos + SizeofResource(modulo, encontrado));
}

}

TEST_CASE("Los 27 sonidos de fábrica están dentro del programa, se decodifican y duran lo esperado") {
  for (std::size_t cual = 0; cual < kSonidos; ++cual) {
    CAPTURE(cual);
    const auto sonido = static_cast<Sonido>(cual);
    const auto datos = sonido_de_fabrica(sonido);
    REQUIRE_FALSE(datos.empty());
    const auto muestras = decodificar_vksn(datos, maximo_de_muestras(sonido));
    REQUIRE(muestras);
    CHECK(muestras->size() > kFrecuenciaDeSonidos / 2);
    CHECK(muestras->size() <= maximo_de_muestras(sonido));
    CHECK(energia(*muestras) > 1000);
  }
}

TEST_CASE("El formato VKSN va y vuelve, y rechaza marca, versión, largos y paquetes que no cierran") {
  const Muestras original = tono(48000);
  const Bytes vksn = codificar_vksn(original);
  REQUIRE(vksn.size() > 11);
  CHECK(vksn.size() < 10000);
  const auto vuelta = decodificar_vksn(vksn, 48000);
  REQUIRE(vuelta);
  CHECK(vuelta->size() == 48000);
  CHECK(energia(*vuelta) > energia(original) / 2);
  CHECK(energia(*vuelta) < energia(original) * 2);
  CHECK_FALSE(decodificar_vksn(vksn, 47999));
  for (const std::size_t lugar : {std::size_t{0}, std::size_t{4}, std::size_t{9}, std::size_t{11}}) {
    Bytes malo = vksn;
    malo[lugar] ^= 0x40;
    CHECK_FALSE(decodificar_vksn(malo, 48000));
  }
  Bytes de_mas = vksn;
  de_mas.push_back(0);
  CHECK_FALSE(decodificar_vksn(de_mas, 48000));
  CHECK_FALSE(decodificar_vksn(gsl::span<const std::uint8_t>(vksn).first(vksn.size() - 1), 48000));
  Bytes sin_paquete(vksn.begin(), vksn.begin() + 11);
  sin_paquete.insert(sin_paquete.end(), {0, 0});
  CHECK_FALSE(decodificar_vksn(sin_paquete, 48000));
  CHECK(codificar_vksn({}).empty());
  CHECK(codificar_vksn(Muestras(kMaximoSegundosDeLlamada * kFrecuenciaDeSonidos + 1)).empty());
}

TEST_CASE("Un sonido propio en WAV se pasa a 48 kHz mono, y uno largo, pesado o roto se rechaza") {
  const auto convertido = decodificar_archivo(wav(44100, 2, 1), maximo_de_muestras(Sonido::mensaje));
  REQUIRE(convertido);
  CHECK(convertido->size() >= 47990);
  CHECK(convertido->size() <= 48010);
  CHECK(energia(*convertido) > 1e6);
  const Muestras exacto = tono(4800);
  CHECK(decodificar_archivo(armar_wav(exacto), 48000) == exacto);
  const Bytes largo = wav(8000, 1, 6);
  CHECK_FALSE(decodificar_archivo(largo, maximo_de_muestras(Sonido::mensaje)));
  CHECK(decodificar_archivo(largo, maximo_de_muestras(Sonido::llamada_entrante)));
  CHECK_FALSE(decodificar_archivo(Bytes(kMaximoArchivoDeSonido + 1, 0), 480000));
  CHECK_FALSE(decodificar_archivo(texto("RIFF....WAVEbasura que no es un wav"), 480000));
  CHECK_FALSE(decodificar_archivo(texto("OggS y nada más"), 480000));
  CHECK_FALSE(decodificar_archivo(texto("hola, esto no es audio"), 480000));
  CHECK_FALSE(decodificar_archivo({}, 480000));
}

TEST_CASE("OGG y MP3 de verdad se pasan a 48 kHz mono, también en un paquete, en las dos versiones") {
  for (const int id : {201, 202, 203}) {
    CAPTURE(id);
    const auto muestras = decodificar_archivo(recurso(id), maximo_de_muestras(Sonido::mensaje));
    REQUIRE(muestras);
    CHECK(muestras->size() >= 2.4 * 48000);
    CHECK(muestras->size() <= 2.65 * 48000);
    CHECK(energia(*muestras) > 1e6);
    CHECK_FALSE(decodificar_archivo(recurso(id), 2 * 48000));
  }
  const auto paquete = leer_paquete(zip({{"paquete.json", texto("{\"version\":1,\"sonidos\":{\"mencion\":\"a.ogg\",\"error\":\"b.mp3\"}}")},
                                         {"a.ogg", recurso(202)},
                                         {"b.mp3", recurso(203)}}));
  REQUIRE(paquete);
  CHECK(paquete->size() == 2);
}

TEST_CASE("Un paquete que repite el mismo aviso muchas veces se decodifica una sola vez") {
  std::string indice = "{\"version\":1,\"sonidos\":{";
  for (int vez = 0; vez < 60; ++vez) {
    indice += (vez == 0 ? "" : ",") + std::string("\"mencion\":\"a.ogg\"");
  }
  indice += "}}";
  const auto pedidos = leer_indice(indice);
  REQUIRE(pedidos);
  CHECK(pedidos->size() == 1);
  const auto paquete = leer_paquete(zip({{"paquete.json", texto(indice)}, {"a.ogg", recurso(201)}}));
  REQUIRE(paquete);
  CHECK(paquete->size() == 1);
}

TEST_CASE("Tres sonidos fuertes a la vez se mezclan sin pasar de 32767 y después hay silencio") {
  Sonidos sonidos(L"");
  const Muestras fuerte(1920, 30000);
  for (int vez = 0; vez < 3; ++vez) {
    REQUIRE(sonidos.poner(fuerte, 1.0f));
  }
  Muestras salida(960);
  Sonidos::procesar(&sonidos, {}, salida);
  CHECK(*std::min_element(salida.begin(), salida.end()) > 0);
  CHECK(*std::max_element(salida.begin(), salida.end()) <= 32767);
  Sonidos::procesar(&sonidos, {}, salida);
  Sonidos::procesar(&sonidos, {}, salida);
  CHECK(std::all_of(salida.begin(), salida.end(), [](std::int16_t muestra) { return muestra == 0; }));
}

TEST_CASE("La salida de los sonidos se abre al sonar y se cierra sola 2 s después del último") {
  Sonidos sonidos(L"");
  const auto ahora = Sonidos::Reloj::now();
  if (!sonidos.sonar(Sonido::error, 100, false, {}, ahora)) {
    MESSAGE("sin salida de audio en esta PC");
    return;
  }
  CHECK(sonidos.abierta());
  REQUIRE(sonidos.cerrar_en());
  CHECK(*sonidos.cerrar_en() > ahora + kCerrarTrasSonar);
  CHECK(*sonidos.cerrar_en() < ahora + kCerrarTrasSonar + std::chrono::seconds(2));
  sonidos.atender(ahora + std::chrono::seconds(1));
  CHECK(sonidos.abierta());
  sonidos.atender(*sonidos.cerrar_en());
  CHECK_FALSE(sonidos.abierta());
  CHECK_FALSE(sonidos.cerrar_en());
  CHECK(sonidos.guardados() > 0);
  sonidos.soltar();
  CHECK(sonidos.guardados() == 0);
}

TEST_CASE("Un paquete .vksonidos se exporta e importa igual, e ignora lo que no es audio o no tiene aviso") {
  const std::vector<SonidoDePaquete> mios = {{Sonido::mencion, "mencion-tano.ogg", tono(24000)},
                                             {Sonido::error, "error.wav", tono(9600, 3000)}};
  const Bytes paquete = armar_paquete(mios);
  REQUIRE_FALSE(paquete.empty());
  const auto leido = leer_paquete(paquete);
  REQUIRE(leido);
  REQUIRE(leido->size() == 2);
  CHECK((*leido)[0].sonido == Sonido::mencion);
  CHECK((*leido)[0].muestras == mios[0].muestras);
  CHECK((*leido)[1].sonido == Sonido::error);
  CHECK((*leido)[1].muestras == mios[1].muestras);

  const auto mezclado = leer_paquete(zip({{"paquete.json",
                                           texto("{\"version\":1,\"autor\":{\"a\":[1,2]},\"sonidos\":{\"mencion\":\"a.wav\","
                                                 "\"no_existe\":\"a.wav\",\"error\":\"nota.txt\",\"susurro\":\"falta.wav\"}}")},
                                          {"a.wav", armar_wav(tono(4800))},
                                          {"nota.txt", texto("no es audio")}}));
  REQUIRE(mezclado);
  REQUIRE(mezclado->size() == 1);
  CHECK((*mezclado)[0].sonido == Sonido::mencion);
}

TEST_CASE("Un paquete con carpetas, rutas, demasiados archivos, una bomba o un índice roto se rechaza") {
  const Bytes indice = texto("{\"version\":1,\"sonidos\":{\"mencion\":\"a.wav\"}}");
  const Bytes audio = armar_wav(tono(4800));
  CHECK(leer_paquete(zip({{"paquete.json", indice}, {"a.wav", audio}})));
  CHECK_FALSE(leer_paquete(zip({{"paquete.json", indice}, {"a.wav", audio}, {"sub/b.wav", audio}})));
  CHECK_FALSE(leer_paquete(zip({{"paquete.json", indice}, {"a.wav", audio}, {"..\\b.wav", audio}})));
  CHECK_FALSE(leer_paquete(zip({{"paquete.json", indice}, {"a.wav", audio}, {"c:b.wav", audio}})));
  CHECK_FALSE(leer_paquete(zip({{"paquete.json", indice}, {"a.wav", audio}, {"carpeta/", {}}})));
  std::vector<std::pair<std::string, Bytes>> muchos = {{"paquete.json", indice}, {"a.wav", audio}};
  for (int i = 0; i < 40; ++i) {
    muchos.emplace_back("relleno" + std::to_string(i) + ".txt", texto("x"));
  }
  CHECK_FALSE(leer_paquete(zip(muchos)));
  CHECK_FALSE(leer_paquete(zip({{"paquete.json", indice}, {"a.wav", Bytes(kMaximoArchivoDeSonido + 1, 0)}})));
  CHECK_FALSE(leer_paquete(zip({{"paquete.json", texto("{\"version\":1,\"sonidos\":{\"mencion\":")}, {"a.wav", audio}})));
  CHECK_FALSE(leer_paquete(zip({{"paquete.json", texto("{\"version\":2,\"sonidos\":{}}")}})));
  CHECK_FALSE(leer_paquete(zip({{"a.wav", audio}})));
  CHECK_FALSE(leer_paquete(texto("esto no es un zip")));
  CHECK_FALSE(leer_paquete({}));
  CHECK_FALSE(leer_indice("{version:1}"));
  CHECK_FALSE(leer_indice("[1]"));
  const auto escapado = leer_indice("{\"version\":1,\"sonidos\":{\"mencion\":\"a\\\\b.wav\"}}");
  REQUIRE(escapado);
  CHECK(escapado->empty());
}

TEST_CASE("Cuándo suena cada aviso: silencios, mensajes, rechazos y No molestar") {
  using ventrik::protocolo::kSilencioAudio;
  using ventrik::protocolo::kSilencioMicrofono;
  CHECK(sonido_de_silencio(0, kSilencioMicrofono) == Sonido::microfono_silenciado);
  CHECK(sonido_de_silencio(kSilencioMicrofono, 0) == Sonido::microfono_activado);
  CHECK(sonido_de_silencio(0, kSilencioMicrofono | kSilencioAudio) == Sonido::audio_silenciado);
  CHECK(sonido_de_silencio(kSilencioMicrofono | kSilencioAudio, kSilencioMicrofono) == Sonido::audio_activado);
  CHECK_FALSE(sonido_de_silencio(1, 1));

  CHECK(sonido_de_negado({ventrik::protocolo::TipoMensaje::entrar_a_canal, ventrik::protocolo::MotivoNegado::lleno}) ==
        Sonido::error);
  CHECK_FALSE(sonido_de_negado({ventrik::protocolo::TipoMensaje::destinos_de_susurro,
                                ventrik::protocolo::MotivoNegado::no_existe}));
  CHECK_FALSE(sonido_de_negado({ventrik::protocolo::TipoMensaje::pedir_historial,
                                ventrik::protocolo::MotivoNegado::demasiado_rapido}));

  CHECK(suena_con(0, true));
  CHECK_FALSE(suena_con(kNoMolestarAlJugar, true));
  CHECK(suena_con(kNoMolestarAlJugar, false));
  CHECK_FALSE(suena_con(kNoMolestarSiempre, false));
  CHECK(es_sonido_de_voz(Sonido::transmitir_termina));
  CHECK_FALSE(es_sonido_de_voz(Sonido::susurro));

  AvisosDelServidor todos;
  todos.servidor.avisar = Avisar::todos;
  const ventrik::protocolo::MensajeChat hola{1, 3, 1, 8, "Beto", 0, "hola"};
  const ventrik::protocolo::MensajeChat a_ana{2, 3, 2, 8, "Beto", 0, "che @Ana"};
  CHECK(sonido_de_mensaje(hola, 7, "Ana", false, todos, 0) == Sonido::mensaje);
  CHECK_FALSE(sonido_de_mensaje(hola, 7, "Ana", true, todos, 0));
  CHECK_FALSE(sonido_de_mensaje(hola, 8, "Beto", false, todos, 0));
  CHECK(sonido_de_mensaje(a_ana, 7, "Ana", true, todos, 0) == Sonido::mencion);
  AvisosDelServidor nada;
  nada.servidor.avisar = Avisar::nada;
  CHECK_FALSE(sonido_de_mensaje(a_ana, 7, "Ana", false, nada, 0));
}

TEST_CASE("Mensaje nuevo en un canal de texto suena cuando su contador sube, también el primero, y no en el que mirás") {
  using namespace ventrik::protocolo;
  Presentado presentado;
  presentado.tu_id = 7;
  presentado.canales = {{1, "General"}, {3, "charla", TipoCanal::texto, false, 5}, {4, "otro", TipoCanal::texto, false, 2}};
  presentado.usuarios = {{7, "Ana", 1}};
  Sala sala(presentado);
  AvisosDelServidor todos;
  todos.servidor.avisar = Avisar::todos;
  CHECK(sonido_de_nuevos(sala, MensajesNuevos{{{3, 6}}}, todos, 0) == Sonido::mensaje);
  CHECK_FALSE(sonido_de_nuevos(sala, MensajesNuevos{{{3, 5}}}, todos, 0));
  CHECK_FALSE(sonido_de_nuevos(sala, MensajesNuevos{{{1, 9}}}, todos, 0));
  AvisosDelServidor nada;
  nada.servidor.avisar = Avisar::nada;
  CHECK_FALSE(sonido_de_nuevos(sala, MensajesNuevos{{{3, 6}}}, nada, 0));
  sala.aplicar(MensajesNuevos{{{3, 6}}});
  CHECK_FALSE(sonido_de_nuevos(sala, MensajesNuevos{{{3, 6}}}, todos, 0));
  CHECK(sonido_de_nuevos(sala, MensajesNuevos{{{3, 7}}}, todos, 0) == Sonido::mensaje);
  sala.elegir_texto(4);
  CHECK_FALSE(sonido_de_nuevos(sala, MensajesNuevos{{{4, 3}}}, todos, 0));
}

TEST_CASE("Suena cuando alguien entra o sale de tu canal, no del de otros") {
  using namespace ventrik::protocolo;
  Presentado presentado;
  presentado.tu_id = 7;
  presentado.canales = {{1, "General"}, {2, "Juegos"}};
  presentado.usuarios = {{7, "Ana", 1}, {8, "Beto", 1}, {9, "Carla", 2}};
  Sala sala(presentado);
  CHECK(sonido_de_gente(sala, UsuarioCambioCanal{9, 1}) == Sonido::alguien_entra);
  CHECK(sonido_de_gente(sala, UsuarioCambioCanal{8, 2}) == Sonido::alguien_sale);
  CHECK(sonido_de_gente(sala, UsuarioSalio{8}) == Sonido::alguien_sale);
  CHECK_FALSE(sonido_de_gente(sala, UsuarioSalio{9}));
  CHECK_FALSE(sonido_de_gente(sala, UsuarioCambioCanal{9, 0}));
  CHECK_FALSE(sonido_de_gente(sala, UsuarioCambioCanal{7, 2}));
  CHECK(sonido_de_gente(sala, UsuarioEntro{{10, "Dani", 1}}) == Sonido::alguien_entra);
  CHECK_FALSE(sonido_de_gente(sala, UsuarioEntro{{10, "Dani", 0}}));
}
