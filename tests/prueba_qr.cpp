#include <cstdint>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_impl_null.h>
#include <qrcodegen.h>

#include "client/ui/qr.h"

namespace ui = ventrik::ui;

namespace {
constexpr const char* kOtpauth =
    "otpauth://totp/Ventrik:Bruno?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&issuer=Ventrik&digits=6&period=30";

std::vector<int> pintados(const ui::Qr& qr) {
  std::vector<int> veces(static_cast<std::size_t>(qr.lado * qr.lado), 0);
  for (const ui::TramoQr& tramo : qr.tramos) {
    for (int x = tramo.columna; x < tramo.columna + tramo.largo; ++x) {
      ++veces[static_cast<std::size_t>(tramo.fila * qr.lado + x)];
    }
  }
  return veces;
}

}

TEST_CASE("el QR de un texto conocido tiene los mismos módulos que qrcodegen, con 4 de borde") {
  std::vector<std::uint8_t> temporal(qrcodegen_BUFFER_LEN_MAX);
  std::vector<std::uint8_t> esperado(qrcodegen_BUFFER_LEN_MAX);
  REQUIRE(qrcodegen_encodeText(kOtpauth, temporal.data(), esperado.data(), qrcodegen_Ecc_MEDIUM,
                               qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, false));
  const auto qr = ui::armar_qr(kOtpauth);
  REQUIRE(qr);
  CHECK(qrcodegen_getSize(esperado.data()) == 41);
  REQUIRE(qr->lado == 41 + 2 * ui::kBordeQr);
  for (const ui::TramoQr& tramo : qr->tramos) {
    CHECK(tramo.largo > 0);
    CHECK(tramo.fila >= ui::kBordeQr);
    CHECK(tramo.fila < qr->lado - ui::kBordeQr);
    CHECK(tramo.columna >= ui::kBordeQr);
    CHECK(tramo.columna + tramo.largo <= qr->lado - ui::kBordeQr);
  }
  const std::vector<int> veces = pintados(*qr);
  int distintos = 0;
  for (int y = 0; y < qr->lado; ++y) {
    for (int x = 0; x < qr->lado; ++x) {
      const bool negro = qrcodegen_getModule(esperado.data(), x - ui::kBordeQr, y - ui::kBordeQr);
      distintos += veces[static_cast<std::size_t>(y * qr->lado + x)] != (negro ? 1 : 0) ? 1 : 0;
    }
  }
  CHECK(distintos == 0);
}

TEST_CASE("el QR crece con el texto y rechaza lo que no entra") {
  const auto corto = ui::armar_qr("HELLO WORLD");
  REQUIRE(corto);
  CHECK(corto->lado == 21 + 2 * ui::kBordeQr);
  CHECK_FALSE(ui::armar_qr(std::string(3000, 'a')));
  CHECK_FALSE(ui::armar_qr(std::string("otpauth\0oculto", 14)));
}

TEST_CASE("el QR se dibuja con un fondo blanco y un rectángulo negro por tramo") {
  const auto qr = ui::armar_qr(kOtpauth);
  REQUIRE(qr);
  REQUIRE_FALSE(qr->tramos.empty());
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  REQUIRE(ImGui_ImplNull_Init());
  ImGui_ImplNull_NewFrame();
  ImGui::NewFrame();
  ImGui::Begin("qr");
  ImDrawList& dibujo = *ImGui::GetWindowDrawList();
  const int antes = dibujo.VtxBuffer.Size;
  ui::dibujar_qr(dibujo, *qr, ImVec2(10, 20), 3);
  CHECK(dibujo.VtxBuffer.Size - antes == 4 * static_cast<int>(1 + qr->tramos.size()));
  const ImDrawVert& fondo = dibujo.VtxBuffer[antes];
  CHECK(fondo.col == IM_COL32_WHITE);
  CHECK(fondo.pos.x == 10);
  CHECK(fondo.pos.y == 20);
  CHECK(dibujo.VtxBuffer[antes + 2].pos.x == 10 + 3 * qr->lado);
  const ImDrawVert& primero = dibujo.VtxBuffer[antes + 4];
  CHECK(primero.col == IM_COL32_BLACK);
  CHECK(primero.pos.x == 10 + 3 * qr->tramos.front().columna);
  CHECK(primero.pos.y == 20 + 3 * qr->tramos.front().fila);
  ImGui::End();
  ImGui::Render();
  ImGui_ImplNull_Shutdown();
  ImGui::DestroyContext();
}
