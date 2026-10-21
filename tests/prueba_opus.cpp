#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <opus.h>

TEST_CASE("Opus es la versión 1.5.2") {
  CHECK(std::string(opus_get_version_string()).find("1.5.2") != std::string::npos);
}

TEST_CASE("Opus comprime y descomprime 20 ms de voz a 24 kbps") {
  constexpr int kFrecuencia = 48000;
  constexpr int kMuestras = 960;  // 20 ms a 48 kHz
  int error = 0;

  OpusEncoder* codificador =
      opus_encoder_create(kFrecuencia, 1, OPUS_APPLICATION_VOIP, &error);
  REQUIRE(error == OPUS_OK);
  REQUIRE(codificador != nullptr);
  CHECK(opus_encoder_ctl(codificador, OPUS_SET_BITRATE(24000)) == OPUS_OK);
  opus_int32 kbps = 0;
  CHECK(opus_encoder_ctl(codificador, OPUS_GET_BITRATE(&kbps)) == OPUS_OK);
  CHECK(kbps == 24000);

  // Un tono de 440 Hz, como una voz sostenida.
  std::vector<opus_int16> entrada(kMuestras);
  for (std::size_t i = 0; i < entrada.size(); ++i) {
    entrada[i] = static_cast<opus_int16>(
        8000.0 * std::sin(2.0 * 3.14159265358979 * 440.0 *
                          static_cast<double>(i) / kFrecuencia));
  }
  std::vector<unsigned char> paquete(1500);
  const opus_int32 largo =
      opus_encode(codificador, entrada.data(), kMuestras, paquete.data(),
                  static_cast<opus_int32>(paquete.size()));
  opus_encoder_destroy(codificador);
  CHECK(largo > 0);
  CHECK(largo < 200);  // a 24 kbps, 20 ms son unos 60 bytes

  OpusDecoder* decodificador = opus_decoder_create(kFrecuencia, 1, &error);
  REQUIRE(error == OPUS_OK);
  REQUIRE(decodificador != nullptr);
  std::vector<opus_int16> salida(kMuestras);
  const int muestras =
      opus_decode(decodificador, paquete.data(), largo, salida.data(), kMuestras, 0);
  opus_decoder_destroy(decodificador);
  CHECK(muestras == kMuestras);
}
