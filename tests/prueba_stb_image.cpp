#include <doctest/doctest.h>
#include <stb_image.h>

namespace {

// El GIF más chico posible: 1x1, un solo píxel transparente (43 bytes).
const unsigned char kGifTransparente[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61,              // "GIF89a"
    0x01, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00,        // 1x1, con paleta de 2 colores
    0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,              // paleta: blanco y negro
    0x21, 0xF9, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00,  // el color 0 es transparente
    0x2C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,  // la imagen
    0x02, 0x02, 0x44, 0x01, 0x00,                    // el píxel, comprimido
    0x3B};                                           // fin

}  // namespace

TEST_CASE("stb_image lee un GIF de 1x1") {
  int ancho = 0;
  int alto = 0;
  int canales = 0;
  unsigned char* pixeles = stbi_load_from_memory(
      kGifTransparente, static_cast<int>(sizeof(kGifTransparente)), &ancho,
      &alto, &canales, 4);
  REQUIRE(pixeles != nullptr);
  CHECK(ancho == 1);
  CHECK(alto == 1);
  CHECK(pixeles[3] == 0);  // transparente
  stbi_image_free(pixeles);
}

TEST_CASE("stb_image rechaza lo que no es un GIF entero") {
  int ancho = 0;
  int alto = 0;
  int canales = 0;
  // Un PNG: stb_image está compilado solo para GIF.
  const unsigned char png[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n',
                               0x00, 0x00, 0x00, 0x0D, 'I',  'H',  'D',  'R'};
  CHECK(stbi_load_from_memory(png, static_cast<int>(sizeof(png)), &ancho, &alto, &canales,
                              4) == nullptr);
  // El GIF cortado por la mitad.
  CHECK(stbi_load_from_memory(kGifTransparente, 20, &ancho, &alto, &canales, 4) == nullptr);
  // Cualquier otra cosa.
  const unsigned char basura[] = "esto no es una imagen";
  CHECK(stbi_load_from_memory(basura, static_cast<int>(sizeof(basura)), &ancho, &alto,
                              &canales, 4) == nullptr);
}
