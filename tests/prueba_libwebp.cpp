#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <doctest/doctest.h>
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>

TEST_CASE("libwebp es la versión 1.6.0") {
  CHECK(WebPGetDecoderVersion() == 0x010600);
  CHECK(WebPGetEncoderVersion() == 0x010600);
}

TEST_CASE("libwebp comprime sin pérdida y vuelve a leer la misma imagen") {
  constexpr int kLado = 4;
  std::vector<std::uint8_t> original(kLado * kLado * 4);
  for (std::size_t i = 0; i < original.size(); ++i) {
    // Colores variados y todo opaco (lo transparente puede cambiar de color).
    original[i] = (i % 4 == 3) ? 255 : static_cast<std::uint8_t>(i * 37);
  }

  std::uint8_t* comprimido = nullptr;
  const std::size_t largo =
      WebPEncodeLosslessRGBA(original.data(), kLado, kLado, kLado * 4, &comprimido);
  REQUIRE(largo > 0);
  REQUIRE(comprimido != nullptr);

  int ancho = 0;
  int alto = 0;
  std::uint8_t* leido = WebPDecodeRGBA(comprimido, largo, &ancho, &alto);
  REQUIRE(leido != nullptr);
  CHECK(ancho == kLado);
  CHECK(alto == kLado);
  CHECK(std::equal(original.begin(), original.end(), leido));
  WebPFree(leido);

  // El lector de animaciones también la entiende, como un solo cuadro.
  const WebPData datos = {comprimido, largo};
  WebPAnimDecoderOptions opciones;
  REQUIRE(WebPAnimDecoderOptionsInit(&opciones));
  WebPAnimDecoder* animacion = WebPAnimDecoderNew(&datos, &opciones);
  REQUIRE(animacion != nullptr);
  WebPAnimInfo info;
  CHECK(WebPAnimDecoderGetInfo(animacion, &info));
  CHECK(info.frame_count == 1u);
  WebPAnimDecoderDelete(animacion);

  WebPFree(comprimido);
}

TEST_CASE("libwebp rechaza datos rotos") {
  const std::uint8_t basura[] = "RIFF\x10\x00\x00\x00WEBPVP8 roto";
  int ancho = 0;
  int alto = 0;
  CHECK(WebPDecodeRGBA(basura, sizeof(basura), &ancho, &alto) == nullptr);
}
