#include <cstdint>
#include <vector>

#include <doctest/doctest.h>
#include <gsl/gsl>

TEST_CASE("gsl::span muestra solo una parte, sin copiar") {
  std::vector<int> numeros{1, 2, 3, 4};
  const gsl::span<int> todos(numeros);
  CHECK(todos.size() == 4u);
  const gsl::span<int> medio = todos.subspan(1, 2);
  CHECK(medio.size() == 2u);
  CHECK(medio[0] == 2);
  CHECK(medio[1] == 3);
}

TEST_CASE("gsl::narrow avisa si un número no entra") {
  CHECK(gsl::narrow<std::uint8_t>(200) == 200);
  CHECK_THROWS_AS(gsl::narrow<std::uint8_t>(300), gsl::narrowing_error);
}
