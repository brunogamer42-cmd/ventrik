#include <functional>
#include <string>
#include <utility>
#include <variant>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_impl_null.h>
#include <imgui_internal.h>

#include "client/sala.h"
#include "client/ui/inicio.h"
#include "client/ui/piezas.h"
#include "client/ui/servidor.h"
#include "client/ui/ventana.h"

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

  void cuadro(const std::function<void()>& dibujar, ImVec2 mouse, bool clic) {
    ImGui_ImplNull_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = tamano_;
    io.AddMousePosEvent(mouse.x, mouse.y);
    io.AddMouseButtonEvent(boton_, clic);
    io.AddInputCharactersUTF8(texto_.c_str());
    texto_.clear();
    if (soltar_ != ImGuiKey_None) {
      io.AddKeyEvent(soltar_, false);
    }
    soltar_ = tecla_;
    if (tecla_ != ImGuiKey_None) {
      io.AddKeyEvent(tecla_, true);
    }
    tecla_ = ImGuiKey_None;
    ImGui::NewFrame();
    CHECK(ImGui::GetCurrentContext()->DebugDrawIdConflictsId == 0);
    ui::pantalla_completa(dibujar);
    ImGui::Render();
    CHECK(ImGui::GetCurrentContext()->ErrorCountCurrentFrame == 0);
  }

  ui::Pedido cuadro(ui::PantallaInicio& pantalla, const Guardados& guardados, ImVec2 mouse, bool clic = false) {
    ui::Pedido pedido;
    cuadro([&] { pedido = ui::dibujar_inicio(pantalla, guardados); }, mouse, clic);
    return pedido;
  }

  ui::PedidoServidor cuadro(ui::PantallaServidor& pantalla, ventrik::cliente::Sala& sala, ImVec2 mouse,
                            bool clic = false) {
    ui::PedidoServidor pedido;
    cuadro([&] { pedido = ui::dibujar_servidor(pantalla, sala); }, mouse, clic);
    return pedido;
  }

  ImVec2 tamano() const { return tamano_; }
  void teclear(std::string texto) { texto_ = std::move(texto); }
  void apretar(ImGuiKey tecla) { tecla_ = tecla; }
  void usar_boton(int boton) { boton_ = boton; }

 private:
  ImVec2 tamano_;
  std::string texto_;
  ImGuiKey tecla_ = ImGuiKey_None;
  ImGuiKey soltar_ = ImGuiKey_None;
  int boton_ = 0;
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

namespace {
using namespace ventrik::protocolo;

ventrik::cliente::Sala sala_llena(bool dueno, std::size_t canales, std::size_t personas, std::size_t mensajes) {
  Presentado presentado;
  presentado.tu_id = 1;
  presentado.dueno = dueno;
  for (std::uint32_t id = 1; id <= canales; ++id) {
    const bool texto = id % 2 == 0;
    presentado.canales.push_back({id, (texto ? "texto " : "voz ") + std::to_string(id) + std::string(id == 2 ? 20 : 0, 'x'),
                                  texto ? TipoCanal::texto : TipoCanal::voz, texto && id % 4 == 0, id * 3});
  }
  for (std::uint32_t id = 1; id <= personas; ++id) {
    presentado.usuarios.push_back({id, "Persona con nombre largo " + std::to_string(id), 1 + (id % 3) * 2});
  }
  ventrik::cliente::Sala sala(presentado);
  sala.empezar();
  Historial historial{sala.tu_canal(), true, {}};
  for (std::uint64_t id = mensajes; id >= 1; --id) {
    historial.mensajes.push_back({id, static_cast<std::uint32_t>(id), "Persona con nombre largo 2",
                                  1761234567890 + id * 3600000, id % 7 == 0 ? std::string(4000, 'w') : "hola " + std::to_string(id)});
  }
  sala.aplicar(historial);
  sala.aplicar(MensajesNuevos{{{2, 50}, {4, 7}}});
  return sala;
}

ui::PantallaServidor pantalla_servidor() {
  ui::PantallaServidor pantalla;
  pantalla.direccion = "203.0.113.20:7700";
  pantalla.hablando = 2;
  pantalla.transmitiendo = true;
  pantalla.nivel = 7;
  for (std::uint32_t i = 0; i < ui::kMuestrasDelGrafico; ++i) {
    pantalla.grafico.push_back({20 + i % 9, i % 13 == 0 ? 1u : 0u, 50});
  }
  return pantalla;
}

void recorrer(SinPantalla& sin_pantalla, ui::PantallaServidor& pantalla, ventrik::cliente::Sala& sala) {
  for (float y = 5; y < sin_pantalla.tamano().y; y += 41) {
    for (float x = 5; x < sin_pantalla.tamano().x; x += 59) {
      sin_pantalla.cuadro(pantalla, sala, ImVec2(x, y));
    }
  }
}

}

TEST_CASE("La pantalla del servidor se dibuja sin errores a cualquier tamaño, escala y cantidad") {
  for (const auto& [escala, tamano] : {std::make_pair(1.0f, ImVec2(1376, 768)), std::make_pair(1.0f, ImVec2(1200, 700)),
                                       std::make_pair(1.0f, ImVec2(1100, 640)), std::make_pair(1.25f, ImVec2(1720, 960)),
                                       std::make_pair(1.5f, ImVec2(1650, 960)), std::make_pair(1.0f, ImVec2(2560, 1440))}) {
    SinPantalla sin_pantalla(escala, tamano);
    CAPTURE(escala);
    CAPTURE(tamano.x);
    for (const bool dueno : {false, true}) {
      CAPTURE(dueno);
      auto sala = sala_llena(dueno, 60, 512, 300);
      auto pantalla = pantalla_servidor();
      recorrer(sin_pantalla, pantalla, sala);
      sala.elegir_texto(4);
      recorrer(sin_pantalla, pantalla, sala);
    }
    auto vacia = sala_llena(false, 1, 1, 0);
    ui::PantallaServidor pantalla;
    recorrer(sin_pantalla, pantalla, vacia);
  }
}

TEST_CASE("Un clic en un canal de texto lo mira y uno en otro canal de voz entra") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(false, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  const auto clic = [&](ImVec2 donde) {
    sin_pantalla.cuadro(pantalla, sala, donde);
    sin_pantalla.cuadro(pantalla, sala, donde, true);
    return sin_pantalla.cuadro(pantalla, sala, donde);
  };
  const auto texto = clic(ImVec2(100, 192));
  REQUIRE(texto.ordenes.size() == 2);
  CHECK(std::get<MirarCanal>(texto.ordenes[0]).canal == 2);
  CHECK(sala.mirando() == 2);
  const auto voz = clic(ImVec2(100, 285));
  REQUIRE(voz.ordenes.size() == 2);
  CHECK(std::get<MirarCanal>(voz.ordenes[0]).canal == 0);
  CHECK(std::get<EntrarACanal>(voz.ordenes[1]).canal == 1);
}

TEST_CASE("El nombre de la tecla para hablar sale de los textos o de Windows") {
  const ventrik::Idioma espanol = ventrik::elegir_idioma("es");
  CHECK(ui::nombre_tecla(espanol, 0) == "Alt izq.");
  CHECK(ui::nombre_tecla(espanol, 0xA4) == "Alt izq.");
  CHECK(ui::nombre_tecla(espanol, 0x05) == "Botón 4 del mouse");
  CHECK_FALSE(ui::nombre_tecla(espanol, 'V').empty());
}

namespace {
bool hay_ventana_emergente() { return ImGui::GetCurrentContext()->OpenPopupStack.Size > 0; }

template <typename Orden>
std::vector<Orden> ordenes_de(const std::vector<ui::PedidoServidor>& pedidos) {
  std::vector<Orden> encontradas;
  for (const auto& pedido : pedidos) {
    for (const auto& orden : pedido.ordenes) {
      if (const auto* cual = std::get_if<Orden>(&orden)) {
        encontradas.push_back(*cual);
      }
    }
  }
  return encontradas;
}

}

TEST_CASE("El dueño crea un canal de texto con el + y el invitado no tiene +") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(true, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  const ImVec2 mas(264, 165);
  sin_pantalla.cuadro(pantalla, sala, mas);
  sin_pantalla.cuadro(pantalla, sala, mas, true);
  sin_pantalla.cuadro(pantalla, sala, mas);
  CHECK(pantalla.dialogo == ui::Dialogo::crear_texto);
  std::vector<ui::PedidoServidor> pedidos;
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  sin_pantalla.teclear("Novedades");
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  sin_pantalla.apretar(ImGuiKey_Enter);
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  const auto creados = ordenes_de<CrearCanal>(pedidos);
  REQUIRE(creados.size() == 1);
  CHECK(creados[0].nombre == "Novedades");
  CHECK(creados[0].tipo == TipoCanal::texto);
  CHECK_FALSE(creados[0].solo_admins);
  CHECK(pantalla.dialogo == ui::Dialogo::ninguno);

  auto invitado = sala_llena(false, 4, 3, 5);
  auto otra = pantalla_servidor();
  sin_pantalla.cuadro(otra, invitado, mas);
  sin_pantalla.cuadro(otra, invitado, mas, true);
  sin_pantalla.cuadro(otra, invitado, mas);
  CHECK(otra.dialogo == ui::Dialogo::ninguno);
}

TEST_CASE("Con un nombre inválido el cuadro avisa y no manda nada") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(true, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  pantalla.dialogo = ui::Dialogo::crear_voz;
  pantalla.abrir_dialogo = true;
  std::vector<ui::PedidoServidor> pedidos;
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  sin_pantalla.teclear(" ");
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  sin_pantalla.apretar(ImGuiKey_Enter);
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  CHECK(ordenes_de<CrearCanal>(pedidos).empty());
  CHECK(pantalla.dialogo == ui::Dialogo::crear_voz);
  CHECK_FALSE(pantalla.error_dialogo.empty());
  sin_pantalla.apretar(ImGuiKey_Escape);
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  CHECK(pantalla.dialogo == ui::Dialogo::ninguno);
}

TEST_CASE("El clic derecho del dueño abre el menú, y cambiar o borrar mandan su orden") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(true, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  const ImVec2 fila(100, 192);
  sin_pantalla.usar_boton(1);
  sin_pantalla.cuadro(pantalla, sala, fila);
  sin_pantalla.cuadro(pantalla, sala, fila, true);
  sin_pantalla.cuadro(pantalla, sala, fila);
  CHECK(hay_ventana_emergente());
  sin_pantalla.usar_boton(0);
  const ImVec2 cambiar(130, 207);
  sin_pantalla.cuadro(pantalla, sala, cambiar);
  sin_pantalla.cuadro(pantalla, sala, cambiar, true);
  sin_pantalla.cuadro(pantalla, sala, cambiar);
  CHECK(pantalla.dialogo == ui::Dialogo::cambiar);
  CHECK(pantalla.canal_dialogo == 2);
  CHECK(std::string(pantalla.nombre_dialogo.data()) == sala.canal(2)->nombre);
  std::vector<ui::PedidoServidor> pedidos;
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  sin_pantalla.apretar(ImGuiKey_Enter);
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  const auto cambiados = ordenes_de<CambiarCanal>(pedidos);
  REQUIRE(cambiados.size() == 1);
  CHECK(cambiados[0].canal == 2);

  pantalla.dialogo = ui::Dialogo::borrar;
  pantalla.canal_dialogo = 3;
  pantalla.abrir_dialogo = true;
  pedidos.clear();
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  sin_pantalla.apretar(ImGuiKey_Enter);
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
  CHECK(ordenes_de<BorrarCanal>(pedidos).empty());
  pantalla.dialogo = ui::Dialogo::borrar;
  pantalla.abrir_dialogo = true;
  for (float x = 520; x < 880 && pantalla.dialogo == ui::Dialogo::borrar; x += 8) {
    const ImVec2 boton(x, 425);
    pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, boton));
    pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, boton, true));
    pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, boton));
  }
  const auto borrados = ordenes_de<BorrarCanal>(pedidos);
  REQUIRE(borrados.size() == 1);
  CHECK(borrados[0].canal == 3);
}

TEST_CASE("El gráfico de la barra de estado abre el diagnóstico") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(false, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  pantalla.diagnostico = {true, 12, 3.6, 70 * 1024 * 1024, 40, 7, 0};
  bool abierto = false;
  for (float x = 150; x < 700 && !abierto; x += 16) {
    const ImVec2 donde(x, 756);
    sin_pantalla.cuadro(pantalla, sala, donde);
    sin_pantalla.cuadro(pantalla, sala, donde, true);
    sin_pantalla.cuadro(pantalla, sala, donde);
    abierto = hay_ventana_emergente();
  }
  CHECK(abierto);
  for (int i = 0; i < 3; ++i) {
    sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  }
}

TEST_CASE("Si borran el canal mientras su cuadro está abierto, el cuadro se cierra; el invitado no tiene menú") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(true, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  pantalla.dialogo = ui::Dialogo::cambiar;
  pantalla.canal_dialogo = 2;
  pantalla.abrir_dialogo = true;
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  CHECK(hay_ventana_emergente());
  sala.aplicar(CanalBorrado{2, 1});
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  CHECK(pantalla.dialogo == ui::Dialogo::ninguno);
  CHECK_FALSE(hay_ventana_emergente());

  auto invitado = sala_llena(false, 4, 3, 5);
  auto otra = pantalla_servidor();
  const ImVec2 fila(100, 192);
  sin_pantalla.usar_boton(1);
  sin_pantalla.cuadro(otra, invitado, fila);
  sin_pantalla.cuadro(otra, invitado, fila, true);
  sin_pantalla.cuadro(otra, invitado, fila);
  CHECK_FALSE(hay_ventana_emergente());
}

TEST_CASE("Dos cuadros iguales tienen la misma huella, y el mouse sobre una fila cambia la imagen") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(false, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  const auto huella = [&](ImVec2 mouse) {
    sin_pantalla.cuadro(pantalla, sala, mouse);
    return ui::huella_del_dibujo(*ImGui::GetDrawData());
  };
  for (int i = 0; i < 3; ++i) {
    huella(ImVec2(900, 400));
  }
  const std::size_t quieta = huella(ImVec2(900, 400));
  CHECK(huella(ImVec2(900, 400)) == quieta);
  CHECK(huella(ImVec2(901, 401)) == quieta);
  CHECK(huella(ImVec2(100, 192)) != quieta);
}

TEST_CASE("Las teclas repetidas no piden cuadros salvo escribiendo, y los cuadros lentos se espacian") {
  constexpr LPARAM kRepetida = 0x40000000;
  std::optional<LPARAM> mouse;
  CHECK(ui::pide_cuadros(WM_KEYDOWN, VK_CONTROL, 0, false, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_KEYDOWN, VK_CONTROL, kRepetida, true, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_SYSKEYDOWN, VK_MENU, kRepetida, false, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_KEYDOWN, VK_BACK, kRepetida, false, mouse));
  CHECK(ui::pide_cuadros(WM_KEYDOWN, VK_BACK, kRepetida, true, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_NCMOUSEMOVE, 0, 0, false, mouse));
  CHECK(ui::pide_cuadros(WM_MOUSEMOVE, 0, 7, false, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_MOUSEMOVE, 0, 7, false, mouse));
  CHECK(ui::pide_cuadros(WM_MOUSEMOVE, 0, 8, false, mouse));
  CHECK(ui::pide_cuadros(WM_MOUSELEAVE, 0, 0, false, mouse));
  CHECK(ui::pide_cuadros(WM_MOUSEMOVE, 0, 8, false, mouse));
  CHECK(ui::espera_entre_cuadros(1.4) == 15);
  CHECK(ui::espera_entre_cuadros(16) == 80);
}
