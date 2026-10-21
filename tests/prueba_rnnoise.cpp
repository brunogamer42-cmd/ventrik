#include <cmath>
#include <cstddef>
#include <vector>

#include <doctest/doctest.h>
#include <rnnoise.h>

TEST_CASE("RNNoise limpia pedazos de 10 ms") {
  REQUIRE(rnnoise_get_frame_size() == 480);  // 10 ms a 48 kHz
  DenoiseState* estado = rnnoise_create(nullptr);
  REQUIRE(estado != nullptr);

  std::vector<float> entrada(480);
  std::vector<float> salida(480);
  for (int pedazo = 0; pedazo < 10; ++pedazo) {
    for (std::size_t i = 0; i < entrada.size(); ++i) {
      entrada[i] = static_cast<float>(
          3000.0 * std::sin(0.05 * static_cast<double>(i + 480 * pedazo)));
    }
    const float voz = rnnoise_process_frame(estado, salida.data(), entrada.data());
    CHECK(voz >= 0.0f);  // probabilidad de que sea voz, de 0 a 1
    CHECK(voz <= 1.0f);
  }
  for (const float muestra : salida) {
    CHECK(std::isfinite(muestra));
  }
  rnnoise_destroy(estado);
}
