#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

struct IUnknown;
#include <d3d9.h>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_impl_null.h>
#include <imgui_internal.h>

#include "client/horas.h"
#include "client/sala.h"
#include "client/ui/ajustes.h"
#include "client/ui/aviso_ventana.h"
#include "client/ui/dibujante.h"
#include "client/ui/inicio.h"
#include "client/ui/menu_avisos.h"
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
    if (rueda_ != 0) {
      io.AddMouseWheelEvent(0, std::exchange(rueda_, 0.0f));
    }
    io.AddInputCharactersUTF8(texto_.c_str());
    texto_.clear();
    if (soltar_ != ImGuiKey_None) {
      io.AddKeyEvent(soltar_, false);
      io.AddKeyEvent(ImGuiMod_Ctrl, false);
    }
    soltar_ = tecla_;
    if (tecla_ != ImGuiKey_None) {
      io.AddKeyEvent(ImGuiMod_Ctrl, control_);
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

  ui::PedidoAjustes cuadro(ui::PantallaAjustes& pantalla, const Guardados& guardados, ImVec2 mouse,
                           bool clic = false) {
    ui::PedidoAjustes pedido;
    cuadro([&] { pedido = ui::dibujar_ajustes(pantalla, guardados); }, mouse, clic);
    return pedido;
  }

  ImVec2 tamano() const { return tamano_; }
  void teclear(std::string texto) { texto_ = std::move(texto); }
  void apretar(ImGuiKey tecla, bool control = false) {
    tecla_ = tecla;
    control_ = control;
  }
  void usar_boton(int boton) { boton_ = boton; }
  void rodar(float cuanto) { rueda_ = cuanto; }

 private:
  ImVec2 tamano_;
  float rueda_ = 0;
  std::string texto_;
  ImGuiKey tecla_ = ImGuiKey_None;
  ImGuiKey soltar_ = ImGuiKey_None;
  bool control_ = false;
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
  ventrik::cliente::poner_susurro(guardados, 0, ventrik::cliente::Susurro{0x05, "", {"Sofi", "Meli"}});
  ventrik::cliente::poner_susurro(guardados, 1, ventrik::cliente::Susurro{0, "Partida 2", {}});
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

ventrik::cliente::Sala sala_llena(bool dueno, std::size_t canales, std::size_t personas, std::size_t mensajes,
                                  const std::string& largo = std::string(4000, 'w')) {
  Presentado presentado;
  presentado.tu_id = 1;
  presentado.rol = dueno ? ventrik::protocolo::Rol::dueno : ventrik::protocolo::Rol::ninguno;
  for (std::uint32_t id = 1; id <= canales; ++id) {
    const bool texto = id % 2 == 0;
    presentado.canales.push_back({id, (texto ? "texto " : "voz ") + std::to_string(id) + std::string(id == 2 ? 20 : 0, 'x'),
                                  texto ? TipoCanal::texto : TipoCanal::voz, texto && id % 4 == 0, id * 3});
  }
  for (std::uint32_t id = 1; id <= personas; ++id) {
    presentado.usuarios.push_back(
        {id, "Persona con nombre largo " + std::to_string(id), id % 5 == 4 ? 0 : 1 + (id % 3) * 2});
  }
  ventrik::cliente::Sala sala(presentado);
  sala.empezar();
  Historial historial{sala.tu_canal(), 0, Sentido::viejos, true, {}};
  for (std::uint64_t id = mensajes; id >= 1; --id) {
    historial.mensajes.push_back({id, static_cast<std::uint32_t>(id), "Persona con nombre largo 2",
                                  1761234567890 + id * 3600000, id % 7 == 0 ? largo : "hola " + std::to_string(id)});
  }
  sala.aplicar(historial);
  sala.aplicar(MensajesNuevos{{{2, 50}, {4, 7}}});
  return sala;
}

ui::PantallaServidor pantalla_servidor() {
  ui::PantallaServidor pantalla;
  pantalla.direccion = "203.0.113.20:7700";
  pantalla.nombre_servidor = "203.0.113.20:7700";
  pantalla.hablando = {2};
  pantalla.personas = {{"Persona con nombre largo 3", 50, true}, {"Persona con nombre largo 2", 150, false}};
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
      pantalla.susurrando = 1;
      pantalla.susurros[0] = "Susurrando a Un nombre bastante largo, de 32, Otro nombre bastante largo y Tano";
      pantalla.te_susurra = "Te susurra: Un nombre bastante largo, de 32";
      recorrer(sin_pantalla, pantalla, sala);
      pantalla.susurrando = 0;
      pantalla.te_susurra.clear();
      sala.elegir_texto(4);
      recorrer(sin_pantalla, pantalla, sala);
      pantalla.silencio = 1;
      recorrer(sin_pantalla, pantalla, sala);
      sala.aplicar(UsuarioCambioCanal{1, 0});
      pantalla.ultima_voz = 3;
      pantalla.silencio = 3;
      recorrer(sin_pantalla, pantalla, sala);
    }
    auto vacia = sala_llena(false, 1, 1, 0);
    ui::PantallaServidor pantalla;
    recorrer(sin_pantalla, pantalla, vacia);
    vacia.aplicar(UsuarioCambioCanal{1, 0});
    CHECK(vacia.revisar().empty());
    recorrer(sin_pantalla, pantalla, vacia);
  }
}

TEST_CASE("Con voz, \"Salir del canal\" pide salir; sin voz, \"Volver a\" pide entrar al último canal") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(false, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  const auto clic = [&](ImVec2 donde) {
    sin_pantalla.cuadro(pantalla, sala, donde);
    sin_pantalla.cuadro(pantalla, sala, donde, true);
    return sin_pantalla.cuadro(pantalla, sala, donde);
  };
  const auto buscar = [&](float desde, float hasta) {
    std::optional<EntrarACanal> pedido;
    for (float y = desde; y < hasta && !pedido; y += 4) {
      const auto ordenes = clic(ImVec2(230, y)).ordenes;
      if (!ordenes.empty() && std::holds_alternative<EntrarACanal>(ordenes.back())) {
        pedido = std::get<EntrarACanal>(ordenes.back());
      }
    }
    return pedido;
  };
  const auto salir = buscar(630, 672);
  REQUIRE(salir);
  CHECK(salir->canal == 0);
  sala.aplicar(UsuarioCambioCanal{1, 0});
  CHECK(sala.revisar().size() == 2);
  CHECK_FALSE(buscar(586, 628));
  pantalla.ultima_voz = 3;
  const auto volver = buscar(586, 628);
  REQUIRE(volver);
  CHECK(volver->canal == 3);
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

ImVec2 en_menu(int fila) {
  const ImGuiWindow& menu = *ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
  return ImVec2(menu.Pos.x + 40, menu.Pos.y + 16 + 22 * static_cast<float>(fila));
}

ImVec2 en_dialogo(float x, float y) {
  const ImGuiWindow& dialogo = *ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
  return ImVec2(dialogo.Pos.x + x, dialogo.Pos.y + y);
}

std::pair<bool, bool> velo_y_ventana(std::string_view nombre) {
  bool velo = false;
  bool ventana = false;
  const ImDrawData& datos = *ImGui::GetDrawData();
  for (const ImDrawList* lista : datos.CmdLists) {
    ventana = ventana || (lista->_OwnerName != nullptr && nombre == lista->_OwnerName);
    for (int v = 0; v + 2 < lista->VtxBuffer.Size; ++v) {
      const ImDrawVert& desde = lista->VtxBuffer[v];
      const ImDrawVert& hasta = lista->VtxBuffer[v + 2];
      velo = velo || (desde.pos.x <= 0.5f && desde.pos.y <= 0.5f && hasta.pos.x >= datos.DisplaySize.x - 1 &&
                      hasta.pos.y >= datos.DisplaySize.y - 1 && (desde.col >> IM_COL32_A_SHIFT) < 255);
    }
  }
  return {velo, ventana};
}

template <typename Pedido>
std::optional<ventrik::cliente::AvisosDelServidor> avisos_de(const std::vector<Pedido>& pedidos) {
  std::optional<ventrik::cliente::AvisosDelServidor> avisos;
  for (const auto& pedido : pedidos) {
    if (pedido.avisos) {
      avisos = pedido.avisos;
    }
  }
  return avisos;
}

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
  const ImVec2 cambiar(130, 305);
  sin_pantalla.cuadro(pantalla, sala, cambiar);
  sin_pantalla.cuadro(pantalla, sala, cambiar, true);
  sin_pantalla.cuadro(pantalla, sala, cambiar);
  CHECK(pantalla.dialogo == ui::Dialogo::cambiar);
  CHECK(pantalla.id_dialogo == 2);
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
  pantalla.id_dialogo = 3;
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

TEST_CASE("El botón del modo abre \"Cómo transmitís\": Por voz, sensibilidad y volumen del micrófono; afuera lo cierra") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(false, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  const auto clic = [&](ImVec2 donde) {
    sin_pantalla.cuadro(pantalla, sala, donde);
    sin_pantalla.cuadro(pantalla, sala, donde, true);
    return sin_pantalla.cuadro(pantalla, sala, donde);
  };
  CHECK_FALSE(pantalla.ajustando);
  clic(ImVec2(140, 705));
  REQUIRE(pantalla.ajustando);
  const auto voz = clic(ImVec2(200, 524));
  REQUIRE(voz.por_voz);
  CHECK(*voz.por_voz);
  pantalla.por_voz = true;
  pantalla.sensibilidad = 30;
  pantalla.nivel = 6;
  sin_pantalla.cuadro(pantalla, sala, ImVec2(100, 538));
  const auto marca = sin_pantalla.cuadro(pantalla, sala, ImVec2(100, 538), true);
  REQUIRE(marca.sensibilidad);
  CHECK(*marca.sensibilidad > 25);
  CHECK(*marca.sensibilidad < 35);
  const auto arrastre = sin_pantalla.cuadro(pantalla, sala, ImVec2(200, 538), true);
  REQUIRE(arrastre.sensibilidad);
  CHECK(*arrastre.sensibilidad > 70);
  CHECK_FALSE(arrastre.guardar);
  CHECK(sin_pantalla.cuadro(pantalla, sala, ImVec2(200, 538)).guardar);
  sin_pantalla.cuadro(pantalla, sala, ImVec2(144, 620));
  const auto microfono = sin_pantalla.cuadro(pantalla, sala, ImVec2(144, 620), true);
  REQUIRE(microfono.volumen_microfono);
  CHECK(*microfono.volumen_microfono >= 95);
  CHECK(*microfono.volumen_microfono <= 105);
  CHECK(sin_pantalla.cuadro(pantalla, sala, ImVec2(144, 620)).guardar);
  clic(ImVec2(800, 300));
  CHECK_FALSE(pantalla.ajustando);

  clic(ImVec2(230, 705));
  sin_pantalla.cuadro(pantalla, sala, ImVec2(87, 620));
  const auto salida = sin_pantalla.cuadro(pantalla, sala, ImVec2(87, 620), true);
  REQUIRE(salida.volumen_salida);
  CHECK(*salida.volumen_salida >= 45);
  CHECK(*salida.volumen_salida <= 55);
  CHECK_FALSE(salida.volumen_microfono);
}

TEST_CASE("Los botones y Ctrl+M / Ctrl+D silencian como en Discord, también escribiendo en el chat") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(false, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  const auto clic = [&](ImVec2 donde) {
    sin_pantalla.cuadro(pantalla, sala, donde);
    sin_pantalla.cuadro(pantalla, sala, donde, true);
    return sin_pantalla.cuadro(pantalla, sala, donde);
  };
  const auto aplicar = [&](const ui::PedidoServidor& pedido) {
    REQUIRE(pedido.silencio);
    pantalla.silencio = *pedido.silencio;
    return static_cast<int>(*pedido.silencio);
  };
  const ImVec2 microfono(38, 705);
  const ImVec2 audio(76, 705);
  CHECK(aplicar(clic(microfono)) == 1);
  CHECK(aplicar(clic(audio)) == 3);
  CHECK(aplicar(clic(audio)) == 1);
  CHECK(aplicar(clic(microfono)) == 0);
  CHECK(aplicar(clic(audio)) == 3);
  CHECK(aplicar(clic(microfono)) == 0);
  const auto atajo = [&](ImGuiKey tecla, bool control) {
    sin_pantalla.apretar(tecla, control);
    const auto pedido = sin_pantalla.cuadro(pantalla, sala, ImVec2(800, 300));
    sin_pantalla.cuadro(pantalla, sala, ImVec2(800, 300));
    return pedido;
  };
  CHECK(aplicar(atajo(ImGuiKey_M, true)) == 1);
  CHECK(aplicar(atajo(ImGuiKey_D, true)) == 3);
  pantalla.enfocar_mensaje = true;
  for (int vez = 0; vez < 5 && !ImGui::GetIO().WantTextInput; ++vez) {
    sin_pantalla.cuadro(pantalla, sala, ImVec2(800, 300));
  }
  REQUIRE(ImGui::GetIO().WantTextInput);
  CHECK(aplicar(atajo(ImGuiKey_D, true)) == 1);
  CHECK(aplicar(atajo(ImGuiKey_M, true)) == 0);
  CHECK_FALSE(atajo(ImGuiKey_M, false).silencio);
}

namespace {
Historial pagina(std::uint32_t canal, std::uint64_t desde, Sentido sentido, std::uint64_t primero, std::uint64_t ultimo,
                 bool hay_mas) {
  Historial historial{canal, desde, sentido, hay_mas, {}};
  for (std::uint64_t id = primero; id <= ultimo; ++id) {
    historial.mensajes.push_back({id, static_cast<std::uint32_t>(id), id % 3 == 0 ? "Ana" : "Beto",
                                  1761234567890 + id * 60000, "mensaje " + std::to_string(id)});
  }
  if (sentido == Sentido::viejos) {
    std::reverse(historial.mensajes.begin(), historial.mensajes.end());
  }
  return historial;
}

ventrik::cliente::Sala sala_con_chat(std::uint64_t primero, std::uint64_t ultimo) {
  auto sala = sala_llena(false, 4, 3, 0);
  REQUIRE(sala.pedir_mas_viejos());
  sala.aplicar(pagina(sala.tu_canal(), 0, Sentido::viejos, primero, ultimo, true));
  return sala;
}

std::vector<ui::PedidoServidor> cuadros(SinPantalla& sin_pantalla, ui::PantallaServidor& pantalla,
                                        ventrik::cliente::Sala& sala, int cuantos, float rueda = 0) {
  std::vector<ui::PedidoServidor> pedidos;
  sin_pantalla.rodar(rueda);
  for (int i = 0; i < cuantos; ++i) {
    pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(900, 400)));
  }
  return pedidos;
}

float borde_de(const ui::PantallaServidor& pantalla, std::uint64_t id) {
  const ImGuiWindow* lista = nullptr;
  for (const ImGuiWindow* ventana : ImGui::GetCurrentContext()->Windows) {
    if (std::string_view(ventana->Name).find("##mensajes") != std::string_view::npos) {
      lista = ventana;
    }
  }
  REQUIRE(lista != nullptr);
  const auto& ids = pantalla.alturas.ids;
  const auto i = static_cast<std::size_t>(std::lower_bound(ids.begin(), ids.end(), id) - ids.begin());
  REQUIRE(i < ids.size());
  REQUIRE(ids[i] == id);
  return pantalla.alturas.desde[i + 1] - lista->Scroll.y;
}
}

TEST_CASE("La lista pide los más viejos arriba y los más nuevos abajo, y al cargarlos no salta") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_con_chat(1001, 1300);
  auto pantalla = pantalla_servidor();
  CHECK(ordenes_de<PedirHistorial>(cuadros(sin_pantalla, pantalla, sala, 3)).empty());
  CHECK(sala.chat().ancla == 0);
  const auto viejos = ordenes_de<PedirHistorial>(cuadros(sin_pantalla, pantalla, sala, 3, 100000));
  REQUIRE(viejos.size() == 1);
  CHECK(viejos[0].sentido == Sentido::viejos);
  CHECK(viejos[0].desde == 1001);
  CHECK(viejos[0].cantidad == ventrik::cliente::kMensajesAlMoverse);
  CHECK(sala.chat().ancla == 1001);
  const float arriba = borde_de(pantalla, 1001);
  sala.aplicar(pagina(sala.tu_canal(), 1001, Sentido::viejos, 873, 1000, true));
  REQUIRE(sala.chat().hay_nuevos);
  cuadros(sin_pantalla, pantalla, sala, 3);
  CHECK(std::abs(borde_de(pantalla, 1001) - arriba) < 1);

  const auto nuevos = ordenes_de<PedirHistorial>(cuadros(sin_pantalla, pantalla, sala, 3, -100000));
  REQUIRE(nuevos.size() == 1);
  CHECK(nuevos[0].sentido == Sentido::nuevos);
  CHECK(nuevos[0].desde == sala.chat().mensajes.back().id);
  const auto abajo = sala.chat().ancla;
  REQUIRE(abajo != 0);
  const float antes = borde_de(pantalla, abajo);
  sala.aplicar(pagina(sala.tu_canal(), nuevos[0].desde, Sentido::nuevos, nuevos[0].desde + 1, 1300, false));
  CHECK_FALSE(sala.chat().hay_nuevos);
  CHECK(sala.chat().mensajes.front().id > 873);
  cuadros(sin_pantalla, pantalla, sala, 3);
  CHECK(std::abs(borde_de(pantalla, abajo) - antes) < 1);
  CHECK(ordenes_de<PedirHistorial>(cuadros(sin_pantalla, pantalla, sala, 3, -100000)).empty());
  CHECK(sala.chat().ancla == 0);
}

TEST_CASE("Al escribir mientras faltan los mensajes más nuevos, la lista salta a lo último") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_con_chat(1001, 1300);
  auto pantalla = pantalla_servidor();
  pantalla.enfocar_mensaje = true;
  cuadros(sin_pantalla, pantalla, sala, 2);
  REQUIRE(ordenes_de<PedirHistorial>(cuadros(sin_pantalla, pantalla, sala, 2, 100000)).size() == 1);
  sala.aplicar(pagina(sala.tu_canal(), 1001, Sentido::viejos, 873, 1000, true));
  REQUIRE(sala.chat().hay_nuevos);
  cuadros(sin_pantalla, pantalla, sala, 2);
  sin_pantalla.teclear("hola");
  std::vector<ui::PedidoServidor> pedidos = cuadros(sin_pantalla, pantalla, sala, 2);
  sin_pantalla.apretar(ImGuiKey_Enter);
  const auto despues = cuadros(sin_pantalla, pantalla, sala, 2);
  pedidos.insert(pedidos.end(), despues.begin(), despues.end());
  const auto enviados = ordenes_de<EnviarChat>(pedidos);
  REQUIRE(enviados.size() == 1);
  CHECK(enviados[0].texto == "hola");
  const auto pedidos_historial = ordenes_de<PedirHistorial>(pedidos);
  REQUIRE(pedidos_historial.size() == 1);
  CHECK(pedidos_historial[0].desde == 0);
  CHECK(pedidos_historial[0].sentido == Sentido::viejos);
  CHECK(sala.chat().mensajes.empty());
  sala.aplicar(pagina(sala.tu_canal(), 0, Sentido::viejos, 1237, 1301, true));
  cuadros(sin_pantalla, pantalla, sala, 3);
  CHECK(sala.chat().ancla == 0);
}

TEST_CASE("Los textos guardados de la pantalla cambian con el dato y con el idioma") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(false, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  pantalla.idioma = ventrik::elegir_idioma("es");
  cuadros(sin_pantalla, pantalla, sala, 1);
  CHECK(pantalla.textos.conectados.texto == "3 conectados");
  const std::string tecla = pantalla.textos.tecla.texto;
  CHECK(tecla == ui::nombre_tecla(pantalla.idioma, 0));
  CHECK(pantalla.textos.ping.texto == std::string(ventrik::texto(pantalla.idioma, ventrik::Clave::servidor_sin_ping)));
  sala.aplicar(UsuarioEntro{{9, "Nueva", 1}});
  sala.aplicar(ventrik::cliente::PingMedido{42});
  pantalla.tecla = 0x05;
  cuadros(sin_pantalla, pantalla, sala, 1);
  CHECK(pantalla.textos.conectados.texto == "4 conectados");
  CHECK(pantalla.textos.tecla.texto == ui::nombre_tecla(pantalla.idioma, 0x05));
  CHECK(pantalla.textos.ping.texto == "Ping 42 ms");
  const std::string subtitulo = pantalla.textos.subtitulo.texto;
  pantalla.idioma = ventrik::elegir_idioma("en");
  cuadros(sin_pantalla, pantalla, sala, 1);
  CHECK(pantalla.textos.conectados.texto != "4 conectados");
  CHECK(pantalla.textos.subtitulo.texto != subtitulo);
  CHECK_FALSE(pantalla.caras.empty());
  ui::soltar_lejanos(pantalla);
  CHECK(pantalla.caras.empty());
  CHECK(pantalla.fechas.empty());
}

TEST_CASE("Si borran el canal mientras su cuadro está abierto, el cuadro se cierra; el invitado no puede editar") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(true, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  pantalla.dialogo = ui::Dialogo::cambiar;
  pantalla.id_dialogo = 2;
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
  CHECK(hay_ventana_emergente());
  sin_pantalla.usar_boton(0);
  const ImVec2 editar(130, 305);
  sin_pantalla.cuadro(otra, invitado, editar);
  sin_pantalla.cuadro(otra, invitado, editar, true);
  sin_pantalla.cuadro(otra, invitado, editar);
  CHECK(otra.dialogo == ui::Dialogo::ninguno);
}

TEST_CASE("Con clic derecho en un canal, cualquiera lo silencia, elige de qué avisar, lo reactiva y lo marca leído") {
  using ventrik::cliente::ajuste_de;
  using ventrik::cliente::Avisar;
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(false, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  std::vector<ui::PedidoServidor> pedidos;
  const auto cuadro = [&](ImVec2 donde, bool clic = false) {
    pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, donde, clic));
  };
  const auto tocar = [&](ImVec2 donde) {
    cuadro(donde);
    cuadro(donde, true);
    cuadro(donde);
  };
  const auto abrir_menu = [&] {
    pedidos.clear();
    sin_pantalla.usar_boton(1);
    tocar(ImVec2(100, 192));
    sin_pantalla.usar_boton(0);
    REQUIRE(hay_ventana_emergente());
  };
  const ImVec2 silenciar(130, 254);
  const ImVec2 avisarme(130, 276);
  const auto elegir = [&](ImVec2 submenu, int fila) {
    abrir_menu();
    cuadro(submenu);
    cuadro(submenu);
    tocar(en_menu(fila));
    const auto avisos = avisos_de(pedidos);
    if (avisos) {
      pantalla.avisos = *avisos;
    }
    return avisos.has_value();
  };

  const std::uint64_t antes = ventrik::cliente::ahora_ms();
  REQUIRE(elegir(silenciar, 1));
  CHECK_FALSE(hay_ventana_emergente());
  CHECK(ajuste_de(pantalla.avisos, 2).silenciado_hasta >= antes + 3600000);
  CHECK(ajuste_de(pantalla.avisos, 2).silenciado_hasta <= ventrik::cliente::ahora_ms() + 3600000);
  CHECK(ajuste_de(pantalla.avisos, 2).aun_con_mencion);
  CHECK(pantalla.avisos.servidor.silenciado_hasta == 0);
  cuadro(ImVec2(5, 5));

  REQUIRE(elegir(avisarme, 1));
  CHECK(ajuste_de(pantalla.avisos, 2).avisar == Avisar::todos);
  CHECK(ventrik::cliente::silenciado(ajuste_de(pantalla.avisos, 2), ventrik::cliente::ahora_ms()));

  REQUIRE(elegir(silenciar, 0));
  CHECK(ajuste_de(pantalla.avisos, 2).silenciado_hasta == 0);
  CHECK(ajuste_de(pantalla.avisos, 2).avisar == Avisar::todos);
  REQUIRE(elegir(avisarme, 0));
  CHECK(pantalla.avisos.canales.empty());

  abrir_menu();
  tocar(ImVec2(130, 232));
  const bool marcado = std::any_of(pedidos.begin(), pedidos.end(), [](const ui::PedidoServidor& pedido) {
    return pedido.leidos == std::vector<std::uint32_t>{2};
  });
  CHECK(marcado);
}

TEST_CASE("El ▾ del servidor lo silencia por 1 hora y la cabecera lo dice; el diálogo cambia de qué avisar y quita un canal") {
  using ventrik::cliente::Avisar;
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(true, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  pantalla.avisos.canales = {{4, {0, true, Avisar::nada}}};
  std::vector<ui::PedidoServidor> pedidos;
  const auto cuadro = [&](ImVec2 donde, bool clic = false) {
    pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, donde, clic));
  };
  const auto tocar = [&](ImVec2 donde) {
    cuadro(donde);
    cuadro(donde, true);
    cuadro(donde);
  };
  const ImVec2 nombre(150, 20);
  const auto aplicar = [&] {
    const auto avisos = avisos_de(pedidos);
    if (avisos) {
      pantalla.avisos = *avisos;
    }
    pedidos.clear();
    return avisos.has_value();
  };

  const std::uint64_t antes = ventrik::cliente::ahora_ms();
  cuadro(ImVec2(5, 5));
  tocar(nombre);
  REQUIRE(hay_ventana_emergente());
  cuadro(en_menu(2));
  cuadro(en_menu(2));
  tocar(en_menu(1));
  const auto guarda = [&] {
    return std::any_of(pedidos.begin(), pedidos.end(), [](const ui::PedidoServidor& pedido) { return pedido.guardar; });
  };
  CHECK(guarda());
  REQUIRE(aplicar());
  const std::uint64_t hasta = pantalla.avisos.servidor.silenciado_hasta;
  CHECK(hasta >= antes + 3600000);
  CHECK(hasta <= ventrik::cliente::ahora_ms() + 3600000);
  cuadro(ImVec2(5, 5));
  CHECK(pantalla.textos.silenciado.texto == ui::estado_de_silencio(pantalla.idioma, pantalla.avisos.servidor));
  CHECK(pantalla.textos.silenciado.texto.find(ventrik::cliente::hora_local(hasta)) != std::string::npos);

  tocar(nombre);
  tocar(en_menu(3));
  const bool todos = std::any_of(pedidos.begin(), pedidos.end(), [](const ui::PedidoServidor& pedido) {
    return pedido.leidos == std::vector<std::uint32_t>{1, 2, 3, 4};
  });
  CHECK(todos);
  pedidos.clear();

  tocar(nombre);
  tocar(en_menu(1));
  cuadro(ImVec2(5, 5));
  REQUIRE(hay_ventana_emergente());
  pedidos.clear();
  tocar(en_dialogo(110, 177));
  CHECK_FALSE(guarda());
  REQUIRE(aplicar());
  CHECK(pantalla.avisos.servidor.avisar == Avisar::todos);
  CHECK(pantalla.avisos.canales.size() == 1);
  tocar(en_dialogo(582, 354));
  CHECK_FALSE(guarda());
  REQUIRE(aplicar());
  CHECK(pantalla.avisos.canales.empty());
  CHECK(pantalla.avisos.servidor.silenciado_hasta == hasta);
  cuadro(ImVec2(5, 5));
  tocar(en_dialogo(40, 233));
  CHECK_FALSE(guarda());
  REQUIRE(aplicar());
  CHECK(pantalla.avisos.contar_todos);
  sin_pantalla.apretar(ImGuiKey_Escape);
  cuadro(ImVec2(5, 5));
  cuadro(ImVec2(5, 5));
  CHECK_FALSE(hay_ventana_emergente());
  CHECK(guarda());
  pedidos.clear();

  tocar(nombre);
  tocar(ImVec2(100, 243));
  const bool desconectar = std::any_of(pedidos.begin(), pedidos.end(),
                                       [](const ui::PedidoServidor& pedido) { return pedido.desconectar; });
  CHECK(desconectar);
}

TEST_CASE("Dos cuadros iguales no cambian nada, y el mouse sobre una fila cambia solo una zona chica") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(false, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  ui::Zonas zonas;
  const auto cambiadas = [&](ImVec2 mouse) {
    sin_pantalla.cuadro(pantalla, sala, mouse);
    return zonas.cambiadas(*ImGui::GetDrawData());
  };
  for (int i = 0; i < 3; ++i) {
    cambiadas(ImVec2(900, 400));
  }
  CHECK(cambiadas(ImVec2(900, 400)).empty());
  CHECK(cambiadas(ImVec2(901, 401)).empty());
  const std::vector<RECT> fila = cambiadas(ImVec2(100, 192));
  REQUIRE(fila.size() == 1);
  CHECK(PtInRect(&fila.front(), POINT{100, 192}));
  CHECK((fila.front().right - fila.front().left) * (fila.front().bottom - fila.front().top) < 1376 * 768 / 8);
  CHECK(cambiadas(ImVec2(100, 192)).empty());
  const std::vector<RECT> vuelta = cambiadas(ImVec2(900, 400));
  REQUIRE(vuelta.size() == 1);
  CHECK(PtInRect(&vuelta.front(), POINT{100, 192}));
}

TEST_CASE("Una ventana sin bordes que cubre el monitor es pantalla completa") {
  const RECT monitor = {0, 0, 1920, 1080};
  CHECK(ui::tapa_el_monitor({0, 0, 1920, 1080}, monitor, WS_POPUP | WS_VISIBLE, 0));
  CHECK(ui::tapa_el_monitor({-8, -8, 1928, 1088}, monitor, WS_POPUP, 0));
  CHECK_FALSE(ui::tapa_el_monitor({0, 0, 1920, 1040}, monitor, WS_POPUP, 0));
  CHECK_FALSE(ui::tapa_el_monitor({-8, -8, 1928, 1088}, monitor, WS_OVERLAPPEDWINDOW | WS_MAXIMIZE, 0));
  CHECK_FALSE(ui::tapa_el_monitor({0, 0, 1920, 1080}, monitor, WS_POPUP, WS_EX_TOOLWINDOW));
  CHECK_FALSE(ui::tapa_el_monitor({1920, 0, 3840, 1080}, monitor, WS_POPUP, 0));
}

TEST_CASE("Si Direct3D no vuelve con Reset en 2 segundos, se arma de nuevo") {
  CHECK(ui::como_recuperar(S_OK, false, 0) == ui::Arreglo::ninguno);
  CHECK(ui::como_recuperar(S_PRESENT_OCCLUDED, false, 0) == ui::Arreglo::ninguno);
  CHECK(ui::como_recuperar(S_OK, true, 0) == ui::Arreglo::reiniciar);
  CHECK(ui::como_recuperar(D3DERR_DEVICENOTRESET, false, 0) == ui::Arreglo::reiniciar);
  CHECK(ui::como_recuperar(D3DERR_DEVICEHUNG, false, 1999) == ui::Arreglo::reiniciar);
  CHECK(ui::como_recuperar(D3DERR_DEVICELOST, true, 1999) == ui::Arreglo::esperar);
  CHECK(ui::como_recuperar(D3DERR_DEVICELOST, false, 2000) == ui::Arreglo::recrear);
  CHECK(ui::como_recuperar(D3DERR_DEVICENOTRESET, false, 2000) == ui::Arreglo::recrear);
  CHECK(ui::como_recuperar(D3DERR_DEVICEREMOVED, false, 0) == ui::Arreglo::recrear);
}

TEST_CASE("Las teclas repetidas no piden cuadros salvo las de edición escribiendo, los caracteres solo escribiendo, y los cuadros lentos se espacian") {
  constexpr LPARAM kRepetida = 0x40000000;
  std::optional<LPARAM> mouse;
  CHECK(ui::pide_cuadros(WM_KEYDOWN, VK_CONTROL, 0, false, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_KEYDOWN, VK_CONTROL, kRepetida, true, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_SYSKEYDOWN, VK_MENU, kRepetida, false, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_KEYDOWN, VK_BACK, kRepetida, false, mouse));
  CHECK(ui::pide_cuadros(WM_KEYDOWN, VK_BACK, kRepetida, true, mouse));
  CHECK(ui::pide_cuadros(WM_KEYDOWN, VK_LEFT, kRepetida, true, mouse));
  CHECK(ui::pide_cuadros(WM_KEYDOWN, VK_DELETE, kRepetida, true, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_KEYDOWN, VK_F5, kRepetida, true, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_KEYDOWN, VK_CAPITAL, kRepetida, true, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_KEYDOWN, 'V', kRepetida, true, mouse));
  CHECK(ui::pide_cuadros(WM_KEYDOWN, VK_F5, 0, true, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_CHAR, 'v', 0, false, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_CHAR, 'v', kRepetida, false, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_DEADCHAR, '`', kRepetida, false, mouse));
  CHECK(ui::pide_cuadros(WM_CHAR, 'v', kRepetida, true, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_NCMOUSEMOVE, 0, 0, false, mouse));
  CHECK(ui::pide_cuadros(WM_MOUSEMOVE, 0, 7, false, mouse));
  CHECK_FALSE(ui::pide_cuadros(WM_MOUSEMOVE, 0, 7, false, mouse));
  CHECK(ui::pide_cuadros(WM_MOUSEMOVE, 0, 8, false, mouse));
  CHECK(ui::pide_cuadros(WM_MOUSELEAVE, 0, 0, false, mouse));
  CHECK(ui::pide_cuadros(WM_MOUSEMOVE, 0, 8, false, mouse));
  CHECK(ui::espera_entre_cuadros(1.4) == 15);
  CHECK(ui::espera_entre_cuadros(16) == 80);
}

TEST_CASE("La ventanita de avisos se abre, se actualiza, no pasa de 3, se cierra y un clic la marca tocada") {
  using ventrik::cliente::Notificacion;
  ui::AvisosEnPantalla en_pantalla(nullptr, 1.0f);
  en_pantalla.mostrar({Notificacion{5, false, "srv \xC2\xB7 #general", "Tano: hola", 1}});
  CHECK(en_pantalla.abiertas() == 1);
  CHECK(en_pantalla.mostrados() == 1);
  en_pantalla.mostrar({Notificacion{5, false, "srv \xC2\xB7 #general", "Tano: hola", 1}});
  CHECK(en_pantalla.mostrados() == 1);
  en_pantalla.mostrar({Notificacion{5, false, "srv \xC2\xB7 #general", "2 mensajes de Tano", 2}});
  CHECK(en_pantalla.abiertas() == 1);
  CHECK(en_pantalla.mostrados() == 1);
  en_pantalla.mostrar({Notificacion{5, false, "a", "b", 2}, Notificacion{6, false, "a", "b", 3},
                       Notificacion{7, false, "a", "b", 4}, Notificacion{8, true, "a", "b", 5}});
  CHECK(en_pantalla.abiertas() == ui::kAvisosALaVez);
  CHECK(en_pantalla.mostrados() == 4);
  const auto alguna = [] {
    HWND ventana = nullptr;
    while ((ventana = FindWindowExW(nullptr, ventana, L"VentrikAviso", nullptr)) != nullptr &&
           GetWindowThreadProcessId(ventana, nullptr) != GetCurrentThreadId()) {
    }
    return ventana;
  };
  const auto clic = [](HWND ventana, int x, int y) {
    SendMessageW(ventana, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessageW(ventana, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
  };
  HWND ventana = alguna();
  REQUIRE(ventana != nullptr);
  SendMessageW(ventana, WM_LBUTTONUP, 0, MAKELPARAM(20, 20));
  CHECK_FALSE(en_pantalla.tocado());
  CHECK(en_pantalla.abiertas() == ui::kAvisosALaVez);
  RECT area = {};
  GetClientRect(ventana, &area);
  clic(ventana, area.right - 5, 5);
  CHECK_FALSE(en_pantalla.tocado());
  CHECK(en_pantalla.abiertas() == ui::kAvisosALaVez - 1);
  ventana = alguna();
  REQUIRE(ventana != nullptr);
  clic(ventana, 20, 20);
  const auto tocado = en_pantalla.tocado();
  REQUIRE(tocado);
  CHECK(tocado->canal >= 6);
  CHECK_FALSE(en_pantalla.tocado());
  CHECK(en_pantalla.abiertas() == ui::kAvisosALaVez - 2);
  en_pantalla.ocultar();
  CHECK(en_pantalla.abiertas() == 0);
  en_pantalla.mostrar({Notificacion{6, false, "a", "b", 3}});
  CHECK(en_pantalla.abiertas() == 0);
}

TEST_CASE("banco del aviso" * doctest::skip()) {
  constexpr int kVueltas = 1000;
  const ui::PiezasAviso piezas(1.0f);
  const ventrik::cliente::Notificacion aviso{5, false, "192.168.0.10:7700 \xC2\xB7 #general",
                                             "3 mensajes de Tano, Sofi y 1 m\xC3\xA1s", 1};
  const HDC pantalla = GetDC(nullptr);
  const HDC dc = CreateCompatibleDC(pantalla);
  const SIZE tamano = ui::medir_aviso(dc, aviso, piezas);
  const HBITMAP lienzo = CreateCompatibleBitmap(pantalla, tamano.cx, tamano.cy);
  const HGDIOBJ antes = SelectObject(dc, lienzo);
  const RECT area = {0, 0, tamano.cx, tamano.cy};
  const auto desde = std::chrono::steady_clock::now();
  for (int i = 0; i < kVueltas; ++i) {
    ui::medir_aviso(dc, aviso, piezas);
    ui::pintar_aviso(dc, area, aviso, piezas, false);
  }
  MESSAGE("ventanita de aviso de " << tamano.cx << "x" << tamano.cy << ": "
                                   << std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - desde)
                                              .count() / kVueltas
                                   << " us por dibujo");
  SelectObject(dc, antes);
  DeleteObject(lienzo);
  DeleteDC(dc);
  ReleaseDC(nullptr, pantalla);
}

TEST_CASE("banco de las zonas que cambian" * doctest::skip()) {
  constexpr int kVueltas = 1000;
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(true, 60, 512, 300, "hola, este es un mensaje comun de unas cuantas palabras");
  auto pantalla = pantalla_servidor();
  sin_pantalla.cuadro(pantalla, sala, ImVec2(900, 400));
  ui::Zonas zonas;
  const auto desde = std::chrono::steady_clock::now();
  for (int i = 0; i < kVueltas; ++i) {
    zonas.cambiadas(*ImGui::GetDrawData());
  }
  MESSAGE("zonas que cambian, servidor con 60 canales y 512 personas: "
          << std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - desde).count() / kVueltas
          << " us por cuadro");
}

TEST_CASE("banco de las pantallas" * doctest::skip()) {
  static constexpr int kCuadros = 1000;
  const auto medir = [](const std::function<void()>& cuadro) {
    for (int i = 0; i < 30; ++i) {
      cuadro();
    }
    const auto desde = std::chrono::steady_clock::now();
    for (int i = 0; i < kCuadros; ++i) {
      cuadro();
    }
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - desde).count() / kCuadros;
  };
  for (const auto& [canales, personas] : {std::make_pair(6u, 10u), std::make_pair(60u, 512u)}) {
    SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
    auto sala = sala_llena(true, canales, personas, 300, "hola, este es un mensaje comun de unas cuantas palabras");
    auto pantalla = pantalla_servidor();
    const double us = medir([&] { sin_pantalla.cuadro(pantalla, sala, ImVec2(900, 400)); });
    MESSAGE("servidor con " << canales << " canales y " << personas << " personas: " << us << " us por cuadro, "
                            << ImGui::GetDrawData()->TotalVtxCount << " vertices");
  }
  {
    using ventrik::cliente::kHastaQueLoActive;
    SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
    auto sala = sala_llena(true, 60, 512, 300, "hola, este es un mensaje comun de unas cuantas palabras");
    auto pantalla = pantalla_servidor();
    pantalla.avisos.servidor.silenciado_hasta = kHastaQueLoActive;
    for (std::uint32_t canal = 1; canal <= 60; canal += 2) {
      ventrik::cliente::cambiar_canal(pantalla.avisos, canal, {kHastaQueLoActive, true, ventrik::cliente::Avisar::nada});
    }
    double us = medir([&] { sin_pantalla.cuadro(pantalla, sala, ImVec2(900, 400)); });
    MESSAGE("servidor silenciado, con 30 de sus 60 canales silenciados: " << us << " us por cuadro, "
                                                                           << ImGui::GetDrawData()->TotalVtxCount
                                                                           << " vertices");
    pantalla.abrir_avisos = true;
    us = medir([&] { sin_pantalla.cuadro(pantalla, sala, ImVec2(900, 400)); });
    MESSAGE("con el dialogo de avisos abierto y 30 canales en la lista: "
            << us << " us por cuadro, " << ImGui::GetDrawData()->TotalVtxCount << " vertices");
  }
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  ui::PantallaInicio pantalla;
  Guardados guardados;
  for (int i = 0; i < 10; ++i) {
    guardados.servidores.push_back({{0x7F000001u, static_cast<std::uint16_t>(7700 + i)}, {}, "Servidor", 0, {}, 0, {}});
  }
  const double us = medir([&] { sin_pantalla.cuadro(pantalla, guardados, ImVec2(900, 400)); });
  MESSAGE("inicio con 10 servidores: " << us << " us por cuadro, " << ImGui::GetDrawData()->TotalVtxCount
                                       << " vertices");
}

TEST_CASE("en Inicio, el clic derecho en una tarjeta silencia ese servidor y abre sus avisos sin conexión") {
  SinPantalla sin_pantalla;
  ui::PantallaInicio pantalla;
  Guardados guardados = muchos();
  guardados.servidores[0].avisos.canales = {{7, {0, true, ventrik::cliente::Avisar::nada}}};
  std::vector<ui::Pedido> pedidos;
  const auto cuadro = [&](ImVec2 donde, bool clic = false) {
    pedidos.push_back(sin_pantalla.cuadro(pantalla, guardados, donde, clic));
  };
  const auto tocar = [&](ImVec2 donde) {
    cuadro(donde);
    cuadro(donde, true);
    cuadro(donde);
  };
  const auto abrir_menu = [&] {
    pedidos.clear();
    sin_pantalla.usar_boton(1);
    tocar(ImVec2(200, 280));
    sin_pantalla.usar_boton(0);
    REQUIRE(hay_ventana_emergente());
  };

  const std::uint64_t antes = ventrik::cliente::ahora_ms();
  abrir_menu();
  cuadro(en_menu(2));
  cuadro(en_menu(2));
  tocar(en_menu(5));
  auto avisos = avisos_de(pedidos);
  REQUIRE(avisos);
  CHECK(avisos->servidor.silenciado_hasta == ventrik::cliente::kHastaQueLoActive);
  CHECK(avisos->servidor.silenciado_hasta > antes);
  CHECK(avisos->canales.size() == 1);

  abrir_menu();
  tocar(en_menu(3));
  CHECK_FALSE(avisos_de(pedidos));
  CHECK(hay_ventana_emergente());
  sin_pantalla.apretar(ImGuiKey_Escape);
  cuadro(ImVec2(5, 5));
  cuadro(ImVec2(5, 5));
  CHECK_FALSE(hay_ventana_emergente());

  abrir_menu();
  tocar(en_menu(1));
  cuadro(ImVec2(5, 5));
  REQUIRE(hay_ventana_emergente());
  CHECK(pantalla.avisos_de == guardados.servidores[0].direccion);
  pedidos.clear();
  tocar(en_dialogo(527, 177));
  avisos = avisos_de(pedidos);
  REQUIRE(avisos);
  CHECK(avisos->servidor.avisar == ventrik::cliente::Avisar::nada);
  CHECK(avisos->canales.size() == 1);
  const bool del_primero = std::all_of(pedidos.begin(), pedidos.end(),
                                       [](const ui::Pedido& pedido) { return !pedido.avisos || pedido.cual == 0; });
  CHECK(del_primero);
}

TEST_CASE("El diálogo de avisos entra en la pantalla y se desplaza aunque tenga los 60 canales con ajuste propio") {
  for (const auto& [escala, tamano] : {std::make_pair(1.0f, ImVec2(1100, 640)), std::make_pair(1.5f, ImVec2(1650, 960))}) {
    SinPantalla sin_pantalla(escala, tamano);
    auto sala = sala_llena(true, 60, 3, 5);
    auto pantalla = pantalla_servidor();
    for (std::uint32_t canal = 1; canal <= 60; ++canal) {
      ventrik::cliente::cambiar_canal(pantalla.avisos, canal, {0, true, ventrik::cliente::Avisar::todos});
    }
    pantalla.abrir_avisos = true;
    sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
    sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
    REQUIRE(hay_ventana_emergente());
    const ImGuiWindow& dialogo = *ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    CHECK(dialogo.Pos.y >= 0);
    CHECK(dialogo.Pos.y + dialogo.Size.y <= tamano.y);
    const ImVec2 lista = en_dialogo(dialogo.Size.x / 2, ui::px(360));
    for (int i = 0; i < 40; ++i) {
      sin_pantalla.rodar(-1);
      sin_pantalla.cuadro(pantalla, sala, lista);
    }
    CHECK(pantalla.avisos.canales.size() == 60);
  }
}

TEST_CASE("Listo cierra el diálogo de avisos, con canales propios o sin ninguno, y el fondo se oscurece de una vez") {
  for (const std::size_t propios : {0u, 1u}) {
    for (const ImVec2 tamano : {ImVec2(1376, 768), ImVec2(1024, 540)}) {
      SinPantalla sin_pantalla(1.0f, tamano);
      auto sala = sala_llena(true, 4, 3, 5);
      auto pantalla = pantalla_servidor();
      if (propios > 0) {
        pantalla.avisos.canales = {{4, {0, true, ventrik::cliente::Avisar::nada}}};
      }
      pantalla.abrir_avisos = true;
      sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
      REQUIRE(hay_ventana_emergente());
      CHECK(ImGui::GetCurrentContext()->DimBgRatio == 1.0f);
      sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
      const ImGuiWindow& dialogo = *ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
      CHECK(dialogo.Pos.y + dialogo.Size.y <= tamano.y);
      const ImVec2 listo = en_dialogo(dialogo.Size.x - 40, dialogo.Size.y - 39);
      bool guarda = false;
      for (const bool clic : {false, true, false}) {
        guarda = sin_pantalla.cuadro(pantalla, sala, listo, clic).guardar || guarda;
      }
      CHECK_FALSE(hay_ventana_emergente());
      CHECK(guarda);
    }
  }
}

TEST_CASE("Los cuadros y su fondo oscuro aparecen y se van en el mismo dibujo, sin pasos intermedios") {
  for (const int como : {0, 1, 2}) {
    SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
    auto sala = sala_llena(true, 4, 3, 5);
    auto pantalla = pantalla_servidor();
    const std::string_view nombre = como == 2 ? "##dialogo" : "##avisos";
    const auto abrir = [&] {
      if (como == 2) {
        pantalla.dialogo = ui::Dialogo::crear_texto;
        pantalla.abrir_dialogo = true;
      } else {
        pantalla.abrir_avisos = true;
      }
    };
    std::vector<std::pair<bool, bool>> vistos;
    const auto cuadro = [&](ImVec2 donde, bool clic = false) {
      sin_pantalla.cuadro(pantalla, sala, donde, clic);
      vistos.push_back(velo_y_ventana(nombre));
    };
    for (int vez = 0; vez < 2; ++vez) {
      abrir();
      for (int i = 0; i < 3; ++i) {
        cuadro(ImVec2(5, 5));
      }
      REQUIRE(vistos.back() == std::make_pair(true, true));
      if (como == 0) {
        const ImGuiWindow& dialogo = *ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        const ImVec2 listo = en_dialogo(dialogo.Size.x - 40, dialogo.Size.y - 39);
        cuadro(listo);
        cuadro(listo, true);
        cuadro(listo);
      } else {
        sin_pantalla.apretar(ImGuiKey_Escape);
        cuadro(ImVec2(5, 5));
      }
      for (int i = 0; i < 3; ++i) {
        cuadro(ImVec2(5, 5));
      }
      CHECK(vistos.back() == std::make_pair(false, false));
    }
    for (const auto& [velo, ventana] : vistos) {
      CHECK(velo == ventana);
    }
  }
}

TEST_CASE("En Rendimiento nada se anima: con el mouse quieto, ninguna pantalla, menú ni cuadro cambia solo") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(true, 4, 3, 5);
  const ImVec2 quieto(5, 5);
  const auto preparar = [&](int cual, ui::PantallaServidor& pantalla) {
    const auto tocar = [&](ImVec2 donde) {
      sin_pantalla.cuadro(pantalla, sala, donde);
      sin_pantalla.cuadro(pantalla, sala, donde, true);
      sin_pantalla.cuadro(pantalla, sala, donde);
    };
    switch (cual) {
      case 1:
        pantalla.abrir_avisos = true;
        break;
      case 2:
        pantalla.dialogo = ui::Dialogo::crear_texto;
        pantalla.abrir_dialogo = true;
        break;
      case 3:
        tocar(ImVec2(150, 20));
        break;
      case 4:
        sin_pantalla.usar_boton(1);
        tocar(ImVec2(100, 192));
        sin_pantalla.usar_boton(0);
        break;
      default:
        break;
    }
  };
  for (int cual = 0; cual <= 4; ++cual) {
    auto pantalla = pantalla_servidor();
    sin_pantalla.cuadro(pantalla, sala, quieto);
    preparar(cual, pantalla);
    for (int i = 0; i < 3; ++i) {
      sin_pantalla.cuadro(pantalla, sala, quieto);
    }
    CHECK(hay_ventana_emergente() == (cual != 0));
    ui::Zonas zonas;
    zonas.cambiadas(*ImGui::GetDrawData());
    std::size_t cambios = 0;
    for (int i = 0; i < 90; ++i) {
      sin_pantalla.cuadro(pantalla, sala, quieto);
      cambios += zonas.cambiadas(*ImGui::GetDrawData()).size();
    }
    CHECK_MESSAGE(cambios == 0, "lo abierto número " << cual << " cambió solo con el tiempo");
    ImGui::GetCurrentContext()->OpenPopupStack.resize(0);
    pantalla.dialogo = ui::Dialogo::ninguno;
  }
}

TEST_CASE("Los menús aparecen enteros de una vez: el dibujo en que nacen ocultos pide otro, y después no se mueven") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(false, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  std::vector<std::pair<int, ImRect>> vistos;
  const auto cuadro = [&](ImVec2 donde, bool clic = false) {
    sin_pantalla.cuadro(pantalla, sala, donde, clic);
    const auto& pila = ImGui::GetCurrentContext()->OpenPopupStack;
    const ImGuiWindow* menu = pila.empty() ? nullptr : pila.back().Window;
    if (menu != nullptr && menu->Active) {
      CHECK((!menu->Hidden || ui::por_aparecer()));
      vistos.emplace_back(pila.Size, menu->Hidden ? ImRect() : menu->Rect());
    }
  };
  const auto revisar = [&] {
    for (std::size_t i = 1; i < vistos.size(); ++i) {
      const auto& [nivel, caja] = vistos[i];
      const auto& [nivel_antes, caja_antes] = vistos[i - 1];
      if (nivel == nivel_antes && caja_antes.GetWidth() > 0) {
        CHECK(caja.Min.x == caja_antes.Min.x);
        CHECK(caja.Min.y == caja_antes.Min.y);
        CHECK(caja.Max.x == caja_antes.Max.x);
        CHECK(caja.Max.y == caja_antes.Max.y);
      }
    }
    vistos.clear();
  };
  cuadro(ImVec2(5, 5));
  for (const ImVec2 nombre : {ImVec2(150, 20), ImVec2(150, 20)}) {
    cuadro(nombre);
    cuadro(nombre, true);
    for (int i = 0; i < 4; ++i) {
      cuadro(nombre);
    }
    for (int i = 0; i < 4; ++i) {
      cuadro(en_menu(2));
    }
    revisar();
    sin_pantalla.apretar(ImGuiKey_Escape);
    cuadro(ImVec2(5, 5));
    sin_pantalla.apretar(ImGuiKey_Escape);
    cuadro(ImVec2(5, 5));
    cuadro(ImVec2(5, 5));
    vistos.clear();
  }
  sin_pantalla.usar_boton(1);
  cuadro(ImVec2(100, 192));
  cuadro(ImVec2(100, 192), true);
  sin_pantalla.usar_boton(0);
  for (int i = 0; i < 4; ++i) {
    cuadro(ImVec2(130, 254));
  }
  revisar();
}

TEST_CASE("En el diálogo de avisos, Escape cierra primero el menú de \"Agregar canal\" y después el diálogo") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(true, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  pantalla.abrir_avisos = true;
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  const ImVec2 agregar = en_dialogo(560, 314);
  sin_pantalla.cuadro(pantalla, sala, agregar);
  sin_pantalla.cuadro(pantalla, sala, agregar, true);
  sin_pantalla.cuadro(pantalla, sala, agregar);
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  REQUIRE(ImGui::GetCurrentContext()->OpenPopupStack.Size == 2);
  sin_pantalla.apretar(ImGuiKey_Escape);
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  CHECK(ImGui::GetCurrentContext()->OpenPopupStack.Size == 1);
  sin_pantalla.apretar(ImGuiKey_Escape);
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
  CHECK_FALSE(hay_ventana_emergente());
}

namespace {
ui::PantallaAjustes pantalla_ajustes() {
  ui::PantallaAjustes pantalla;
  pantalla.dispositivos = {{"Micrófono USB", "Micrófono de la cámara con un nombre muy largo que no entra"},
                           {"Auriculares", "Parlantes"}};
  pantalla.hay_bajar_otros = true;
  pantalla.personas = {"Sofi", "Meli", "Un nombre bastante largo, de 32"};
  pantalla.canales = {"General", "Partida 2"};
  return pantalla;
}

ui::PantallaAjustes pantalla_segura(bool con_maestra) {
  auto pantalla = pantalla_ajustes();
  pantalla.seccion = ui::Seccion::seguridad;
  pantalla.estado_identidad = ui::EstadoDeIdentidad::lista;
  pantalla.identidad = "CTBC-P44I-2FC4-QX7M";
  pantalla.con_maestra = con_maestra;
  return pantalla;
}

ui::PedidoAjustes bloqueo(SinPantalla& sin_pantalla, ui::PantallaAjustes& pantalla, const Guardados& guardados,
                          ImVec2 mouse, bool clic = false) {
  ui::PedidoAjustes pedido;
  sin_pantalla.cuadro([&] { pedido = ui::dibujar_bloqueo(pantalla, guardados); }, mouse, clic);
  return pedido;
}

}

TEST_CASE("Ajustes se dibuja sin errores en todas sus secciones, a cualquier tamaño y escala") {
  for (const auto& [escala, tamano] : {std::make_pair(1.0f, ImVec2(1100, 640)), std::make_pair(1.0f, ImVec2(1376, 768)),
                                       std::make_pair(1.5f, ImVec2(1650, 960))}) {
    SinPantalla sin_pantalla(escala, tamano);
    const Guardados guardados = muchos();
    for (std::size_t seccion = 0; seccion < ui::kSecciones; ++seccion) {
      auto pantalla = pantalla_ajustes();
      pantalla.seccion = static_cast<ui::Seccion>(seccion);
      pantalla.probando_microfono = seccion % 2 == 0;
      pantalla.nivel = 13;
      for (float y = 5; y < tamano.y; y += 61) {
        for (float x = 5; x < tamano.x; x += 97) {
          const auto pedido = sin_pantalla.cuadro(pantalla, guardados, ImVec2(x, y));
          CHECK_FALSE(pedido.guardados);
        }
      }
    }
  }
}

TEST_CASE("En Ajustes, el buscador salta a la sección del ajuste, una casilla lo cambia y Volver o Escape salen") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1280, 900));
  Guardados guardados = muchos();
  auto pantalla = pantalla_ajustes();
  pantalla.idioma = ventrik::elegir_idioma("es");
  ui::poner(pantalla.buscar, "eco");
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  CHECK(pantalla.seccion == ui::Seccion::audio);
  CHECK(pantalla.coinciden[static_cast<std::size_t>(ui::Seccion::audio)]);
  CHECK_FALSE(pantalla.coinciden[static_cast<std::size_t>(ui::Seccion::general)]);
  ui::poner(pantalla.buscar, "");
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  CHECK(std::all_of(pantalla.coinciden.begin(), pantalla.coinciden.end(), [](bool si) { return si; }));

  pantalla.seccion = ui::Seccion::general;
  std::vector<ui::PedidoAjustes> pedidos;
  const auto tocar = [&](ImVec2 donde) {
    pedidos.clear();
    for (const bool clic : {false, true, false}) {
      pedidos.push_back(sin_pantalla.cuadro(pantalla, guardados, donde, clic));
    }
  };
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  tocar(ImVec2(305, 197));
  const auto cambiados = std::find_if(pedidos.begin(), pedidos.end(), [](const auto& pedido) { return pedido.guardados; });
  REQUIRE(cambiados != pedidos.end());
  CHECK(ventrik::cliente::ajuste_u8(*cambiados->guardados, ventrik::cliente::Ajuste::entrar_al_ultimo) == 0);
  tocar(ImVec2(305, 151));
  CHECK(std::any_of(pedidos.begin(), pedidos.end(), [](const auto& pedido) { return pedido.arrancar_con_windows == true; }));

  tocar(ImVec2(50, 27));
  CHECK(std::any_of(pedidos.begin(), pedidos.end(), [](const auto& pedido) { return pedido.volver; }));
  sin_pantalla.apretar(ImGuiKey_Escape);
  CHECK(sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5)).volver);
}

TEST_CASE("En Ajustes, el Escape que cancela la tecla o cierra una lista no saca de la pantalla") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1280, 900));
  const Guardados guardados = muchos();
  auto pantalla = pantalla_ajustes();
  pantalla.seccion = ui::Seccion::teclas;
  pantalla.eligiendo_tecla = true;
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  pantalla.eligiendo_tecla = false;
  sin_pantalla.apretar(ImGuiKey_Escape);
  CHECK_FALSE(sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5)).volver);

  pantalla.seccion = ui::Seccion::general;
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  for (const bool clic : {false, true, false}) {
    sin_pantalla.cuadro(pantalla, guardados, ImVec2(350, 270), clic);
  }
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(350, 270));
  REQUIRE(ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
  sin_pantalla.apretar(ImGuiKey_Escape);
  CHECK_FALSE(sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5)).volver);
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  CHECK_FALSE(ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
  sin_pantalla.apretar(ImGuiKey_Escape);
  CHECK(sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5)).volver);
}

TEST_CASE("En Ajustes, Agregar susurro suma uno y abre a quién; con cuatro ya no se puede") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1280, 900));
  Guardados guardados = muchos();
  auto pantalla = pantalla_ajustes();
  pantalla.seccion = ui::Seccion::teclas;
  const auto tocar = [&](ImVec2 donde) {
    std::optional<Guardados> cambiados;
    for (const bool clic : {false, true, false}) {
      auto pedido = sin_pantalla.cuadro(pantalla, guardados, donde, clic);
      if (pedido.guardados) {
        cambiados = std::move(pedido.guardados);
      }
    }
    return cambiados;
  };
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  const auto agregado = tocar(ImVec2(1170, 328));
  REQUIRE(agregado);
  CHECK(ventrik::cliente::susurro_de(*agregado, 2));
  CHECK(pantalla.destinos_de == 2);
  guardados = *agregado;
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  CHECK(ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
  const auto quieta = [&] {
    ui::Zonas zonas;
    zonas.cambiadas(*ImGui::GetDrawData());
    std::size_t cambios = 0;
    for (int i = 0; i < 60; ++i) {
      sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
      cambios += zonas.cambiadas(*ImGui::GetDrawData()).size();
    }
    return cambios == 0;
  };
  CHECK(quieta());
  sin_pantalla.apretar(ImGuiKey_Escape);
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  CHECK_FALSE(ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
  for (const bool clic : {false, true, false}) {
    sin_pantalla.cuadro(pantalla, guardados, ImVec2(1190, 222), clic);
  }
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  CHECK(ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
  CHECK(quieta());
  sin_pantalla.apretar(ImGuiKey_Escape);
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  ventrik::cliente::poner_susurro(guardados, 3, ventrik::cliente::Susurro{});
  sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
  CHECK_FALSE(tocar(ImVec2(1170, 434)));
}

TEST_CASE("En Ajustes, las secciones que llegan más adelante no tienen controles") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1280, 900));
  const Guardados guardados = muchos();
  for (const auto seccion : {ui::Seccion::apariencia, ui::Seccion::red, ui::Seccion::descargas, ui::Seccion::prestar,
                             ui::Seccion::seguridad, ui::Seccion::amigos, ui::Seccion::copias, ui::Seccion::acerca}) {
    auto pantalla = pantalla_ajustes();
    pantalla.seccion = seccion;
    for (float y = 70; y < 400; y += 23) {
      for (float x = 270; x < 1270; x += 47) {
        for (const bool clic : {false, true, false}) {
          const auto pedido = sin_pantalla.cuadro(pantalla, guardados, ImVec2(x, y), clic);
          CHECK_FALSE(pedido.guardados);
          CHECK_FALSE(pedido.probar_salida);
          CHECK_FALSE(pedido.elegir_tecla);
        }
      }
    }
    CHECK(pantalla.seccion == seccion);
  }
}

TEST_CASE("Ajustes no se anima con el mouse quieto") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  const Guardados guardados = muchos();
  for (std::size_t seccion = 0; seccion < ui::kSecciones; ++seccion) {
    auto pantalla = pantalla_ajustes();
    pantalla.seccion = static_cast<ui::Seccion>(seccion);
    for (int i = 0; i < 3; ++i) {
      sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
    }
    ui::Zonas zonas;
    zonas.cambiadas(*ImGui::GetDrawData());
    std::size_t cambios = 0;
    for (int i = 0; i < 60; ++i) {
      sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
      cambios += zonas.cambiadas(*ImGui::GetDrawData()).size();
    }
    CHECK_MESSAGE(cambios == 0, "la sección " << seccion << " cambió sola");
  }
}

TEST_CASE("banco de ajustes" * doctest::skip()) {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  const Guardados guardados = muchos();
  for (const auto seccion : {ui::Seccion::general, ui::Seccion::audio, ui::Seccion::avisos}) {
    auto pantalla = pantalla_ajustes();
    pantalla.seccion = seccion;
    for (int i = 0; i < 30; ++i) {
      sin_pantalla.cuadro(pantalla, guardados, ImVec2(900, 400));
    }
    const auto desde = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
      sin_pantalla.cuadro(pantalla, guardados, ImVec2(900, 400));
    }
    MESSAGE("ajustes, sección " << static_cast<int>(seccion) << ": "
                                << std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - desde)
                                           .count() /
                                       1000
                                << " us por cuadro, " << ImGui::GetDrawData()->TotalVtxCount << " vertices");
  }
}

TEST_CASE("La pantalla de la contraseña maestra se dibuja sin errores a cualquier tamaño, al abrir y bloqueado") {
  for (const auto& [escala, tamano] : {std::make_pair(1.0f, ImVec2(1100, 640)), std::make_pair(1.0f, ImVec2(1376, 768)),
                                       std::make_pair(1.5f, ImVec2(1650, 960))}) {
    SinPantalla sin_pantalla(escala, tamano);
    const Guardados guardados = muchos();
    for (const bool conectado : {false, true}) {
      for (const bool calculando : {false, true}) {
        auto pantalla = pantalla_ajustes();
        pantalla.con_conexion = conectado;
        pantalla.calculando = calculando;
        pantalla.error_identidad = "La contraseña no es correcta.";
        for (float y = 5; y < tamano.y; y += 61) {
          for (float x = 5; x < tamano.x; x += 97) {
            const auto pedido = bloqueo(sin_pantalla, pantalla, guardados, ImVec2(x, y));
            CHECK_FALSE(pedido.desbloquear);
            CHECK_FALSE(pedido.guardados);
          }
        }
      }
    }
  }
}

TEST_CASE("En la pantalla de la contraseña maestra, Enter desbloquea solo con algo escrito") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  const Guardados guardados = muchos();
  auto pantalla = pantalla_ajustes();
  const auto desbloquea = [&] {
    bool desbloquear = false;
    for (int i = 0; i < 3; ++i) {
      desbloquear = bloqueo(sin_pantalla, pantalla, guardados, ImVec2(5, 5)).desbloquear || desbloquear;
    }
    return desbloquear;
  };
  pantalla.enfocar_contrasena = true;
  desbloquea();
  sin_pantalla.apretar(ImGuiKey_Enter);
  CHECK_FALSE(desbloquea());
  pantalla.enfocar_contrasena = true;
  desbloquea();
  sin_pantalla.teclear("secreta");
  desbloquea();
  sin_pantalla.apretar(ImGuiKey_Enter);
  CHECK(desbloquea());
  CHECK(std::string(pantalla.contrasena.data()) == "secreta");
}

TEST_CASE("Olvidar la contraseña maestra ofrece importar una copia o crear otra identidad, solo sin conexión") {
  for (const bool conectado : {false, true}) {
    SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
    const Guardados guardados = muchos();
    auto pantalla = pantalla_ajustes();
    pantalla.con_conexion = conectado;
    bool importar = false;
    for (float y = 190; y < 600 && pantalla.dialogo_identidad == ui::DialogoIdentidad::ninguno; y += 12) {
      for (float x = 470; x < 910 && pantalla.dialogo_identidad == ui::DialogoIdentidad::ninguno; x += 20) {
        for (const bool clic : {false, true, false}) {
          const auto pedido = bloqueo(sin_pantalla, pantalla, guardados, ImVec2(x, y), clic);
          importar = importar || pedido.importar_identidad;
          CHECK_FALSE(pedido.desbloquear);
        }
      }
    }
    CHECK(importar == !conectado);
    CHECK((pantalla.dialogo_identidad == ui::DialogoIdentidad::crear) == !conectado);
  }
}

TEST_CASE("En Seguridad, Activar o Cambiar y Quitar abren su diálogo, y el bloqueo automático pide la contraseña maestra") {
  for (const bool con_maestra : {false, true}) {
    SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
    const Guardados guardados = muchos();
    auto pantalla = pantalla_segura(con_maestra);
    std::vector<ui::DialogoIdentidad> abiertos;
    bool cambio = false;
    for (float y = 150; y < 262; y += 12) {
      for (float x = 1000; x < 1330; x += 14) {
        for (const bool clic : {false, true, false}) {
          cambio = sin_pantalla.cuadro(pantalla, guardados, ImVec2(x, y), clic).guardados.has_value() || cambio;
        }
        if (pantalla.dialogo_identidad == ui::DialogoIdentidad::ninguno) {
          continue;
        }
        if (std::find(abiertos.begin(), abiertos.end(), pantalla.dialogo_identidad) == abiertos.end()) {
          abiertos.push_back(pantalla.dialogo_identidad);
        }
        for (int i = 0; i < 4 && pantalla.dialogo_identidad != ui::DialogoIdentidad::ninguno; ++i) {
          sin_pantalla.apretar(ImGuiKey_Escape);
          sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
          sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
        }
        REQUIRE(pantalla.dialogo_identidad == ui::DialogoIdentidad::ninguno);
      }
    }
    std::sort(abiertos.begin(), abiertos.end());
    if (con_maestra) {
      CHECK(abiertos == std::vector<ui::DialogoIdentidad>{ui::DialogoIdentidad::cambiar, ui::DialogoIdentidad::quitar});
    } else {
      CHECK(abiertos == std::vector<ui::DialogoIdentidad>{ui::DialogoIdentidad::activar});
      CHECK_FALSE(cambio);
    }
  }
}

TEST_CASE("Los diálogos de la contraseña maestra se dibujan a cualquier tamaño y piden lo necesario") {
  for (const auto& [escala, tamano] : {std::make_pair(1.0f, ImVec2(1100, 640)), std::make_pair(1.5f, ImVec2(1650, 960))}) {
    SinPantalla sin_pantalla(escala, tamano);
    const Guardados guardados = muchos();
    for (const auto cual : {ui::DialogoIdentidad::activar, ui::DialogoIdentidad::cambiar, ui::DialogoIdentidad::quitar}) {
      for (const bool calculando : {false, true}) {
        auto pantalla = pantalla_segura(cual != ui::DialogoIdentidad::activar);
        pantalla.dialogo_identidad = cual;
        pantalla.abrir_dialogo_identidad = true;
        pantalla.calculando = calculando;
        pantalla.error_identidad = calculando ? "" : "La contraseña no es correcta.";
        for (int i = 0; i < 3; ++i) {
          CHECK_FALSE(sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5)).confirmar_identidad);
        }
        CHECK(pantalla.dialogo_identidad == cual);
        for (int i = 0; i < 2; ++i) {
          sin_pantalla.apretar(ImGuiKey_Escape);
          sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
          sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5));
        }
        CHECK((pantalla.dialogo_identidad == ui::DialogoIdentidad::ninguno) == !calculando);
      }
    }
  }
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  const Guardados guardados = muchos();
  auto pantalla = pantalla_segura(true);
  const auto confirma = [&] {
    bool confirmar = false;
    for (int i = 0; i < 3; ++i) {
      confirmar = sin_pantalla.cuadro(pantalla, guardados, ImVec2(5, 5)).confirmar_identidad || confirmar;
    }
    return confirmar;
  };
  pantalla.dialogo_identidad = ui::DialogoIdentidad::quitar;
  pantalla.abrir_dialogo_identidad = true;
  confirma();
  sin_pantalla.apretar(ImGuiKey_Enter);
  CHECK_FALSE(confirma());
  pantalla.enfocar_contrasena = true;
  confirma();
  sin_pantalla.teclear("la de antes");
  confirma();
  sin_pantalla.apretar(ImGuiKey_Enter);
  CHECK(confirma());
  CHECK(std::string(pantalla.actual.data()) == "la de antes");
}

TEST_CASE("Los avisos de seguridad se ven juntos a cualquier tamaño, esperan a otro diálogo y Entendido los borra") {
  for (const auto& [escala, tamano] : {std::make_pair(1.0f, ImVec2(1100, 640)), std::make_pair(1.0f, ImVec2(1376, 768)),
                                       std::make_pair(1.5f, ImVec2(1650, 960))}) {
    SinPantalla sin_pantalla(escala, tamano);
    auto sala = sala_llena(true, 4, 3, 5);
    auto pantalla = pantalla_servidor();
    pantalla.dialogo = ui::Dialogo::crear_texto;
    pantalla.abrir_dialogo = true;
    for (int i = 0; i < 30; ++i) {
      pantalla.seguridad.push_back("Persona con nombre largo 2 borr\xC3\xB3 el canal \xC2\xABtexto " + std::to_string(i) +
                                   "\xC2\xBB (27/10/2026 21:40).");
    }
    for (int i = 0; i < 3; ++i) {
      sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
    }
    CHECK(ImGui::GetCurrentContext()->OpenPopupStack.Size == 1);
    sin_pantalla.apretar(ImGuiKey_Escape);
    for (int i = 0; i < 3; ++i) {
      sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
    }
    CHECK(pantalla.dialogo == ui::Dialogo::ninguno);
    CHECK(hay_ventana_emergente());
    CHECK(pantalla.seguridad.size() == 30);
    for (float y = 5; y < tamano.y; y += 61) {
      sin_pantalla.cuadro(pantalla, sala, ImVec2(tamano.x / 2, y));
    }
    sin_pantalla.apretar(ImGuiKey_Enter);
    sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
    sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5));
    CHECK(pantalla.seguridad.empty());
    CHECK_FALSE(hay_ventana_emergente());
  }
}

TEST_CASE("El dueño hace admin con clic derecho y confirmación, y saca admin sin ella; los demás no tienen ese menú") {
  using ventrik::protocolo::Rol;
  for (const Rol mio : {Rol::dueno, Rol::admin}) {
    SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
    Presentado presentado{1, mio, "Servidor", {{1, "General"}},
                          {{1, "Ana", 1}, {2, "Beto", 1}, {3, "Carla", 1, 0, Rol::admin}}};
    ventrik::cliente::Sala sala(presentado);
    sala.empezar();
    auto pantalla = pantalla_servidor();
    std::vector<ui::PedidoServidor> pedidos;
    const auto tocar = [&](ImVec2 donde, int boton = 0) {
      sin_pantalla.usar_boton(boton);
      for (const bool clic : {false, true, false}) {
        pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, donde, clic));
      }
      sin_pantalla.usar_boton(0);
    };
    bool confirmo = false;
    for (float y = 100; y < 500; y += 6) {
      tocar(ImVec2(100, y), 1);
      if (hay_ventana_emergente()) {
        tocar(en_menu(0));
      }
      if (pantalla.dialogo == ui::Dialogo::admin) {
        CHECK(pantalla.id_dialogo == 2);
        const ImGuiWindow& dialogo = *ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        const ImVec2 desde = dialogo.Pos;
        const ImVec2 tamano = dialogo.Size;
        for (float x = desde.x + 20; x < desde.x + tamano.x && pantalla.dialogo == ui::Dialogo::admin; x += 8) {
          tocar(ImVec2(x, desde.y + tamano.y - 30));
        }
        confirmo = true;
      }
      for (int i = 0; i < 3 && hay_ventana_emergente(); ++i) {
        sin_pantalla.apretar(ImGuiKey_Escape);
        pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
        pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, ImVec2(5, 5)));
      }
    }
    const auto nombrados = ordenes_de<NombrarAdmin>(pedidos);
    const auto hace = [&](std::uint32_t id, bool admin) {
      return std::any_of(nombrados.begin(), nombrados.end(),
                         [&](const NombrarAdmin& orden) { return orden.usuario == id && orden.admin == admin; });
    };
    CHECK(confirmo == (mio == Rol::dueno));
    CHECK(hace(2, true) == (mio == Rol::dueno));
    CHECK(hace(3, false) == (mio == Rol::dueno));
    CHECK(std::all_of(nombrados.begin(), nombrados.end(), [](const NombrarAdmin& orden) {
      return (orden.usuario == 2 && orden.admin) || (orden.usuario == 3 && !orden.admin);
    }));
  }
}

TEST_CASE("El ▾ del servidor del dueño libera un nombre: lo pide y lo manda, y sin un nombre válido no") {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(true, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  std::vector<ui::PedidoServidor> pedidos;
  const auto cuadro = [&](ImVec2 donde, bool clic = false) {
    pedidos.push_back(sin_pantalla.cuadro(pantalla, sala, donde, clic));
  };
  const auto tocar = [&](ImVec2 donde) {
    cuadro(donde);
    cuadro(donde, true);
    cuadro(donde);
  };
  cuadro(ImVec2(5, 5));
  tocar(ImVec2(150, 20));
  tocar(ImVec2(100, 221));
  REQUIRE(pantalla.dialogo == ui::Dialogo::liberar);
  cuadro(ImVec2(5, 5));
  cuadro(ImVec2(5, 5));
  sin_pantalla.apretar(ImGuiKey_Enter);
  cuadro(ImVec2(5, 5));
  cuadro(ImVec2(5, 5));
  CHECK(pantalla.dialogo == ui::Dialogo::liberar);
  CHECK_FALSE(pantalla.error_dialogo.empty());
  cuadro(ImVec2(5, 5));
  sin_pantalla.teclear("Tano");
  cuadro(ImVec2(5, 5));
  sin_pantalla.apretar(ImGuiKey_Enter);
  cuadro(ImVec2(5, 5));
  cuadro(ImVec2(5, 5));
  CHECK(pantalla.dialogo == ui::Dialogo::ninguno);
  const auto liberados = ordenes_de<LiberarNombre>(pedidos);
  REQUIRE(liberados.size() == 1);
  CHECK(liberados[0].nombre == "Tano");

  auto invitado = sala_llena(false, 4, 3, 5);
  auto otra = pantalla_servidor();
  for (const ImVec2 donde : {ImVec2(150, 20), ImVec2(100, 221)}) {
    sin_pantalla.cuadro(otra, invitado, donde);
    sin_pantalla.cuadro(otra, invitado, donde, true);
    sin_pantalla.cuadro(otra, invitado, donde);
  }
  CHECK(otra.dialogo == ui::Dialogo::ninguno);
}

TEST_CASE("banco de los avisos de seguridad" * doctest::skip()) {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  auto sala = sala_llena(true, 4, 3, 5);
  auto pantalla = pantalla_servidor();
  for (int i = 0; i < 5; ++i) {
    pantalla.seguridad.push_back("Persona con nombre largo 2 borr\xC3\xB3 el canal \xC2\xABtexto\xC2\xBB (27/10/2026 21:40).");
  }
  for (int i = 0; i < 30; ++i) {
    sin_pantalla.cuadro(pantalla, sala, ImVec2(600, 400));
  }
  const auto desde = std::chrono::steady_clock::now();
  for (int i = 0; i < 1000; ++i) {
    sin_pantalla.cuadro(pantalla, sala, ImVec2(600, 400));
  }
  MESSAGE("avisos de seguridad: "
          << std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - desde).count() / 1000
          << " us por cuadro, " << ImGui::GetDrawData()->TotalVtxCount << " vertices");
}

TEST_CASE("banco de la contraseña maestra" * doctest::skip()) {
  SinPantalla sin_pantalla(1.0f, ImVec2(1376, 768));
  const Guardados guardados = muchos();
  auto pantalla = pantalla_ajustes();
  for (int i = 0; i < 30; ++i) {
    bloqueo(sin_pantalla, pantalla, guardados, ImVec2(600, 400));
  }
  const auto desde = std::chrono::steady_clock::now();
  for (int i = 0; i < 1000; ++i) {
    bloqueo(sin_pantalla, pantalla, guardados, ImVec2(600, 400));
  }
  MESSAGE("contraseña maestra: "
          << std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - desde).count() / 1000
          << " us por cuadro, " << ImGui::GetDrawData()->TotalVtxCount << " vertices");
}
