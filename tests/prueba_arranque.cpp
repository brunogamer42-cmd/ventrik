#include <string>

#include <doctest/doctest.h>
#include <windows.h>

#include "client/arranque.h"

TEST_CASE("Abrir Ventrik al iniciar Windows escribe y borra su entrada en el registro del usuario") {
  const std::wstring clave = L"Software\\Ventrik\\PruebaDeArranque";
  CHECK_FALSE(ventrik::cliente::arranca_con_windows(clave));
  REQUIRE(ventrik::cliente::arrancar_con_windows(true, clave));
  CHECK(ventrik::cliente::arranca_con_windows(clave));
  REQUIRE(ventrik::cliente::arrancar_con_windows(false, clave));
  CHECK_FALSE(ventrik::cliente::arranca_con_windows(clave));
  CHECK(ventrik::cliente::arrancar_con_windows(false, clave));
  RegDeleteKeyW(HKEY_CURRENT_USER, clave.c_str());
  RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Ventrik");
}
