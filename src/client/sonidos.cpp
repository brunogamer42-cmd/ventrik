#include "client/sonidos.h"

#include <algorithm>
#include <string_view>

#include <windows.h>

#include <miniaudio.h>
#include <opus.h>

#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#include "client/motor_voz.h"
#include "common/archivos.h"
#include "common/protocol/bytes.h"
#include "common/protocol/pedidos.h"

namespace ventrik::cliente {
namespace {
constexpr std::array<std::uint8_t, 4> kMarcaVksn = {'V', 'K', 'S', 'N'};
constexpr std::uint8_t kVersionVksn = 1;
constexpr opus_int32 kBitsDeSonido = 48000;
constexpr int kPrimerRecurso = 101;
constexpr std::size_t kMemoriaDeVorbis = 2 * 1024 * 1024;
constexpr auto kRetirado = static_cast<Sonido>(0xFF);

struct CerrarVorbis {
  void operator()(stb_vorbis* vorbis) const { stb_vorbis_close(vorbis); }
};

bool empieza_con(gsl::span<const std::uint8_t> datos, std::string_view marca) {
  return datos.size() >= marca.size() && std::equal(marca.begin(), marca.end(), datos.begin(),
                                                    [](char letra, std::uint8_t byte) {
                                                      return static_cast<std::uint8_t>(letra) == byte;
                                                    });
}

bool es_mp3(gsl::span<const std::uint8_t> datos) {
  return empieza_con(datos, "ID3") || (datos.size() >= 2 && datos[0] == 0xFF && (datos[1] & 0xE0) == 0xE0);
}

std::optional<Muestras> convertir(gsl::span<const std::int16_t> entrada, ma_uint32 canales, ma_uint32 frecuencia,
                                  std::size_t maximo) {
  const ma_uint64 cuadros = entrada.size() / canales;
  const ma_uint64 salen = ma_convert_frames(nullptr, 0, ma_format_s16, 1, kFrecuenciaDeSonidos, entrada.data(),
                                            cuadros, ma_format_s16, canales, frecuencia);
  if (salen == 0 || salen > maximo) {
    return std::nullopt;
  }
  Muestras muestras(static_cast<std::size_t>(salen));
  muestras.resize(static_cast<std::size_t>(ma_convert_frames(muestras.data(), salen, ma_format_s16, 1,
                                                             kFrecuenciaDeSonidos, entrada.data(), cuadros,
                                                             ma_format_s16, canales, frecuencia)));
  return muestras.empty() ? std::nullopt : std::optional<Muestras>(std::move(muestras));
}

std::optional<Muestras> de_ogg(gsl::span<const std::uint8_t> datos, std::size_t maximo) {
  std::vector<char> memoria(kMemoriaDeVorbis);
  const stb_vorbis_alloc reserva{memoria.data(), static_cast<int>(memoria.size())};
  int error = 0;
  const std::unique_ptr<stb_vorbis, CerrarVorbis> vorbis(
      stb_vorbis_open_memory(datos.data(), static_cast<int>(datos.size()), &error, &reserva));
  if (!vorbis) {
    return std::nullopt;
  }
  const stb_vorbis_info info = stb_vorbis_get_info(vorbis.get());
  if (info.channels < 1 || info.channels > 2 || info.sample_rate < 8000 || info.sample_rate > kFrecuenciaDeSonidos) {
    return std::nullopt;
  }
  const auto canales = static_cast<std::size_t>(info.channels);
  const std::uint64_t tope = (std::uint64_t{maximo} * info.sample_rate / kFrecuenciaDeSonidos + 1) * canales;
  Muestras leidas;
  std::array<short, 4096> tramo{};
  for (int cuadros = 0; (cuadros = stb_vorbis_get_samples_short_interleaved(vorbis.get(), info.channels, tramo.data(),
                                                                             static_cast<int>(tramo.size()))) > 0;) {
    leidas.insert(leidas.end(), tramo.begin(), tramo.begin() + static_cast<std::ptrdiff_t>(cuadros * canales));
    if (leidas.size() > tope) {
      return std::nullopt;
    }
  }
  return convertir(leidas, static_cast<ma_uint32>(canales), info.sample_rate, maximo);
}

std::optional<Muestras> de_miniaudio(gsl::span<const std::uint8_t> datos, ma_encoding_format formato,
                                     std::size_t maximo) {
  ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 1, kFrecuenciaDeSonidos);
  config.encodingFormat = formato;
  const auto decodificador = std::make_unique<ma_decoder>();
  if (ma_decoder_init_memory(datos.data(), datos.size(), &config, decodificador.get()) != MA_SUCCESS) {
    return std::nullopt;
  }
  const auto cerrar = gsl::finally([&decodificador] { ma_decoder_uninit(decodificador.get()); });
  Muestras muestras(maximo + 1);
  ma_uint64 leidas = 0;
  if (ma_decoder_read_pcm_frames(decodificador.get(), muestras.data(), muestras.size(), &leidas) != MA_SUCCESS ||
      leidas == 0 || leidas > maximo) {
    return std::nullopt;
  }
  muestras.resize(static_cast<std::size_t>(leidas));
  return muestras;
}

std::size_t indice(Sonido sonido) { return static_cast<std::size_t>(sonido); }

}

std::size_t maximo_de_muestras(Sonido sonido) {
  const bool llamada = sonido == Sonido::llamada_entrante || sonido == Sonido::llamando;
  return (llamada ? kMaximoSegundosDeLlamada : kMaximoSegundosDeSonido) * kFrecuenciaDeSonidos;
}

std::vector<std::uint8_t> codificar_vksn(gsl::span<const std::int16_t> muestras) {
  std::vector<std::uint64_t> memoria((static_cast<std::size_t>(opus_encoder_get_size(1)) + 7) / 8);
  auto* codificador = reinterpret_cast<OpusEncoder*>(memoria.data());
  const std::size_t paquetes = (muestras.size() + kMuestrasPorPaqueteDeSonido - 1) / kMuestrasPorPaqueteDeSonido;
  if (muestras.empty() || muestras.size() > kMaximoSegundosDeLlamada * kFrecuenciaDeSonidos ||
      opus_encoder_init(codificador, kFrecuenciaDeSonidos, 1, OPUS_APPLICATION_AUDIO) != OPUS_OK ||
      opus_encoder_ctl(codificador, OPUS_SET_BITRATE(kBitsDeSonido)) != OPUS_OK) {
    return {};
  }
  protocolo::Escritor escritor;
  escritor.escribir_bytes(kMarcaVksn);
  escritor.escribir_u8(kVersionVksn);
  escritor.escribir_u32(static_cast<std::uint32_t>(muestras.size()));
  escritor.escribir_u16(static_cast<std::uint16_t>(paquetes));
  std::array<std::int16_t, kMuestrasPorPaqueteDeSonido> pedazo{};
  std::array<std::uint8_t, protocolo::kMaximoOpus> comprimido{};
  for (std::size_t desde = 0; desde < muestras.size(); desde += pedazo.size()) {
    const auto parte = muestras.subspan(desde, std::min(pedazo.size(), muestras.size() - desde));
    std::fill(std::copy(parte.begin(), parte.end(), pedazo.begin()), pedazo.end(), std::int16_t{0});
    const opus_int32 largo = opus_encode(codificador, pedazo.data(), static_cast<int>(pedazo.size()),
                                         comprimido.data(), static_cast<opus_int32>(comprimido.size()));
    if (largo <= 0) {
      return {};
    }
    escritor.escribir_u16(static_cast<std::uint16_t>(largo));
    escritor.escribir_bytes(gsl::span<const std::uint8_t>(comprimido).first(static_cast<std::size_t>(largo)));
  }
  return escritor.ok() ? std::move(escritor).soltar() : std::vector<std::uint8_t>();
}

std::optional<Muestras> decodificar_vksn(gsl::span<const std::uint8_t> datos, std::size_t maximo) {
  protocolo::Lector lector(datos);
  const auto marca = lector.leer_bytes(kMarcaVksn.size());
  const std::uint8_t version = lector.leer_u8();
  const std::size_t cuantas = lector.leer_u32();
  const std::size_t paquetes = lector.leer_u16();
  std::vector<std::uint64_t> memoria((static_cast<std::size_t>(opus_decoder_get_size(1)) + 7) / 8);
  auto* decodificador = reinterpret_cast<OpusDecoder*>(memoria.data());
  if (!lector.ok() || !std::equal(marca.begin(), marca.end(), kMarcaVksn.begin()) || version != kVersionVksn ||
      cuantas == 0 || cuantas > maximo ||
      paquetes != (cuantas + kMuestrasPorPaqueteDeSonido - 1) / kMuestrasPorPaqueteDeSonido ||
      opus_decoder_init(decodificador, kFrecuenciaDeSonidos, 1) != OPUS_OK) {
    return std::nullopt;
  }
  Muestras muestras(paquetes * kMuestrasPorPaqueteDeSonido);
  for (std::size_t cual = 0; cual < paquetes; ++cual) {
    const std::size_t largo = lector.leer_u16();
    const auto paquete = lector.leer_bytes(largo);
    if (!lector.ok() || largo == 0 || largo > protocolo::kMaximoOpus ||
        opus_decode(decodificador, paquete.data(), static_cast<opus_int32>(largo),
                    muestras.data() + cual * kMuestrasPorPaqueteDeSonido,
                    static_cast<int>(kMuestrasPorPaqueteDeSonido), 0) != static_cast<int>(kMuestrasPorPaqueteDeSonido)) {
      return std::nullopt;
    }
  }
  if (!lector.terminado()) {
    return std::nullopt;
  }
  muestras.resize(cuantas);
  return muestras;
}

std::optional<Muestras> decodificar_archivo(gsl::span<const std::uint8_t> datos, std::size_t maximo) {
  if (datos.empty() || datos.size() > kMaximoArchivoDeSonido) {
    return std::nullopt;
  }
  if (empieza_con(datos, "OggS")) {
    return de_ogg(datos, maximo);
  }
  if (empieza_con(datos, "RIFF") && datos.size() >= 12 && empieza_con(datos.subspan(8), "WAVE")) {
    return de_miniaudio(datos, ma_encoding_format_wav, maximo);
  }
  return es_mp3(datos) ? de_miniaudio(datos, ma_encoding_format_mp3, maximo) : std::nullopt;
}

std::vector<std::uint8_t> armar_wav(gsl::span<const std::int16_t> muestras) {
  const auto bytes = static_cast<std::uint32_t>(muestras.size() * sizeof(std::int16_t));
  protocolo::Escritor escritor;
  const auto marca = [&escritor](std::string_view texto) {
    escritor.escribir_bytes(gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(texto.data()), 4));
  };
  marca("RIFF");
  escritor.escribir_u32(36 + bytes);
  marca("WAVE");
  marca("fmt ");
  escritor.escribir_u32(16);
  escritor.escribir_u16(1);
  escritor.escribir_u16(1);
  escritor.escribir_u32(kFrecuenciaDeSonidos);
  escritor.escribir_u32(kFrecuenciaDeSonidos * 2);
  escritor.escribir_u16(2);
  escritor.escribir_u16(16);
  marca("data");
  escritor.escribir_u32(bytes);
  for (const std::int16_t muestra : muestras) {
    escritor.escribir_u16(static_cast<std::uint16_t>(muestra));
  }
  return std::move(escritor).soltar();
}

gsl::span<const std::uint8_t> sonido_de_fabrica(Sonido sonido) {
  const HMODULE modulo = GetModuleHandleW(nullptr);
  const HRSRC recurso = FindResourceW(modulo, MAKEINTRESOURCEW(kPrimerRecurso + static_cast<int>(indice(sonido))),
                                      RT_RCDATA);
  const HGLOBAL cargado = recurso != nullptr ? LoadResource(modulo, recurso) : nullptr;
  const void* datos = cargado != nullptr ? LockResource(cargado) : nullptr;
  return datos == nullptr ? gsl::span<const std::uint8_t>()
                          : gsl::span<const std::uint8_t>(static_cast<const std::uint8_t*>(datos),
                                                          SizeofResource(modulo, recurso));
}

std::wstring archivo_propio(const std::wstring& carpeta, Sonido sonido) {
  return carpeta + L"\\sonidos\\" + (indice(sonido) < 9 ? L"0" : L"") + std::to_wstring(indice(sonido) + 1) + L".vksn";
}

bool Sonidos::sonar(Sonido sonido, int volumen, bool propio, const std::string& salida, Reloj::time_point ahora) {
  const auto muestras = volumen > 0 ? cargar(sonido, propio) : nullptr;
  if (!muestras) {
    return false;
  }
  if (salida_ && abierta_en_ != salida) {
    cerrar();
  }
  if (!salida_) {
    salida_ = std::make_unique<Audio>();
    if (!salida_->abrir(kFrecuenciaDeSonidos, &Sonidos::procesar, this, ElegidosDeAudio{{}, salida}, false)) {
      salida_.reset();
      return false;
    }
    abierta_en_ = salida;
  }
  if (!poner(*muestras, static_cast<float>(volumen) / 100)) {
    return false;
  }
  if (std::none_of(en_uso_.begin(), en_uso_.end(), [&muestras](const auto& usado) { return usado.second == muestras; })) {
    en_uso_.emplace_back(sonido, muestras);
  }
  const auto dura = std::chrono::milliseconds(muestras->size() * 1000 / kFrecuenciaDeSonidos);
  cerrar_en_ = std::max(cerrar_en_.value_or(ahora), ahora + dura + kCerrarTrasSonar);
  return true;
}

void Sonidos::atender(Reloj::time_point ahora) {
  if (cerrar_en_ && ahora >= *cerrar_en_) {
    cerrar();
  }
}

void Sonidos::cerrar() {
  salida_.reset();
  Sonando viejo;
  while (cola_.sacar(viejo)) {
  }
  sonando_ = {};
  en_uso_.clear();
  cerrar_en_.reset();
}

void Sonidos::olvidar(Sonido sonido) {
  for (auto& usado : en_uso_) {
    usado.first = usado.first == sonido ? kRetirado : usado.first;
  }
  guardados_.erase(std::remove_if(guardados_.begin(), guardados_.end(),
                                  [sonido](const auto& guardado) { return guardado.first == sonido; }),
                   guardados_.end());
}

void Sonidos::soltar() { guardados_.clear(); }

std::size_t Sonidos::guardados() const {
  std::size_t total = 0;
  for (const auto& guardado : guardados_) {
    total += guardado.second->size() * sizeof(std::int16_t);
  }
  return total;
}

bool Sonidos::poner(gsl::span<const std::int16_t> muestras, float volumen) {
  return cola_.poner(Sonando{muestras.data(), muestras.size(), 0, volumen});
}

std::shared_ptr<const Muestras> Sonidos::cargar(Sonido sonido, bool propio) {
  const auto guardado = std::find_if(guardados_.begin(), guardados_.end(),
                                     [sonido](const auto& cual) { return cual.first == sonido; });
  if (guardado != guardados_.end()) {
    std::rotate(guardado, guardado + 1, guardados_.end());
    return guardados_.back().second;
  }
  const auto usado = std::find_if(en_uso_.begin(), en_uso_.end(), [sonido](const auto& cual) { return cual.first == sonido; });
  if (usado != en_uso_.end()) {
    return usado->second;
  }
  std::optional<Muestras> muestras;
  if (propio) {
    cifrado::Secreto contenido;
    if (leer_archivo(archivo_propio(carpeta_, sonido), kMaximoArchivoDeSonido, contenido) == LecturaArchivo::leido) {
      muestras = decodificar_vksn(contenido.bytes(), maximo_de_muestras(sonido));
    }
  }
  if (!muestras) {
    muestras = decodificar_vksn(sonido_de_fabrica(sonido), maximo_de_muestras(sonido));
  }
  if (!muestras) {
    return nullptr;
  }
  auto nuevo = std::make_shared<const Muestras>(std::move(*muestras));
  guardados_.emplace_back(sonido, nuevo);
  while (guardados() > kTopeDeSonidosGuardados && guardados_.size() > 1) {
    guardados_.erase(guardados_.begin());
  }
  return nuevo;
}

void Sonidos::procesar(void* yo, gsl::span<const std::int16_t>, gsl::span<std::int16_t> salida) {
  auto& sonidos = *static_cast<Sonidos*>(yo);
  Sonando nuevo;
  while (sonidos.cola_.sacar(nuevo)) {
    const auto libre = std::find_if(sonidos.sonando_.begin(), sonidos.sonando_.end(),
                                    [](const Sonando& sonando) { return sonando.datos == nullptr; });
    if (libre != sonidos.sonando_.end()) {
      *libre = nuevo;
    }
  }
  while (!salida.empty()) {
    const auto parte = salida.first(std::min(salida.size(), sonidos.mezcla_.size()));
    const auto mezcla = gsl::make_span(sonidos.mezcla_).first(parte.size());
    std::fill(mezcla.begin(), mezcla.end(), 0.0f);
    for (Sonando& sonando : sonidos.sonando_) {
      const std::size_t tramo = sonando.datos == nullptr ? 0 : std::min(parte.size(), sonando.largo - sonando.posicion);
      for (std::size_t i = 0; i < tramo; ++i) {
        mezcla[i] += static_cast<float>(sonando.datos[sonando.posicion + i]) * sonando.volumen;
      }
      sonando.posicion += tramo;
      if (sonando.datos != nullptr && sonando.posicion == sonando.largo) {
        sonando = {};
      }
    }
    limitar(mezcla, parte, sonidos.limite_);
    salida = salida.subspan(parte.size());
  }
}

}
