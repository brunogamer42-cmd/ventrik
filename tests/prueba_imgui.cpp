#include <string>

#include <windows.h>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_impl_null.h>
#include <imgui_impl_win32.h>

TEST_CASE("Dear ImGui es la versión 1.93.0") {
  CHECK(std::string(ImGui::GetVersion()) == "1.93.0");
}

TEST_CASE("Dear ImGui arma una ventana sin pantalla") {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;  // no dejar un imgui.ini
  REQUIRE(ImGui_ImplNull_Init());
  for (int cuadro = 0; cuadro < 3; ++cuadro) {
    ImGui_ImplNull_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Ventrik");
    ImGui::Text("Hola");
    ImGui::End();
    ImGui::Render();
  }
  const ImDrawData* dibujo = ImGui::GetDrawData();
  REQUIRE(dibujo != nullptr);
  CHECK(dibujo->CmdListsCount > 0);
  ImGui_ImplNull_Shutdown();
  ImGui::DestroyContext();
}

TEST_CASE("el módulo Win32 de Dear ImGui lee la escala de la pantalla") {
  const HMONITOR monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
  CHECK(ImGui_ImplWin32_GetDpiScaleForMonitor(monitor) > 0.0f);
}
