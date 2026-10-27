#include "client/limpieza.h"

#include <algorithm>
#include <cstdlib>

#include <rnnoise.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

namespace ventrik::cliente {
namespace {
constexpr unsigned kFrecuenciaDeRnnoise = 48000;
constexpr float kNoEsVoz = 0.5f;
constexpr float kBajaSinVoz = 0.3f;
constexpr float kNivelDelVolumen = 12000;
constexpr spx_int32_t kGananciaMaxima = 20;

spx_int32_t supresion(Ruido ruido) {
  return ruido == Ruido::baja ? -15 : ruido == Ruido::media ? -25 : -35;
}

int pico_de(gsl::span<const std::int16_t> muestras) {
  int pico = 0;
  for (const std::int16_t muestra : muestras) {
    pico = std::max(pico, std::abs(static_cast<int>(muestra)));
  }
  return pico;
}

}

Limpieza::Limpieza(unsigned frecuencia) : bloque_(std::min<std::size_t>(frecuencia / 100, 480)) {
  spx_int32_t tasa = static_cast<spx_int32_t>(frecuencia);
  eco_ = speex_echo_state_init(static_cast<int>(bloque_), static_cast<int>(frecuencia / 5));
  speex_echo_ctl(eco_, SPEEX_ECHO_SET_SAMPLING_RATE, &tasa);
  ruido_previo_ = speex_preprocess_state_init(static_cast<int>(bloque_), static_cast<int>(frecuencia));
  volumen_previo_ = speex_preprocess_state_init(static_cast<int>(bloque_), static_cast<int>(frecuencia));
  spx_int32_t no = 0;
  spx_int32_t si = 1;
  float nivel = kNivelDelVolumen;
  spx_int32_t maxima = kGananciaMaxima;
  speex_preprocess_ctl(volumen_previo_, SPEEX_PREPROCESS_SET_DENOISE, &no);
  speex_preprocess_ctl(volumen_previo_, SPEEX_PREPROCESS_SET_AGC, &si);
  speex_preprocess_ctl(volumen_previo_, SPEEX_PREPROCESS_SET_AGC_LEVEL, &nivel);
  speex_preprocess_ctl(volumen_previo_, SPEEX_PREPROCESS_SET_AGC_MAX_GAIN, &maxima);
  aplicar(Ruido::apagado);
  if (frecuencia == kFrecuenciaDeRnnoise) {
    rnnoise_.resize((static_cast<std::size_t>(rnnoise_get_size()) + 7) / 8);
    if (rnnoise_init(reinterpret_cast<DenoiseState*>(rnnoise_.data()), nullptr) != 0) {
      rnnoise_.clear();
    }
  }
}

Limpieza::~Limpieza() {
  speex_echo_state_destroy(eco_);
  speex_preprocess_state_destroy(ruido_previo_);
  speex_preprocess_state_destroy(volumen_previo_);
}

void Limpieza::usar(const Limpiar& limpiar) {
  ruido_ = limpiar.ruido;
  eco_prendido_ = limpiar.eco;
  volumen_ = limpiar.volumen;
}

void Limpieza::escuchar(gsl::span<const std::int16_t> salida) {
  if (eco_prendido_ && salida.size() == bloque_) {
    speex_echo_playback(eco_, salida.data());
  }
}

void Limpieza::aplicar(Ruido ruido) {
  spx_int32_t quitar = ruido != Ruido::apagado ? 1 : 0;
  spx_int32_t cuanto = supresion(ruido);
  speex_preprocess_ctl(ruido_previo_, SPEEX_PREPROCESS_SET_DENOISE, &quitar);
  speex_preprocess_ctl(ruido_previo_, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &cuanto);
  ruido_puesto_ = ruido;
}

int Limpieza::limpiar(gsl::span<std::int16_t> captura) {
  if (captura.size() != bloque_) {
    return pico_de(captura);
  }
  const Ruido ruido = ruido_;
  if (ruido != ruido_puesto_) {
    aplicar(ruido);
  }
  if (eco_prendido_) {
    speex_echo_capture(eco_, captura.data(), sin_eco_.data());
    std::copy_n(sin_eco_.begin(), captura.size(), captura.begin());
  }
  if (!rnnoise_.empty() && ruido >= Ruido::media) {
    std::copy(captura.begin(), captura.end(), entrada_.begin());
    const float voz =
        rnnoise_process_frame(reinterpret_cast<DenoiseState*>(rnnoise_.data()), salida_.data(), entrada_.data());
    const float objetivo = ruido == Ruido::alta && voz < kNoEsVoz ? kBajaSinVoz : 1.0f;
    const float paso = (objetivo - ganancia_) / static_cast<float>(captura.size());
    for (std::size_t i = 0; i < captura.size(); ++i) {
      ganancia_ += paso;
      captura[i] = static_cast<std::int16_t>(std::clamp(salida_[i] * ganancia_, -32768.0f, 32767.0f));
    }
    ganancia_ = objetivo;
  } else if (ruido != Ruido::apagado) {
    speex_preprocess_run(ruido_previo_, captura.data());
  }
  const int pico = pico_de(captura);
  if (volumen_) {
    speex_preprocess_run(volumen_previo_, captura.data());
  }
  return pico;
}

}
