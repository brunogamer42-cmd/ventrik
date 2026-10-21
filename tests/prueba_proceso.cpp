#include "common/proceso.h"
#include "doctest/doctest.h"

TEST_CASE("DEP queda activo después de activarlo") {
  ventrik::activar_dep();
  CHECK(ventrik::dep_activo());
}
