#include <algorithm>

#include <doctest/doctest.h>

#include "client/ui/piezas.h"

namespace ui = ventrik::ui;

namespace {
bool igual(ImVec2 a, ImVec2 b) { return a.x == b.x && a.y == b.y; }
}

TEST_CASE("los colores del tema son los de aspecto.md") {
  CHECK(ui::color::fondo == IM_COL32(0x16, 0x1B, 0x1E, 255));
  CHECK(ui::color::naranja == IM_COL32(0xF2, 0x8B, 0x30, 255));
  CHECK(ui::rgb(0xFFFFFF, 15) == IM_COL32(255, 255, 255, 15));
}

TEST_CASE("el octógono corta las cuatro esquinas a 45 grados") {
  const auto puntos = ui::octogono(ImVec2(10, 20), ImVec2(110, 54), 6);
  CHECK(igual(puntos[0], ImVec2(16, 20)));
  CHECK(igual(puntos[1], ImVec2(104, 20)));
  CHECK(igual(puntos[2], ImVec2(110, 26)));
  CHECK(igual(puntos[3], ImVec2(110, 48)));
  CHECK(igual(puntos[4], ImVec2(104, 54)));
  CHECK(igual(puntos[5], ImVec2(16, 54)));
  CHECK(igual(puntos[6], ImVec2(10, 48)));
  CHECK(igual(puntos[7], ImVec2(10, 26)));
}

TEST_CASE("las formas con esquinas cortadas van en el sentido de las agujas del reloj") {
  const auto principal = ui::cortes_opuestos(ImVec2(0, 0), ImVec2(100, 38), 7, true);
  CHECK(igual(principal[0], ImVec2(7, 0)));
  CHECK(igual(principal[1], ImVec2(100, 0)));
  CHECK(igual(principal[3], ImVec2(93, 38)));
  CHECK(igual(principal[5], ImVec2(0, 7)));
  const auto etiqueta = ui::cortes_opuestos(ImVec2(0, 0), ImVec2(100, 28), 8, false);
  CHECK(igual(etiqueta[1], ImVec2(92, 0)));
  CHECK(igual(etiqueta[2], ImVec2(100, 8)));
  CHECK(igual(etiqueta[4], ImVec2(8, 28)));
  const auto avatar = ui::corte_abajo_derecha(ImVec2(0, 0), ImVec2(26, 26), 7);
  CHECK(igual(avatar[2], ImVec2(26, 19)));
  CHECK(igual(avatar[3], ImVec2(19, 26)));
  for (const auto& forma : {principal, etiqueta}) {
    float area = 0;
    for (std::size_t i = 0; i < forma.size(); ++i) {
      const ImVec2 a = forma[i];
      const ImVec2 b = forma[(i + 1) % forma.size()];
      area += a.x * b.y - b.x * a.y;
    }
    CHECK(area > 0);
  }
}

TEST_CASE("el color del avatar sale del nombre y no cambia con las mayúsculas") {
  CHECK(ui::color_de_avatar("Tano") == ui::color_de_avatar("TANO"));
  CHECK(ui::color_de_avatar("Tano") == ui::color_de_avatar("Tano"));
  bool distintos = false;
  for (const char* nombre : {"Ana", "Beto", "Carla", "Dani", "Eli"}) {
    const ImU32 color = ui::color_de_avatar(nombre);
    CHECK(std::find(ui::color::avatares.begin(), ui::color::avatares.end(), color) != ui::color::avatares.end());
    distintos = distintos || color != ui::color_de_avatar("Tano");
  }
  CHECK(distintos);
}

TEST_CASE("las iniciales salen de las dos primeras palabras") {
  CHECK(ui::iniciales("escuadrón nocturno") == "EN");
  CHECK(ui::iniciales("Amigos") == "AM");
  CHECK(ui::iniciales("ñandú") == "ÑA");
  CHECK(ui::iniciales("T") == "T");
  CHECK(ui::iniciales("") == "");
  CHECK(ui::iniciales("\xF0\x9F\x98\x80 x") == "\xF0\x9F\x98\x80X");
}
