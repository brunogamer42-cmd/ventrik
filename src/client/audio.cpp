#include "client/audio.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include <process.h>

#include <miniaudio.h>

#include "client/guardados.h"
#include "common/pendiente.h"
#include "common/sistema.h"
#include "common/utf.h"

VENTRIK_PENDIENTE("puente de audio WinMM para XP: falta una prueba larga (hubo cortes esporadicos en el paso 1.1)")
VENTRIK_PENDIENTE("cerrar los parlantes tras un rato sin voz (12.2): falta medir en una PC real cuanto tardan en volver")

namespace ventrik::cliente {
namespace {
#if defined(VENTRIK_CLASICA)
constexpr unsigned kPeriodoMs = 20;
#else
constexpr unsigned kPeriodoMs = 10;
#endif
constexpr unsigned kEntradaMs = 80;
constexpr unsigned kAdelantoMs = 30;
constexpr unsigned kSalidaMs = 640;
constexpr unsigned kPedazosDeEntrada = kEntradaMs / kPeriodoMs;
constexpr unsigned kPedazosAdelantados = (kAdelantoMs + kPeriodoMs - 1) / kPeriodoMs;
constexpr std::size_t kLugaresDeSalida = kSalidaMs / kPeriodoMs;
constexpr std::uint32_t kMaximoEnEsperaMs = 120;
constexpr std::uint32_t kTopeEnEsperaMs = 250;
constexpr int kCasiSilencio = 1000;

bool es_windows_xp() {
  OSVERSIONINFOEXW pedido = {};
  pedido.dwOSVersionInfoSize = sizeof(pedido);
  pedido.dwMajorVersion = 6;
  return VerifyVersionInfoW(&pedido, VER_MAJORVERSION,
                            VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL)) == FALSE;
}

bool en_cola(const WAVEHDR& cabecera) {
  return (*static_cast<const volatile DWORD*>(&cabecera.dwFlags) & WHDR_INQUEUE) != 0;
}

bool terminado(const WAVEHDR& cabecera) {
  return (*static_cast<const volatile DWORD*>(&cabecera.dwFlags) & WHDR_DONE) != 0;
}

void cargar_bibliotecas() {
  for (const wchar_t* dll : {L"avrt.dll", L"mmdevapi.dll", L"dsound.dll"}) {
    cargar_de_sistema(dll);
  }
}

template <typename Capacidades, typename Contar, typename Leer>
std::vector<std::string> nombres_de(Contar contar, Leer leer) {
  std::vector<std::string> nombres;
  for (UINT i = 0; i < contar(); ++i) {
    Capacidades capacidades = {};
    if (leer(i, &capacidades, sizeof(capacidades)) == MMSYSERR_NOERROR) {
      nombres.push_back(a_utf8(capacidades.szPname));
    }
  }
  return nombres;
}

UINT indice_de(const std::vector<std::string>& nombres, const std::string& nombre) {
  const auto encontrado = std::find(nombres.begin(), nombres.end(), nombre);
  return nombre.empty() || encontrado == nombres.end() ? WAVE_MAPPER
                                                       : static_cast<UINT>(encontrado - nombres.begin());
}

ListaDeAudio de_winmm() {
  return {nombres_de<WAVEINCAPSW>(waveInGetNumDevs, waveInGetDevCapsW),
          nombres_de<WAVEOUTCAPSW>(waveOutGetNumDevs, waveOutGetDevCapsW)};
}

const ma_device_id* id_de(const ma_device_info* lista, ma_uint32 cuantos, const std::string& nombre) {
  for (ma_uint32 i = 0; i < cuantos && !nombre.empty(); ++i) {
    if (nombre == lista[i].name) {
      return &lista[i].id;
    }
  }
  return nullptr;
}

bool callado(gsl::span<const std::int16_t> pedazo) {
  return std::all_of(pedazo.begin(), pedazo.end(),
                     [](std::int16_t muestra) { return muestra > -kCasiSilencio && muestra < kCasiSilencio; });
}

}

bool tirar_pedazo(std::uint32_t espera_ms, gsl::span<const std::int16_t> pedazo) {
  return espera_ms >= kTopeEnEsperaMs || (espera_ms >= kMaximoEnEsperaMs && callado(pedazo));
}

ListaDeAudio dispositivos_de_audio() {
  if (es_windows_xp()) {
    return de_winmm();
  }
  cargar_bibliotecas();
  ma_context contexto;
  if (ma_context_init(nullptr, 0, nullptr, &contexto) != MA_SUCCESS) {
    return {};
  }
  ma_device_info* salidas = nullptr;
  ma_device_info* entradas = nullptr;
  ma_uint32 cuantas_salidas = 0;
  ma_uint32 cuantas_entradas = 0;
  ListaDeAudio lista;
  const auto agregar = [](std::vector<std::string>& nombres, std::string_view nombre) {
    if (!nombre.empty() && nombre.size() <= kMaximoNombreDeDispositivo && es_utf8_valido(nombre)) {
      nombres.emplace_back(nombre);
    }
  };
  if (ma_context_get_devices(&contexto, &salidas, &cuantas_salidas, &entradas, &cuantas_entradas) == MA_SUCCESS) {
    for (ma_uint32 i = 0; i < cuantas_entradas; ++i) {
      agregar(lista.microfonos, entradas[i].name);
    }
    for (ma_uint32 i = 0; i < cuantas_salidas; ++i) {
      agregar(lista.salidas, salidas[i].name);
    }
  }
  ma_context_uninit(&contexto);
  return lista;
}

Audio::Audio() : dispositivo_(std::make_unique<ma_device>()), contexto_(std::make_unique<ma_context>()) {}

Audio::~Audio() {
  cerrar();
}

bool Audio::abrir(unsigned frecuencia, Procesar procesar, void* datos, const ElegidosDeAudio& elegidos,
                  bool con_microfono) {
  cerrar();
  procesar_ = procesar;
  datos_ = datos;
  if (es_windows_xp()) {
    return abrir_puente(frecuencia, elegidos, con_microfono);
  }
  ma_device_config config = ma_device_config_init(con_microfono ? ma_device_type_duplex : ma_device_type_playback);
  config.capture.format = ma_format_s16;
  config.capture.channels = 1;
  config.playback.format = ma_format_s16;
  config.playback.channels = 1;
  config.sampleRate = frecuencia;
  config.periodSizeInMilliseconds = kPeriodoMs;
  config.performanceProfile = ma_performance_profile_low_latency;
  config.wasapi.usage = ma_wasapi_usage_pro_audio;
  config.dataCallback = &Audio::desde_miniaudio;
  config.pUserData = this;
  cargar_bibliotecas();
  if (ma_context_init(nullptr, 0, nullptr, contexto_.get()) != MA_SUCCESS) {
    return false;
  }
  hay_contexto_ = true;
  ma_device_info* salidas = nullptr;
  ma_device_info* entradas = nullptr;
  ma_uint32 cuantas_salidas = 0;
  ma_uint32 cuantas_entradas = 0;
  if ((!elegidos.microfono.empty() || !elegidos.salida.empty()) &&
      ma_context_get_devices(contexto_.get(), &salidas, &cuantas_salidas, &entradas, &cuantas_entradas) ==
          MA_SUCCESS) {
    config.capture.pDeviceID = id_de(entradas, cuantas_entradas, elegidos.microfono);
    config.playback.pDeviceID = id_de(salidas, cuantas_salidas, elegidos.salida);
  }
  if (ma_device_init(contexto_.get(), &config, dispositivo_.get()) != MA_SUCCESS) {
    cerrar();
    return false;
  }
  hay_miniaudio_ = true;
  hilo_listo_ = false;
  fija_.fijar({{this, sizeof(*this)}, {dispositivo_.get(), sizeof(ma_device)}, {contexto_.get(), sizeof(ma_context)}});
  if (ma_device_start(dispositivo_.get()) != MA_SUCCESS) {
    cerrar();
    return false;
  }
  return true;
}

bool Audio::abrir_puente(unsigned frecuencia, const ElegidosDeAudio& elegidos, bool con_microfono) {
  const ListaDeAudio dispositivos = de_winmm();
  WAVEFORMATEX formato = {};
  formato.wFormatTag = WAVE_FORMAT_PCM;
  formato.nChannels = 1;
  formato.nSamplesPerSec = frecuencia;
  formato.wBitsPerSample = 16;
  formato.nBlockAlign = 2;
  formato.nAvgBytesPerSec = frecuencia * 2;
  aviso_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (aviso_ == nullptr ||
      (con_microfono && waveInOpen(&entrada_, indice_de(dispositivos.microfonos, elegidos.microfono), &formato,
                                   reinterpret_cast<DWORD_PTR>(aviso_), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) ||
      waveOutOpen(&salida_, indice_de(dispositivos.salidas, elegidos.salida), &formato,
                  con_microfono ? 0 : reinterpret_cast<DWORD_PTR>(aviso_), 0,
                  con_microfono ? CALLBACK_NULL : CALLBACK_EVENT) != MMSYSERR_NOERROR) {
    cerrar();
    return false;
  }
  const std::size_t muestras = frecuencia / 1000 * kPeriodoMs;
  pedazos_entrada_.resize(kPedazosDeEntrada);
  pedazos_salida_.resize(kLugaresDeSalida);
  muestras_.assign(muestras * (kPedazosDeEntrada + kLugaresDeSalida + 1), 0);
  gsl::span<std::int16_t> libres(muestras_);
  for (auto* lista : {&pedazos_entrada_, &pedazos_salida_}) {
    for (Pedazo& pedazo : *lista) {
      pedazo.muestras = libres.first(muestras);
      libres = libres.subspan(muestras);
      pedazo.cabecera.lpData = reinterpret_cast<LPSTR>(pedazo.muestras.data());
      pedazo.cabecera.dwBufferLength = static_cast<DWORD>(muestras * sizeof(std::int16_t));
    }
  }
  borrador_ = libres;
  fija_.fijar({{this, sizeof(*this)},
               {muestras_.data(), muestras_.size() * sizeof(std::int16_t)},
               {pedazos_entrada_.data(), pedazos_entrada_.size() * sizeof(Pedazo)},
               {pedazos_salida_.data(), pedazos_salida_.size() * sizeof(Pedazo)}});
  for (Pedazo& pedazo : pedazos_entrada_) {
    if (entrada_ != nullptr) {
      waveInPrepareHeader(entrada_, &pedazo.cabecera, sizeof(WAVEHDR));
      waveInAddBuffer(entrada_, &pedazo.cabecera, sizeof(WAVEHDR));
    }
  }
  for (Pedazo& pedazo : pedazos_salida_) {
    waveOutPrepareHeader(salida_, &pedazo.cabecera, sizeof(WAVEHDR));
  }
  for (unsigned i = 0; i < kPedazosAdelantados; ++i) {
    escribir_silencio();
  }
  seguir_ = true;
  hilo_ = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, &Audio::hilo, this, 0, nullptr));
  if (hilo_ == nullptr) {
    cerrar();
    return false;
  }
  SetThreadPriority(hilo_, THREAD_PRIORITY_TIME_CRITICAL);
  if (entrada_ != nullptr) {
    waveInStart(entrada_);
  }
  return true;
}

void Audio::cerrar() {
  fija_.soltar();
  if (hay_miniaudio_) {
    ma_device_uninit(dispositivo_.get());
    hay_miniaudio_ = false;
  }
  if (hay_contexto_) {
    ma_context_uninit(contexto_.get());
    hay_contexto_ = false;
  }
  seguir_ = false;
  if (hilo_ != nullptr) {
    SetEvent(aviso_);
    WaitForSingleObject(hilo_, INFINITE);
    CloseHandle(hilo_);
    hilo_ = nullptr;
  }
  if (entrada_ != nullptr) {
    waveInReset(entrada_);
    for (Pedazo& pedazo : pedazos_entrada_) {
      waveInUnprepareHeader(entrada_, &pedazo.cabecera, sizeof(WAVEHDR));
    }
    waveInClose(entrada_);
    entrada_ = nullptr;
  }
  if (salida_ != nullptr) {
    waveOutReset(salida_);
    for (Pedazo& pedazo : pedazos_salida_) {
      waveOutUnprepareHeader(salida_, &pedazo.cabecera, sizeof(WAVEHDR));
    }
    waveOutClose(salida_);
    salida_ = nullptr;
  }
  if (aviso_ != nullptr) {
    CloseHandle(aviso_);
    aviso_ = nullptr;
  }
  pedazos_entrada_.clear();
  pedazos_salida_.clear();
  siguiente_entrada_ = 0;
}

unsigned __stdcall Audio::hilo(void* yo) {
  static_cast<Audio*>(yo)->atender();
  return 0;
}

void Audio::atender() {
  no_frenar_este_hilo();
  while (seguir_) {
    WaitForSingleObject(aviso_, 100);
    despertares_.fetch_add(1, std::memory_order_relaxed);
    if (entrada_ == nullptr) {
      solo_salida();
      continue;
    }
    while (seguir_ && terminado(pedazos_entrada_[siguiente_entrada_].cabecera)) {
      Pedazo& llego = pedazos_entrada_[siguiente_entrada_];
      const std::size_t grabadas = std::min<std::size_t>(
          llego.cabecera.dwBytesRecorded / sizeof(std::int16_t), llego.muestras.size());
      const auto en_espera = std::count_if(pedazos_salida_.begin(), pedazos_salida_.end(),
                                           [](const Pedazo& pedazo) { return en_cola(pedazo.cabecera); });
      if (en_espera == 0) {
        vacios_.fetch_add(1, std::memory_order_relaxed);
        for (unsigned i = 1; i < kPedazosAdelantados; ++i) {
          escribir_silencio();
        }
      }
      const auto espera_ms = static_cast<std::uint32_t>(en_espera * kPeriodoMs);
      espera_de_salida_ms_ = espera_ms;
      espera_maxima_ms_.store(std::max(espera_maxima_ms_.load(std::memory_order_relaxed), espera_ms),
                              std::memory_order_relaxed);
      Pedazo* sale = libre_para_salida();
      const gsl::span<std::int16_t> destino = sale != nullptr ? sale->muestras : borrador_;
      procesar_(datos_, gsl::span<const std::int16_t>(llego.muestras).first(grabadas), destino);
      if (sale != nullptr && !tirar_pedazo(espera_ms, destino)) {
        escribir(*sale);
      } else {
        descartados_.fetch_add(1, std::memory_order_relaxed);
        descartados_con_voz_.fetch_add(callado(destino) ? 0 : 1, std::memory_order_relaxed);
      }
      llego.cabecera.dwFlags &= ~static_cast<DWORD>(WHDR_DONE);
      waveInAddBuffer(entrada_, &llego.cabecera, sizeof(WAVEHDR));
      siguiente_entrada_ = (siguiente_entrada_ + 1) % pedazos_entrada_.size();
    }
  }
}

void Audio::solo_salida() {
  for (Pedazo* sale = libre_para_salida(); seguir_ && sale != nullptr; sale = libre_para_salida()) {
    procesar_(datos_, {}, sale->muestras);
    escribir(*sale);
  }
}

Audio::Pedazo* Audio::libre_para_salida() {
  const auto libre = std::find_if(pedazos_salida_.begin(), pedazos_salida_.end(),
                                  [](const Pedazo& pedazo) { return !en_cola(pedazo.cabecera); });
  return libre == pedazos_salida_.end() ? nullptr : &*libre;
}

void Audio::escribir(Pedazo& pedazo) {
  waveOutWrite(salida_, &pedazo.cabecera, sizeof(WAVEHDR));
}

void Audio::escribir_silencio() {
  if (Pedazo* pedazo = libre_para_salida()) {
    std::fill(pedazo->muestras.begin(), pedazo->muestras.end(), std::int16_t{0});
    escribir(*pedazo);
  }
}

void Audio::desde_miniaudio(ma_device* dispositivo, void* salida, const void* entrada,
                            std::uint32_t cuadros) {
  if (salida == nullptr) {
    return;
  }
  auto* yo = static_cast<Audio*>(dispositivo->pUserData);
  if (!std::exchange(yo->hilo_listo_, true)) {
    no_frenar_este_hilo();
  }
  yo->despertares_.fetch_add(1, std::memory_order_relaxed);
  yo->procesar_(yo->datos_,
                entrada == nullptr ? gsl::span<const std::int16_t>()
                                   : gsl::span<const std::int16_t>(static_cast<const std::int16_t*>(entrada), cuadros),
                gsl::span<std::int16_t>(static_cast<std::int16_t*>(salida), cuadros));
}

}
