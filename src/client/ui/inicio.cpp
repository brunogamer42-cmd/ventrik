#include "client/ui/inicio.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include <windows.h>

#include "client/ui/piezas.h"
#include "common/cifrado/huella.h"
#include "common/texto.h"
#include "common/utf.h"
#include "common/version.h"

namespace ventrik::ui {
namespace {
constexpr float kMargen = 20;
constexpr float kAltoBarra = 68;
constexpr float kAltoPie = 34;
constexpr float kAnchoDerecha = 320;
constexpr float kSeparacion = 24;
constexpr float kAltoTarjeta = 165;
constexpr float kEntreTarjetas = 12;
constexpr float kRelleno = 17;
constexpr float kAltoBoton = 44;

std::string fecha_local(std::uint64_t milisegundos) {
  const std::uint64_t desde_1601 = milisegundos * 10000 + 116444736000000000ull;
  const FILETIME universal = {static_cast<DWORD>(desde_1601), static_cast<DWORD>(desde_1601 >> 32)};
  FILETIME local = {};
  SYSTEMTIME partes = {};
  std::array<wchar_t, 64> fecha{};
  std::array<wchar_t, 32> hora{};
  if (!FileTimeToLocalFileTime(&universal, &local) || !FileTimeToSystemTime(&local, &partes) ||
      GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &partes, nullptr, fecha.data(),
                     static_cast<int>(fecha.size())) == 0 ||
      GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &partes, nullptr, hora.data(),
                     static_cast<int>(hora.size())) == 0) {
    return {};
  }
  return a_utf8(std::wstring(fecha.data()) + L" " + hora.data());
}

class Dibujo {
 public:
  Dibujo(PantallaInicio& pantalla, const cliente::Guardados& guardados)
      : pantalla_(pantalla), guardados_(guardados), ancho_(ImGui::GetIO().DisplaySize.x),
        alto_(ImGui::GetIO().DisplaySize.y) {}

  Pedido dibujar() {
    barra();
    servidores();
    ImGui::SetCursorScreenPos(ImVec2(ancho_ - px(kMargen + kAnchoDerecha), px(93)));
    ImGui::BeginChild("##derecha", ImVec2(px(kMargen + kAnchoDerecha), alto_ - px(kAltoPie + kMargen + 93)));
    const ImVec2 origen = ImGui::GetCursorScreenPos();
    float y = invitacion(origen) + px(16);
    y = futuro(ImVec2(origen.x, y), Clave::inicio_tu_estado, Clave::inicio_tu_estado_ayuda,
               Clave::inicio_disponible, "4", "##estado") + px(16);
    y = futuro(ImVec2(origen.x, y), Clave::inicio_tu_identidad, Clave::inicio_tu_identidad_ayuda,
               Clave::inicio_hacer_copia, "4", "##identidad");
    ImGui::SetCursorScreenPos(ImVec2(origen.x, y));
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndChild();
    pie();
    return pedido_;
  }

 private:
  std::string_view t(Clave clave) const { return texto(pantalla_.idioma, clave); }

  std::string llega_en(std::string_view etapa) const { return con_datos(t(Clave::inicio_llega_en), {etapa}); }

  void apagado(const char* id, std::string_view etiqueta, Estilo estilo, ImVec2 tamano, Icono icono,
               std::string_view etapa) const {
    ImGui::BeginDisabled();
    boton(id, etiqueta, estilo, tamano, icono);
    ImGui::EndDisabled();
    cartel(llega_en(etapa));
  }

  void barra() const {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const float alto = px(kAltoBarra);
    dibujo.AddRectFilled(ImVec2(0, 0), ImVec2(ancho_, alto), tono(color::panel));
    dibujo.AddRectFilled(ImVec2(0, alto), ImVec2(ancho_, alto + 1), tono(color::linea));
    triangulos(dibujo, ImVec2(0, 0), ImVec2(ancho_, alto));
    emblema(ImVec2(px(kMargen), px(13)), px(42), pantalla_.logo);
    const std::string_view programa = t(Clave::titulo_ventana);
    escribir(ImVec2(px(73), (alto - medir(programa, Letra::titulo, medida::letra_servidor).y) / 2), programa,
             color::texto, Letra::titulo, medida::letra_servidor);

    float x = ancho_ - px(kMargen);
    const float y = (alto - px(medida::alto_biselado)) / 2;
    if (!guardados_.nombre.empty()) {
      const float ancho_chip = px(68) + medir(guardados_.nombre, Letra::fuerte, medida::letra).x;
      x -= ancho_chip;
      ImGui::SetCursorScreenPos(ImVec2(x, y));
      boton("##yo", "", Estilo::biselado, ImVec2(ancho_chip, px(medida::alto_biselado)));
      avatar(ImVec2(x + px(10), y + 1), px(medida::alto_biselado) - 2, iniciales(guardados_.nombre),
             color::avatar_propio, color::texto, medida::letra_chica);
      escribir(ImVec2(x + px(54), y + (px(medida::alto_biselado) - medir("", Letra::fuerte, medida::letra).y) / 2),
               guardados_.nombre, color::texto, Letra::fuerte);
      x -= px(8);
    }
    for (const auto& [clave, icono, etapa, id] :
         {std::make_tuple(Clave::inicio_ajustes, Icono::ajustes, "3", "##ajustes"),
          std::make_tuple(Clave::inicio_reportar, Icono::reportar, "7", "##reportar")}) {
      const float ancho = ancho_boton(t(clave), Estilo::biselado, icono);
      x -= ancho;
      ImGui::SetCursorScreenPos(ImVec2(x, y));
      apagado(id, t(clave), Estilo::biselado, ImVec2(ancho, px(medida::alto_biselado)), icono, etapa);
      x -= px(8);
    }
    const std::string_view rendimiento = t(Clave::inicio_modo_rendimiento);
    const std::string_view visual = t(Clave::inicio_modo_visual);
    ImGui::SetCursorScreenPos(ImVec2(x - ancho_selector_modo(rendimiento, visual), (alto - px(44)) / 2));
    selector_modo(rendimiento, visual, llega_en("10"));
  }

  void servidores() {
    const float izquierda = px(kMargen);
    const float derecha = ancho_ - px(kMargen + kAnchoDerecha + kSeparacion);
    escribir(ImVec2(izquierda, px(92)), t(Clave::inicio_tus_servidores), color::texto, Letra::negrita,
             medida::letra_pagina);
    escribir(ImVec2(izquierda, px(127)), t(Clave::inicio_subtitulo), color::apagado);
    const std::array<std::tuple<Clave, Icono, const char*, const char*>, 3> botones = {
        std::make_tuple(Clave::inicio_explorar, Icono::lupa, "7", "##explorar"),
        std::make_tuple(Clave::inicio_buscar_red, Icono::pantalla, "6", "##buscar_red"),
        std::make_tuple(Clave::inicio_hostear, Icono::mas, "6", "##hostear")};
    float ancho_botones = px(16);
    for (const auto& [clave, icono, etapa, id] : botones) {
      ancho_botones += ancho_boton(t(clave), Estilo::esquineros, icono);
    }
    const float fin_textos =
        izquierda + px(24) +
        std::max(medir(t(Clave::inicio_tus_servidores), Letra::negrita, medida::letra_pagina).x,
                 medir(t(Clave::inicio_subtitulo), Letra::normal, medida::letra).x);
    const bool debajo = derecha - ancho_botones < fin_textos;
    const float bajada = debajo ? px(54) : 0;
    float x = debajo ? izquierda : derecha - ancho_botones;
    for (const auto& [clave, icono, etapa, id] : botones) {
      const float ancho = ancho_boton(t(clave), Estilo::esquineros, icono);
      ImGui::SetCursorScreenPos(ImVec2(x, px(debajo ? 160.0f : 110.0f)));
      apagado(id, t(clave), Estilo::esquineros, ImVec2(ancho, px(medida::alto_esquineros)), icono, etapa);
      x += ancho + px(8);
    }
    ImGui::SetCursorScreenPos(ImVec2(izquierda, px(164) + bajada));
    campo("##buscar", t(Clave::inicio_buscar), pantalla_.buscar,
          ImVec2(std::min(px(420), derecha - izquierda), px(42)), Icono::lupa, 0, color::panel);

    const std::wstring buscado = nombre_para_comparar(pantalla_.buscar.data());
    std::vector<std::size_t> visibles;
    for (std::size_t i = 0; i < guardados_.servidores.size(); ++i) {
      const auto& servidor = guardados_.servidores[i];
      if (buscado.empty() || nombre_para_comparar(red::a_texto(servidor.direccion) + " " + servidor.nombre +
                                                  " " + servidor.nombre_canal)
                                     .find(buscado) != std::wstring::npos) {
        visibles.push_back(i);
      }
    }

    const float arriba = px(222) + bajada;
    const ImVec2 tamano(derecha - izquierda, std::max(px(kAltoTarjeta), alto_ - px(kAltoPie + kMargen) - arriba));
    ImGui::SetCursorScreenPos(ImVec2(izquierda, arriba));
    ImGui::BeginChild("##tarjetas", tamano);
    const ImVec2 origen = ImGui::GetCursorScreenPos();
    if (visibles.empty()) {
      escribir_parrafo(t(guardados_.servidores.empty() ? Clave::inicio_sin_servidores : Clave::inicio_sin_resultados),
                       color::apagado, std::min(tamano.x, px(560)));
    }
    const std::size_t columnas = tamano.x >= px(600) ? 2 : 1;
    const std::size_t filas = (visibles.size() + columnas - 1) / columnas;
    const float alto_total = static_cast<float>(filas) * px(kAltoTarjeta + kEntreTarjetas);
    const float util =
        tamano.x - (alto_total - px(kEntreTarjetas) > tamano.y ? ImGui::GetStyle().ScrollbarSize + px(4) : 0);
    const float ancho = (util - px(kEntreTarjetas) * static_cast<float>(columnas - 1)) / static_cast<float>(columnas);
    for (std::size_t k = 0; k < visibles.size(); ++k) {
      ImGui::PushID(red::a_texto(guardados_.servidores[visibles[k]].direccion).c_str());
      tarjeta(visibles[k], ImVec2(origen.x + static_cast<float>(k % columnas) * (ancho + px(kEntreTarjetas)),
                                  origen.y + static_cast<float>(k / columnas) * px(kAltoTarjeta + kEntreTarjetas)),
              ancho);
      ImGui::PopID();
    }
    if (!visibles.empty()) {
      ImGui::SetCursorScreenPos(origen);
      ImGui::Dummy(ImVec2(util, alto_total - px(kEntreTarjetas)));
    }
    ImGui::EndChild();
  }

  void tarjeta(std::size_t cual, ImVec2 desde, float ancho) {
    const cliente::ServidorGuardado& servidor = guardados_.servidores[cual];
    const Intento& intento = pantalla_.intento;
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const ImVec2 hasta(desde.x + ancho, desde.y + px(kAltoTarjeta));
    const bool encima = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(desde, hasta);
    dibujo.AddRectFilled(desde, hasta, tono(color::panel), px(2));
    dibujo.AddRect(desde, hasta, tono(encima ? color::texto : color::linea), px(2));

    const float x = desde.x + px(15);
    const float interior = ancho - px(30);
    avatar(ImVec2(x, desde.y + px(15)), px(48), "", color::elevado, color::texto2, medida::letra);
    dibujar_icono(dibujo, Icono::servidor, ImVec2(x + px(24), desde.y + px(39)), px(20), tono(color::texto2));
    escribir_cortado(ImVec2(x + px(60), desde.y + px(17)), interior - px(60), red::a_texto(servidor.direccion),
                     color::texto, Letra::fuerte, medida::letra_tarjeta);

    const bool es_este = intento.fase != Fase::quieta && intento.direccion == servidor.direccion &&
                         (!intento.desde_formulario || intento.fase == Fase::conectada);
    ImU32 punto = color::apagado;
    std::string estado = con_datos(t(Clave::inicio_ultima_vez), {fecha_local(servidor.ultima_vez_ms)});
    if (es_este && intento.fase == Fase::conectando) {
      punto = color::amarillo;
      estado = std::string(t(Clave::inicio_conectando));
    } else if (es_este && intento.fase == Fase::conectada) {
      punto = color::verde;
      estado = intento.mensaje;
    }
    dibujo.AddRectFilled(ImVec2(x + px(60), desde.y + px(46)), ImVec2(x + px(66), desde.y + px(52)), tono(punto));
    escribir_cortado(ImVec2(x + px(73), desde.y + px(42)), interior - px(73), estado, color::apagado, Letra::normal,
                     medida::letra_mini);

    ImGui::SetCursorScreenPos(ImVec2(x, desde.y + px(76)));
    if (es_este && intento.fase == Fase::error) {
      ImGui::PushClipRect(ImVec2(x, desde.y + px(76)), ImVec2(x + interior, desde.y + px(104)), true);
      escribir_parrafo(intento.mensaje, color::rojo, interior, medida::letra_mini);
      ImGui::PopClipRect();
      cartel(intento.mensaje);
    } else {
      std::string descripcion = con_datos(t(Clave::inicio_entraste_como), {servidor.nombre});
      if (!servidor.nombre_canal.empty()) {
        descripcion += " " + con_datos(t(Clave::inicio_ultimo_canal), {servidor.nombre_canal});
      }
      escribir_cortado(ImVec2(x, desde.y + px(76)), interior, descripcion, color::texto2);
    }

    const float y = desde.y + px(107);
    const bool ocupado = es_este && intento.fase != Fase::error;
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    if (boton("##entrar",
              t(!ocupado                             ? Clave::inicio_entrar
                : intento.fase == Fase::conectando ? Clave::inicio_cancelar
                                                    : Clave::inicio_desconectar),
              Estilo::principal,
              ImVec2(interior - px(kAltoBoton + 8), px(kAltoBoton)))) {
      pedido_ = {ocupado ? Accion::desconectar : Accion::entrar_a_guardado, cual};
    }
    ImGui::SetCursorScreenPos(ImVec2(hasta.x - px(15 + kAltoBoton), y));
    if (boton("##mas", "", Estilo::esquineros, ImVec2(px(kAltoBoton), px(kAltoBoton)), Icono::puntos)) {
      ImGui::OpenPopup("##menu");
    }
    if (ImGui::BeginPopup("##menu")) {
      if (ImGui::Selectable(std::string(t(Clave::inicio_entrar_dueno)).c_str())) {
        poner(pantalla_.direccion, red::a_texto(servidor.direccion));
        poner(pantalla_.huella, cifrado::Huella(servidor.huella).a_texto());
        poner(pantalla_.nombre, servidor.nombre);
        pantalla_.codigo.fill('\0');
        pantalla_.enfocar_codigo = true;
      }
      if (ImGui::Selectable(std::string(t(Clave::inicio_quitar)).c_str())) {
        pedido_ = {Accion::quitar, cual};
      }
      ImGui::EndPopup();
    }
  }

  template <typename Contenido>
  float con_panel(ImVec2 desde, Clave titulo, bool futuro, Contenido&& contenido) {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const float ancho = px(kAnchoDerecha);
    dibujo.ChannelsSplit(2);
    dibujo.ChannelsSetCurrent(1);
    ImGui::BeginDisabled(futuro);
    const float x = desde.x + px(kRelleno);
    escribir(ImVec2(x, desde.y + px(17)), t(titulo), color::texto, Letra::titulo, medida::letra_panel);
    ImGui::SetCursorScreenPos(ImVec2(x, desde.y + px(52)));
    contenido(x, ancho - px(2 * kRelleno));
    ImGui::EndDisabled();
    const float abajo = ImGui::GetItemRectMax().y + px(kRelleno);
    dibujo.ChannelsSetCurrent(0);
    panel(desde, ImVec2(desde.x + ancho, abajo));
    dibujo.ChannelsMerge();
    return abajo;
  }

  float invitacion(ImVec2 desde) {
    return con_panel(desde, Clave::inicio_invitacion, false, [this](float x, float interior) {
      escribir_parrafo(t(Clave::inicio_invitacion_ayuda), color::apagado, interior, medida::letra_chica);
      float y = ImGui::GetItemRectMax().y + px(12);
      bool enter = false;
      for (const auto& [id, pista, campo_texto] :
           {std::make_tuple("##direccion", Clave::inicio_direccion, &pantalla_.direccion),
            std::make_tuple("##huella", Clave::inicio_huella, &pantalla_.huella),
            std::make_tuple("##nombre", Clave::inicio_nombre, &pantalla_.nombre),
            std::make_tuple("##codigo", Clave::inicio_codigo, &pantalla_.codigo)}) {
        const bool es_codigo = campo_texto == &pantalla_.codigo;
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        if (es_codigo && pantalla_.enfocar_codigo) {
          ImGui::SetKeyboardFocusHere();
          pantalla_.enfocar_codigo = false;
        }
        enter |= campo(id, t(pista), *campo_texto, ImVec2(interior, px(medida::alto_campo)), Icono::ninguno,
                       ImGuiInputTextFlags_EnterReturnsTrue | (es_codigo ? ImGuiInputTextFlags_Password : 0));
        y += px(medida::alto_campo + 8);
      }
      const Intento& intento = pantalla_.intento;
      const bool conectando = intento.desde_formulario && intento.fase == Fase::conectando;
      ImGui::SetCursorScreenPos(ImVec2(x, y + px(2)));
      if (boton("##entrar_invitacion", t(conectando ? Clave::inicio_cancelar : Clave::inicio_entrar),
                Estilo::principal, ImVec2(interior, px(medida::alto_principal))) ||
          (enter && !conectando)) {
        pedido_ = {conectando ? Accion::desconectar : Accion::entrar_con_formulario, 0};
      }
      const std::string_view aviso = !pantalla_.error_formulario.empty() ? std::string_view(pantalla_.error_formulario)
                                     : conectando ? t(Clave::inicio_conectando)
                                     : intento.desde_formulario && intento.fase == Fase::error
                                         ? std::string_view(intento.mensaje)
                                         : std::string_view();
      if (!aviso.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(x, ImGui::GetItemRectMax().y + px(12)));
        escribir_parrafo(aviso, conectando ? color::apagado : color::rojo, interior, medida::letra_chica);
      }
    });
  }

  float futuro(ImVec2 desde, Clave titulo, Clave ayuda, Clave accion, std::string_view etapa, const char* id) {
    return con_panel(desde, titulo, true, [&](float, float interior) {
      escribir_parrafo(t(ayuda), color::texto2, interior, medida::letra_chica);
      ImGui::SetCursorScreenPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + px(14)));
      boton(id, t(accion), Estilo::contorneado, ImVec2(interior, px(medida::alto_principal)));
      cartel(llega_en(etapa));
    });
  }

  void pie() const {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const float arriba = alto_ - px(kAltoPie);
    dibujo.AddRectFilled(ImVec2(0, arriba), ImVec2(ancho_, alto_), tono(color::panel));
    dibujo.AddRectFilled(ImVec2(0, arriba), ImVec2(ancho_, arriba + 1), tono(color::linea));
    const std::string version = con_datos(t(Clave::inicio_version), {a_texto(kVersionPrograma)});
    const float y = arriba + (px(kAltoPie) - medir(version, Letra::normal, medida::letra_chica).y) / 2;
    escribir(ImVec2(px(kMargen), y), version, color::apagado, Letra::normal, medida::letra_chica);
    escribir(ImVec2(px(kMargen + 20) + medir(version, Letra::normal, medida::letra_chica).x, y),
             t(Clave::inicio_pie_modo), color::apagado, Letra::normal, medida::letra_chica);
    if (!pantalla_.aviso.empty()) {
      escribir(ImVec2(ancho_ - px(kMargen) - medir(pantalla_.aviso, Letra::normal, medida::letra_chica).x, y),
               pantalla_.aviso, color::amarillo, Letra::normal, medida::letra_chica);
    }
  }

  PantallaInicio& pantalla_;
  const cliente::Guardados& guardados_;
  const float ancho_;
  const float alto_;
  Pedido pedido_;
};

}

void poner(Campo& campo, std::string_view texto) {
  campo.fill('\0');
  std::copy_n(texto.begin(), std::min(texto.size(), campo.size() - 1), campo.begin());
}

Pedido dibujar_inicio(PantallaInicio& pantalla, const cliente::Guardados& guardados) {
  return Dibujo(pantalla, guardados).dibujar();
}

}
