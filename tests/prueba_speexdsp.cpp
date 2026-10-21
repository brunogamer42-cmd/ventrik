#include <algorithm>
#include <vector>

#include <doctest/doctest.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#include <speex/speex_resampler.h>

namespace {

constexpr int kMuestras = 480;  // 10 ms a 48 kHz

}  // namespace

TEST_CASE("SpeexDSP cancela el eco de un pedazo de 10 ms") {
  SpeexEchoState* eco = speex_echo_state_init(kMuestras, kMuestras * 10);
  REQUIRE(eco != nullptr);
  spx_int32_t frecuencia = 48000;
  CHECK(speex_echo_ctl(eco, SPEEX_ECHO_SET_SAMPLING_RATE, &frecuencia) == 0);

  // Sin sonido en el micrófono ni en los parlantes, sale silencio.
  std::vector<spx_int16_t> microfono(kMuestras, 0);
  std::vector<spx_int16_t> parlantes(kMuestras, 0);
  std::vector<spx_int16_t> salida(kMuestras, 1);
  speex_echo_cancellation(eco, microfono.data(), parlantes.data(), salida.data());
  CHECK(std::all_of(salida.begin(), salida.end(), [](spx_int16_t m) { return m == 0; }));
  speex_echo_state_destroy(eco);
}

TEST_CASE("SpeexDSP preprocesa un pedazo de 10 ms") {
  SpeexPreprocessState* previo = speex_preprocess_state_init(kMuestras, 48000);
  REQUIRE(previo != nullptr);
  int activar = 1;
  CHECK(speex_preprocess_ctl(previo, SPEEX_PREPROCESS_SET_DENOISE, &activar) == 0);
  std::vector<spx_int16_t> pedazo(kMuestras, 100);
  speex_preprocess_run(previo, pedazo.data());
  speex_preprocess_state_destroy(previo);
}

TEST_CASE("SpeexDSP pasa de 48 kHz a 16 kHz") {
  int error = 0;
  SpeexResamplerState* cambio =
      speex_resampler_init(1, 48000, 16000, SPEEX_RESAMPLER_QUALITY_VOIP, &error);
  REQUIRE(error == RESAMPLER_ERR_SUCCESS);
  REQUIRE(cambio != nullptr);
  std::vector<spx_int16_t> entrada(kMuestras, 1000);
  std::vector<spx_int16_t> salida(kMuestras);
  spx_uint32_t largo_entrada = kMuestras;
  spx_uint32_t largo_salida = kMuestras;
  CHECK(speex_resampler_process_int(cambio, 0, entrada.data(), &largo_entrada,
                                    salida.data(), &largo_salida) == RESAMPLER_ERR_SUCCESS);
  CHECK(largo_entrada == static_cast<spx_uint32_t>(kMuestras));
  CHECK(largo_salida > 0u);
  CHECK(largo_salida <= 160u);  // un tercio
  speex_resampler_destroy(cambio);
}
