#include <string>

#include <doctest/doctest.h>
#include <miniaudio.h>

TEST_CASE("miniaudio es la versión 0.11.25") {
  CHECK(std::string(ma_version_string()) == "0.11.25");
}

TEST_CASE("miniaudio arranca el sistema de audio de Windows") {
  // No abre el micrófono ni los parlantes: solo comprueba que encuentra
  // con qué hablarles (WASAPI en Windows modernos, DirectSound o WinMM en XP).
  ma_context contexto = {};
  REQUIRE(ma_context_init(nullptr, 0, nullptr, &contexto) == MA_SUCCESS);
  const std::string sistema =
      std::string("Sistema de audio: ") + ma_get_backend_name(contexto.backend);
  MESSAGE(sistema);
  CHECK(ma_context_uninit(&contexto) == MA_SUCCESS);
}
