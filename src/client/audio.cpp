#include "client/audio.h"

#include <algorithm>

#include <process.h>

#include <miniaudio.h>

#include "common/pendiente.h"

VENTRIK_PENDIENTE("puente de audio WinMM para XP: falta una prueba larga (hubo cortes esporadicos en el paso 1.1)")

namespace ventrik::cliente {
namespace {
constexpr unsigned kPeriodoMs = 10;
constexpr unsigned kPedazosDeEntrada = 8;
constexpr unsigned kPedazosAdelantados = 3;
constexpr std::size_t kLugaresDeSalida = 64;

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

}

Audio::Audio() : dispositivo_(std::make_unique<ma_device>()) {}

Audio::~Audio() {
  cerrar();
}

bool Audio::abrir(unsigned frecuencia, Procesar procesar, void* datos) {
  cerrar();
  procesar_ = procesar;
  datos_ = datos;
  if (es_windows_xp()) {
    return abrir_puente(frecuencia);
  }
  ma_device_config config = ma_device_config_init(ma_device_type_duplex);
  config.capture.format = ma_format_s16;
  config.capture.channels = 1;
  config.playback.format = ma_format_s16;
  config.playback.channels = 1;
  config.sampleRate = frecuencia;
  config.periodSizeInMilliseconds = kPeriodoMs;
  config.performanceProfile = ma_performance_profile_low_latency;
  config.dataCallback = &Audio::desde_miniaudio;
  config.pUserData = this;
  if (ma_device_init(nullptr, &config, dispositivo_.get()) != MA_SUCCESS) {
    return false;
  }
  hay_miniaudio_ = true;
  if (ma_device_start(dispositivo_.get()) != MA_SUCCESS) {
    cerrar();
    return false;
  }
  return true;
}

bool Audio::abrir_puente(unsigned frecuencia) {
  WAVEFORMATEX formato = {};
  formato.wFormatTag = WAVE_FORMAT_PCM;
  formato.nChannels = 1;
  formato.nSamplesPerSec = frecuencia;
  formato.wBitsPerSample = 16;
  formato.nBlockAlign = 2;
  formato.nAvgBytesPerSec = frecuencia * 2;
  aviso_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (aviso_ == nullptr ||
      waveInOpen(&entrada_, WAVE_MAPPER, &formato, reinterpret_cast<DWORD_PTR>(aviso_), 0,
                 CALLBACK_EVENT) != MMSYSERR_NOERROR ||
      waveOutOpen(&salida_, WAVE_MAPPER, &formato, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
    cerrar();
    return false;
  }
  const std::size_t muestras = frecuencia / 1000 * kPeriodoMs;
  borrador_.assign(muestras, 0);
  pedazos_entrada_.resize(kPedazosDeEntrada);
  pedazos_salida_.resize(kLugaresDeSalida);
  for (auto* lista : {&pedazos_entrada_, &pedazos_salida_}) {
    for (Pedazo& pedazo : *lista) {
      pedazo.muestras.assign(muestras, 0);
      pedazo.cabecera.lpData = reinterpret_cast<LPSTR>(pedazo.muestras.data());
      pedazo.cabecera.dwBufferLength = static_cast<DWORD>(muestras * sizeof(std::int16_t));
    }
  }
  for (Pedazo& pedazo : pedazos_entrada_) {
    waveInPrepareHeader(entrada_, &pedazo.cabecera, sizeof(WAVEHDR));
    waveInAddBuffer(entrada_, &pedazo.cabecera, sizeof(WAVEHDR));
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
  waveInStart(entrada_);
  return true;
}

void Audio::cerrar() {
  if (hay_miniaudio_) {
    ma_device_uninit(dispositivo_.get());
    hay_miniaudio_ = false;
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
  while (seguir_) {
    WaitForSingleObject(aviso_, 100);
    while (seguir_ && terminado(pedazos_entrada_[siguiente_entrada_].cabecera)) {
      Pedazo& llego = pedazos_entrada_[siguiente_entrada_];
      const std::size_t grabadas = std::min<std::size_t>(
          llego.cabecera.dwBytesRecorded / sizeof(std::int16_t), llego.muestras.size());
      if (!algo_sonando()) {
        for (unsigned i = 1; i < kPedazosAdelantados; ++i) {
          escribir_silencio();
        }
      }
      Pedazo* sale = libre_para_salida();
      procesar_(datos_, gsl::span<const std::int16_t>(llego.muestras).first(grabadas),
                sale != nullptr ? gsl::span<std::int16_t>(sale->muestras)
                                : gsl::span<std::int16_t>(borrador_));
      if (sale != nullptr) {
        escribir(*sale);
      }
      llego.cabecera.dwFlags &= ~static_cast<DWORD>(WHDR_DONE);
      waveInAddBuffer(entrada_, &llego.cabecera, sizeof(WAVEHDR));
      siguiente_entrada_ = (siguiente_entrada_ + 1) % pedazos_entrada_.size();
    }
  }
}

Audio::Pedazo* Audio::libre_para_salida() {
  const auto libre = std::find_if(pedazos_salida_.begin(), pedazos_salida_.end(),
                                  [](const Pedazo& pedazo) { return !en_cola(pedazo.cabecera); });
  return libre == pedazos_salida_.end() ? nullptr : &*libre;
}

bool Audio::algo_sonando() const {
  return std::any_of(pedazos_salida_.begin(), pedazos_salida_.end(),
                     [](const Pedazo& pedazo) { return en_cola(pedazo.cabecera); });
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
  if (salida == nullptr || entrada == nullptr) {
    return;
  }
  auto* yo = static_cast<Audio*>(dispositivo->pUserData);
  yo->procesar_(yo->datos_,
                gsl::span<const std::int16_t>(static_cast<const std::int16_t*>(entrada), cuadros),
                gsl::span<std::int16_t>(static_cast<std::int16_t*>(salida), cuadros));
}

}
