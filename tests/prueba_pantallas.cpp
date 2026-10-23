#include <string>
#include <utility>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_impl_null.h>
#include <imgui_internal.h>

#include "client/ui/inicio.h"
#include "client/ui/piezas.h"

namespace ui = ventrik::ui;
using ventrik::cliente::Guardados;

namespace {
class SinPantalla {
 public:
  explicit SinPantalla(float escala = 1.0f, ImVec2 tamano = ImVec2(1280, 900)) : tamano_(tamano) {
    REQUIRE(ui::preparar_interfaz(escala));
    REQUIRE(ImGui_ImplNull_Init());
  }
  ~SinPantalla() {
    ImGui_ImplNull_Shutdown();
    ImGui::DestroyContext();
  }
  SinPantalla(const SinPantalla&) = delete;
  SinPantalla& operator=(const SinPantalla&) = delete;

  ui::Pedido cuadro(ui::PantallaInicio& pantalla, const Guardados& guardados, ImVec2 mouse, bool clic = false) {
    ImGui_ImplNull_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = tamano_;
    io.AddMousePosEvent(mouse.x, mouse.y);
    io.AddMouseButtonEvent(0, clic);
    ImGui::NewFrame();
    CHECK(ImGui::GetCurrentContext()->DebugDrawIdConflictsId == 0);
    ui::Pedido pedido;
    ui::pantalla_completa([&] { pedido = ui::dibujar_inicio(pantalla, guardados); });
    ImGui::Render();
    CHECK(ImGui::GetCurrentContext()->ErrorCountCurrentFrame == 0);
    return pedido;
  }

  ImVec2 tamano() const { return tamano_; }

 private:
  ImVec2 tamano_;
};

Guardados muchos() {
  Guardados guardados;
  guardados.nombre = "Tano";
  for (std::uint32_t i = 1; i <= 32; ++i) {
    ventrik::cliente::ServidorGuardado servidor;
    servidor.direccion = {0xC0A80000 + i, static_cast<std::uint16_t>(7700 + i)};
    servidor.nombre = i % 2 == 0 ? "Tano" : "Un nombre bastante largo, de 32";
    servidor.nombre_canal = i % 3 == 0 ? "" : "Canal con un nombre largo de 30";
    servidor.ultima_vez_ms = 1761234567890 + i;
    guardados.servidores.push_back(servidor);
  }
  return guardados;
}

void recorrer(SinPantalla& sin_pantalla, ui::PantallaInicio& pantalla, const Guardados& guardados) {
  for (float y = 5; y < sin_pantalla.tamano().y; y += 37) {
    for (float x = 5; x < sin_pantalla.tamano().x; x += 53) {
      CHECK(sin_pantalla.cuadro(pantalla, guardados, ImVec2(x, y)).accion == ui::Accion::ninguna);
    }
  }
}

}

TEST_CASE("Barlow se mide como en el navegador: el tamaño de ImGui es 1,2 veces el de CSS") {
  SinPantalla sin_pantalla;
  ImGui_ImplNull_NewFrame();
  ImGui::NewFrame();
  ImFontAtlas& atlas = *ImGui::GetIO().Fonts;
  REQUIRE(atlas.Fonts.Size == 4);
  for (ImFont* fuente : atlas.Fonts) {
    const ImFontBaked* medida = fuente->GetFontBaked(100 * ui::medida::em);
    CHECK(medida->Ascent == doctest::Approx(100).epsilon(0.02));
    CHECK(medida->Descent == doctest::Approx(-20).epsilon(0.05));
  }
  ImGui::EndFrame();
}

TEST_CASE("la pantalla Inicio se dibuja sin errores ni identificadores repetidos") {
  SinPantalla sin_pantalla;
  ui::PantallaInicio pantalla;
  Guardados vacio;
  recorrer(sin_pantalla, pantalla, vacio);

  const Guardados guardados = muchos();
  recorrer(sin_pantalla, pantalla, guardados);

  pantalla.intento = {guardados.servidores[0].direccion, false, ui::Fase::conectando, {}};
  recorrer(sin_pantalla, pantalla, guardados);
  pantalla.intento = {guardados.servidores[2].direccion, true, ui::Fase::conectada, "Conectado como Tano"};
  recorrer(sin_pantalla, pantalla, guardados);
  pantalla.intento = {guardados.servidores[1].direccion, false, ui::Fase::error, std::string(300, 'x')};
  pantalla.error_formulario = std::string(200, 'y');
  pantalla.aviso = "aviso";
  recorrer(sin_pantalla, pantalla, guardados);

  ui::poner(pantalla.buscar, "nada que coincida");
  recorrer(sin_pantalla, pantalla, guardados);
}

TEST_CASE("en Inicio, el botón Entrar de una tarjeta pide entrar a ese servidor") {
  SinPantalla sin_pantalla;
  ui::PantallaInicio pantalla;
  const Guardados guardados = muchos();
  const ImVec2 entrar(200, 350);
  sin_pantalla.cuadro(pantalla, guardados, entrar);
  sin_pantalla.cuadro(pantalla, guardados, entrar, true);
  const ui::Pedido pedido = sin_pantalla.cuadro(pantalla, guardados, entrar, false);
  CHECK(pedido.accion == ui::Accion::entrar_a_guardado);
  CHECK(pedido.cual == 0);
}

TEST_CASE("Inicio se dibuja en el tamaño mínimo y con la escala de Windows al 125 y al 150 %") {
  for (const auto& [escala, tamano] : {std::make_pair(1.0f, ImVec2(1100, 640)), std::make_pair(1.25f, ImVec2(1375, 800)),
                                       std::make_pair(1.5f, ImVec2(1650, 960))}) {
    SinPantalla sin_pantalla(escala, tamano);
    ui::PantallaInicio pantalla;
    pantalla.intento = {muchos().servidores[1].direccion, false, ui::Fase::error, std::string(300, 'x')};
    recorrer(sin_pantalla, pantalla, muchos());
  }
}

TEST_CASE("en Inicio, \"Entrar como dueño\" del menú de una tarjeta completa el formulario") {
  SinPantalla sin_pantalla;
  ui::PantallaInicio pantalla;
  const Guardados guardados = muchos();
  const auto clic = [&](ImVec2 donde) {
    sin_pantalla.cuadro(pantalla, guardados, donde);
    sin_pantalla.cuadro(pantalla, guardados, donde, true);
    sin_pantalla.cuadro(pantalla, guardados, donde);
  };
  clic(ImVec2(425, 351));
  clic(ImVec2(460, 366));
  CHECK(std::string(pantalla.direccion.data()) == "192.168.0.1:7701");
  CHECK(std::string(pantalla.nombre.data()) == guardados.servidores[0].nombre);
  CHECK(std::string(pantalla.huella.data()).size() == 64);
  CHECK(pantalla.codigo[0] == '\0');
}
