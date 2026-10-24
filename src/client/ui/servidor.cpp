#include "client/ui/servidor.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

#include <windows.h>

#include "client/guardados.h"
#include "client/horas.h"
#include "client/ui/inicio.h"
#include "common/utf.h"

namespace ventrik::ui {
namespace {
using protocolo::TipoCanal;

constexpr float kMargenChat = 36;
constexpr float kSangria = 40;
constexpr float kAltoCabecera = 50;
constexpr float kAltoAbajo = 82;
constexpr float kAltoTuAudio = 243;
constexpr float kAltoDia = 30;
constexpr float kAltoGrupo = 27;
constexpr float kArribaMensajes = 28;
constexpr std::uint64_t kMsPorDia = 24ull * 60 * 60 * 1000;

float actualizar(Alturas& alturas, const cliente::Chat& chat, float ancho) {
  const bool mismo = alturas.canal == chat.canal && alturas.ancho == ancho;
  if (mismo && alturas.ids.size() == chat.mensajes.size() && alturas.desde.size() == chat.mensajes.size() + 1 &&
      (chat.mensajes.empty() ||
       (alturas.ids.front() == chat.mensajes.front().id && alturas.ids.back() == chat.mensajes.back().id))) {
    return 0;
  }
  Alturas nuevas{chat.canal, ancho, {}, {}, {}, {0}, {}};
  for (const auto& mensaje : chat.mensajes) {
    const auto viejo = std::lower_bound(alturas.ids.begin(), alturas.ids.end(), mensaje.id);
    const bool conocido = mismo && viejo != alturas.ids.end() && *viejo == mensaje.id;
    const auto i = static_cast<std::size_t>(viejo - alturas.ids.begin());
    nuevas.ids.push_back(mensaje.id);
    nuevas.dias.push_back(conocido ? alturas.dias[i] : cliente::dia_local(mensaje.hora_ms));
    nuevas.textos.push_back(conocido ? alturas.textos[i] : alto_envuelto(mensaje.texto, ancho));
  }
  for (std::size_t i = 0; i < nuevas.ids.size(); ++i) {
    const bool nuevo = i == 0 || nuevas.dias[i] != nuevas.dias[i - 1];
    nuevas.nuevo_dia.push_back(nuevo);
    nuevas.desde.push_back(nuevas.desde.back() + (nuevo ? px(kAltoDia) : 0) + px(20) + nuevas.textos[i] + px(11));
  }
  float agregado = 0;
  if (mismo && !alturas.ids.empty()) {
    const auto primero = std::lower_bound(nuevas.ids.begin(), nuevas.ids.end(), alturas.ids.front());
    if (primero != nuevas.ids.end() && *primero == alturas.ids.front()) {
      agregado = nuevas.desde[static_cast<std::size_t>(primero - nuevas.ids.begin())];
    }
  }
  alturas = std::move(nuevas);
  return agregado;
}

void punteado(ImDrawList& dibujo, ImVec2 desde, ImVec2 hasta, ImU32 color) {
  const float paso = px(4);
  for (float x = desde.x; x < hasta.x; x += paso) {
    dibujo.AddRectFilled(ImVec2(x, desde.y), ImVec2(std::min(x + px(2), hasta.x), desde.y + 1), color);
    dibujo.AddRectFilled(ImVec2(x, hasta.y - 1), ImVec2(std::min(x + px(2), hasta.x), hasta.y), color);
  }
  for (float y = desde.y; y < hasta.y; y += paso) {
    dibujo.AddRectFilled(ImVec2(desde.x, y), ImVec2(desde.x + 1, std::min(y + px(2), hasta.y)), color);
    dibujo.AddRectFilled(ImVec2(hasta.x - 1, y), ImVec2(hasta.x, std::min(y + px(2), hasta.y)), color);
  }
}

class Dibujo {
 public:
  Dibujo(PantallaServidor& pantalla, cliente::Sala& sala)
      : pantalla_(pantalla), sala_(sala), ancho_(ImGui::GetIO().DisplaySize.x),
        alto_(ImGui::GetIO().DisplaySize.y), hoy_(cliente::dia_local(cliente::ahora_ms())),
        ayer_(cliente::dia_local(cliente::ahora_ms() - kMsPorDia)) {
    const auto yo = sala_.usuarios().find(sala_.tu_id());
    mi_nombre_ = yo != sala_.usuarios().end() ? yo->second.nombre : std::string();
  }

  PedidoServidor dibujar() {
    barra();
    lateral();
    chat();
    estado();
    dialogo();
    return std::move(pedido_);
  }

 private:
  std::string_view t(Clave clave) const { return texto(pantalla_.idioma, clave); }

  std::string llega_en(std::string_view etapa) const { return con_datos(t(Clave::inicio_llega_en), {etapa}); }

  void agregar(std::vector<cliente::Orden> ordenes) {
    std::move(ordenes.begin(), ordenes.end(), std::back_inserter(pedido_.ordenes));
  }

  template <typename Dibujar>
  void apagado(std::string_view aviso, Dibujar dibujar) const {
    ImGui::BeginDisabled();
    dibujar();
    ImGui::EndDisabled();
    cartel(aviso);
  }

  void abrir(Dialogo cual, std::uint32_t canal, std::string_view nombre, bool solo_admins) {
    pantalla_.dialogo = cual;
    pantalla_.abrir_dialogo = true;
    pantalla_.canal_dialogo = canal;
    poner(pantalla_.nombre_dialogo, nombre);
    pantalla_.marca_dialogo = solo_admins ? 1 : 0;
    pantalla_.error_dialogo.clear();
  }

  void menu_de_canal(const protocolo::CanalInfo& canal) {
    if (!sala_.dueno() || !ImGui::BeginPopupContextItem("##menu")) {
      return;
    }
    if (ImGui::Selectable(std::string(t(Clave::servidor_menu_cambiar)).c_str())) {
      abrir(Dialogo::cambiar, canal.id, canal.nombre, canal.solo_admins);
    }
    if (canal.id != protocolo::kCanalGeneral &&
        ImGui::Selectable(std::string(t(Clave::servidor_menu_borrar)).c_str())) {
      abrir(Dialogo::borrar, canal.id, canal.nombre, false);
    }
    ImGui::EndPopup();
  }

  std::string perdida(Clave clave) const {
    std::uint64_t perdidos = 0;
    std::uint64_t recibidos = 0;
    for (const Muestra& muestra : pantalla_.grafico) {
      perdidos += muestra.perdidos;
      recibidos += muestra.recibidos;
    }
    const std::uint64_t por_mil = perdidos + recibidos == 0 ? 0 : perdidos * 1000 / (perdidos + recibidos);
    return con_datos(t(clave), {std::to_string(por_mil / 10), std::to_string(por_mil % 10)});
  }

  std::string personas(std::uint32_t canal) const {
    return std::to_string(std::count_if(sala_.usuarios().begin(), sala_.usuarios().end(),
                                        [canal](const auto& par) { return par.second.canal == canal; }));
  }

  void barra() {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const float alto = px(medida::alto_barra);
    dibujo.AddRectFilled(ImVec2(0, 0), ImVec2(ancho_, alto), tono(color::panel));
    dibujo.AddRectFilled(ImVec2(0, alto), ImVec2(ancho_, alto + 1), tono(color::borde_oscuro));
    triangulos(dibujo, ImVec2(0, 0), ImVec2(ancho_, alto));
    emblema(ImVec2(px(12), px(8)), px(42), pantalla_.logo);
    const ImVec2 nombre = medir(pantalla_.direccion, Letra::titulo, medida::letra_servidor);
    escribir(ImVec2(px(63), px(8)), pantalla_.direccion, color::texto, Letra::titulo, medida::letra_servidor);
    const std::string_view pronto = t(Clave::servidor_chip_pronto);
    ImGui::SetCursorScreenPos(ImVec2(px(63), px(35)));
    chip(ImGui::GetCursorScreenPos(), pronto);
    ImGui::InvisibleButton("##frase", ImVec2(ancho_chip(pronto), px(16)));
    cartel(t(Clave::servidor_frase_pronto));

    const std::string_view transmite = t(pantalla_.transmitiendo ? Clave::servidor_transmitiendo
                                                                   : Clave::servidor_sin_transmitir);
    const float ancho_etiqueta = std::max(ancho_transmitiendo(t(Clave::servidor_transmitiendo)),
                                          ancho_transmitiendo(t(Clave::servidor_sin_transmitir)));
    float x = ancho_ - px(6) - ancho_etiqueta;
    etiqueta_transmitiendo(ImVec2(x, (alto - px(medida::alto_transmitiendo)) / 2), ancho_etiqueta, transmite,
                           pantalla_.transmitiendo);
    const std::string tecla_elegida = nombre_tecla(pantalla_.idioma, pantalla_.tecla);
    x -= px(9) + ancho_tecla(tecla_elegida);
    tecla(ImVec2(x, (alto - px(medida::alto_tecla)) / 2), tecla_elegida, color::pozo);
    const std::string_view hablar_con = t(Clave::servidor_hablar_con);
    x -= px(9) + medir(hablar_con, Letra::normal, medida::letra).x;
    escribir_en(x, alto / 2, hablar_con, color::apagado);
    const std::string_view rendimiento = t(Clave::inicio_modo_rendimiento);
    const std::string_view visual = t(Clave::inicio_modo_visual);
    x -= px(10) + ancho_selector_modo(rendimiento, visual);
    ImGui::SetCursorScreenPos(ImVec2(x, (alto - px(44)) / 2));
    selector_modo(rendimiento, visual, llega_en("10"));

    struct Boton {
      Clave clave;
      Icono icono;
      const char* etapa;
      const char* id;
    };
    std::vector<Boton> botones = {{Clave::servidor_microfono, Icono::microfono, "3", "##microfono"},
                                  {Clave::servidor_audio, Icono::auriculares, "3", "##audio"},
                                  {Clave::servidor_servidores, Icono::servidor, "6", "##servidores"},
                                  {Clave::servidor_perfil, Icono::persona, "10", "##perfil"}};
    if (sala_.dueno()) {
      botones.push_back({Clave::servidor_personalizar, Icono::lapiz, "10", "##personalizar"});
    }
    botones.push_back({Clave::inicio_ajustes, Icono::ajustes, "3", "##ajustes"});
    botones.push_back({Clave::inicio_desconectar, Icono::encendido, nullptr, "##desconectar"});
    float desde = px(63) + std::max(nombre.x, ancho_chip(pronto)) + px(12);
    float necesario = 0;
    for (const Boton& cual : botones) {
      necesario += ancho_boton(t(cual.clave), Estilo::biselado, cual.icono) + px(4);
    }
    const bool solo_iconos = desde + necesario > x - px(8);
    for (const Boton& cual : botones) {
      const std::string_view etiqueta = solo_iconos ? std::string_view() : t(cual.clave);
      const float ancho = solo_iconos ? px(medida::alto_biselado) : ancho_boton(etiqueta, Estilo::biselado, cual.icono);
      const ImVec2 tamano(ancho, px(medida::alto_biselado));
      ImGui::SetCursorScreenPos(ImVec2(desde, (alto - tamano.y) / 2));
      if (cual.etapa == nullptr) {
        pedido_.desconectar = boton(cual.id, etiqueta, Estilo::biselado, tamano, cual.icono);
        if (solo_iconos) {
          cartel(t(cual.clave));
        }
      } else {
        apagado(solo_iconos ? con_datos(t(Clave::servidor_boton_y_etapa), {t(cual.clave), llega_en(cual.etapa)})
                            : llega_en(cual.etapa),
                [&] { boton(cual.id, etiqueta, Estilo::biselado, tamano, cual.icono); });
      }
      desde += ancho + px(4);
    }
  }

  void lateral() {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const float arriba = px(medida::alto_barra) + 1;
    const float abajo = alto_ - px(medida::alto_estado);
    const float derecha = px(medida::ancho_lateral);
    dibujo.AddRectFilled(ImVec2(0, arriba), ImVec2(derecha, abajo), tono(color::panel));
    dibujo.AddRectFilled(ImVec2(derecha - 1, arriba), ImVec2(derecha, abajo), tono(color::borde_oscuro));
    triangulos(dibujo, ImVec2(0, arriba), ImVec2(derecha - 1, abajo));
    escribir_en(px(12), arriba + px(24), t(Clave::servidor_canales), color::texto, Letra::titulo, medida::letra_bloque);
    const std::string conectados =
        con_datos(t(Clave::servidor_conectados), {std::to_string(sala_.usuarios().size())});
    escribir_en(derecha - px(12) - medir(conectados, Letra::normal, medida::letra_chica).x, arriba + px(25), conectados,
                color::apagado, Letra::normal, medida::letra_chica);
    ImGui::SetCursorScreenPos(ImVec2(px(10), arriba + px(46)));
    apagado(t(Clave::servidor_pronto), [&] {
      campo("##buscar", t(Clave::servidor_buscar_usuario), pantalla_.buscar, ImVec2(derecha - px(21), px(34)),
            Icono::lupa, 0, color::pozo);
    });
    const float lista = arriba + px(90);
    const float audio = abajo - px(9 + kAltoTuAudio);
    ImGui::SetCursorScreenPos(ImVec2(0, lista));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, px(medida::barra_lista));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0);
    for (const auto& [cual, valor] : {std::make_pair(ImGuiCol_ScrollbarBg, color::pozo),
                                      std::make_pair(ImGuiCol_ScrollbarGrab, color::borde_contorneado),
                                      std::make_pair(ImGuiCol_ScrollbarGrabHovered, color::apagado),
                                      std::make_pair(ImGuiCol_ScrollbarGrabActive, color::apagado)}) {
      ImGui::PushStyleColor(cual, valor);
    }
    ImGui::BeginChild("##canales", ImVec2(derecha - px(4), audio - px(2) - lista));
    canales();
    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    tu_audio(ImVec2(px(10), audio), ImVec2(derecha - px(10), abajo - px(9)));
  }

  bool visible(float y, float alto) const {
    const float arriba = ImGui::GetWindowPos().y;
    return y + alto >= arriba && y <= arriba + ImGui::GetWindowHeight();
  }

  void canales() {
    const ImVec2 origen = ImGui::GetCursorScreenPos();
    const float ancho = ImGui::GetContentRegionAvail().x;
    float y = grupo(origen.x, origen.y, ancho, t(Clave::servidor_canales_texto), "##mas_texto", Dialogo::crear_texto);
    for (const auto& canal : sala_.canales()) {
      if (canal.tipo == TipoCanal::texto) {
        y = fila_texto(origen.x, y, ancho, canal);
      }
    }
    y = grupo(origen.x, y + px(7), ancho, t(Clave::servidor_canales_voz), "##mas_voz", Dialogo::crear_voz);
    for (const auto& canal : sala_.canales()) {
      if (canal.tipo != TipoCanal::voz) {
        continue;
      }
      y = fila_voz(origen.x, y, ancho, canal);
      for (const auto& par : sala_.usuarios()) {
        if (par.second.canal == canal.id && pantalla_.plegados.count(canal.id) == 0) {
          y = fila_persona(origen.x, y, ancho, par.second);
        }
      }
    }
    ImGui::SetCursorScreenPos(ImVec2(origen.x, y));
    ImGui::Dummy(ImVec2(1, px(4)));
  }

  float grupo(float x, float y, float ancho, std::string_view etiqueta, const char* id, Dialogo crear) {
    const float alto = px(kAltoGrupo);
    const float medio = y + px(15);
    if (visible(y, alto)) {
      ImDrawList& dibujo = *ImGui::GetWindowDrawList();
      escribir_espaciado(ImVec2(x + px(18), medio - medir(etiqueta, Letra::negrita, medida::letra_mini).y / 2),
                         etiqueta, color::apagado, Letra::negrita, medida::letra_mini, px(1.2f));
      if (sala_.dueno()) {
        const ImVec2 desde(x + ancho - px(11) - px(medida::boton_mas), medio - px(medida::boton_mas) / 2);
        const ImVec2 hasta(desde.x + px(medida::boton_mas), desde.y + px(medida::boton_mas));
        ImGui::SetCursorScreenPos(desde);
        if (ImGui::InvisibleButton(id, ImVec2(hasta.x - desde.x, hasta.y - desde.y), ImGuiButtonFlags_EnableNav)) {
          abrir(crear, 0, "", false);
        }
        const bool encima = ImGui::IsItemHovered();
        punteado(dibujo, desde, hasta, tono(encima ? color::texto : color::apagado));
        dibujar_icono(dibujo, Icono::mas, ImVec2((desde.x + hasta.x) / 2, (desde.y + hasta.y) / 2), px(9),
                      tono(encima ? color::texto : color::texto2));
        cartel(t(crear == Dialogo::crear_texto ? Clave::servidor_nuevo_texto : Clave::servidor_nuevo_voz));
      }
    }
    return y + alto;
  }

  float fila_texto(float x, float y, float ancho, const protocolo::CanalInfo& canal) {
    const float alto = px(medida::fila_texto);
    if (!visible(y, alto)) {
      return y + alto;
    }
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const ImVec2 desde(x + px(10), y);
    const ImVec2 hasta(x + ancho - px(7), y + alto);
    ImGui::SetCursorScreenPos(desde);
    ImGui::PushID(static_cast<int>(canal.id));
    if (ImGui::InvisibleButton("##texto", ImVec2(hasta.x - desde.x, alto), ImGuiButtonFlags_EnableNav)) {
      agregar(sala_.elegir_texto(canal.id));
      pantalla_.enfocar_mensaje = true;
    }
    const bool encima = ImGui::IsItemHovered();
    menu_de_canal(canal);
    ImGui::PopID();
    const bool elegido = sala_.mirando() == canal.id;
    if (elegido || encima) {
      dibujo.AddRectFilled(desde, hasta, tono(color::seleccion));
    }
    if (elegido) {
      dibujo.AddRectFilled(desde, ImVec2(desde.x + px(3), hasta.y), tono(color::claro));
    }
    const float medio = y + alto / 2;
    dibujar_icono(dibujo, Icono::numeral, ImVec2(x + px(25), medio), px(11), tono(color::apagado));
    const std::uint32_t nuevos = sala_.nuevos(canal.id);
    float derecha = hasta.x - px(8);
    if (nuevos > 0) {
      derecha -= contador_nuevos(ImVec2(derecha, medio), nuevos) + px(9);
    }
    if (canal.solo_admins) {
      dibujar_icono(dibujo, Icono::candado, ImVec2(derecha - px(6), medio), px(12), tono(color::apagado));
      derecha -= px(20);
    }
    escribir_en(x + px(40), medio, canal.nombre, nuevos > 0 || elegido ? color::texto : color::texto2,
                nuevos > 0 ? Letra::fuerte : Letra::normal, medida::letra, derecha - x - px(40));
    return y + alto;
  }

  float fila_voz(float x, float y, float ancho, const protocolo::CanalInfo& canal) {
    const float alto = px(medida::fila_voz);
    if (!visible(y, alto)) {
      return y + alto;
    }
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const ImVec2 desde(x + px(10), y);
    const ImVec2 hasta(x + ancho - px(7), y + alto);
    const bool plegado = pantalla_.plegados.count(canal.id) != 0;
    const float flecha = px(26);
    ImGui::PushID(static_cast<int>(canal.id));
    ImGui::SetCursorScreenPos(desde);
    if (ImGui::InvisibleButton("##plegar", ImVec2(flecha, alto), ImGuiButtonFlags_EnableNav) &&
        pantalla_.plegados.erase(canal.id) == 0) {
      pantalla_.plegados.insert(canal.id);
    }
    bool encima = ImGui::IsItemHovered();
    ImGui::SetCursorScreenPos(ImVec2(desde.x + flecha, desde.y));
    if (ImGui::InvisibleButton("##voz", ImVec2(hasta.x - desde.x - flecha, alto), ImGuiButtonFlags_EnableNav)) {
      agregar(sala_.elegir_voz(canal.id));
    }
    encima = encima || ImGui::IsItemHovered();
    menu_de_canal(canal);
    ImGui::PopID();
    const bool aca = canal.id == sala_.tu_canal();
    if (aca) {
      dibujo.AddRectFilled(desde, hasta, tono(color::elevado));
      dibujo.AddRectFilled(desde, ImVec2(desde.x + px(3), hasta.y), tono(color::claro));
    } else if (encima) {
      dibujo.AddRectFilled(desde, hasta, tono(color::seleccion));
    }
    const float medio = y + alto / 2;
    dibujar_icono(dibujo, plegado ? Icono::flecha_derecha : Icono::flecha_abajo, ImVec2(x + px(24), medio), px(11),
                  tono(color::texto2));
    const std::string cuantos = personas(canal.id);
    float derecha = hasta.x - px(8) - medir(cuantos, Letra::normal, medida::letra).x;
    escribir_en(derecha, medio, cuantos, color::texto2);
    if (aca) {
      const std::string_view estas = t(Clave::servidor_estas_aca);
      derecha -= px(7) + medir(estas, Letra::normal, medida::letra).x;
      escribir_en(derecha, medio, estas, color::texto2);
    }
    escribir_en(x + px(40), medio, canal.nombre, color::texto, Letra::fuerte, medida::letra, derecha - x - px(48));
    return y + alto;
  }

  float fila_persona(float x, float y, float ancho, const protocolo::UsuarioInfo& usuario) const {
    const float alto = px(medida::fila_voz);
    if (!visible(y, alto)) {
      return y + alto;
    }
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const bool yo = usuario.id == sala_.tu_id();
    const bool habla = yo ? pantalla_.transmitiendo : pantalla_.hablando == usuario.id;
    const ImU32 color_voz = yo ? color::naranja : color::celeste;
    const float medio = y + alto / 2;
    avatar(ImVec2(x + px(38), medio - px(medida::avatar_lista) / 2), px(medida::avatar_lista),
           iniciales(usuario.nombre), color_de_avatar(usuario.nombre), color::texto, medida::letra_mini,
           habla ? color_voz : 0);
    const float derecha = x + ancho - px(15);
    if (habla) {
      dibujar_icono(dibujo, Icono::voz, ImVec2(derecha - px(8), medio), px(14), tono(color_voz));
    }
    const std::string_view nombre = yo ? t(Clave::servidor_vos) : std::string_view(usuario.nombre);
    escribir_en(x + px(74), medio, nombre, color::texto2, Letra::normal, medida::letra, derecha - x - px(94));
    return y + alto;
  }

  void deslizador(const char* id, ImVec2 desde, float ancho, float valor) const {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(ImVec2(desde.x, desde.y - px(8)));
    const float corte = desde.x + ancho * valor;
    apagado(llega_en("3"), [&] {
      ImGui::InvisibleButton(id, ImVec2(ancho, px(16)));
      dibujo.AddRectFilled(ImVec2(desde.x, desde.y - px(3)), ImVec2(corte, desde.y + px(3)), tono(color::claro), px(3));
      dibujo.AddRectFilled(ImVec2(corte, desde.y - px(3)), ImVec2(desde.x + ancho, desde.y + px(3)),
                           tono(color::linea), px(3));
      dibujo.AddCircleFilled(ImVec2(corte, desde.y), px(7), tono(color::claro));
    });
  }

  void tu_audio(ImVec2 desde, ImVec2 hasta) {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    dibujo.AddRectFilled(desde, hasta, tono(color::pozo));
    triangulos(dibujo, desde, hasta);
    const float x = desde.x + px(13);
    const float derecha = hasta.x - px(14);
    escribir_en(x, desde.y + px(22), t(Clave::servidor_tu_audio), color::texto, Letra::negrita);
    constexpr int kSegmentos = 10;
    medidor(ImVec2(derecha - px(kSegmentos * (medida::segmento + medida::entre_segmentos) - medida::entre_segmentos),
                   desde.y + px(17)),
            kSegmentos, pantalla_.transmitiendo ? pantalla_.nivel : 0, color::naranja);
    escribir_en(x, desde.y + px(48), t(Clave::servidor_como_transmitis), color::texto2);
    ImGui::SetCursorScreenPos(ImVec2(x, desde.y + px(64)));
    selector_opciones("##como", {t(Clave::servidor_tecla_para_hablar), t(Clave::servidor_por_voz)}, 0,
                      ImVec2(derecha - x, px(38)), 1, llega_en("3"));
    const float fila = desde.y + px(126);
    escribir_en(x, fila, t(Clave::servidor_tecla), color::texto2);
    if (pantalla_.eligiendo_tecla) {
      escribir_en(x + px(38), fila, t(Clave::servidor_apreta_tecla), color::celeste);
    } else {
      tecla(ImVec2(x + px(38), fila - px(medida::alto_tecla) / 2), nombre_tecla(pantalla_.idioma, pantalla_.tecla),
            color::elevado);
      const std::string_view cambiar = t(Clave::servidor_cambiar_tecla);
      const float ancho = ancho_boton(cambiar, Estilo::contorneado);
      ImGui::SetCursorScreenPos(ImVec2(derecha - ancho, fila - px(16)));
      if (boton("##cambiar_tecla", cambiar, Estilo::contorneado, ImVec2(ancho, px(32)))) {
        pedido_.elegir_tecla = true;
      }
    }
    escribir_en(x, desde.y + px(159), t(Clave::servidor_volumen_microfono), color::texto2);
    deslizador("##volumen_microfono", ImVec2(x, desde.y + px(178)), derecha - x, 0.7f);
    escribir_en(x, desde.y + px(202), t(Clave::servidor_volumen_salida), color::texto2);
    deslizador("##volumen_salida", ImVec2(x, desde.y + px(221)), derecha - x, 0.85f);
  }

  void chat() {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const float izquierda = px(medida::ancho_lateral);
    const float arriba = px(medida::alto_barra) + 1;
    const float abajo = alto_ - px(medida::alto_estado);
    dibujo.AddRectFilled(ImVec2(izquierda, arriba), ImVec2(ancho_, abajo), tono(color::fondo));
    const float lado = px(medida::triangulo);
    dibujo.AddTriangleFilled(ImVec2(izquierda, arriba), ImVec2(izquierda + lado, arriba),
                             ImVec2(izquierda, arriba + lado), tono(color::triangulo));
    cabecera(ImVec2(izquierda + px(6), arriba + px(6)), ImVec2(ancho_ - px(6), arriba + px(kAltoCabecera) - px(6)));
    dibujo.AddRectFilled(ImVec2(izquierda, arriba + px(kAltoCabecera)), ImVec2(ancho_, arriba + px(kAltoCabecera) + 1),
                         tono(color::campo_chat));
    mensajes(ImVec2(izquierda, arriba + px(kAltoCabecera) + 1), ImVec2(ancho_, abajo - px(kAltoAbajo)));
    escribir_mensaje(izquierda, abajo - px(kAltoAbajo) + px(14));
  }

  void cabecera(ImVec2 desde, ImVec2 hasta) {
    marco_cabecera(desde, hasta);
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const auto* canal = sala_.canal(sala_.chat().canal);
    if (canal == nullptr) {
      return;
    }
    const bool es_texto = canal->tipo == TipoCanal::texto;
    const float medio = (desde.y + hasta.y) / 2;
    float x = desde.x + px(12);
    if (es_texto) {
      dibujar_icono(dibujo, Icono::numeral, ImVec2(x + px(5), medio), px(12), tono(color::apagado));
      x += px(19);
    }
    x = escribir_en(x, medio, canal->nombre, color::texto, Letra::fuerte, medida::letra_bloque) + px(9);
    std::string subtitulo;
    if (es_texto) {
      const auto* voz = sala_.canal(sala_.tu_canal());
      subtitulo = con_datos(t(Clave::servidor_canal_texto), {voz != nullptr ? voz->nombre : std::string()});
    } else {
      const std::string cuantos = personas(canal->id);
      subtitulo = cuantos == "1" ? std::string(t(Clave::servidor_chat_voz_una))
                                 : con_datos(t(Clave::servidor_chat_voz), {cuantos});
    }
    const float boton_panel = hasta.x - px(40);
    const float fin_subtitulo = es_texto ? boton_panel : boton_panel - px(220);
    escribir_en(x, medio, subtitulo, color::apagado, Letra::normal, medida::letra, fin_subtitulo - x - px(8));
    ImGui::SetCursorScreenPos(ImVec2(boton_panel, medio - px(14)));
    apagado(t(Clave::servidor_pronto), [&] {
      ImGui::InvisibleButton("##panel", ImVec2(px(28), px(28)));
      dibujar_icono(dibujo, Icono::panel, ImVec2(boton_panel + px(14), medio), px(15), tono(color::texto2));
    });
    if (!es_texto) {
      compartir(ImVec2(boton_panel - px(8), medio));
    }
  }

  void compartir(ImVec2 derecha) {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const std::string_view etiqueta = t(Clave::servidor_compartir);
    const std::string_view pronto = t(Clave::servidor_chip_pronto);
    const float ancho = px(16 + 15 + 8) + medir(etiqueta, Letra::normal, medida::letra).x + px(8) + ancho_chip(pronto) +
                        px(10);
    const ImVec2 desde(derecha.x - ancho, derecha.y - px(14));
    const ImVec2 hasta(derecha.x, derecha.y + px(14));
    ImGui::SetCursorScreenPos(desde);
    apagado(llega_en("12"), [&] {
      ImGui::InvisibleButton("##compartir", ImVec2(ancho, hasta.y - desde.y));
      dibujo.AddRectFilled(desde, hasta, tono(color::chip), px(4));
      dibujar_icono(dibujo, Icono::pantalla, ImVec2(desde.x + px(16 + 7), derecha.y), px(15), tono(color::texto2));
      const float x = escribir_en(desde.x + px(16 + 15 + 8), derecha.y, etiqueta, color::texto2) + px(8);
      chip(ImVec2(x, derecha.y - medir(pronto, Letra::negrita, medida::letra_mini).y / 2 - px(2)), pronto);
    });
  }

  std::string dia(std::uint64_t hora_ms) const {
    const std::uint32_t cual = cliente::dia_local(hora_ms);
    return cual == hoy_    ? std::string(t(Clave::servidor_hoy))
           : cual == ayer_ ? std::string(t(Clave::servidor_ayer))
                           : cliente::fecha_local(hora_ms);
  }

  void mensajes(ImVec2 desde, ImVec2 hasta) {
    const cliente::Chat& chat = sala_.chat();
    ImGui::SetCursorScreenPos(desde);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, px(medida::barra_lista));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, color::borde_contorneado);
    ImGui::BeginChild("##mensajes", ImVec2(hasta.x - desde.x, hasta.y - desde.y));
    const float ancho_texto = ImGui::GetContentRegionAvail().x - px(kMargenChat + kSangria + 24);
    const bool otro_chat = pantalla_.alturas.canal != chat.canal;
    bool abajo = otro_chat || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - px(2);
    const float agregado = actualizar(pantalla_.alturas, chat, ancho_texto);
    float scroll = ImGui::GetScrollY();
    if (agregado > 0 && !abajo) {
      scroll += agregado;
      ImGui::SetScrollY(scroll);
    }
    const Alturas& alturas = pantalla_.alturas;
    const float vista = ImGui::GetWindowHeight();
    const float total = px(kArribaMensajes) + alturas.desde.back() + px(8);
    const float corrimiento = std::max(0.0f, vista - total);
    const float base = ImGui::GetWindowPos().y - scroll + corrimiento + px(kArribaMensajes);
    const float x = ImGui::GetWindowPos().x + px(kMargenChat);
    if (chat.pidiendo) {
      escribir(ImVec2(x, base - px(kArribaMensajes) + px(6)), t(Clave::servidor_cargando), color::apagado,
               Letra::normal, medida::letra_chica);
    }
    const float tope = ImGui::GetWindowPos().y + vista;
    auto primero = std::upper_bound(alturas.desde.begin(), alturas.desde.end(), ImGui::GetWindowPos().y - base);
    std::size_t i = primero == alturas.desde.begin() ? 0 : static_cast<std::size_t>(primero - alturas.desde.begin()) - 1;
    for (; i < chat.mensajes.size() && base + alturas.desde[i] < tope; ++i) {
      mensaje(chat.mensajes[i], alturas.nuevo_dia[i], x, base + alturas.desde[i], ancho_texto);
    }
    ImGui::SetCursorPos(ImVec2(0, corrimiento + total - 1));
    ImGui::Dummy(ImVec2(1, 1));
    if (abajo) {
      ImGui::SetScrollHereY(1.0f);
    }
    if ((corrimiento > 0 || !abajo) && scroll <= 0 && chat.hay_mas) {
      if (auto orden = sala_.pedir_mas_viejos()) {
        pedido_.ordenes.push_back(std::move(*orden));
      }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
  }

  void mensaje(const protocolo::MensajeGuardado& guardado, bool nuevo_dia, float x, float y, float ancho_texto) const {
    if (nuevo_dia) {
      escribir(ImVec2(x, y + px(8)), dia(guardado.hora_ms), color::apagado, Letra::normal, medida::letra_chica);
      y += px(kAltoDia);
    }
    avatar(ImVec2(x, y), px(medida::avatar_chat), iniciales(guardado.autor), color_de_avatar(guardado.autor),
           color::texto, medida::letra_mini);
    const float texto_x = x + px(kSangria);
    const std::string_view autor =
        guardado.autor == mi_nombre_ ? t(Clave::servidor_vos) : std::string_view(guardado.autor);
    const float fin = escribir_en(texto_x, y + px(9), autor, color::texto, Letra::fuerte);
    escribir_en(fin + px(8), y + px(9), cliente::hora_local(guardado.hora_ms), color::apagado);
    escribir_envuelto(ImVec2(texto_x, y + px(20)), guardado.texto, color::texto, ancho_texto);
  }

  void escribir_mensaje(float izquierda, float y) {
    const auto* canal = sala_.canal(sala_.chat().canal);
    const std::string nombre = canal != nullptr ? canal->nombre : std::string();
    const bool puede = sala_.puede_escribir();
    const float alto = px(medida::alto_campo_chat);
    ImGui::SetCursorScreenPos(ImVec2(izquierda + px(14), y));
    apagado(llega_en("5"), [&] { boton("##clip", "", Estilo::contorneado, ImVec2(px(38), alto), Icono::clip); });
    const float enviar_ancho = px(44);
    const float campo_desde = izquierda + px(60);
    const float campo_hasta = ancho_ - px(14) - enviar_ancho - px(8);
    const std::string pista =
        !puede ? con_datos(t(Clave::servidor_solo_admins), {nombre})
        : canal != nullptr && canal->tipo == TipoCanal::texto ? con_datos(t(Clave::servidor_escribi_texto), {nombre})
                                                               : con_datos(t(Clave::servidor_escribi_voz), {nombre});
    ImGui::BeginDisabled(!puede);
    ImGui::SetCursorScreenPos(ImVec2(campo_desde, y));
    if (pantalla_.enfocar_mensaje && puede) {
      ImGui::SetKeyboardFocusHere();
      pantalla_.enfocar_mensaje = false;
    }
    bool enviar = campo("##mensaje", pista, pantalla_.mensaje, ImVec2(campo_hasta - campo_desde, alto),
                        Icono::ninguno, ImGuiInputTextFlags_EnterReturnsTrue, color::campo_chat);
    ImGui::SetCursorScreenPos(ImVec2(ancho_ - px(14) - enviar_ancho, y));
    enviar = boton("##enviar", "", Estilo::principal, ImVec2(enviar_ancho, alto + 1), Icono::enviar) || enviar;
    ImGui::EndDisabled();
    if (enviar && puede) {
      if (auto orden = sala_.enviar(std::string(pantalla_.mensaje.data()))) {
        pedido_.ordenes.push_back(std::move(*orden));
        std::fill(pantalla_.mensaje.begin(), pantalla_.mensaje.end(), '\0');
      }
      pantalla_.enfocar_mensaje = true;
    }
    const float nota = y + alto + px(15);
    if (sala_.negado()) {
      escribir_en(izquierda + px(14), nota, cliente::explicar(pantalla_.idioma, sala_.negado()->motivo), color::rojo,
                  Letra::normal, medida::letra_chica);
      return;
    }
    const std::string_view pronto = t(Clave::servidor_chip_pronto);
    chip(ImVec2(izquierda + px(14), nota - medir(pronto, Letra::negrita, medida::letra_mini).y / 2 - px(2)), pronto);
    const float x = izquierda + px(22) + ancho_chip(pronto);
    escribir_en(x, nota, t(Clave::servidor_nota_gifs), color::apagado, Letra::normal, medida::letra_chica,
                ancho_ - px(14) - x);
  }

  void estado() {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const float arriba = alto_ - px(medida::alto_estado);
    dibujo.AddRectFilled(ImVec2(0, arriba), ImVec2(ancho_, alto_), tono(color::panel));
    dibujo.AddRectFilled(ImVec2(0, arriba), ImVec2(ancho_, arriba + 1), tono(color::borde_oscuro));
    const float medio = arriba + px(medida::alto_estado) / 2;
    dibujo.AddRectFilled(ImVec2(px(12), medio - px(4.5f)), ImVec2(px(21), medio + px(4.5f)), tono(color::verde));
    float x = escribir_en(px(27), medio, t(Clave::servidor_conectado), color::texto) + px(24);
    x = escribir_en(x, medio, pantalla_.direccion, color::texto2) + px(24);
    const std::string ping = sala_.ping() ? con_datos(t(Clave::servidor_ping), {std::to_string(*sala_.ping())})
                                          : std::string(t(Clave::servidor_sin_ping));
    x = escribir_en(x, medio, ping, color::texto2) + px(8);
    grafico(ImVec2(x, medio - px(7)));
    ImGui::SetCursorScreenPos(ImVec2(x - px(4), arriba));
    if (ImGui::InvisibleButton("##grafico", ImVec2(px(72), px(medida::alto_estado)), ImGuiButtonFlags_EnableNav)) {
      ImGui::OpenPopup("##diagnostico");
    }
    cartel(t(Clave::servidor_ver_diagnostico));
    diagnostico(ImVec2(x, arriba - px(6)));
    x += px(64 + 8);
    x = escribir_en(x, medio, perdida(Clave::servidor_perdidos), color::apagado) + px(24);
    dibujar_icono(dibujo, Icono::candado, ImVec2(x + px(6), medio), px(13), tono(color::texto2));
    x = escribir_en(x + px(18), medio, t(Clave::servidor_cifrado), color::texto2) + px(24);
    escribir_en(x, medio,
                t(!pantalla_.hay_audio        ? Clave::servidor_sin_audio
                  : pantalla_.calidad_alta    ? Clave::servidor_voz_alta
                                              : Clave::servidor_voz_baja),
                color::texto2);
    const std::string_view huella = t(Clave::servidor_huella_verificada);
    const float huella_x = ancho_ - px(12) - medir(huella, Letra::normal, medida::letra).x;
    escribir_en(huella_x, medio, huella, color::texto2);
    dibujar_icono(dibujo, Icono::verificado, ImVec2(huella_x - px(13), medio), px(14), tono(color::verde));
  }

  void diagnostico(ImVec2 abajo_izquierda) const {
    ImGui::SetNextWindowPos(abajo_izquierda, ImGuiCond_Always, ImVec2(0, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(16), px(12)));
    if (!ImGui::BeginPopup("##diagnostico")) {
      ImGui::PopStyleVar();
      return;
    }
    const DatosDiagnostico& datos = pantalla_.diagnostico;
    {
      const ConLetra letra(Letra::titulo, medida::letra_bloque);
      ImGui::TextUnformatted(std::string(t(Clave::servidor_diag_titulo)).c_str());
    }
    for (const std::string& linea : {
             sala_.ping() ? con_datos(t(Clave::servidor_diag_ping), {std::to_string(*sala_.ping())})
                          : std::string(t(Clave::servidor_sin_ping)),
             datos.voz_lista ? con_datos(t(Clave::servidor_diag_ping_voz), {std::to_string(datos.ping_voz)})
                             : std::string(t(Clave::servidor_diag_ping_voz_sin)),
             con_datos(t(Clave::servidor_diag_procesador), {std::to_string(static_cast<int>(datos.procesador + 0.5))}),
             con_datos(t(Clave::servidor_diag_ram), {std::to_string(datos.ram_bytes / (1024 * 1024))}),
             perdida(Clave::servidor_diag_perdida),
             con_datos(t(Clave::servidor_diag_colchon), {std::to_string(datos.colchon_ms)}),
             con_datos(t(Clave::servidor_diag_recortes), {std::to_string(datos.recortes)}),
             con_datos(t(Clave::servidor_diag_salida), {std::to_string(datos.salida_ms)})}) {
      ImGui::TextUnformatted(linea.c_str());
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar();
  }

  void dialogo() {
    if (pantalla_.dialogo == Dialogo::ninguno) {
      return;
    }
    if (pantalla_.abrir_dialogo) {
      ImGui::OpenPopup("##dialogo");
      pantalla_.abrir_dialogo = false;
    }
    const Dialogo cual = pantalla_.dialogo;
    const auto* canal = sala_.canal(pantalla_.canal_dialogo);
    const bool de_canal = cual == Dialogo::cambiar || cual == Dialogo::borrar;
    const bool con_nombre = cual != Dialogo::borrar;
    const bool con_marca = cual == Dialogo::crear_texto ||
                           (cual == Dialogo::cambiar && canal != nullptr && canal->tipo == TipoCanal::texto);
    const float ancho = px(380);
    const float alto = px(64) + (con_nombre ? px(46) : px(40)) + (con_marca ? px(48) : 0) +
                       (pantalla_.error_dialogo.empty() ? 0 : px(22)) + px(56);
    ImGui::SetNextWindowPos(ImVec2(ancho_ / 2, alto_ / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(ancho, alto));
    if (!ImGui::BeginPopupModal("##dialogo", nullptr,
                                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoScrollbar)) {
      pantalla_.dialogo = Dialogo::ninguno;
      return;
    }
    if (de_canal && canal == nullptr) {
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      pantalla_.dialogo = Dialogo::ninguno;
      return;
    }
    const ImVec2 desde = ImGui::GetWindowPos();
    triangulos(*ImGui::GetWindowDrawList(), desde, ImVec2(desde.x + ancho, desde.y + alto));
    const float x = desde.x + px(20);
    const float interior = ancho - px(40);
    float y = desde.y + px(20);
    const bool es_texto = canal != nullptr && canal->tipo == TipoCanal::texto;
    const std::string nombre_canal = canal == nullptr ? std::string() : canal->nombre;
    const std::string titulo =
        cual == Dialogo::crear_texto ? std::string(t(Clave::servidor_nuevo_texto))
        : cual == Dialogo::crear_voz ? std::string(t(Clave::servidor_nuevo_voz))
        : cual == Dialogo::cambiar
            ? con_datos(t(es_texto ? Clave::servidor_cambiar_canal_texto : Clave::servidor_cambiar_canal), {nombre_canal})
            : con_datos(t(es_texto ? Clave::servidor_borrar_canal_texto : Clave::servidor_borrar_canal), {nombre_canal});
    escribir_cortado(ImVec2(x, y), interior, titulo, color::texto, Letra::titulo, medida::letra_tarjeta);
    y += px(36);
    bool confirmar = false;
    if (con_nombre) {
      if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
      }
      ImGui::SetCursorScreenPos(ImVec2(x, y));
      confirmar = campo("##nombre_canal", t(Clave::servidor_nombre_del_canal), pantalla_.nombre_dialogo,
                        ImVec2(interior, px(36)), Icono::ninguno, ImGuiInputTextFlags_EnterReturnsTrue, color::pozo);
      y += px(46);
    } else {
      escribir_cortado(ImVec2(x, y), interior,
                       t(canal->tipo == TipoCanal::texto ? Clave::servidor_borrar_texto : Clave::servidor_borrar_voz),
                       color::texto2);
      y += px(40);
    }
    if (con_marca) {
      ImGui::SetCursorScreenPos(ImVec2(x, y));
      const int tocada =
          selector_opciones("##marca", {t(Clave::servidor_todos_escriben), t(Clave::servidor_solo_admins_escriben)},
                            pantalla_.marca_dialogo, ImVec2(interior, px(38)), -1, {});
      pantalla_.marca_dialogo = tocada >= 0 ? tocada : pantalla_.marca_dialogo;
      y += px(48);
    }
    if (!pantalla_.error_dialogo.empty()) {
      escribir_cortado(ImVec2(x, y), interior, pantalla_.error_dialogo, color::rojo, Letra::normal, medida::letra_chica);
      y += px(22);
    }
    const std::string_view si = t(cual == Dialogo::borrar    ? Clave::servidor_borrar_si
                                  : cual == Dialogo::cambiar ? Clave::servidor_guardar
                                                             : Clave::servidor_crear);
    const std::string_view no = t(Clave::inicio_cancelar);
    const float ancho_no = ancho_boton(no, Estilo::contorneado);
    const float ancho_si = ancho_boton(si, Estilo::principal);
    ImGui::SetCursorScreenPos(ImVec2(x + interior - ancho_no, y + px(6)));
    const bool cancelar = boton("##no", no, Estilo::contorneado, ImVec2(ancho_no, px(medida::alto_principal))) ||
                          ImGui::IsKeyPressed(ImGuiKey_Escape);
    ImGui::SetCursorScreenPos(ImVec2(x + interior - ancho_no - px(10) - ancho_si, y + px(6)));
    confirmar = boton("##si", si, Estilo::principal, ImVec2(ancho_si, px(medida::alto_principal))) || confirmar;
    if (cancelar || (confirmar && confirmar_dialogo(canal, con_marca))) {
      ImGui::CloseCurrentPopup();
      pantalla_.dialogo = Dialogo::ninguno;
    }
    ImGui::EndPopup();
  }

  bool confirmar_dialogo(const protocolo::CanalInfo* canal, bool con_marca) {
    const std::string nombre(pantalla_.nombre_dialogo.data());
    const bool solo_admins = con_marca && pantalla_.marca_dialogo == 1;
    if (pantalla_.dialogo == Dialogo::borrar) {
      pedido_.ordenes.emplace_back(protocolo::BorrarCanal{canal->id});
    } else if (!es_nombre_valido(nombre)) {
      pantalla_.error_dialogo = t(Clave::conectar_nombre_invalido);
      return false;
    } else if (pantalla_.dialogo == Dialogo::cambiar) {
      pedido_.ordenes.emplace_back(protocolo::CambiarCanal{canal->id, nombre, solo_admins});
    } else {
      pedido_.ordenes.emplace_back(
          protocolo::CrearCanal{nombre, con_marca ? TipoCanal::texto : TipoCanal::voz, solo_admins});
    }
    return true;
  }

  void grafico(ImVec2 desde) const {
    const auto& muestras = pantalla_.grafico;
    if (muestras.empty()) {
      return;
    }
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    std::uint32_t maximo = 100;
    for (const Muestra& muestra : muestras) {
      maximo = std::max(maximo, muestra.ping);
    }
    const float paso = px(64) / static_cast<float>(kMuestrasDelGrafico - 1);
    const float alto = px(14);
    std::vector<ImVec2> puntos;
    for (std::size_t i = 0; i < muestras.size(); ++i) {
      const float x = desde.x + px(64) - static_cast<float>(muestras.size() - 1 - i) * paso;
      puntos.emplace_back(x, desde.y + alto - 1 -
                                 (alto - 2) * static_cast<float>(muestras[i].ping) / static_cast<float>(maximo));
      if (muestras[i].perdidos > 0) {
        dibujo.AddLine(ImVec2(x, desde.y), ImVec2(x, desde.y + alto), tono(color::rojo), px(1));
      }
    }
    dibujo.AddPolyline(puntos.data(), static_cast<int>(puntos.size()), tono(color::verde), 0, px(1));
  }

  PantallaServidor& pantalla_;
  cliente::Sala& sala_;
  float ancho_;
  float alto_;
  std::uint32_t hoy_;
  std::uint32_t ayer_;
  std::string mi_nombre_;
  PedidoServidor pedido_;
};

}

std::string nombre_tecla(Idioma idioma, std::uint8_t tecla) {
  const std::uint8_t real = cliente::tecla_para_hablar(tecla);
  constexpr std::array<std::pair<std::uint8_t, Clave>, 9> kConocidas = {{
      {VK_LMENU, Clave::tecla_alt_izquierda},
      {VK_RMENU, Clave::tecla_alt_derecha},
      {VK_LCONTROL, Clave::tecla_ctrl_izquierda},
      {VK_RCONTROL, Clave::tecla_ctrl_derecha},
      {VK_LSHIFT, Clave::tecla_mayus_izquierda},
      {VK_RSHIFT, Clave::tecla_mayus_derecha},
      {VK_MBUTTON, Clave::tecla_mouse_medio},
      {VK_XBUTTON1, Clave::tecla_mouse_4},
      {VK_XBUTTON2, Clave::tecla_mouse_5},
  }};
  for (const auto& [codigo, clave] : kConocidas) {
    if (codigo == real) {
      return std::string(texto(idioma, clave));
    }
  }
  const bool extendida = (real >= VK_PRIOR && real <= VK_HELP) || (real >= VK_LWIN && real <= VK_APPS) ||
                         real == VK_DIVIDE || real == VK_NUMLOCK;
  const UINT escaneo = MapVirtualKeyW(real, 0);
  std::array<wchar_t, 64> nombre{};
  if (escaneo != 0 &&
      GetKeyNameTextW(static_cast<LONG>(escaneo << 16 | (extendida ? 1u << 24 : 0u)), nombre.data(),
                      static_cast<int>(nombre.size())) > 0) {
    return a_utf8(nombre.data());
  }
  return con_datos(texto(idioma, Clave::tecla_otra), {std::to_string(real)});
}

PedidoServidor dibujar_servidor(PantallaServidor& pantalla, cliente::Sala& sala) {
  return Dibujo(pantalla, sala).dibujar();
}

}
