#include "client/ui/menu_avisos.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>
#include <windows.h>

#include "client/horas.h"
#include "client/ui/piezas.h"
#include "common/utf.h"

namespace ventrik::ui {
namespace {
using cliente::AjusteDeAvisos;
using cliente::Avisar;
using cliente::AvisosDelServidor;

struct Plazo {
  Clave clave;
  std::uint64_t minutos;
};

constexpr std::array<Plazo, 6> kPlazos = {{{Clave::avisos_15_minutos, 15},
                                           {Clave::avisos_1_hora, 60},
                                           {Clave::avisos_3_horas, 180},
                                           {Clave::avisos_8_horas, 480},
                                           {Clave::avisos_24_horas, 1440},
                                           {Clave::avisos_siempre, 0}}};

constexpr std::array<std::pair<Avisar, Clave>, 4> kModos = {{{Avisar::como_el_servidor, Clave::avisos_como_el_servidor},
                                                             {Avisar::todos, Clave::avisos_todos},
                                                             {Avisar::menciones, Clave::avisos_menciones},
                                                             {Avisar::nada, Clave::avisos_nada}}};

struct Cuadro {
  Avisar modo;
  Clave titulo;
  Clave ayuda;
};

constexpr std::array<Cuadro, 3> kCuadros = {{{Avisar::todos, Clave::avisos_todos, Clave::avisos_todos_ayuda},
                                             {Avisar::menciones, Clave::avisos_menciones, Clave::avisos_menciones_ayuda},
                                             {Avisar::nada, Clave::avisos_nada, Clave::avisos_nada_ayuda}}};

std::string s(Idioma idioma, Clave clave) { return std::string(texto(idioma, clave)); }

std::uint64_t vence(std::uint64_t minutos, std::uint64_t ahora) {
  return minutos == 0 ? cliente::kHastaQueLoActive : ahora + minutos * 60000;
}

std::string nombre_de(Idioma idioma, const protocolo::CanalInfo& canal) {
  return canal.tipo == protocolo::TipoCanal::voz ? con_datos(texto(idioma, Clave::avisos_chat_de), {canal.nombre})
                                                 : "#" + canal.nombre;
}

std::string describir(Idioma idioma, const AjusteDeAvisos& ajuste, std::uint64_t ahora) {
  if (cliente::silenciado(ajuste, ahora)) {
    return estado_de_silencio(idioma, ajuste);
  }
  return s(idioma, ajuste.avisar == Avisar::todos       ? Clave::avisos_todos_aunque
                   : ajuste.avisar == Avisar::menciones ? Clave::avisos_menciones
                   : ajuste.avisar == Avisar::nada      ? Clave::avisos_nada
                                                        : Clave::avisos_como_el_servidor);
}

bool menu_con_valor(const std::string& etiqueta, std::string_view valor) {
  ImGuiWindow& ventana = *ImGui::GetCurrentWindow();
  const ImVec2 donde = ImGui::GetCursorScreenPos();
  const float hasta = donde.x + ImGui::GetContentRegionAvail().x;
  const float ancho = ImGui::CalcTextSize(valor.data(), valor.data() + valor.size()).x;
  ImGuiMenuColumns& columnas = ventana.DC.MenuColumns;
  const float total = columnas.DeclColumns(0, 0, ancho, 0);
  const bool abierto = ImGui::BeginMenu(etiqueta.c_str());
  ventana.DrawList->AddText(ImVec2(donde.x + columnas.OffsetShortcut + std::max(0.0f, hasta - donde.x - total),
                                   donde.y + ventana.DC.CurrLineTextBaseOffset),
                            ImGui::GetColorU32(ImGuiCol_TextDisabled), valor.data(), valor.data() + valor.size());
  return abierto;
}

}

std::string estado_de_silencio(Idioma idioma, const AjusteDeAvisos& ajuste) {
  return ajuste.silenciado_hasta == cliente::kHastaQueLoActive
             ? s(idioma, Clave::avisos_silenciado_siempre)
             : con_datos(texto(idioma, Clave::avisos_silenciado_hasta), {cliente::hora_local(ajuste.silenciado_hasta)});
}

std::optional<AjusteDeAvisos> menu_silenciar(Idioma idioma, Clave etiqueta, const AjusteDeAvisos& actual,
                                             std::uint64_t ahora) {
  if (!ImGui::BeginMenu(s(idioma, etiqueta).c_str())) {
    return std::nullopt;
  }
  std::optional<AjusteDeAvisos> nuevo;
  if (cliente::silenciado(actual, ahora) && ImGui::MenuItem(s(idioma, Clave::avisos_volver_a_activar).c_str())) {
    nuevo = actual;
    nuevo->silenciado_hasta = 0;
  }
  for (const Plazo& plazo : kPlazos) {
    if (ImGui::MenuItem(s(idioma, plazo.clave).c_str())) {
      nuevo = actual;
      nuevo->silenciado_hasta = vence(plazo.minutos, ahora);
    }
  }
  ImGui::Separator();
  if (ImGui::MenuItem(s(idioma, Clave::avisos_aun_con_mencion).c_str(), nullptr, actual.aun_con_mencion)) {
    nuevo = actual;
    nuevo->aun_con_mencion = !actual.aun_con_mencion;
  }
  ImGui::EndMenu();
  return nuevo;
}

void titulo_de_menu(std::string_view titulo) {
  std::wstring grande = a_utf16(titulo);
  CharUpperBuffW(grande.data(), static_cast<DWORD>(grande.size()));
  const std::string arriba = a_utf8(grande);
  const ImVec2 donde = ImGui::GetCursorScreenPos();
  escribir_espaciado(ImVec2(donde.x, donde.y + px(2)), arriba, color::apagado, Letra::negrita, medida::letra_mini,
                     px(1.2f));
  ImGui::Dummy(ImVec2(medir(arriba, Letra::negrita, medida::letra_mini).x, px(18)));
}

void nota_de_menu(std::string_view nota) {
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + px(240));
  ImGui::PushStyleColor(ImGuiCol_Text, tono(color::apagado));
  ImGui::TextWrapped("%.*s", static_cast<int>(nota.size()), nota.data());
  ImGui::PopStyleColor();
  ImGui::PopTextWrapPos();
}

std::optional<Avisar> menu_avisarme(Idioma idioma, Clave etiqueta, Avisar actual) {
  if (!menu_con_valor(s(idioma, etiqueta), texto(idioma, kModos[static_cast<std::size_t>(actual)].second))) {
    return std::nullopt;
  }
  std::optional<Avisar> elegido;
  for (const auto& [modo, clave] : kModos) {
    if (ImGui::MenuItem(s(idioma, clave).c_str(), nullptr, modo == actual)) {
      elegido = modo;
    }
  }
  ImGui::EndMenu();
  return elegido;
}

EleccionDeServidor menu_servidor(Idioma idioma, const AvisosDelServidor& avisos, bool conectado, protocolo::Rol rol,
                                 std::uint64_t ahora) {
  EleccionDeServidor eleccion;
  const std::string pronto = s(idioma, Clave::servidor_chip_pronto);
  ImGui::MenuItem(s(idioma, Clave::avisos_invitar).c_str(), pronto.c_str(), false, false);
  eleccion.abrir_avisos = ImGui::MenuItem(s(idioma, Clave::avisos_del_servidor).c_str());
  if (const auto ajuste = menu_silenciar(idioma, Clave::avisos_silenciar_servidor, avisos.servidor, ahora)) {
    eleccion.avisos = avisos;
    eleccion.avisos->servidor = *ajuste;
  }
  eleccion.marcar_todo = ImGui::MenuItem(s(idioma, Clave::avisos_marcar_todo).c_str(), nullptr, false, conectado);
  ImGui::Separator();
  const std::string solo = rol != protocolo::Rol::ninguno ? pronto : s(idioma, Clave::avisos_admins);
  ImGui::MenuItem(s(idioma, Clave::avisos_bots).c_str(), solo.c_str(), false, false);
  ImGui::MenuItem(s(idioma, Clave::avisos_configuracion).c_str(), solo.c_str(), false, false);
  eleccion.liberar_nombre =
      conectado && rol == protocolo::Rol::dueno && ImGui::MenuItem(s(idioma, Clave::avisos_liberar_nombre).c_str());
  if (conectado) {
    ImGui::Separator();
    eleccion.desconectar = ImGui::MenuItem(s(idioma, Clave::inicio_desconectar).c_str());
  }
  return eleccion;
}

std::optional<AvisosDelServidor> menu_de_canal(Idioma idioma, const AvisosDelServidor& avisos,
                                               const protocolo::CanalInfo& canal, std::uint64_t ahora) {
  const bool voz = canal.tipo == protocolo::TipoCanal::voz;
  AjusteDeAvisos ajuste = cliente::ajuste_de(avisos, canal.id);
  bool cambio = false;
  if (const auto nuevo =
          menu_silenciar(idioma, voz ? Clave::avisos_silenciar_chat : Clave::avisos_silenciar_canal, ajuste, ahora)) {
    ajuste = *nuevo;
    cambio = true;
  }
  if (const auto modo =
          menu_avisarme(idioma, voz ? Clave::avisos_avisarme_chat : Clave::avisos_avisarme_de, ajuste.avisar)) {
    ajuste.avisar = *modo;
    cambio = true;
  }
  if (!cambio) {
    return std::nullopt;
  }
  AvisosDelServidor nuevos = avisos;
  cliente::cambiar_canal(nuevos, canal.id, ajuste);
  return nuevos;
}

EleccionDeAvisos dialogo_avisos(Idioma idioma, std::string_view servidor, const AvisosDelServidor& avisos,
                                const std::vector<protocolo::CanalInfo>* canales, std::uint64_t ahora, bool& abrir) {
  if (abrir) {
    ImGui::OpenPopup("##avisos");
    abrir = false;
  }
  if (!ImGui::IsPopupOpen("##avisos")) {
    return {};
  }
  const float ancho = px(640);
  const float interior = ancho - px(40);
  const std::string nota = s(idioma, Clave::avisos_nota);
  const std::size_t filas = canales != nullptr ? avisos.canales.size() : 1;
  const float alto_nota = alto_envuelto(nota, interior, medida::letra_chica);
  const float alto_fijo = px(294 + 40 + 10 + 14 + medida::alto_principal + 20) + alto_nota;
  const ImVec2 pantalla = ImGui::GetIO().DisplaySize;
  const float renglon = px(41);
  const float caben = std::max(1.0f, std::floor((pantalla.y - px(40) - alto_fijo) / renglon));
  const float alto_filas = renglon * std::min(static_cast<float>(filas), caben);
  const float alto = alto_fijo + alto_filas;
  ImGui::SetNextWindowPos(ImVec2(pantalla.x / 2, pantalla.y / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(ancho, alto));
  if (!ImGui::BeginPopupModal("##avisos", nullptr,
                              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoScrollbar)) {
    return {};
  }
  ImGuiStorage& memoria = *ImGui::GetStateStorage();
  const ImGuiID encima = ImGui::GetID("##encima");
  if (ImGui::IsKeyPressed(ImGuiKey_Escape) && memoria.GetBool(encima, true)) {
    cerrar_ventana_emergente();
    ImGui::EndPopup();
    return {std::nullopt, true};
  }
  std::optional<AvisosDelServidor> nuevo;
  const auto cambiar = [&nuevo, &avisos](auto&& tocar) {
    if (!nuevo) {
      nuevo = avisos;
    }
    tocar(*nuevo);
  };
  ImDrawList& dibujo = *ImGui::GetWindowDrawList();
  const ImVec2 desde = ImGui::GetWindowPos();
  triangulos(dibujo, desde, ImVec2(desde.x + ancho, desde.y + alto));
  const float x = desde.x + px(20);
  float y = desde.y + px(20);
  escribir_cortado(ImVec2(x, y), interior, con_datos(texto(idioma, Clave::avisos_titulo), {std::string(servidor)}),
                   color::texto, Letra::titulo, medida::letra_tarjeta);
  y += px(36);
  escribir(ImVec2(x, y), texto(idioma, Clave::avisos_silenciar_el_servidor), color::texto2, Letra::normal,
           medida::letra_chica);
  y += px(20);
  const bool callado = cliente::silenciado(avisos.servidor, ahora);
  std::vector<std::string> plazos = {s(idioma, Clave::avisos_no)};
  for (const Plazo& plazo : kPlazos) {
    plazos.push_back(s(idioma, plazo.clave));
  }
  if (const auto elegido = desplegable("##silenciar", ImVec2(x, y - px(4)), px(300), plazos, callado ? -1 : 0,
                                       callado ? estado_de_silencio(idioma, avisos.servidor) : std::string())) {
    const std::uint64_t hasta = *elegido == 0 ? 0 : vence(kPlazos[static_cast<std::size_t>(*elegido - 1)].minutos, ahora);
    cambiar([hasta](AvisosDelServidor& a) { a.servidor.silenciado_hasta = hasta; });
  }
  if (casilla(texto(idioma, Clave::avisos_aun_con_mencion), ImVec2(x + px(316), y), avisos.servidor.aun_con_mencion)) {
    cambiar([](AvisosDelServidor& a) { a.servidor.aun_con_mencion = !a.servidor.aun_con_mencion; });
  }
  y += px(46);
  escribir(ImVec2(x, y), texto(idioma, Clave::avisos_avisarme_de_titulo), color::texto2, Letra::negrita,
           medida::letra_chica);
  y += px(22);
  const float hueco = px(10);
  const float ancho_cuadro = (interior - 2 * hueco) / 3;
  const auto esquina_de = [&](std::size_t i) { return ImVec2(x + static_cast<float>(i) * (ancho_cuadro + hueco), y); };
  for (std::size_t i = 0; i < kCuadros.size(); ++i) {
    const Avisar modo = kCuadros[i].modo;
    ImGui::SetCursorScreenPos(esquina_de(i));
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::InvisibleButton("##modo", ImVec2(ancho_cuadro, px(66))) &&
        nuevo.value_or(avisos).servidor.avisar != modo) {
      cambiar([modo](AvisosDelServidor& a) { a.servidor.avisar = modo; });
    }
    ImGui::PopID();
  }
  const Avisar elegido_ahora = nuevo.value_or(avisos).servidor.avisar;
  for (std::size_t i = 0; i < kCuadros.size(); ++i) {
    const Cuadro& cuadro = kCuadros[i];
    const bool elegido = elegido_ahora == cuadro.modo;
    const ImVec2 esquina = esquina_de(i);
    const ImVec2 otra(esquina.x + ancho_cuadro, y + px(66));
    dibujo.AddRectFilled(esquina, otra, tono(elegido ? color::elevado : color::pozo));
    dibujo.AddRect(esquina, otra, tono(elegido ? color::claro : color::linea), 0, 0, elegido ? px(1.5f) : px(1));
    const ImVec2 centro(esquina.x + px(18), esquina.y + px(22));
    dibujo.AddCircle(centro, px(6), tono(color::linea2), 0, px(1.5f));
    if (elegido) {
      dibujo.AddCircleFilled(centro, px(3.5f), tono(color::claro));
    }
    escribir_en(esquina.x + px(32), esquina.y + px(22), texto(idioma, cuadro.titulo), color::texto, Letra::fuerte,
                medida::letra, ancho_cuadro - px(40));
    escribir_en(esquina.x + px(32), esquina.y + px(44), texto(idioma, cuadro.ayuda), color::apagado, Letra::normal,
                medida::letra_chica, ancho_cuadro - px(40));
  }
  y += px(80);
  if (casilla(texto(idioma, Clave::avisos_no_contar_todos), ImVec2(x, y), !avisos.contar_todos)) {
    cambiar([](AvisosDelServidor& a) { a.contar_todos = !a.contar_todos; });
  }
  y += px(30);
  if (casilla(texto(idioma, Clave::avisos_sonidos_de_voz), ImVec2(x, y), avisos.sonidos_de_voz)) {
    cambiar([](AvisosDelServidor& a) { a.sonidos_de_voz = !a.sonidos_de_voz; });
  }
  y += px(40);
  const float alto_caja = px(40) + alto_filas;
  dibujo.AddRect(ImVec2(x, y), ImVec2(x + interior, y + alto_caja), tono(color::linea));
  escribir_en(x + px(12), y + px(20), texto(idioma, Clave::avisos_canales), color::texto, Letra::fuerte);
  if (canales != nullptr) {
    const std::string_view agregar = texto(idioma, Clave::avisos_agregar_canal);
    const float ancho_agregar = ancho_boton(agregar, Estilo::contorneado);
    ImGui::SetCursorScreenPos(ImVec2(x + interior - px(8) - ancho_agregar, y + px(5)));
    if (boton("##agregar", agregar, Estilo::contorneado, ImVec2(ancho_agregar, px(30)))) {
      ImGui::OpenPopup("##agregar_canal");
    }
    if (ImGui::BeginPopup("##agregar_canal")) {
      for (const auto& canal : *canales) {
        ImGui::PushID(static_cast<int>(canal.id));
        if (ImGui::BeginMenu(nombre_de(idioma, canal).c_str())) {
          if (auto cambiados = menu_de_canal(idioma, nuevo.value_or(avisos), canal, ahora)) {
            nuevo = std::move(cambiados);
          }
          ImGui::EndMenu();
        }
        ImGui::PopID();
      }
      ImGui::EndPopup();
    }
  }
  ImGui::SetCursorScreenPos(ImVec2(x, y + px(40)));
  if (filas > 0) {
    ImGui::BeginChild("##filas", ImVec2(interior, alto_filas));
  }
  ImDrawList& renglones = *ImGui::GetWindowDrawList();
  const float ancho_filas = ImGui::GetContentRegionAvail().x;
  float fila = ImGui::GetCursorScreenPos().y;
  ImGui::Dummy(ImVec2(1, renglon * static_cast<float>(filas)));
  for (const auto& propio : avisos.canales) {
    if (canales == nullptr) {
      break;
    }
    renglones.AddLine(ImVec2(x, fila), ImVec2(x + ancho_filas, fila), tono(color::linea));
    const float medio = fila + px(20);
    const auto canal = std::find_if(canales->begin(), canales->end(),
                                    [&propio](const protocolo::CanalInfo& c) { return c.id == propio.canal; });
    const std::string nombre =
        canal != canales->end() ? nombre_de(idioma, *canal) : "#" + std::to_string(propio.canal);
    const std::string_view quitar = texto(idioma, Clave::avisos_quitar);
    const float ancho_quitar = ancho_boton(quitar, Estilo::esquineros);
    const float derecha = x + ancho_filas - px(8) - ancho_quitar;
    const std::string estado = describir(idioma, propio.ajuste, ahora);
    const float ancho_estado = medir(estado, Letra::normal, medida::letra_chica).x;
    escribir_en(x + px(12), medio, nombre, color::texto, Letra::fuerte, medida::letra, derecha - ancho_estado - x - px(30));
    escribir_en(derecha - px(12) - ancho_estado, medio, estado, color::apagado, Letra::normal, medida::letra_chica);
    ImGui::SetCursorScreenPos(ImVec2(derecha, medio - px(15)));
    ImGui::PushID(static_cast<int>(propio.canal));
    if (boton("##quitar", quitar, Estilo::esquineros, ImVec2(ancho_quitar, px(30)))) {
      const std::uint32_t id = propio.canal;
      cambiar([id](AvisosDelServidor& a) { cliente::cambiar_canal(a, id, {}); });
    }
    ImGui::PopID();
    fila += renglon;
  }
  if (canales == nullptr) {
    renglones.AddLine(ImVec2(x, fila), ImVec2(x + ancho_filas, fila), tono(color::linea));
    escribir_en(x + px(12), fila + px(20),
                con_datos(texto(idioma, Clave::avisos_sin_conexion), {std::to_string(avisos.canales.size())}),
                color::apagado, Letra::normal, medida::letra_chica, ancho_filas - px(24));
  }
  if (filas > 0) {
    ImGui::EndChild();
  }
  y += alto_caja + px(10);
  escribir_envuelto(ImVec2(x, y), nota, color::apagado, interior, medida::letra_chica);
  y += alto_nota + px(14);
  const std::string_view listo = texto(idioma, Clave::avisos_listo);
  const float ancho_listo = ancho_boton(listo, Estilo::principal);
  ImGui::SetCursorScreenPos(ImVec2(x + interior - ancho_listo, y));
  const bool cerrado = boton("##listo", listo, Estilo::principal, ImVec2(ancho_listo, px(medida::alto_principal)));
  if (cerrado) {
    cerrar_ventana_emergente();
  }
  memoria.SetBool(encima, ImGui::GetCurrentContext()->OpenPopupStack.back().Window == ImGui::GetCurrentWindow());
  ImGui::EndPopup();
  return {std::move(nuevo), cerrado};
}

}
