#include <algorithm>

#include "client/ui/piezas.h"

namespace ventrik::ui {

void dibujar_icono(ImDrawList& dibujo, Icono icono, ImVec2 centro, float lado, ImU32 color) {
  const float u = lado / 15;
  const float grosor = std::max(1.0f, 1.4f * u);
  const auto p = [&](float x, float y) { return ImVec2(centro.x + x * u, centro.y + y * u); };
  switch (icono) {
    case Icono::lupa:
      dibujo.AddCircle(p(-1.5f, -1.5f), 5 * u, color, 0, grosor);
      dibujo.AddLine(p(2.2f, 2.2f), p(6, 6), color, grosor);
      break;
    case Icono::pantalla:
      dibujo.AddRect(p(-6.5f, -5.5f), p(6.5f, 3.5f), color, u, 0, grosor);
      dibujo.AddLine(p(-3, 6.5f), p(3, 6.5f), color, grosor);
      break;
    case Icono::mas:
      dibujo.AddLine(p(-6, 0), p(6, 0), color, grosor);
      dibujo.AddLine(p(0, -6), p(0, 6), color, grosor);
      break;
    case Icono::reportar:
      dibujo.AddRect(p(-4, -4), p(4, 4), color, u, 0, grosor);
      for (const float y : {-2.0f, 2.0f}) {
        dibujo.AddLine(p(-7, y), p(-4, y), color, grosor);
        dibujo.AddLine(p(4, y), p(7, y), color, grosor);
      }
      break;
    case Icono::ajustes:
      for (int fila = 0; fila < 3; ++fila) {
        const float y = -4.5f + 4.5f * static_cast<float>(fila);
        dibujo.AddLine(p(-6.5f, y), p(6.5f, y), color, grosor);
        dibujo.AddCircleFilled(p(fila == 1 ? 2.5f : -2.5f, y), 1.9f * u, color);
      }
      break;
    case Icono::puntos:
      for (const float x : {-4.5f, 0.0f, 4.5f}) {
        dibujo.AddCircleFilled(p(x, 0), 1.4f * u, color);
      }
      break;
    case Icono::servidor:
      for (const float y : {-5.5f, 0.5f}) {
        dibujo.AddRect(p(-7, y), p(7, y + 5), color, u, 0, grosor);
        dibujo.AddLine(p(-4.5f, y + 2.5f), p(-1, y + 2.5f), color, grosor);
      }
      break;
    case Icono::ninguno:
      break;
  }
}

}
