#include "client/ui/piezas.h"

#include <algorithm>
#include <cfloat>
#include <initializer_list>
#include <utility>

#include <windows.h>

#include <imgui_internal.h>

#include "common/texto.h"
#include "common/utf.h"

namespace ventrik::ui {
namespace {
ImFont* fuente(Letra letra) { return ImGui::GetIO().Fonts->Fonts[static_cast<int>(letra)]; }

float tamano_real(float tamano) { return px(tamano * medida::em); }

ImDrawList& lienzo() { return *ImGui::GetWindowDrawList(); }

const char* principio(std::string_view texto) { return texto.empty() ? "" : texto.data(); }

const char* final_de(std::string_view texto) { return principio(texto) + texto.size(); }

}

bool preparar_interfaz(float escala, ImTextureFormat letras) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->TexDesiredFormat = letras;
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.ConfigInputTextCursorBlink = false;
  io.ConfigScrollbarScrollByPage = false;
  ImGui::GetCurrentContext()->ConfigNavWindowingKeyNext = 0;
  ImGui::GetCurrentContext()->ConfigNavWindowingKeyPrev = 0;
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
           {ImGuiCol_InputTextCursor, color::texto},
           {ImGuiCol_ModalWindowDimBg, rgb(0x000000, 100)}}) {
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
  ImGui::GetCurrentContext()->DimBgRatio = 1.0f;
  dibujar();
  ImGui::End();
}

bool por_aparecer() {
  const ImVector<ImGuiWindow*>& ventanas = ImGui::GetCurrentContext()->Windows;
  return std::any_of(ventanas.begin(), ventanas.end(), [](const ImGuiWindow* ventana) {
    return ventana->Active && ventana->Hidden &&
           (ventana->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) != 0;
  });
}

bool casilla(std::string_view etiqueta, ImVec2 donde, bool valor) {
  const float lado = px(18);
  ImGui::SetCursorScreenPos(donde);
  ImGui::PushID(etiqueta.data(), etiqueta.data() + etiqueta.size());
  const bool tocada =
      ImGui::InvisibleButton("##casilla", ImVec2(lado + px(8) + medir(etiqueta, Letra::normal, medida::letra).x, lado));
  ImGui::PopID();
  const bool marcada = valor != tocada;
  ImDrawList& dibujo = *ImGui::GetWindowDrawList();
  const ImVec2 hasta(donde.x + lado, donde.y + lado);
  dibujo.AddRectFilled(donde, hasta, tono(marcada ? color::claro : color::pozo), px(2));
  dibujo.AddRect(donde, hasta, tono(marcada ? color::claro : color::linea2), px(2));
  if (marcada) {
    ImGui::RenderCheckMark(&dibujo, ImVec2(donde.x + px(4), donde.y + px(4)), tono(color::tinta), lado - px(8));
  }
  escribir_en(donde.x + lado + px(8), donde.y + lado / 2, etiqueta, color::texto);
  return tocada;
}

std::optional<float> deslizador(const char* id, ImVec2 desde, float ancho, float valor) {
  ImDrawList& dibujo = *ImGui::GetWindowDrawList();
  ImGui::SetCursorScreenPos(ImVec2(desde.x, desde.y - px(8)));
  std::optional<float> nuevo;
  ImGui::InvisibleButton(id, ImVec2(ancho, px(16)));
  if (ImGui::IsItemActive()) {
    nuevo = std::clamp((ImGui::GetIO().MousePos.x - desde.x) / ancho, 0.0f, 1.0f);
  }
  const float corte = desde.x + ancho * nuevo.value_or(valor);
  dibujo.AddRectFilled(ImVec2(desde.x, desde.y - px(3)), ImVec2(corte, desde.y + px(3)), tono(color::claro), px(3));
  dibujo.AddRectFilled(ImVec2(corte, desde.y - px(3)), ImVec2(desde.x + ancho, desde.y + px(3)), tono(color::linea),
                       px(3));
  dibujo.AddCircleFilled(ImVec2(corte, desde.y), px(7), tono(color::claro));
  return nuevo;
}

std::optional<int> desplegable(const char* id, ImVec2 donde, float ancho, const std::vector<std::string>& opciones,
                               int elegida, std::string_view vista) {
  ImGui::SetCursorScreenPos(donde);
  ImGui::SetNextItemWidth(ancho);
  const bool valida = elegida >= 0 && static_cast<std::size_t>(elegida) < opciones.size();
  const std::string mostrar(vista.empty() && valida ? std::string_view(opciones[static_cast<std::size_t>(elegida)])
                                                    : vista);
  for (const auto& [cual, valor] : {std::make_pair(ImGuiCol_FrameBg, color::pozo),
                                    std::make_pair(ImGuiCol_FrameBgHovered, color::pozo),
                                    std::make_pair(ImGuiCol_Border, color::linea2), std::make_pair(ImGuiCol_Button, 0u),
                                    std::make_pair(ImGuiCol_ButtonHovered, 0u)}) {
    ImGui::PushStyleColor(cual, valor);
  }
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, px(1));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(10), px(6)));
  const bool abierto = ImGui::BeginCombo(id, mostrar.c_str());
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(5);
  std::optional<int> nueva;
  if (abierto) {
    for (std::size_t i = 0; i < opciones.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      if (ImGui::Selectable(opciones[i].c_str(), static_cast<int>(i) == elegida)) {
        nueva = static_cast<int>(i);
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  return nueva;
}

void cerrar_ventana_emergente() {
  ImGui::CloseCurrentPopup();
  ImGui::GetCurrentWindow()->Hidden = true;
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
  return fuente(letra)->CalcTextSizeA(tamano_real(tamano), FLT_MAX, 0.0f, principio(texto),
                                      final_de(texto));
}

void escribir(ImVec2 donde, std::string_view texto, ImU32 color, Letra letra, float tamano) {
  lienzo().AddText(fuente(letra), tamano_real(tamano), donde, tono(color), principio(texto),
                   final_de(texto));
}

void escribir_cortado(ImVec2 donde, float ancho, std::string_view texto, ImU32 color, Letra letra,
                      float tamano) {
  const ConLetra con_letra(letra, tamano);
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  const ImVec2 hasta(donde.x + ancho, donde.y + ImGui::GetFontSize());
  ImGui::RenderTextEllipsis(&lienzo(), donde, hasta, hasta.x, principio(texto), final_de(texto), nullptr);
  ImGui::PopStyleColor();
}

float escribir_en(float x, float medio, std::string_view texto, ImU32 color, Letra letra, float tamano, float ancho) {
  const ImVec2 tamano_texto = medir(texto, letra, tamano);
  const ImVec2 donde(x, medio - tamano_texto.y / 2);
  if (ancho > 0) {
    escribir_cortado(donde, ancho, texto, color, letra, tamano);
    return x + std::min(ancho, tamano_texto.x);
  }
  escribir(donde, texto, color, letra, tamano);
  return x + tamano_texto.x;
}

float escribir_espaciado(ImVec2 donde, std::string_view texto, ImU32 color, Letra letra, float tamano,
                         float espacio) {
  const char* resto = principio(texto);
  const char* fin = final_de(texto);
  while (resto < fin) {
    unsigned int letra_suelta = 0;
    const int largo = ImTextCharFromUtf8(&letra_suelta, resto, fin);
    const std::string_view una(resto, static_cast<std::size_t>(std::max(largo, 1)));
    escribir(donde, una, color, letra, tamano);
    donde.x += medir(una, letra, tamano).x + espacio;
    resto += una.size();
  }
  return donde.x;
}

float alto_envuelto(std::string_view texto, float ancho, float tamano) {
  return fuente(Letra::normal)
      ->CalcTextSizeA(tamano_real(tamano), FLT_MAX, ancho, principio(texto), final_de(texto))
      .y;
}

void escribir_envuelto(ImVec2 donde, std::string_view texto, ImU32 color, float ancho, float tamano) {
  lienzo().AddText(fuente(Letra::normal), tamano_real(tamano), donde, tono(color), principio(texto),
                   final_de(texto), ancho);
}

void escribir_parrafo(std::string_view texto, ImU32 color, float ancho, float tamano) {
  const ConLetra letra(Letra::normal, tamano);
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ancho);
  ImGui::TextUnformatted(principio(texto), final_de(texto));
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
}

void cartel(std::string_view texto) {
  ImGui::SetItemTooltip("%.*s", static_cast<int>(texto.size()), principio(texto));
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
            float tamano_letra, ImU32 aro) {
  const auto forma = corte_abajo_derecha(desde, ImVec2(desde.x + lado, desde.y + lado), lado * 7 / 26);
  lienzo().AddConvexPolyFilled(forma.data(), static_cast<int>(forma.size()), tono(fondo));
  if (aro != 0) {
    const auto borde = corte_abajo_derecha(ImVec2(desde.x + px(1), desde.y + px(1)),
                                           ImVec2(desde.x + lado - px(1), desde.y + lado - px(1)),
                                           lado * 7 / 26 - px(1));
    lienzo().AddPolyline(borde.data(), static_cast<int>(borde.size()), tono(aro), ImDrawFlags_Closed, px(2));
  }
  const ImVec2 tamano = medir(letras, Letra::fuerte, tamano_letra);
  escribir(ImVec2(desde.x + (lado - tamano.x) / 2, desde.y + (lado - tamano.y) / 2), letras, color_letra,
           Letra::fuerte, tamano_letra);
}

ImU32 color_de_avatar(std::string_view nombre) {
  std::uint32_t suma = 2166136261u;
  for (const wchar_t letra : nombre_para_comparar(nombre)) {
    suma = (suma ^ static_cast<std::uint32_t>(letra)) * 16777619u;
  }
  return color::avatares[suma % color::avatares.size()];
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

bool campo(const char* id, std::string_view pista, gsl::span<char> texto, ImVec2 tamano,
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
  const bool listo = ImGui::InputTextWithHint(id, std::string(pista).c_str(), texto.data(),
                                               static_cast<std::size_t>(texto.size()), opciones);
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

int selector_opciones(const char* id, std::initializer_list<std::string_view> opciones, int elegida,
                      ImVec2 tamano, int apagada, std::string_view cartel_apagada) {
  ImDrawList& dibujo = lienzo();
  const ImVec2 desde = ImGui::GetCursorScreenPos();
  dibujo.AddRectFilled(desde, ImVec2(desde.x + tamano.x, desde.y + tamano.y), tono(color::pozo));
  dibujo.AddRect(desde, ImVec2(desde.x + tamano.x, desde.y + tamano.y), tono(color::linea));
  const float margen = px(4);
  const float ancho = (tamano.x - 2 * margen) / static_cast<float>(opciones.size());
  const float alto = tamano.y - 2 * margen;
  int tocada = -1;
  int cual = 0;
  ImGui::PushID(id);
  for (const std::string_view opcion : opciones) {
    const ImVec2 lugar(desde.x + margen + ancho * static_cast<float>(cual), desde.y + margen);
    ImGui::SetCursorScreenPos(lugar);
    ImGui::BeginDisabled(cual == apagada);
    ImGui::PushID(cual);
    if (ImGui::InvisibleButton("##opcion", ImVec2(ancho, alto), ImGuiButtonFlags_EnableNav)) {
      tocada = cual;
    }
    ImGui::PopID();
    const bool es_elegida = cual == elegida;
    if (es_elegida) {
      dibujo.AddRectFilled(lugar, ImVec2(lugar.x + ancho, lugar.y + alto), tono(color::claro));
    }
    const Letra letra = es_elegida ? Letra::fuerte : Letra::normal;
    const ImVec2 medida_texto = medir(opcion, letra, medida::letra);
    escribir(ImVec2(lugar.x + (ancho - medida_texto.x) / 2, lugar.y + (alto - medida_texto.y) / 2), opcion,
             es_elegida ? color::tinta : color::texto, letra);
    ImGui::EndDisabled();
    if (cual == apagada) {
      cartel(cartel_apagada);
    }
    ++cual;
  }
  ImGui::PopID();
  ImGui::SetCursorScreenPos(ImVec2(desde.x, desde.y + tamano.y));
  return tocada;
}

float ancho_tecla(std::string_view nombre) {
  return medir(nombre, Letra::negrita, medida::letra_chica).x + px(2 * medida::margen_tecla);
}

void tecla(ImVec2 desde, std::string_view nombre, ImU32 relleno) {
  const ImVec2 hasta(desde.x + ancho_tecla(nombre), desde.y + px(medida::alto_tecla));
  ImDrawList& dibujo = lienzo();
  dibujo.AddRectFilled(desde, hasta, tono(relleno), px(2));
  dibujo.AddRect(desde, hasta, tono(color::linea2), px(2));
  const ImVec2 tamano = medir(nombre, Letra::negrita, medida::letra_chica);
  escribir(ImVec2(desde.x + px(medida::margen_tecla), (desde.y + hasta.y - tamano.y) / 2), nombre, color::texto,
           Letra::negrita, medida::letra_chica);
}

float ancho_transmitiendo(std::string_view texto) {
  return medir(texto, Letra::negrita, medida::letra_transmitiendo).x + px(46);
}

void etiqueta_transmitiendo(ImVec2 desde, float ancho, std::string_view texto, bool transmitiendo) {
  const ImVec2 hasta(desde.x + ancho, desde.y + px(medida::alto_transmitiendo));
  const auto forma = cortes_opuestos(desde, hasta, px(medida::corte_transmitiendo), false);
  ImDrawList& dibujo = lienzo();
  dibujo.AddConvexPolyFilled(forma.data(), static_cast<int>(forma.size()),
                             tono(transmitiendo ? color::naranja : color::elevado));
  const ImU32 tinta = transmitiendo ? color::tinta_naranja : color::apagado;
  const float lado = px(9);
  const float medio = (desde.y + hasta.y) / 2;
  dibujo.AddRectFilled(ImVec2(desde.x + px(15), medio - lado / 2), ImVec2(desde.x + px(15) + lado, medio + lado / 2),
                       tono(tinta));
  const ImVec2 tamano = medir(texto, Letra::negrita, medida::letra_transmitiendo);
  escribir(ImVec2(desde.x + px(32), medio - tamano.y / 2), texto, tinta, Letra::negrita,
           medida::letra_transmitiendo);
}

void aviso_silencio(ImVec2 desde, float ancho, std::string_view texto, Icono icono) {
  const ImVec2 hasta(desde.x + ancho, desde.y + px(medida::alto_transmitiendo));
  const auto forma = cortes_opuestos(desde, hasta, px(medida::corte_transmitiendo), false);
  ImDrawList& dibujo = lienzo();
  dibujo.AddConvexPolyFilled(forma.data(), static_cast<int>(forma.size()), tono(color::fondo_rojo));
  dibujo.AddPolyline(forma.data(), static_cast<int>(forma.size()), tono(color::borde_rojo), ImDrawFlags_Closed, 1.0f);
  const float medio = (desde.y + hasta.y) / 2;
  dibujar_icono_tachado(dibujo, icono, ImVec2(desde.x + px(19), medio), px(13), tono(color::rojo_claro));
  const ImVec2 tamano = medir(texto, Letra::negrita, medida::letra_transmitiendo);
  escribir(ImVec2(desde.x + px(32), medio - tamano.y / 2), texto, color::rojo_claro, Letra::negrita,
           medida::letra_transmitiendo);
}

float contador_nuevos(ImVec2 derecha, std::uint32_t cantidad) {
  const std::string numero = std::to_string(cantidad);
  const ImVec2 tamano = medir(numero, Letra::negrita, medida::letra_chica);
  const float ancho = std::max(px(medida::contador), tamano.x + px(8));
  const ImVec2 desde(derecha.x - ancho, derecha.y - px(medida::contador) / 2);
  lienzo().AddRectFilled(desde, ImVec2(derecha.x, desde.y + px(medida::contador)), tono(color::claro));
  escribir(ImVec2(desde.x + (ancho - tamano.x) / 2, derecha.y - tamano.y / 2), numero, color::tinta, Letra::negrita,
           medida::letra_chica);
  return ancho;
}

float ancho_chip(std::string_view texto) {
  return medir(texto, Letra::negrita, medida::letra_mini).x +
         px(12 + static_cast<float>(texto.size()) * 0.8f);
}

void chip(ImVec2 desde, std::string_view texto) {
  const ImVec2 tamano = medir(texto, Letra::negrita, medida::letra_mini);
  lienzo().AddRectFilled(desde, ImVec2(desde.x + ancho_chip(texto), desde.y + tamano.y + px(4)), tono(color::elevado),
                         px(2));
  escribir_espaciado(ImVec2(desde.x + px(6), desde.y + px(2)), texto, color::texto2, Letra::negrita,
                     medida::letra_mini, px(0.8f));
}

void marco_cabecera(ImVec2 desde, ImVec2 hasta) {
  ImDrawList& dibujo = lienzo();
  const float paso = px(14);
  const float ancho = hasta.x - desde.x;
  for (float x = hasta.x - paso; x > desde.x; x -= paso) {
    const auto alfa = static_cast<std::uint32_t>(11 * (x - desde.x) / ancho);
    dibujo.AddLine(ImVec2(x, desde.y), ImVec2(x, hasta.y), tono(rgb(0xDDE6E6, alfa)));
  }
  for (float y = desde.y + paso; y < hasta.y; y += paso) {
    dibujo.AddRectFilledMultiColor(ImVec2(desde.x, y), ImVec2(hasta.x, y + 1), tono(rgb(0xDDE6E6, 0)),
                                   tono(color::cuadricula), tono(color::cuadricula), tono(rgb(0xDDE6E6, 0)));
  }
  escuadras(dibujo, desde, hasta, px(medida::escuadra), px(medida::grosor_escuadra), tono(color::escuadra_chat));
}

}
