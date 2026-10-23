#include "client/ui/piezas.h"

#include <cfloat>
#include <initializer_list>
#include <utility>

#include <windows.h>

#include <imgui_internal.h>

#include "common/utf.h"

namespace ventrik::ui {
namespace {
ImFont* fuente(Letra letra) { return ImGui::GetIO().Fonts->Fonts[static_cast<int>(letra)]; }

float tamano_real(float tamano) { return px(tamano * medida::em); }

ImDrawList& lienzo() { return *ImGui::GetWindowDrawList(); }

}

bool preparar_interfaz(float escala) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.ConfigInputTextCursorBlink = false;
  io.ConfigErrorRecoveryEnableTooltip = false;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  for (const wchar_t* nombre : {L"BARLOW_NORMAL", L"BARLOW_FUERTE", L"BARLOW_NEGRITA", L"BARLOW_TITULO"}) {
    const HRSRC recurso = FindResourceW(nullptr, nombre, RT_RCDATA);
    const HGLOBAL cargado = recurso != nullptr ? LoadResource(nullptr, recurso) : nullptr;
    void* datos = cargado != nullptr ? LockResource(cargado) : nullptr;
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    if (datos == nullptr ||
        io.Fonts->AddFontFromMemoryTTF(datos, static_cast<int>(SizeofResource(nullptr, recurso)),
                                       medida::letra * medida::em, &config) == nullptr) {
      return false;
    }
  }
  ImGuiStyle& estilo = ImGui::GetStyle();
  estilo.FontSizeBase = medida::letra * medida::em;
  estilo.FontScaleDpi = escala;
  estilo.WindowPadding = ImVec2(10, 8);
  estilo.WindowRounding = 0;
  estilo.WindowBorderSize = 0;
  estilo.ChildBorderSize = 0;
  estilo.PopupRounding = 2;
  estilo.FrameRounding = 2;
  estilo.ItemSpacing = ImVec2(8, 6);
  estilo.ScrollbarSize = 10;
  estilo.ScrollbarRounding = 2;
  estilo.DisabledAlpha = 0.45f;
  for (const auto& [cual, valor] : std::initializer_list<std::pair<ImGuiCol, ImU32>>{
           {ImGuiCol_Text, color::texto},
           {ImGuiCol_TextDisabled, color::apagado},
           {ImGuiCol_WindowBg, color::fondo},
           {ImGuiCol_ChildBg, 0},
           {ImGuiCol_PopupBg, color::panel},
           {ImGuiCol_Border, color::linea2},
           {ImGuiCol_FrameBg, 0},
           {ImGuiCol_FrameBgHovered, 0},
           {ImGuiCol_FrameBgActive, 0},
           {ImGuiCol_ScrollbarBg, 0},
           {ImGuiCol_ScrollbarGrab, color::linea},
           {ImGuiCol_ScrollbarGrabHovered, color::linea2},
           {ImGuiCol_ScrollbarGrabActive, color::texto2},
           {ImGuiCol_Header, color::elevado},
           {ImGuiCol_HeaderHovered, color::elevado},
           {ImGuiCol_HeaderActive, color::linea2},
           {ImGuiCol_NavCursor, color::celeste},
           {ImGuiCol_TextSelectedBg, rgb(0x5CC2D6, 90)},
           {ImGuiCol_InputTextCursor, color::texto}}) {
    estilo.Colors[cual] = ImGui::ColorConvertU32ToFloat4(valor);
  }
  estilo.ScaleAllSizes(escala);
  return true;
}

void pantalla_completa(const std::function<void()>& dibujar) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("##ventrik", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar();
  dibujar();
  ImGui::End();
}

float px(float medida) { return medida * ImGui::GetStyle().FontScaleDpi; }

ImU32 tono(ImU32 color) { return ImGui::GetColorU32(color); }

ConLetra::ConLetra(Letra letra, float tamano) {
  ImGui::PushFont(fuente(letra), tamano * medida::em);
}

ConLetra::~ConLetra() { ImGui::PopFont(); }

std::array<ImVec2, 8> octogono(ImVec2 desde, ImVec2 hasta, float corte) {
  return {ImVec2(desde.x + corte, desde.y), ImVec2(hasta.x - corte, desde.y),
          ImVec2(hasta.x, desde.y + corte), ImVec2(hasta.x, hasta.y - corte),
          ImVec2(hasta.x - corte, hasta.y), ImVec2(desde.x + corte, hasta.y),
          ImVec2(desde.x, hasta.y - corte), ImVec2(desde.x, desde.y + corte)};
}

std::array<ImVec2, 6> cortes_opuestos(ImVec2 desde, ImVec2 hasta, float corte, bool arriba_izquierda) {
  if (arriba_izquierda) {
    return {ImVec2(desde.x + corte, desde.y), ImVec2(hasta.x, desde.y), ImVec2(hasta.x, hasta.y - corte),
            ImVec2(hasta.x - corte, hasta.y), ImVec2(desde.x, hasta.y), ImVec2(desde.x, desde.y + corte)};
  }
  return {desde, ImVec2(hasta.x - corte, desde.y), ImVec2(hasta.x, desde.y + corte),
          hasta, ImVec2(desde.x + corte, hasta.y), ImVec2(desde.x, hasta.y - corte)};
}

std::array<ImVec2, 5> corte_abajo_derecha(ImVec2 desde, ImVec2 hasta, float corte) {
  return {desde, ImVec2(hasta.x, desde.y), ImVec2(hasta.x, hasta.y - corte),
          ImVec2(hasta.x - corte, hasta.y), ImVec2(desde.x, hasta.y)};
}

std::string iniciales(std::string_view nombre) {
  const std::wstring palabras = a_utf16(nombre);
  std::wstring letras;
  const auto agregar = [&](std::size_t i) {
    letras += palabras[i];
    if (IS_HIGH_SURROGATE(palabras[i]) && i + 1 < palabras.size()) {
      letras += palabras[i + 1];
    }
  };
  if (palabras.empty()) {
    return {};
  }
  agregar(0);
  const std::size_t espacio = palabras.find(L' ');
  const std::size_t segunda = espacio != std::wstring::npos ? espacio + 1 : letras.size();
  if (segunda < palabras.size()) {
    agregar(segunda);
  }
  CharUpperBuffW(letras.data(), static_cast<DWORD>(letras.size()));
  return a_utf8(letras);
}

ImVec2 medir(std::string_view texto, Letra letra, float tamano) {
  return fuente(letra)->CalcTextSizeA(tamano_real(tamano), FLT_MAX, 0.0f, texto.data(),
                                      texto.data() + texto.size());
}

void escribir(ImVec2 donde, std::string_view texto, ImU32 color, Letra letra, float tamano) {
  lienzo().AddText(fuente(letra), tamano_real(tamano), donde, tono(color), texto.data(),
                   texto.data() + texto.size());
}

void escribir_cortado(ImVec2 donde, float ancho, std::string_view texto, ImU32 color, Letra letra,
                      float tamano) {
  const ConLetra con_letra(letra, tamano);
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  const ImVec2 hasta(donde.x + ancho, donde.y + ImGui::GetFontSize());
  ImGui::RenderTextEllipsis(&lienzo(), donde, hasta, hasta.x, texto.data(), texto.data() + texto.size(), nullptr);
  ImGui::PopStyleColor();
}

void escribir_parrafo(std::string_view texto, ImU32 color, float ancho, float tamano) {
  const ConLetra letra(Letra::normal, tamano);
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ancho);
  ImGui::TextUnformatted(texto.data(), texto.data() + texto.size());
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
}

void cartel(std::string_view texto) {
  ImGui::SetItemTooltip("%.*s", static_cast<int>(texto.size()), texto.data());
}

void escuadras(ImDrawList& dibujo, ImVec2 desde, ImVec2 hasta, float largo, float grosor, ImU32 color) {
  for (int esquina = 0; esquina < 4; ++esquina) {
    const float x = (esquina & 1) != 0 ? hasta.x - largo : desde.x;
    const float y = (esquina & 2) != 0 ? hasta.y - grosor : desde.y;
    dibujo.AddRectFilled(ImVec2(x, y), ImVec2(x + largo, y + grosor), color);
    const float x_vertical = (esquina & 1) != 0 ? hasta.x - grosor : desde.x;
    const float y_vertical = (esquina & 2) != 0 ? hasta.y - largo : desde.y;
    dibujo.AddRectFilled(ImVec2(x_vertical, y_vertical),
                         ImVec2(x_vertical + grosor, y_vertical + largo), color);
  }
}

void triangulos(ImDrawList& dibujo, ImVec2 desde, ImVec2 hasta) {
  const float lado = px(medida::triangulo);
  const ImU32 color = tono(color::triangulo);
  dibujo.AddTriangleFilled(desde, ImVec2(desde.x + lado, desde.y), ImVec2(desde.x, desde.y + lado), color);
  dibujo.AddTriangleFilled(hasta, ImVec2(hasta.x - lado, hasta.y), ImVec2(hasta.x, hasta.y - lado), color);
}

void panel(ImVec2 desde, ImVec2 hasta) {
  ImDrawList& dibujo = lienzo();
  dibujo.AddRectFilled(desde, hasta, tono(color::panel));
  dibujo.AddRect(desde, hasta, tono(color::linea));
  triangulos(dibujo, ImVec2(desde.x + 1, desde.y + 1), ImVec2(hasta.x - 1, hasta.y - 1));
}

void avatar(ImVec2 desde, float lado, std::string_view letras, ImU32 fondo, ImU32 color_letra,
            float tamano_letra) {
  const auto forma = corte_abajo_derecha(desde, ImVec2(desde.x + lado, desde.y + lado), lado * 7 / 26);
  lienzo().AddConvexPolyFilled(forma.data(), static_cast<int>(forma.size()), tono(fondo));
  const ImVec2 tamano = medir(letras, Letra::fuerte, tamano_letra);
  escribir(ImVec2(desde.x + (lado - tamano.x) / 2, desde.y + (lado - tamano.y) / 2), letras, color_letra,
           Letra::fuerte, tamano_letra);
}

void emblema(ImVec2 desde, float lado, ImTextureID logo) {
  ImDrawList& dibujo = lienzo();
  const ImVec2 hasta(desde.x + lado, desde.y + lado);
  dibujo.AddRectFilled(desde, hasta, tono(color::emblema), px(2));
  dibujo.AddRect(desde, hasta, tono(color::linea), px(2));
  if (logo != ImTextureID{}) {
    dibujo.AddImage(ImTextureRef(logo), ImVec2(desde.x + px(3), desde.y + px(3)),
                    ImVec2(hasta.x - px(3), hasta.y - px(3)));
  }
}

float ancho_boton(std::string_view texto, Estilo estilo, Icono icono) {
  const float margen = estilo == Estilo::biselado      ? medida::margen_biselado
                       : estilo == Estilo::principal   ? medida::margen_principal
                       : estilo == Estilo::contorneado ? medida::margen_contorneado
                                                       : medida::margen_esquineros;
  const float con_icono = icono != Icono::ninguno ? medida::icono + medida::separacion_icono : 0;
  return medir(texto, estilo == Estilo::principal ? Letra::fuerte : Letra::normal, medida::letra).x +
         px(2 * margen + con_icono);
}

bool boton(const char* id, std::string_view texto, Estilo estilo, ImVec2 tamano, Icono icono) {
  if (tamano.x <= 0) {
    tamano.x = ancho_boton(texto, estilo, icono);
  }
  const ImVec2 desde = ImGui::GetCursorScreenPos();
  const ImVec2 hasta(desde.x + tamano.x, desde.y + tamano.y);
  const bool apretado = ImGui::InvisibleButton(id, tamano, ImGuiButtonFlags_EnableNav);
  const bool encima = ImGui::IsItemHovered();
  ImDrawList& dibujo = lienzo();
  ImU32 color_texto = color::texto;
  switch (estilo) {
    case Estilo::biselado: {
      const auto borde = octogono(desde, hasta, px(medida::bisel));
      dibujo.AddConvexPolyFilled(borde.data(), static_cast<int>(borde.size()), tono(color::borde_oscuro));
      const ImVec2 adentro(desde.x + 1, desde.y + 1);
      const ImVec2 afuera(hasta.x - 1, hasta.y - 1);
      const auto relleno = octogono(adentro, afuera, px(medida::bisel) - 1);
      dibujo.AddConvexPolyFilled(relleno.data(), static_cast<int>(relleno.size()),
                                 tono(encima ? color::bisel_abajo_encima : color::bisel_abajo));
      dibujo.PushClipRect(adentro, ImVec2(afuera.x, (adentro.y + afuera.y) / 2), true);
      dibujo.AddConvexPolyFilled(relleno.data(), static_cast<int>(relleno.size()),
                                 tono(encima ? color::bisel_arriba_encima : color::bisel_arriba));
      dibujo.PopClipRect();
      dibujo.AddLine(ImVec2(adentro.x + px(medida::bisel), adentro.y + 0.5f),
                     ImVec2(afuera.x - px(medida::bisel), adentro.y + 0.5f), tono(color::brillo));
      break;
    }
    case Estilo::esquineros:
      dibujo.AddRectFilled(desde, hasta, tono(color::fondo_esquineros));
      escuadras(dibujo, desde, hasta, px(medida::escuadra), px(medida::grosor_escuadra),
                tono(encima ? color::texto : color::escuadra));
      break;
    case Estilo::contorneado:
      dibujo.AddRectFilled(desde, hasta, tono(color::pozo), px(2));
      dibujo.AddRect(desde, hasta, tono(encima ? color::texto2 : color::borde_contorneado), px(2));
      break;
    case Estilo::principal: {
      const auto forma = cortes_opuestos(desde, hasta, px(medida::corte_principal), true);
      dibujo.AddConvexPolyFilled(forma.data(), static_cast<int>(forma.size()),
                                 tono(encima ? color::texto : color::claro));
      color_texto = color::tinta;
      break;
    }
  }
  const Letra letra = estilo == Estilo::principal ? Letra::fuerte : Letra::normal;
  const ImVec2 medida_texto = medir(texto, letra, medida::letra);
  const float con_icono =
      icono != Icono::ninguno ? px(medida::icono + (texto.empty() ? 0 : medida::separacion_icono)) : 0;
  const float x = desde.x + (tamano.x - medida_texto.x - con_icono) / 2;
  if (icono != Icono::ninguno) {
    dibujar_icono(dibujo, icono, ImVec2(x + px(medida::icono) / 2, desde.y + tamano.y / 2),
                  px(medida::icono), tono(color_texto));
  }
  escribir(ImVec2(x + con_icono, desde.y + (tamano.y - medida_texto.y) / 2), texto, color_texto, letra);
  return apretado;
}

bool campo(const char* id, std::string_view pista, Campo& texto, ImVec2 tamano,
           Icono icono, ImGuiInputTextFlags opciones, ImU32 relleno) {
  const ImVec2 desde = ImGui::GetCursorScreenPos();
  const ImVec2 hasta(desde.x + tamano.x, desde.y + tamano.y);
  ImDrawList& dibujo = lienzo();
  dibujo.AddRectFilled(desde, hasta, tono(relleno), px(2));
  dibujo.AddRect(desde, hasta,
                 tono(ImGui::IsMouseHoveringRect(desde, hasta) ? color::texto2 : color::linea2), px(2));
  const float izquierda = px(icono != Icono::ninguno ? 37 : medida::margen_campo);
  if (icono != Icono::ninguno) {
    dibujar_icono(dibujo, icono, ImVec2(desde.x + px(19), (desde.y + hasta.y) / 2), px(medida::icono),
                  tono(color::apagado));
  }
  const ConLetra letra(Letra::normal, medida::letra);
  ImGui::SetCursorScreenPos(ImVec2(desde.x + izquierda, (desde.y + hasta.y - ImGui::GetFontSize()) / 2));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
  ImGui::SetNextItemWidth(tamano.x - izquierda - px(medida::margen_campo));
  const bool listo = ImGui::InputTextWithHint(id, std::string(pista).c_str(), texto.data(), texto.size(), opciones);
  ImGui::PopStyleVar();
  return listo;
}

float ancho_selector_modo(std::string_view rendimiento, std::string_view visual) {
  return medir(rendimiento, Letra::fuerte, medida::letra).x + medir(visual, Letra::normal, medida::letra).x +
         px(62);
}

void selector_modo(std::string_view rendimiento, std::string_view visual, std::string_view cartel_visual) {
  ImDrawList& dibujo = lienzo();
  const ImVec2 desde = ImGui::GetCursorScreenPos();
  const float primera = medir(rendimiento, Letra::fuerte, medida::letra).x + px(24);
  const float segunda = medir(visual, Letra::normal, medida::letra).x + px(24);
  const ImVec2 hasta(desde.x + ancho_selector_modo(rendimiento, visual), desde.y + px(44));
  const float ancho = hasta.x - desde.x;
  const ImU32 naranja = tono(color::naranja);
  escuadras(dibujo, desde, hasta, px(12), px(2), naranja);
  const auto tramo = [&](float y, float largo, float centro) {
    dibujo.AddRectFilled(ImVec2(desde.x + ancho * (centro - largo / 2), y),
                         ImVec2(desde.x + ancho * (centro + largo / 2), y + px(2)), naranja);
  };
  tramo(desde.y, 0.34f, 0.58f);
  tramo(hasta.y - px(2), 0.30f, 0.72f);
  const ImVec2 pozo_desde(desde.x + px(3), desde.y + px(3));
  const ImVec2 pozo_hasta(hasta.x - px(3), hasta.y - px(3));
  dibujo.AddRectFilled(pozo_desde, pozo_hasta, tono(color::pozo));
  dibujo.AddRect(pozo_desde, pozo_hasta, tono(color::borde_modo));
  const ImVec2 elegida(pozo_desde.x + px(4), pozo_desde.y + px(4));
  const float alto = pozo_hasta.y - px(4) - elegida.y;
  dibujo.AddRectFilled(elegida, ImVec2(elegida.x + primera, elegida.y + alto), tono(color::claro));
  const ImVec2 primera_texto = medir(rendimiento, Letra::fuerte, medida::letra);
  escribir(ImVec2(elegida.x + px(12), elegida.y + (alto - primera_texto.y) / 2), rendimiento, color::tinta,
           Letra::fuerte);
  ImGui::SetCursorScreenPos(ImVec2(elegida.x + primera, elegida.y));
  ImGui::BeginDisabled();
  ImGui::InvisibleButton("##modo_visual", ImVec2(segunda, alto));
  escribir(ImVec2(elegida.x + primera + px(12), elegida.y + (alto - primera_texto.y) / 2), visual,
           color::texto2);
  ImGui::EndDisabled();
  cartel(cartel_visual);
}

}
