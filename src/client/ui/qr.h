#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include <imgui.h>

namespace ventrik::ui {
constexpr int kBordeQr = 4;

struct TramoQr {
  int fila = 0;
  int columna = 0;
  int largo = 0;
};

struct Qr {
  int lado = 0;
  std::vector<TramoQr> tramos;
};

std::optional<Qr> armar_qr(std::string_view texto);

void dibujar_qr(ImDrawList& dibujo, const Qr& qr, ImVec2 desde, float modulo);

}
