#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_impl_null.h>
#include <imgui_internal.h>

#include "client/sala.h"
#include "client/ui/aviso_ventana.h"
#include "client/ui/dibujante.h"
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
  presentado.dueno = dueno;
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
