#include <algorithm>

#include "client/ui/piezas.h"

namespace ventrik::ui {
namespace {
constexpr float kPi = 3.14159265f;
}

void dibujar_icono(ImDrawList& dibujo, Icono icono, ImVec2 centro, float lado, ImU32 color) {
  const float u = lado / 15;
  const float grosor = std::max(1.0f, 1.4f * u);
  const auto p = [&](float x, float y) { return ImVec2(centro.x + x * u, centro.y + y * u); };
  const auto arco = [&](float x, float y, float radio, float desde, float hasta) {
    dibujo.PathArcTo(p(x, y), radio * u, desde, hasta, 16);
    dibujo.PathStroke(color, 0, grosor);
  };
  const auto trazo = [&](std::initializer_list<ImVec2> puntos, bool cerrado) {
    for (const ImVec2& punto : puntos) {
      dibujo.PathLineTo(p(punto.x, punto.y));
    }
    dibujo.PathStroke(color, cerrado ? ImDrawFlags_Closed : 0, grosor);
  };
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
    case Icono::microfono:
      dibujo.AddRect(p(-2.5f, -7), p(2.5f, 2), color, 2.5f * u, 0, grosor);
      arco(0, -1.5f, 5, 0, kPi);
      dibujo.AddLine(p(0, 3.5f), p(0, 7), color, grosor);
      break;
    case Icono::auriculares:
      arco(0, 1, 6, kPi, 2 * kPi);
      dibujo.AddRect(p(-7, 1), p(-3.5f, 6.5f), color, u, 0, grosor);
      dibujo.AddRect(p(3.5f, 1), p(7, 6.5f), color, u, 0, grosor);
      break;
    case Icono::persona:
      dibujo.AddCircle(p(0, -3.5f), 3.2f * u, color, 0, grosor);
      arco(0, 7, 6, 1.1f * kPi, 1.9f * kPi);
      break;
    case Icono::lapiz:
      trazo({ImVec2(-6, 6), ImVec2(-6, 3), ImVec2(3, -6), ImVec2(6, -3), ImVec2(-3, 6)}, true);
      break;
    case Icono::encendido:
      arco(0, 0.8f, 5.6f, -0.32f * kPi, 1.32f * kPi);
      dibujo.AddLine(p(0, -6.5f), p(0, -0.5f), color, grosor);
      break;
    case Icono::numeral:
      dibujo.AddLine(p(-1.5f, -6), p(-3, 6), color, grosor);
      dibujo.AddLine(p(3, -6), p(1.5f, 6), color, grosor);
      dibujo.AddLine(p(-5.5f, -2), p(5.5f, -2), color, grosor);
      dibujo.AddLine(p(-6, 2.5f), p(5, 2.5f), color, grosor);
      break;
    case Icono::candado:
      dibujo.AddRect(p(-5, -1), p(5, 6), color, u, 0, grosor);
      arco(0, -2, 3.3f, kPi, 2 * kPi);
      dibujo.AddLine(p(-3.3f, -2), p(-3.3f, -1), color, grosor);
      dibujo.AddLine(p(3.3f, -2), p(3.3f, -1), color, grosor);
      break;
    case Icono::flecha_derecha:
      trazo({ImVec2(-2, -4.5f), ImVec2(2.5f, 0), ImVec2(-2, 4.5f)}, false);
      break;
    case Icono::flecha_izquierda:
      trazo({ImVec2(2, -4.5f), ImVec2(-2.5f, 0), ImVec2(2, 4.5f)}, false);
      break;
    case Icono::atras:
    case Icono::adelante: {
      const float lado_flecha = icono == Icono::adelante ? 1.0f : -1.0f;
      dibujo.AddLine(p(-5 * lado_flecha, 0), p(5 * lado_flecha, 0), color, grosor);
      trazo({ImVec2(1 * lado_flecha, -4), ImVec2(5 * lado_flecha, 0), ImVec2(1 * lado_flecha, 4)}, false);
      break;
    }
    case Icono::reproducir:
      dibujo.AddTriangleFilled(p(-2.5f, -4.5f), p(4, 0), p(-2.5f, 4.5f), color);
      break;
    case Icono::flecha_abajo:
      trazo({ImVec2(-4.5f, -2), ImVec2(0, 2.5f), ImVec2(4.5f, -2)}, false);
      break;
    case Icono::voz:
      for (const auto& [x, alto] : {std::make_pair(-6.0f, 3.0f), std::make_pair(-3.0f, 6.5f),
                                    std::make_pair(0.0f, 9.0f), std::make_pair(3.0f, 6.5f),
                                    std::make_pair(6.0f, 3.0f)}) {
        dibujo.AddLine(p(x, -alto / 2), p(x, alto / 2), color, std::max(1.0f, 1.8f * u));
      }
      break;
    case Icono::enviar:
      trazo({ImVec2(-6.5f, -0.5f), ImVec2(6.5f, -6.5f), ImVec2(0.5f, 6.5f), ImVec2(-0.5f, 0.5f)}, true);
      break;
    case Icono::clip:
      dibujo.PathArcTo(p(3, -3), 2.6f * u, -0.75f * kPi, 0.25f * kPi, 8);
      dibujo.PathArcTo(p(-3, 3), 2.6f * u, 0.25f * kPi, 1.25f * kPi, 8);
      dibujo.PathStroke(color, ImDrawFlags_Closed, grosor);
      dibujo.AddLine(p(-1.8f, 1.8f), p(1.8f, -1.8f), color, grosor);
      break;
    case Icono::panel:
      dibujo.AddRect(p(-6.5f, -6), p(6.5f, 6), color, u, 0, grosor);
      dibujo.AddLine(p(2, -6), p(2, 6), color, grosor);
      break;
    case Icono::verificado:
      dibujo.AddCircle(p(0, 0), 6.5f * u, color, 0, grosor);
      trazo({ImVec2(-3, 0), ImVec2(-0.8f, 2.5f), ImVec2(3.2f, -2.5f)}, false);
      break;
    case Icono::teclado:
      dibujo.AddRect(p(-7, -4.5f), p(7, 4.5f), color, u, 0, grosor);
      for (const float x : {-4.5f, -1.5f, 1.5f, 4.5f}) {
        dibujo.AddRectFilled(p(x - 0.7f, -2.2f), p(x + 0.7f, -0.8f), color);
      }
      dibujo.AddLine(p(-3.5f, 2), p(3.5f, 2), color, grosor);
      break;
    case Icono::parlante:
      trazo({ImVec2(-6.5f, -2), ImVec2(-3.5f, -2), ImVec2(0.5f, -5.5f), ImVec2(0.5f, 5.5f), ImVec2(-3.5f, 2),
             ImVec2(-6.5f, 2)},
            true);
      arco(1, 0, 4.5f, -0.3f * kPi, 0.3f * kPi);
      break;
    case Icono::pulso:
      trazo({ImVec2(-7, 0), ImVec2(-3.5f, 0), ImVec2(-1.5f, -4.5f), ImVec2(1.5f, 4.5f), ImVec2(3.5f, 0), ImVec2(7, 0)},
            false);
      break;
    case Icono::flecha_arriba:
      trazo({ImVec2(-4.5f, 2), ImVec2(0, -2.5f), ImVec2(4.5f, 2)}, false);
      break;
    case Icono::campana:
      arco(0, -1.5f, 4, kPi, 2 * kPi);
      trazo({ImVec2(-4, -1.5f), ImVec2(-4, 2.5f), ImVec2(-6, 4.5f), ImVec2(6, 4.5f), ImVec2(4, 2.5f), ImVec2(4, -1.5f)},
            false);
      dibujo.AddLine(p(-1.5f, 6.5f), p(1.5f, 6.5f), color, grosor);
      break;
    case Icono::colgar:
      arco(0, 4, 7, 1.15f * kPi, 1.85f * kPi);
      trazo({ImVec2(-6.2f, 0.8f), ImVec2(-6.8f, 3.5f), ImVec2(-3, 4), ImVec2(-2.6f, 2)}, false);
      trazo({ImVec2(6.2f, 0.8f), ImVec2(6.8f, 3.5f), ImVec2(3, 4), ImVec2(2.6f, 2)}, false);
      break;
    case Icono::ninguno:
      break;
  }
}

void dibujar_icono_tachado(ImDrawList& dibujo, Icono icono, ImVec2 centro, float lado, ImU32 color) {
  dibujar_icono(dibujo, icono, centro, lado, color);
  const float medio = lado / 2;
  dibujo.AddLine(ImVec2(centro.x - medio, centro.y - medio), ImVec2(centro.x + medio, centro.y + medio), color,
                 std::max(1.0f, lado / 10));
}

}
