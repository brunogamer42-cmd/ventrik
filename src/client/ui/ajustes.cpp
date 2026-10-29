#include "client/ui/ajustes.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <windows.h>

#include "client/horas.h"
#include "client/mi_identidad.h"
#include "client/silencios.h"
#include "client/ui/menu_avisos.h"
#include "client/ui/servidor.h"
#include "common/utf.h"

namespace ventrik::ui {
namespace {
using cliente::Ajuste;

constexpr float kAltoBarra = 54;
constexpr float kAnchoMenu = 250;
constexpr float kAltoItem = 37;
constexpr float kMargen = 28;
constexpr float kRelleno = 18;
constexpr float kAltoDesplegable = 30;
constexpr int kSegmentos = 20;
constexpr Clave kSinGrupo = Clave::titulo_ventana;

struct Item {
  Seccion seccion;
  Clave nombre;
  Clave grupo;
  int etapa;
};

constexpr std::array<Item, kSecciones> kMenu = {{{Seccion::general, Clave::ajustes_general, Clave::ajustes_grupo_programa, 0},
                                                 {Seccion::apariencia, Clave::ajustes_apariencia, kSinGrupo, 10},
                                                 {Seccion::teclas, Clave::ajustes_teclas, kSinGrupo, 0},
                                                 {Seccion::audio, Clave::ajustes_audio, Clave::ajustes_grupo_audio, 0},
                                                 {Seccion::avisos, Clave::ajustes_avisos, kSinGrupo, 0},
                                                 {Seccion::red, Clave::ajustes_red, Clave::ajustes_grupo_conexion, 6},
                                                 {Seccion::descargas, Clave::ajustes_descargas, kSinGrupo, 5},
                                                 {Seccion::prestar, Clave::ajustes_prestar, kSinGrupo, 13},
                                                 {Seccion::seguridad, Clave::ajustes_seguridad, Clave::ajustes_grupo_privacidad, 0},
                                                 {Seccion::amigos, Clave::ajustes_amigos, kSinGrupo, 14},
                                                 {Seccion::copias, Clave::ajustes_copias, kSinGrupo, 14},
                                                 {Seccion::acerca, Clave::ajustes_acerca, kSinGrupo, 11}}};

constexpr std::array<std::pair<Seccion, Clave>, 37> kBuscables = {{
    {Seccion::general, Clave::ajustes_iniciar_con_windows},
    {Seccion::general, Clave::ajustes_entrar_al_ultimo},
    {Seccion::general, Clave::ajustes_al_cerrar},
    {Seccion::general, Clave::ajustes_al_minimizar},
    {Seccion::general, Clave::ajustes_idioma},
    {Seccion::general, Clave::ajustes_al_entrar},
    {Seccion::teclas, Clave::ajustes_tecla_hablar},
    {Seccion::teclas, Clave::ajustes_agregar_susurro},
    {Seccion::teclas, Clave::ajustes_tecla_microfono},
    {Seccion::teclas, Clave::ajustes_tecla_audio},
    {Seccion::teclas, Clave::ajustes_teclas_sin_foco},
    {Seccion::audio, Clave::ajustes_microfono},
    {Seccion::audio, Clave::ajustes_salida},
    {Seccion::audio, Clave::ajustes_probar_salida},
    {Seccion::audio, Clave::ajustes_voces},
    {Seccion::audio, Clave::ajustes_sonidos_aviso},
    {Seccion::audio, Clave::ajustes_por_voz},
    {Seccion::audio, Clave::ajustes_probar_microfono},
    {Seccion::audio, Clave::ajustes_escucharme},
    {Seccion::audio, Clave::ajustes_seguir_al_soltar},
    {Seccion::audio, Clave::ajustes_quitar_ruido},
    {Seccion::audio, Clave::ajustes_eco},
    {Seccion::audio, Clave::ajustes_volumen_automatico},
    {Seccion::audio, Clave::ajustes_bajar_otros},
    {Seccion::avisos, Clave::ajustes_volumen_avisos},
    {Seccion::avisos, Clave::ajustes_sonar_por},
    {Seccion::avisos, Clave::ajustes_no_molestar},
    {Seccion::avisos, Clave::ajustes_paquete},
    {Seccion::avisos, Clave::ajustes_avisos_texto},
    {Seccion::avisos, Clave::ajustes_avisos_voz},
    {Seccion::avisos, Clave::ajustes_avisos_conexion},
    {Seccion::avisos, Clave::ajustes_avisos_privados},
    {Seccion::avisos, Clave::ajustes_silenciados},
    {Seccion::seguridad, Clave::maestra},
    {Seccion::seguridad, Clave::bloqueo},
    {Seccion::seguridad, Clave::seguridad_copia},
    {Seccion::seguridad, Clave::seguridad_exportar}}};

struct Grupo {
  std::size_t primero;
  std::size_t cuantos;
  int etapa;
};

constexpr std::array<Grupo, 4> kGruposDeSonidos = {{{0, 14, 0}, {14, 4, 0}, {18, 5, 14}, {23, 4, 0}}};
constexpr std::array<std::size_t, 15> kSuenanYa = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 14, 15, 26};

struct ItemDeSonido {
  Clave nombre;
  Clave nota;
};

constexpr std::array<ItemDeSonido, cliente::kSonidos> kItemsDeSonido = {{
    {Clave::sonido_entrar_canal, kSinGrupo},
    {Clave::sonido_salir_canal, kSinGrupo},
    {Clave::sonido_alguien_entra, kSinGrupo},
    {Clave::sonido_alguien_sale, kSinGrupo},
    {Clave::sonido_mic_silenciado, kSinGrupo},
    {Clave::sonido_mic_activado, kSinGrupo},
    {Clave::sonido_audio_silenciado, kSinGrupo},
    {Clave::sonido_audio_activado, kSinGrupo},
    {Clave::sonido_transmitir_empieza, kSinGrupo},
    {Clave::sonido_transmitir_termina, kSinGrupo},
    {Clave::sonido_te_movieron, kSinGrupo},
    {Clave::sonido_susurro, Clave::sonido_susurro_nota},
    {Clave::sonido_conexion_perdida, kSinGrupo},
    {Clave::sonido_reconectado, kSinGrupo},
    {Clave::sonido_mensaje, Clave::sonido_mensaje_nota},
    {Clave::sonido_mencion, kSinGrupo},
    {Clave::sonido_mensaje_amigo, kSinGrupo},
    {Clave::sonido_archivo_listo, kSinGrupo},
    {Clave::sonido_llamada_entrante, Clave::sonido_llamada_nota},
    {Clave::sonido_llamando, Clave::sonido_llamando_nota},
    {Clave::sonido_llamada_termina, kSinGrupo},
    {Clave::sonido_solicitud_amistad, kSinGrupo},
    {Clave::sonido_amigo_conectado, kSinGrupo},
    {Clave::sonido_pedido_entrada, kSinGrupo},
    {Clave::sonido_alerta_seguridad, kSinGrupo},
    {Clave::sonido_pantalla_compartida, kSinGrupo},
    {Clave::sonido_error, kSinGrupo},
}};

struct DatosDeDialogo {
  Clave titulo;
  Clave ayuda;
  Clave si;
  Clave calculando;
  bool actual;
  bool nueva;
  bool repetir;
};

constexpr std::array<DatosDeDialogo, 8> kDialogos = {{
    {kSinGrupo, kSinGrupo, kSinGrupo, kSinGrupo, false, false, false},
    {Clave::seguridad_exportar_titulo, Clave::seguridad_exportar_ayuda, Clave::seguridad_guardar_copia,
     Clave::seguridad_protegiendo, false, true, true},
    {Clave::seguridad_importar_titulo, Clave::seguridad_importar_ayuda, Clave::seguridad_abrir_copia,
     Clave::seguridad_abriendo, false, true, false},
    {Clave::seguridad_reemplazar_titulo, Clave::seguridad_reemplazar_ayuda, Clave::seguridad_reemplazar_si, kSinGrupo,
     false, false, false},
    {Clave::seguridad_crear_titulo, Clave::seguridad_crear_ayuda, Clave::seguridad_crear_si, kSinGrupo, false, false,
     false},
    {Clave::maestra_activar_titulo, Clave::maestra_activar_ayuda, Clave::maestra_activar, Clave::maestra_protegiendo,
     false, true, true},
    {Clave::maestra_cambiar_titulo, Clave::maestra_cambiar_ayuda, Clave::ajustes_cambiar, Clave::maestra_protegiendo,
     true, true, true},
    {Clave::maestra_quitar_titulo, Clave::maestra_quitar_ayuda, Clave::maestra_quitar, Clave::maestra_comprobando, true,
     false, false},
}};

std::wstring en_minusculas(std::string_view texto) {
  std::wstring ancho = a_utf16(texto);
  CharLowerBuffW(ancho.data(), static_cast<DWORD>(ancho.size()));
  return ancho;
}

bool algo_silenciado(const cliente::AvisosDelServidor& avisos) {
  return avisos.servidor.silenciado_hasta != 0 || avisos.servidor.avisar != cliente::Avisar::menciones ||
         !avisos.canales.empty();
}

class Dibujo {
 public:
  Dibujo(PantallaAjustes& pantalla, const cliente::Guardados& guardados)
      : pantalla_(pantalla), guardados_(guardados), ancho_(ImGui::GetIO().DisplaySize.x),
        alto_(ImGui::GetIO().DisplaySize.y) {}

  PedidoAjustes dibujar() {
    barra();
    menu();
    contenido();
    avisos();
    dialogo_de_identidad();
    if (cambiados_) {
      pedido_.guardados = std::move(*cambiados_);
    }
    pantalla_.ocupado = pantalla_.eligiendo_tecla ||
                        ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) ||
                        ImGui::IsAnyItemActive();
    return std::move(pedido_);
  }

  PedidoAjustes bloqueo() {
    const bool conectado = pantalla_.con_conexion;
    const float ancho = px(440);
    const float interior = ancho - px(40);
    const std::string_view ayuda = t(conectado ? Clave::bloqueo_ayuda_conectado : Clave::bloqueo_ayuda_abrir);
    const std::string_view olvido = t(Clave::bloqueo_olvidaste);
    const std::string_view estado =
        pantalla_.calculando ? t(Clave::bloqueo_abriendo) : std::string_view(pantalla_.error_identidad);
    const float alto_ayuda = alto_envuelto(ayuda, interior, medida::letra_chica);
    const float alto_olvido = conectado ? 0 : px(21) + alto_envuelto(olvido, interior, medida::letra_chica) + px(56);
    const float alto = px(56) + alto_ayuda + px(14) + px(46) + (estado.empty() ? 0 : px(24)) + px(56) + alto_olvido;
    const ImVec2 desde(std::floor((ancho_ - ancho) / 2), std::floor(std::max(0.0f, (alto_ - alto) / 2)));
    panel(desde, ImVec2(desde.x + ancho, desde.y + alto));
    const float x = desde.x + px(20);
    float y = desde.y + px(20);
    escribir_cortado(ImVec2(x, y), interior, t(conectado ? Clave::bloqueo_titulo : Clave::bloqueo_titulo_abrir),
                     color::texto, Letra::titulo, medida::letra_tarjeta);
    y += px(36);
    escribir_envuelto(ImVec2(x, y), ayuda, color::texto2, interior, medida::letra_chica);
    y += alto_ayuda + px(14);
    ImGui::BeginDisabled(pantalla_.calculando);
    if (pantalla_.dialogo_identidad == DialogoIdentidad::ninguno && std::exchange(pantalla_.enfocar_contrasena, false)) {
      ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    bool confirmar = campo("##contrasena_maestra", t(Clave::seguridad_contrasena), pantalla_.contrasena,
                           ImVec2(interior, px(36)), Icono::ninguno,
                           ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_Password, color::pozo);
    y += px(46);
    if (!estado.empty()) {
      escribir_cortado(ImVec2(x, y), interior, estado, pantalla_.calculando ? color::apagado : color::rojo,
                       Letra::normal, medida::letra_chica);
      y += px(24);
    }
    float derecha = x + interior;
    confirmar = a_la_izquierda("##desbloquear", t(Clave::bloqueo_desbloquear), Estilo::principal, derecha, y + px(6)) ||
                confirmar;
    y += px(56);
    if (!conectado) {
      ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + interior, y + 1), tono(color::linea));
      y += px(21);
      escribir_envuelto(ImVec2(x, y), olvido, color::apagado, interior, medida::letra_chica);
      y += alto_envuelto(olvido, interior, medida::letra_chica) + px(10);
      derecha = x + interior;
      if (a_la_izquierda("##crear_otra", t(Clave::seguridad_crear_otra), Estilo::contorneado, derecha, y)) {
        abrir_dialogo(DialogoIdentidad::crear);
      }
      pedido_.importar_identidad =
          a_la_izquierda("##importar_copia", t(Clave::bloqueo_importar), Estilo::esquineros, derecha, y);
    }
    ImGui::EndDisabled();
    pedido_.desbloquear = confirmar && !pantalla_.calculando && pantalla_.contrasena[0] != '\0' &&
                          pantalla_.dialogo_identidad == DialogoIdentidad::ninguno;
    dialogo_de_identidad();
    return std::move(pedido_);
  }

 private:
  std::string_view t(Clave clave) const { return texto(pantalla_.idioma, clave); }
  std::string s(Clave clave) const { return std::string(t(clave)); }
  const cliente::Guardados& actuales() const { return cambiados_ ? *cambiados_ : guardados_; }
  cliente::Guardados& cambiar() {
    if (!cambiados_) {
      cambiados_ = guardados_;
    }
    return *cambiados_;
  }
  std::uint8_t valor(Ajuste ajuste, std::uint8_t de_fabrica) const {
    return cliente::ajuste_o(actuales(), ajuste, de_fabrica);
  }
  void fijar(Ajuste ajuste, std::uint8_t nuevo) { cliente::poner_ajuste(cambiar(), ajuste, nuevo); }

  void barra() {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const float alto = px(kAltoBarra);
    dibujo.AddRectFilled(ImVec2(0, 0), ImVec2(ancho_, alto), tono(color::panel));
    dibujo.AddRectFilled(ImVec2(0, alto), ImVec2(ancho_, alto + 1), tono(color::borde_oscuro));
    triangulos(dibujo, ImVec2(0, 0), ImVec2(ancho_, alto));
    const std::string_view volver = t(Clave::ajustes_volver);
    const float ancho = ancho_boton(volver, Estilo::contorneado, Icono::flecha_izquierda);
    ImGui::SetCursorScreenPos(ImVec2(px(16), (alto - px(32)) / 2));
    pedido_.volver = boton("##volver", volver, Estilo::contorneado, ImVec2(ancho, px(32)), Icono::flecha_izquierda) ||
                     (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !pantalla_.ocupado && !pantalla_.eligiendo_tecla &&
                      !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
    const float fin = escribir_en(px(32) + ancho, alto / 2, t(Clave::ajustes_titulo), color::texto, Letra::titulo,
                                  medida::letra_servidor);
    escribir_en(fin + px(16), alto / 2, t(Clave::ajustes_se_guardan), color::apagado, Letra::normal,
                medida::letra_chica);
  }

  void buscar() {
    const std::string buscado(pantalla_.buscar.data());
    if (buscado == pantalla_.buscado) {
      return;
    }
    pantalla_.buscado = buscado;
    const std::wstring pedido = en_minusculas(buscado);
    const auto esta = [&pedido, this](Clave clave) { return en_minusculas(t(clave)).find(pedido) != std::wstring::npos; };
    for (std::size_t i = 0; i < kMenu.size(); ++i) {
      pantalla_.coinciden[i] =
          pedido.empty() || esta(kMenu[i].nombre) ||
          std::any_of(kBuscables.begin(), kBuscables.end(), [&](const auto& buscable) {
            return buscable.first == kMenu[i].seccion && esta(buscable.second);
          });
    }
    const auto actual = static_cast<std::size_t>(pantalla_.seccion);
    const auto primera = std::find(pantalla_.coinciden.begin(), pantalla_.coinciden.end(), true);
    if (!pantalla_.coinciden[actual] && primera != pantalla_.coinciden.end()) {
      pantalla_.seccion = kMenu[static_cast<std::size_t>(primera - pantalla_.coinciden.begin())].seccion;
    }
  }

  void menu() {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const float arriba = px(kAltoBarra) + 1;
    const float derecha = px(kAnchoMenu);
    dibujo.AddRectFilled(ImVec2(0, arriba), ImVec2(derecha, alto_), tono(color::panel));
    dibujo.AddRectFilled(ImVec2(derecha - 1, arriba), ImVec2(derecha, alto_), tono(color::borde_oscuro));
    ImGui::SetCursorScreenPos(ImVec2(px(12), arriba + px(14)));
    campo("##buscar_ajuste", t(Clave::ajustes_buscar), pantalla_.buscar, ImVec2(derecha - px(24), px(36)), Icono::lupa,
          0, color::pozo);
    buscar();
    ImGui::SetCursorScreenPos(ImVec2(0, arriba + px(60)));
    ImGui::BeginChild("##menu_ajustes", ImVec2(derecha - 1, alto_ - arriba - px(60)));
    ImDrawList& lista = *ImGui::GetWindowDrawList();
    float y = ImGui::GetCursorScreenPos().y;
    bool alguna = false;
    for (std::size_t i = 0; i < kMenu.size(); ++i) {
      const Item& item = kMenu[i];
      if (!pantalla_.coinciden[i]) {
        continue;
      }
      alguna = true;
      if (item.seccion == Seccion::acerca) {
        lista.AddRectFilled(ImVec2(px(12), y + px(6)), ImVec2(derecha - px(12), y + px(7)), tono(color::linea));
        y += px(14);
      }
      if (item.grupo != kSinGrupo && pantalla_.buscado.empty()) {
        escribir_espaciado(ImVec2(px(24), y + px(10)), t(item.grupo), color::apagado, Letra::negrita,
                           medida::letra_mini, px(1.2f));
        y += px(32);
      }
      const ImVec2 desde(px(12), y);
      const ImVec2 hasta(derecha - px(12), y + px(kAltoItem));
      ImGui::SetCursorScreenPos(desde);
      ImGui::PushID(static_cast<int>(i));
      if (ImGui::InvisibleButton("##seccion", ImVec2(hasta.x - desde.x, hasta.y - desde.y), ImGuiButtonFlags_EnableNav)) {
        pantalla_.seccion = item.seccion;
      }
      const bool encima = ImGui::IsItemHovered();
      ImGui::PopID();
      const bool elegida = pantalla_.seccion == item.seccion;
      if (elegida || encima) {
        lista.AddRectFilled(desde, hasta, tono(elegida ? color::claro : color::seleccion), px(2));
      }
      escribir_en(desde.x + px(12), (desde.y + hasta.y) / 2, t(item.nombre), elegida ? color::tinta : color::texto,
                  elegida ? Letra::fuerte : Letra::normal);
      if (item.etapa != 0) {
        const std::string_view pronto = t(Clave::servidor_chip_pronto);
        chip(ImVec2(hasta.x - px(12) - ancho_chip(pronto), (desde.y + hasta.y) / 2 - px(8)), pronto);
      }
      y += px(kAltoItem);
    }
    if (!alguna) {
      escribir_envuelto(ImVec2(px(24), y + px(10)), t(Clave::ajustes_sin_resultados), color::apagado,
                        derecha - px(48), medida::letra_chica);
      y += px(40);
    }
    ImGui::SetCursorScreenPos(ImVec2(0, y));
    ImGui::Dummy(ImVec2(1, px(12)));
    ImGui::EndChild();
  }

  float titulo(ImVec2 desde, Clave clave) {
    escribir_en(desde.x + px(kRelleno), desde.y + px(34), t(clave), color::texto, Letra::titulo, medida::letra_panel);
    return desde.y + px(64);
  }

  void etiqueta(ImVec2 donde, Clave clave) {
    escribir(donde, t(clave), color::apagado, Letra::normal, medida::letra_chica);
  }

  void nota(ImVec2 donde, float ancho, Clave clave) {
    escribir_envuelto(donde, t(clave), color::apagado, ancho, medida::letra_chica);
  }

  std::optional<int> lista(const char* id, ImVec2 donde, float ancho, std::initializer_list<Clave> claves, int elegida) {
    std::vector<std::string> opciones;
    for (const Clave clave : claves) {
      opciones.push_back(s(clave));
    }
    return desplegable(id, donde, ancho, opciones, elegida);
  }

  void contenido() {
    const float x = px(kAnchoMenu + kMargen);
    const float arriba = px(kAltoBarra) + 1;
    ImGui::SetCursorScreenPos(ImVec2(px(kAnchoMenu), arriba));
    ImGui::BeginChild("##seccion_ajustes", ImVec2(ancho_ - px(kAnchoMenu), alto_ - arriba));
    const float y = ImGui::GetCursorScreenPos().y + px(24);
    const float ancho = ancho_ - x - px(kMargen);
    float fin = y;
    switch (pantalla_.seccion) {
      case Seccion::general:
        fin = general(ImVec2(x, y), ancho);
        break;
      case Seccion::teclas:
        fin = teclas(ImVec2(x, y), ancho);
        break;
      case Seccion::audio:
        fin = audio(ImVec2(x, y), ancho);
        break;
      case Seccion::avisos:
        fin = avisos_y_sonidos(ImVec2(x, y), ancho);
        break;
      case Seccion::seguridad:
        fin = seguridad(ImVec2(x, y), ancho);
        break;
      default:
        fin = futura(ImVec2(x, y), ancho);
        break;
    }
    ImGui::SetCursorScreenPos(ImVec2(x, fin));
    ImGui::Dummy(ImVec2(1, px(kMargen)));
    ImGui::EndChild();
  }

  float futura(ImVec2 desde, float ancho) {
    const Item& item = kMenu[static_cast<std::size_t>(pantalla_.seccion)];
    const float alto = px(130);
    panel(desde, ImVec2(desde.x + ancho, desde.y + alto));
    const float y = titulo(desde, item.nombre);
    const std::string_view en_desarrollo = t(Clave::ajustes_en_desarrollo);
    chip(ImVec2(desde.x + px(kRelleno), y - px(4)), en_desarrollo);
    escribir(ImVec2(desde.x + px(kRelleno), y + px(24)),
             con_datos(t(Clave::ajustes_llega_en), {std::to_string(item.etapa)}), color::texto2, Letra::normal,
             medida::letra_chica);
    return desde.y + alto;
  }

  float general(ImVec2 desde, float ancho) {
    const float alto = px(432);
    panel(desde, ImVec2(desde.x + ancho, desde.y + alto));
    float y = titulo(desde, Clave::ajustes_general);
    const float x = desde.x + px(kRelleno);
    const float interior = ancho - 2 * px(kRelleno);
    const float mitad = (interior - px(kRelleno)) / 2;
    if (casilla(t(Clave::ajustes_iniciar_con_windows), ImVec2(x, y), pantalla_.arranca_con_windows)) {
      pedido_.arrancar_con_windows = !pantalla_.arranca_con_windows;
    }
    y += px(46);
    const bool al_ultimo = valor(Ajuste::entrar_al_ultimo, 1) == 1;
    if (casilla(t(Clave::ajustes_entrar_al_ultimo), ImVec2(x, y), al_ultimo)) {
      fijar(Ajuste::entrar_al_ultimo, al_ultimo ? 0 : 1);
    }
    y += px(48);
    etiqueta(ImVec2(x, y), Clave::ajustes_al_cerrar);
    etiqueta(ImVec2(x + mitad + px(kRelleno), y), Clave::ajustes_idioma);
    if (const auto cerrar = lista("##al_cerrar", ImVec2(x, y + px(18)), mitad,
                                  {Clave::ajustes_cerrar_bandeja, Clave::ajustes_cerrar_salir},
                                  valor(Ajuste::al_cerrar, 0))) {
      fijar(Ajuste::al_cerrar, static_cast<std::uint8_t>(*cerrar));
    }
    idioma(ImVec2(x + mitad + px(kRelleno), y + px(18)), mitad);
    y += px(74);
    etiqueta(ImVec2(x, y), Clave::ajustes_al_minimizar);
    if (const auto minimizar = lista("##al_minimizar", ImVec2(x, y + px(18)), mitad,
                                     {Clave::ajustes_minimizar_barra, Clave::ajustes_minimizar_bandeja},
                                     valor(Ajuste::al_minimizar, 0))) {
      fijar(Ajuste::al_minimizar, static_cast<std::uint8_t>(*minimizar));
    }
    y += px(74);
    etiqueta(ImVec2(x, y), Clave::ajustes_al_entrar);
    if (const auto entrar = lista("##al_entrar", ImVec2(x, y + px(18)), interior,
                                  {Clave::ajustes_entrar_chat, Clave::ajustes_entrar_voz}, valor(Ajuste::al_entrar, 0))) {
      fijar(Ajuste::al_entrar, static_cast<std::uint8_t>(*entrar));
    }
    nota(ImVec2(x, y + px(58)), interior, Clave::ajustes_entrar_nota);
    return desde.y + alto;
  }

  void idioma(ImVec2 donde, float ancho) {
    const std::string elegido = cliente::ajuste_texto(actuales(), Ajuste::idioma);
    std::vector<std::string> opciones = {
        con_datos(t(Clave::ajustes_idioma_windows), {texto(idioma_de_windows(), Clave::idioma_nombre)})};
    int actual = 0;
    for (std::size_t i = 0; i < cantidad_idiomas(); ++i) {
      opciones.emplace_back(texto(Idioma{i}, Clave::idioma_nombre));
      actual = codigo_de(Idioma{i}) == elegido ? static_cast<int>(i) + 1 : actual;
    }
    if (const auto nuevo = desplegable("##idioma", donde, ancho, opciones, actual)) {
      cliente::poner_texto(cambiar(), Ajuste::idioma,
                           *nuevo == 0 ? std::string_view() : codigo_de(Idioma{static_cast<std::size_t>(*nuevo - 1)}));
    }
  }

  bool fila_de_tecla(ImVec2 desde, float ancho, std::string_view nombre, std::string_view tecla_actual, bool se_puede,
                     const char* id) {
    const float alto = px(53);
    const float medio = desde.y + alto / 2;
    ImGui::GetWindowDrawList()->AddRect(desde, ImVec2(desde.x + ancho, desde.y + alto + 1), tono(color::linea));
    const std::string_view cambiar_texto = t(Clave::ajustes_cambiar);
    const float ancho_cambiar = ancho_boton(cambiar_texto, Estilo::esquineros);
    const float derecha = desde.x + ancho - px(12) - ancho_cambiar;
    escribir_en(desde.x + px(12), medio, nombre, color::texto, Letra::normal, medida::letra,
                derecha - ancho_tecla(tecla_actual) - desde.x - px(36));
    ImGui::SetCursorScreenPos(ImVec2(derecha, medio - px(medida::alto_esquineros) / 2));
    ImGui::BeginDisabled(!se_puede);
    const bool tocado = boton(id, cambiar_texto, Estilo::esquineros, ImVec2(ancho_cambiar, px(medida::alto_esquineros)));
    ImGui::EndDisabled();
    if (!se_puede) {
      cartel(t(Clave::ajustes_tecla_fija));
    }
    tecla(ImVec2(derecha - px(12) - ancho_tecla(tecla_actual), medio - px(medida::alto_tecla) / 2), tecla_actual,
          color::pozo);
    return tocado;
  }

  std::string nombre_de_tecla(std::size_t cual) const {
    if (pantalla_.eligiendo_tecla && pantalla_.tecla_de == cual) {
      return s(Clave::ajustes_apreta_una_tecla);
    }
    if (cual == 0) {
      return nombre_tecla(pantalla_.idioma, cliente::tecla_para_hablar(actuales().tecla_hablar));
    }
    const auto susurro = cliente::susurro_de(actuales(), cual - 1);
    return susurro && susurro->tecla != 0 ? nombre_tecla(pantalla_.idioma, susurro->tecla) : s(Clave::ajustes_sin_tecla);
  }

  std::string nombre_de_hablar() const { return nombre_de_tecla(0); }

  void fila_de_susurro(ImVec2 desde, float ancho, std::size_t lugar, bool& abrir_destinos) {
    const auto susurro = cliente::susurro_de(actuales(), lugar);
    const std::string nombre =
        con_datos(t(Clave::ajustes_susurro), {std::to_string(lugar + 1), a_quien(pantalla_.idioma, *susurro)});
    ImGui::PushID(static_cast<int>(lugar));
    if (fila_de_tecla(desde, ancho, nombre, nombre_de_tecla(lugar + 1), true, "##tecla_susurro")) {
      ImGui::OpenPopup("##susurro");
    }
    if (ImGui::BeginPopup("##susurro")) {
      if (ImGui::MenuItem(s(Clave::ajustes_cambiar_tecla).c_str())) {
        pedido_.elegir_tecla = true;
        pedido_.tecla_de = lugar + 1;
      }
      if (ImGui::MenuItem(s(Clave::ajustes_elegir_a_quien).c_str())) {
        pantalla_.destinos_de = lugar;
        abrir_destinos = true;
      }
      if (ImGui::MenuItem(s(Clave::ajustes_quitar_susurro).c_str())) {
        cliente::poner_susurro(cambiar(), lugar, std::nullopt);
      }
      ImGui::EndPopup();
    }
    ImGui::PopID();
  }

  float teclas(ImVec2 desde, float ancho) {
    std::vector<std::size_t> lugares;
    for (std::size_t lugar = 0; lugar < protocolo::kLugaresDeSusurro; ++lugar) {
      if (cliente::susurro_de(actuales(), lugar)) {
        lugares.push_back(lugar);
      }
    }
    const float fila = px(53);
    const float alto = px(64 + 110) + fila * static_cast<float>(6 + lugares.size());
    panel(desde, ImVec2(desde.x + ancho, desde.y + alto));
    float y = titulo(desde, Clave::ajustes_teclas);
    const float x = desde.x + px(kRelleno);
    const float interior = ancho - 2 * px(kRelleno);
    if (fila_de_tecla(ImVec2(x, y), interior, t(Clave::ajustes_tecla_hablar), nombre_de_tecla(0), true,
                      "##tecla_hablar")) {
      pedido_.elegir_tecla = true;
      pedido_.tecla_de = 0;
    }
    y += fila;
    bool abrir_destinos = false;
    for (const std::size_t lugar : lugares) {
      fila_de_susurro(ImVec2(x, y), interior, lugar, abrir_destinos);
      y += fila;
    }
    ImGui::GetWindowDrawList()->AddRect(ImVec2(x, y), ImVec2(x + interior, y + fila + 1), tono(color::linea));
    const std::string_view agregar = t(Clave::ajustes_agregar_susurro);
    const float ancho_agregar = ancho_boton(agregar, Estilo::contorneado);
    escribir_en(x + px(12), y + fila / 2, t(Clave::ajustes_susurros_nota), color::apagado, Letra::normal, medida::letra,
                interior - ancho_agregar - px(36));
    ImGui::SetCursorScreenPos(ImVec2(x + interior - px(12) - ancho_agregar, y + fila / 2 - px(16)));
    ImGui::BeginDisabled(lugares.size() == protocolo::kLugaresDeSusurro);
    if (boton("##agregar_susurro", agregar, Estilo::contorneado, ImVec2(ancho_agregar, px(32)))) {
      std::size_t libre = 0;
      while (std::find(lugares.begin(), lugares.end(), libre) != lugares.end()) {
        ++libre;
      }
      cliente::poner_susurro(cambiar(), libre, cliente::Susurro{});
      pantalla_.destinos_de = libre;
      abrir_destinos = true;
    }
    ImGui::EndDisabled();
    y += fila;
    for (const auto& [nombre, atajo, id] :
         {std::make_tuple(Clave::ajustes_tecla_microfono, Clave::ajustes_ctrl_m, "##tecla_microfono"),
          std::make_tuple(Clave::ajustes_tecla_audio, Clave::ajustes_ctrl_d, "##tecla_audio"),
          std::make_tuple(Clave::ajustes_tecla_modo, Clave::ajustes_ctrl_mayus_r, "##tecla_modo"),
          std::make_tuple(Clave::ajustes_tecla_canales, Clave::ajustes_ctrl_1_9, "##tecla_canales")}) {
      fila_de_tecla(ImVec2(x, y), interior, t(nombre), t(atajo), false, id);
      y += fila;
    }
    if (abrir_destinos) {
      ImGui::OpenPopup("##destinos");
    }
    destinos();
    y += px(20);
    const bool sin_foco = valor(Ajuste::teclas_sin_foco, 1) == 1;
    if (casilla(t(Clave::ajustes_teclas_sin_foco), ImVec2(x, y), sin_foco)) {
      fijar(Ajuste::teclas_sin_foco, sin_foco ? 0 : 1);
    }
    nota(ImVec2(x, y + px(40)), interior, Clave::ajustes_teclas_nota);
    return desde.y + alto;
  }

  void destinos() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(px(320), 0), ImVec2(px(520), px(440)));
    if (!ImGui::BeginPopup("##destinos")) {
      return;
    }
    const std::size_t lugar = pantalla_.destinos_de;
    auto susurro = cliente::susurro_de(actuales(), lugar);
    if (!susurro) {
      cerrar_ventana_emergente();
      ImGui::EndPopup();
      return;
    }
    ImGui::TextUnformatted(con_datos(t(Clave::ajustes_susurro_a_quien), {std::to_string(lugar + 1)}).c_str());
    std::vector<std::string> personas = pantalla_.personas;
    std::vector<std::string> canales = pantalla_.canales;
    for (const std::string& nombre : susurro->personas) {
      if (std::find(personas.begin(), personas.end(), nombre) == personas.end()) {
        personas.push_back(nombre);
      }
    }
    if (!susurro->canal.empty() && std::find(canales.begin(), canales.end(), susurro->canal) == canales.end()) {
      canales.push_back(susurro->canal);
    }
    if (pantalla_.personas.empty() && pantalla_.canales.empty()) {
      ImGui::TextDisabled("%s", s(Clave::ajustes_susurro_sin_servidor).c_str());
    }
    bool cambio = false;
    if (!personas.empty()) {
      ImGui::Separator();
      ImGui::TextDisabled("%s", s(Clave::ajustes_susurro_a_personas).c_str());
    }
    for (std::size_t i = 0; i < personas.size(); ++i) {
      const auto elegida = std::find(susurro->personas.begin(), susurro->personas.end(), personas[i]);
      const bool esta = elegida != susurro->personas.end();
      ImGui::PushID(static_cast<int>(i));
      ImGui::BeginDisabled(!esta && susurro->personas.size() >= protocolo::kMaximoPersonasEnSusurro);
      const bool tocada = casilla(personas[i], ImGui::GetCursorScreenPos(), esta);
      ImGui::EndDisabled();
      if (tocada) {
        if (esta) {
          susurro->personas.erase(elegida);
        } else {
          susurro->personas.push_back(personas[i]);
        }
        susurro->canal.clear();
        cambio = true;
      }
      ImGui::PopID();
    }
    if (!canales.empty()) {
      ImGui::Separator();
      ImGui::TextDisabled("%s", s(Clave::ajustes_susurro_a_canal).c_str());
    }
    for (std::size_t i = 0; i < canales.size(); ++i) {
      ImGui::PushID(static_cast<int>(personas.size() + i));
      if (casilla(canales[i], ImGui::GetCursorScreenPos(), susurro->canal == canales[i])) {
        susurro->canal = susurro->canal == canales[i] ? std::string() : canales[i];
        susurro->personas.clear();
        cambio = true;
      }
      ImGui::PopID();
    }
    if (cambio) {
      cliente::poner_susurro(cambiar(), lugar, susurro);
    }
    ImGui::EndPopup();
  }

  std::optional<int> dispositivo(const char* id, ImVec2 donde, float ancho, const std::vector<std::string>& nombres,
                                 Ajuste ajuste, Clave primera = Clave::ajustes_el_de_windows) {
    const std::string elegido = cliente::ajuste_texto(actuales(), ajuste);
    std::vector<std::string> opciones = {s(primera)};
    opciones.insert(opciones.end(), nombres.begin(), nombres.end());
    const auto esta = std::find(nombres.begin(), nombres.end(), elegido);
    const int actual = elegido.empty() || esta == nombres.end() ? 0 : static_cast<int>(esta - nombres.begin()) + 1;
    const auto nuevo = desplegable(id, donde, ancho, opciones, actual);
    if (nuevo) {
      cliente::poner_texto(cambiar(), ajuste,
                           *nuevo == 0 ? std::string_view() : nombres[static_cast<std::size_t>(*nuevo - 1)]);
    }
    return nuevo;
  }

  float audio(ImVec2 desde, float ancho) {
    escribir_en(desde.x, desde.y + px(10), t(Clave::ajustes_audio), color::texto, Letra::titulo, medida::letra_tarjeta);
    const float arriba = desde.y + px(40);
    const float columna = (ancho - px(kRelleno)) / 2;
    const float izquierda = dispositivos_y_volumenes(ImVec2(desde.x, arriba), columna);
    const float derecha = como_transmitis(ImVec2(desde.x + columna + px(kRelleno), arriba), columna);
    return std::max(izquierda, derecha);
  }

  float dispositivos_y_volumenes(ImVec2 desde, float ancho) {
    const float x = desde.x + px(kRelleno);
    const float interior = ancho - 2 * px(kRelleno);
    const float alto_dispositivos = px(286);
    panel(desde, ImVec2(desde.x + ancho, desde.y + alto_dispositivos));
    float y = titulo(desde, Clave::ajustes_dispositivos);
    etiqueta(ImVec2(x, y), Clave::ajustes_microfono);
    dispositivo("##microfono", ImVec2(x, y + px(18)), interior, pantalla_.dispositivos.microfonos, Ajuste::microfono);
    y += px(72);
    etiqueta(ImVec2(x, y), Clave::ajustes_salida);
    dispositivo("##salida", ImVec2(x, y + px(18)), interior, pantalla_.dispositivos.salidas, Ajuste::salida);
    y += px(72);
    const std::string_view probar = t(Clave::ajustes_probar_salida);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    pedido_.probar_salida = boton("##probar_salida", probar, Estilo::contorneado,
                                  ImVec2(ancho_boton(probar, Estilo::contorneado), px(36)));
    nota(ImVec2(x, y + px(46)), interior, Clave::ajustes_dispositivos_nota);
    y = desde.y + alto_dispositivos + px(kRelleno);
    const float alto_volumenes = px(150);
    panel(ImVec2(desde.x, y), ImVec2(desde.x + ancho, y + alto_volumenes));
    const float fila = titulo(ImVec2(desde.x, y), Clave::ajustes_volumenes);
    const float tercio = (interior - 2 * px(kRelleno)) / 3;
    const std::array<std::pair<Clave, const char*>, 3> volumenes = {{{Clave::ajustes_microfono, "##vol_microfono"},
                                                                     {Clave::ajustes_voces, "##vol_voces"},
                                                                     {Clave::ajustes_sonidos_aviso, "##vol_avisos"}}};
    for (std::size_t i = 0; i < volumenes.size(); ++i) {
      const auto& [nombre, id] = volumenes[i];
      const float columna = x + static_cast<float>(i) * (tercio + px(kRelleno));
      etiqueta(ImVec2(columna, fila), nombre);
      const std::uint8_t actual = nombre == Clave::ajustes_microfono ? actuales().volumen_microfono
                                  : nombre == Clave::ajustes_voces   ? actuales().volumen_salida
                                                                     : valor(Ajuste::volumen_avisos, 100);
      if (const auto nuevo = deslizador(id, ImVec2(columna, fila + px(32)), tercio,
                                        static_cast<float>(actual) / cliente::kVolumenMaximo)) {
        const auto cuanto = static_cast<std::uint8_t>(std::lround(*nuevo * cliente::kVolumenMaximo));
        if (nombre == Clave::ajustes_microfono) {
          cambiar().volumen_microfono = cuanto;
        } else if (nombre == Clave::ajustes_voces) {
          cambiar().volumen_salida = cuanto;
        } else {
          fijar(Ajuste::volumen_avisos, cuanto);
        }
      }
    }
    nota(ImVec2(x, fila + px(52)), interior, Clave::ajustes_volumenes_nota);
    y += alto_volumenes + px(kRelleno);
    const float alto_calidad = px(110);
    ImGui::GetWindowDrawList()->AddRect(ImVec2(desde.x, y), ImVec2(desde.x + ancho, y + alto_calidad),
                                        tono(color::linea2));
    const float texto_calidad = titulo(ImVec2(desde.x, y), Clave::ajustes_calidad);
    nota(ImVec2(x, texto_calidad - px(8)), interior, Clave::ajustes_calidad_nota);
    return y + alto_calidad;
  }

  void medidor(ImVec2 desde, float ancho, bool por_voz) {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const float hueco = px(3);
    const float segmento = (ancho - hueco * (kSegmentos - 1)) / kSegmentos;
    for (int i = 0; i < kSegmentos; ++i) {
      const float x = desde.x + static_cast<float>(i) * (segmento + hueco);
      const bool prendido = pantalla_.probando_microfono && i < pantalla_.nivel;
      dibujo.AddRectFilled(ImVec2(x, desde.y), ImVec2(x + segmento, desde.y + px(26)),
                           tono(prendido ? color::verde : color::pozo));
      dibujo.AddRect(ImVec2(x, desde.y), ImVec2(x + segmento, desde.y + px(26)), tono(color::linea));
    }
    if (por_voz) {
      const float marca = desde.x + ancho * static_cast<float>(actuales().sensibilidad) / cliente::kSensibilidadMaxima;
      dibujo.AddRectFilled(ImVec2(marca - px(1), desde.y - px(4)), ImVec2(marca + px(1), desde.y + px(30)),
                           tono(color::naranja));
    }
  }

  float como_transmitis(ImVec2 desde, float ancho) {
    const float x = desde.x + px(kRelleno);
    const float interior = ancho - 2 * px(kRelleno);
    const float alto_transmitis = px(430);
    panel(desde, ImVec2(desde.x + ancho, desde.y + alto_transmitis));
    float y = titulo(desde, Clave::ajustes_como_transmitis);
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    const bool por_voz = actuales().por_voz;
    for (const bool es_voz : {false, true}) {
      const ImVec2 esquina(x, y);
      const ImVec2 otra(x + interior, y + px(64));
      ImGui::SetCursorScreenPos(esquina);
      if (ImGui::InvisibleButton(es_voz ? "##modo_voz" : "##modo_tecla", ImVec2(interior, px(64))) &&
          es_voz != por_voz) {
        cambiar().por_voz = es_voz;
      }
      const bool elegido = actuales().por_voz == es_voz;
      dibujo.AddRectFilled(esquina, otra, tono(elegido ? color::elevado : color::pozo));
      dibujo.AddRect(esquina, otra, tono(elegido ? color::claro : color::linea), 0, 0, elegido ? px(1.5f) : px(1));
      escribir_en(x + px(14), y + px(22), t(es_voz ? Clave::ajustes_por_voz : Clave::ajustes_tecla_para_hablar),
                  color::texto, Letra::fuerte);
      escribir_en(x + px(14), y + px(44),
                  t(es_voz ? Clave::ajustes_por_voz_ayuda : Clave::ajustes_tecla_para_hablar_ayuda), color::apagado,
                  Letra::normal, medida::letra_chica, interior - px(28));
      y += px(74);
    }
    escribir_en(x, y + px(8), t(Clave::ajustes_medidor), color::texto, Letra::fuerte);
    const std::string_view ayuda = t(Clave::ajustes_medidor_ayuda);
    escribir_en(x + interior - medir(ayuda, Letra::normal, medida::letra_chica).x, y + px(8), ayuda, color::apagado,
                Letra::normal, medida::letra_chica);
    medidor(ImVec2(x, y + px(24)), interior, actuales().por_voz);
    y += px(66);
    const std::string_view probar = t(pantalla_.probando_microfono ? Clave::ajustes_dejar_de_probar
                                                                  : Clave::ajustes_probar_microfono);
    const float ancho_probar = ancho_boton(probar, Estilo::principal);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    if (boton("##probar_microfono", probar, Estilo::principal, ImVec2(ancho_probar, px(medida::alto_principal)))) {
      pedido_.probar_microfono = !pantalla_.probando_microfono;
    }
    if (casilla(t(Clave::ajustes_escucharme), ImVec2(x + ancho_probar + px(16), y + px(10)), pantalla_.escuchandome)) {
      pedido_.escucharme = !pantalla_.escuchandome;
    }
    y += px(54);
    const float mitad = (interior - px(kRelleno)) / 2;
    if (actuales().por_voz) {
      etiqueta(ImVec2(x, y), Clave::ajustes_sensibilidad);
      if (const auto nueva = deslizador("##sensibilidad_ajustes", ImVec2(x, y + px(34)), mitad,
                                        static_cast<float>(actuales().sensibilidad) / cliente::kSensibilidadMaxima)) {
        cambiar().sensibilidad = static_cast<std::uint8_t>(std::lround(*nueva * cliente::kSensibilidadMaxima));
      }
    } else {
      etiqueta(ImVec2(x, y), Clave::ajustes_tecla);
      const std::string nombre = nombre_de_hablar();
      tecla(ImVec2(x, y + px(20)), nombre, color::pozo);
      const std::string_view cambiar_texto = t(Clave::ajustes_cambiar_tecla);
      ImGui::SetCursorScreenPos(ImVec2(x + ancho_tecla(nombre) + px(10), y + px(16)));
      if (boton("##cambiar_tecla_audio", cambiar_texto, Estilo::esquineros,
                ImVec2(ancho_boton(cambiar_texto, Estilo::esquineros), px(medida::alto_esquineros)))) {
        pedido_.elegir_tecla = true;
      }
    }
    etiqueta(ImVec2(x + mitad + px(kRelleno), y), Clave::ajustes_seguir_al_soltar);
    if (const auto seguir = lista("##seguir_al_soltar", ImVec2(x + mitad + px(kRelleno), y + px(18)), mitad,
                                  {Clave::ajustes_segundos_0, Clave::ajustes_segundos_02, Clave::ajustes_segundos_05,
                                   Clave::ajustes_segundos_1},
                                  valor(Ajuste::seguir_al_soltar, 1))) {
      fijar(Ajuste::seguir_al_soltar, static_cast<std::uint8_t>(*seguir));
    }
    return limpieza(ImVec2(desde.x, desde.y + alto_transmitis + px(kRelleno)), ancho);
  }

  float limpieza(ImVec2 desde, float ancho) {
    const float x = desde.x + px(kRelleno);
    const float interior = ancho - 2 * px(kRelleno);
    const float alto = px(360);
    panel(desde, ImVec2(desde.x + ancho, desde.y + alto));
    float y = titulo(desde, Clave::ajustes_limpieza);
    const cliente::AjustesDeLimpieza limpia = cliente::limpieza_de(actuales(), pantalla_.un_nucleo);
    const float ancho_nivel = px(110);
    const bool quitar = limpia.ruido != cliente::Ruido::apagado;
    if (casilla(t(Clave::ajustes_quitar_ruido), ImVec2(x, y + px(6)), quitar)) {
      fijar(Ajuste::ruido, static_cast<std::uint8_t>(quitar ? cliente::Ruido::apagado : cliente::Ruido::media));
    }
    ImGui::BeginDisabled(!quitar);
    if (const auto nivel = lista("##nivel_ruido", ImVec2(x + interior - ancho_nivel, y), ancho_nivel,
                                 {Clave::ajustes_ruido_baja, Clave::ajustes_ruido_media, Clave::ajustes_ruido_alta},
                                 quitar ? static_cast<int>(limpia.ruido) - 1 : 1)) {
      fijar(Ajuste::ruido, static_cast<std::uint8_t>(*nivel + 1));
    }
    ImGui::EndDisabled();
    y += px(50);
    etiqueta(ImVec2(x, y), Clave::ajustes_eco);
    constexpr std::array<cliente::Eco, 3> kEcos = {cliente::Eco::automatico, cliente::Eco::prendido,
                                                   cliente::Eco::apagado};
    const int eco = static_cast<int>(std::find(kEcos.begin(), kEcos.end(), limpia.eco) - kEcos.begin());
    if (const auto nuevo = lista("##eco", ImVec2(x, y + px(18)), interior * 0.6f,
                                 {Clave::ajustes_eco_automatica, Clave::ajustes_eco_siempre, Clave::ajustes_eco_apagada},
                                 eco)) {
      fijar(Ajuste::eco, static_cast<std::uint8_t>(kEcos[static_cast<std::size_t>(*nuevo)]));
    }
    y += px(74);
    if (casilla(t(Clave::ajustes_volumen_automatico), ImVec2(x, y), limpia.volumen)) {
      fijar(Ajuste::volumen_automatico, limpia.volumen ? 0 : 1);
    }
    y += px(46);
    ImGui::BeginDisabled(!pantalla_.hay_bajar_otros);
    const bool bajar = limpia.bajar != cliente::Bajar::nada;
    if (casilla(t(Clave::ajustes_bajar_otros), ImVec2(x, y + px(6)), bajar)) {
      fijar(Ajuste::bajar_otros, static_cast<std::uint8_t>(bajar ? cliente::Bajar::nada : cliente::Bajar::un_poco));
    }
    if (const auto cuanto = lista("##bajar_cuanto", ImVec2(x + interior - ancho_nivel, y), ancho_nivel,
                                  {Clave::ajustes_bajar_un_poco, Clave::ajustes_bajar_bastante, Clave::ajustes_bajar_mucho},
                                  bajar ? static_cast<int>(limpia.bajar) - 1 : 0)) {
      fijar(Ajuste::bajar_otros, static_cast<std::uint8_t>(*cuanto + 1));
    }
    ImGui::EndDisabled();
    if (!pantalla_.hay_bajar_otros) {
      cartel(t(Clave::ajustes_bajar_sin));
    }
    nota(ImVec2(x, y + px(52)), interior, Clave::ajustes_limpieza_nota);
    return desde.y + alto;
  }

  float avisos_y_sonidos(ImVec2 desde, float ancho) {
    const float x = desde.x + px(kRelleno);
    const float interior = ancho - 2 * px(kRelleno);
    const float mitad = (interior - px(kRelleno)) / 2;
    const std::size_t grupo = std::min(pantalla_.grupo_de_sonidos, kGruposDeSonidos.size() - 1);
    const Grupo& elegido = kGruposDeSonidos[grupo];
    const float fila = px(47);
    const float filas = fila * static_cast<float>((elegido.cuantos + 1) / 2);
    const bool llamadas = elegido.etapa != 0;
    const float alto_nota = alto_envuelto(t(Clave::ajustes_sonidos_nota), interior, medida::letra_chica);
    const float alto = px(64 + 74 + 74 + 40 + 90 + 82 + 98 + 28 + 52 + 12) + (llamadas ? px(36) : 0) + filas + alto_nota +
                       px(kRelleno) + (pantalla_.aviso_de_sonidos.empty() ? 0 : px(28));
    panel(desde, ImVec2(desde.x + ancho, desde.y + alto));
    float y = titulo(desde, Clave::ajustes_avisos);
    const float derecha = x + mitad + px(kRelleno);
    etiqueta(ImVec2(x, y), Clave::ajustes_volumen_avisos);
    if (const auto nuevo = deslizador("##volumen_de_avisos", ImVec2(x, y + px(34)), mitad,
                                      static_cast<float>(valor(Ajuste::volumen_avisos, 100)) / cliente::kVolumenMaximo)) {
      fijar(Ajuste::volumen_avisos, static_cast<std::uint8_t>(std::lround(*nuevo * cliente::kVolumenMaximo)));
    }
    etiqueta(ImVec2(derecha, y), Clave::ajustes_sonar_por);
    dispositivo("##sonar_por", ImVec2(derecha, y + px(18)), mitad, pantalla_.dispositivos.salidas, Ajuste::sonar_por,
                Clave::ajustes_sonar_igual);
    y += px(74);
    etiqueta(ImVec2(x, y), Clave::ajustes_no_molestar);
    if (const auto nuevo = lista("##no_molestar", ImVec2(x, y + px(18)), mitad,
                                 {Clave::ajustes_no_molestar_nunca, Clave::ajustes_no_molestar_juego,
                                  Clave::ajustes_no_molestar_siempre},
                                 valor(Ajuste::no_molestar, cliente::kNoMolestarAlJugar))) {
      fijar(Ajuste::no_molestar, static_cast<std::uint8_t>(*nuevo));
    }
    ImGui::BeginDisabled();
    casilla(t(Clave::ajustes_llamadas_no_molestar), ImVec2(derecha, y + px(24)), true);
    ImGui::EndDisabled();
    cartel(con_datos(t(Clave::ajustes_llega_en), {"14"}));
    y += px(74);
    const bool minimizada = valor(Ajuste::avisos_minimizada, 1) == 1;
    if (casilla(t(Clave::ajustes_avisos_minimizada), ImVec2(x, y), minimizada)) {
      fijar(Ajuste::avisos_minimizada, minimizada ? 0 : 1);
    }
    y += px(40);
    const std::array<std::pair<Ajuste, Clave>, 4> que_avisa = {{{Ajuste::avisos_de_texto, Clave::ajustes_avisos_texto},
                                                                {Ajuste::avisos_de_voz, Clave::ajustes_avisos_voz},
                                                                {Ajuste::avisos_de_conexion, Clave::ajustes_avisos_conexion},
                                                                {Ajuste::avisos_privados, Clave::ajustes_avisos_privados}}};
    for (std::size_t i = 0; i < que_avisa.size(); ++i) {
      const auto& [ajuste, clave] = que_avisa[i];
      const bool prendido = valor(ajuste, ajuste == Ajuste::avisos_privados ? 0 : 1) == 1;
      ImGui::PushID(static_cast<int>(clave));
      if (casilla("", ImVec2(i % 2 == 0 ? x : derecha, y + px(i < 2 ? 0.0f : 40.0f)), prendido)) {
        fijar(ajuste, prendido ? 0 : 1);
      }
      ImGui::PopID();
      escribir_en((i % 2 == 0 ? x : derecha) + px(26), y + px(i < 2 ? 9.0f : 49.0f), t(clave), color::texto, Letra::normal,
                  medida::letra, mitad - px(26));
    }
    y += px(90);
    silenciados(ImVec2(x, y), interior);
    y += px(82);
    paquete(ImVec2(x, y), interior);
    y += px(98);
    if (!pantalla_.aviso_de_sonidos.empty()) {
      escribir_en(x, y + px(9), pantalla_.aviso_de_sonidos, pantalla_.aviso_es_error ? color::rojo : color::verde,
                  Letra::normal, medida::letra_chica, interior);
      y += px(28);
    }
    escribir(ImVec2(x, y), t(Clave::ajustes_cada_aviso), color::texto2, Letra::normal, medida::letra_chica);
    y += px(28);
    const std::array<std::string, 4> nombres = {
        con_datos(t(Clave::ajustes_grupo_voz), {std::to_string(kGruposDeSonidos[0].cuantos)}),
        con_datos(t(Clave::ajustes_grupo_chat), {std::to_string(kGruposDeSonidos[1].cuantos)}),
        con_datos(t(Clave::ajustes_grupo_llamadas), {std::to_string(kGruposDeSonidos[2].cuantos)}),
        con_datos(t(Clave::ajustes_grupo_servidor), {std::to_string(kGruposDeSonidos[3].cuantos)})};
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    const int tocado = selector_opciones("##grupo_de_sonidos", {nombres[0], nombres[1], nombres[2], nombres[3]},
                                         static_cast<int>(grupo), ImVec2(interior, px(38)), -1, {});
    if (tocado >= 0) {
      pantalla_.grupo_de_sonidos = static_cast<std::size_t>(tocado);
    }
    y += px(52);
    if (llamadas) {
      ImGui::GetWindowDrawList()->AddRect(ImVec2(x, y), ImVec2(x + interior, y + px(37)), tono(color::linea));
      escribir_en(x + px(12), y + px(18), t(Clave::ajustes_llamadas_en_desarrollo), color::apagado, Letra::normal,
                  medida::letra_chica);
      y += px(36);
    }
    for (std::size_t i = 0; i < elegido.cuantos; ++i) {
      const float columna = i % 2 == 0 ? x : x + interior / 2;
      sonido(ImVec2(columna, y + fila * static_cast<float>(i / 2)), interior / 2, elegido.primero + i, llamadas);
    }
    y += filas + px(12);
    escribir_envuelto(ImVec2(x, y), t(Clave::ajustes_sonidos_nota), color::apagado, interior, medida::letra_chica);
    return desde.y + alto;
  }

  void silenciados(ImVec2 desde, float interior) {
    const float x = desde.x;
    const float y = desde.y;
    const float alto_caja = px(70);
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + interior, y + alto_caja), tono(color::pozo));
    ImGui::GetWindowDrawList()->AddRect(ImVec2(x, y), ImVec2(x + interior, y + alto_caja), tono(color::linea));
    escribir_en(x + px(12), y + px(22), t(Clave::ajustes_silenciados), color::texto, Letra::fuerte);
    const auto cuantos = static_cast<std::size_t>(
        std::count_if(actuales().servidores.begin(), actuales().servidores.end(),
                      [](const cliente::ServidorGuardado& servidor) { return algo_silenciado(servidor.avisos); }));
    const std::string_view ver = t(Clave::ajustes_ver_y_cambiar);
    const float ancho_ver = ancho_boton(ver, Estilo::contorneado);
    escribir_en(x + px(12), y + px(46),
                cuantos == 0 ? s(Clave::ajustes_silenciados_nada)
                             : con_datos(t(Clave::ajustes_silenciados_resumen), {std::to_string(cuantos)}),
                color::apagado, Letra::normal, medida::letra_chica, interior - ancho_ver - px(36));
    ImGui::SetCursorScreenPos(ImVec2(x + interior - px(12) - ancho_ver, y + px(17)));
    if (boton("##ver_silenciados", ver, Estilo::contorneado, ImVec2(ancho_ver, px(36)))) {
      ImGui::OpenPopup("##silenciados");
    }
    if (ImGui::BeginPopup("##silenciados")) {
      for (const auto& servidor : actuales().servidores) {
        ImGui::PushID(red::a_texto(servidor.direccion).c_str());
        if (ImGui::MenuItem(red::a_texto(servidor.direccion).c_str(), nullptr, algo_silenciado(servidor.avisos))) {
          pantalla_.avisos_de = servidor.direccion;
          pantalla_.abrir_avisos = true;
        }
        ImGui::PopID();
      }
      ImGui::EndPopup();
    }
  }

  void paquete(ImVec2 desde, float interior) {
    const float x = desde.x;
    const float y = desde.y;
    ImGui::GetWindowDrawList()->AddRect(ImVec2(x, y), ImVec2(x + interior, y + px(84)), tono(color::linea));
    escribir(ImVec2(x + px(12), y + px(12)), t(Clave::ajustes_paquete), color::apagado, Letra::normal,
             medida::letra_chica);
    std::size_t propios = 0;
    for (std::size_t sonido = 0; sonido < cliente::kSonidos; ++sonido) {
      propios += cliente::ajuste_texto(actuales(), cliente::ajuste_de_propio(sonido)).empty() ? 0 : 1;
    }
    const std::array<std::pair<Clave, const char*>, 3> botones = {{{Clave::ajustes_importar, "##importar_sonidos"},
                                                                   {Clave::ajustes_exportar, "##exportar_sonidos"},
                                                                   {Clave::ajustes_predeterminados, "##sonidos_de_fabrica"}}};
    float derecha = x + interior - px(12);
    for (auto cual = botones.rbegin(); cual != botones.rend(); ++cual) {
      const std::string_view texto_boton = t(cual->first);
      const Estilo estilo = cual->first == Clave::ajustes_predeterminados ? Estilo::esquineros : Estilo::contorneado;
      const float ancho_del_boton = ancho_boton(texto_boton, estilo);
      derecha -= ancho_del_boton;
      ImGui::SetCursorScreenPos(ImVec2(derecha, y + px(34)));
      ImGui::BeginDisabled(cual->first != Clave::ajustes_importar && propios == 0);
      if (boton(cual->second, texto_boton, estilo, ImVec2(ancho_del_boton, px(36)))) {
        (cual->first == Clave::ajustes_importar   ? pedido_.importar_sonidos
         : cual->first == Clave::ajustes_exportar ? pedido_.exportar_sonidos
                                                  : pedido_.sonidos_de_fabrica) = true;
      }
      ImGui::EndDisabled();
      derecha -= px(8);
    }
    const std::string vista = propios == 0 ? s(Clave::ajustes_paquete_fabrica)
                                           : con_datos(t(Clave::ajustes_paquete_mio), {std::to_string(propios)});
    desplegable("##paquete", ImVec2(x + px(12), y + px(37)), derecha - x - px(12), {vista}, 0);
  }

  void sonido(ImVec2 desde, float ancho, std::size_t cual, bool en_desarrollo) {
    const float alto = px(47);
    const float medio = desde.y + alto / 2;
    ImGui::GetWindowDrawList()->AddRect(desde, ImVec2(desde.x + ancho, desde.y + alto + 1), tono(color::linea));
    ImGui::PushID(static_cast<int>(cual));
    const bool pronto = en_desarrollo || std::find(kSuenanYa.begin(), kSuenanYa.end(), cual) == kSuenanYa.end();
    const bool prendido = cliente::sonido_prendido(actuales(), cual);
    ImGui::BeginDisabled(pronto);
    if (casilla("", ImVec2(desde.x + px(12), medio - px(9)), prendido)) {
      fijar(cliente::ajuste_de_sonido(cual), prendido ? 0 : 1);
    }
    const float ancho_lista = std::min(px(170), ancho * 0.36f);
    const float lista_x = desde.x + ancho - px(12) - ancho_lista;
    const float probar_x = lista_x - px(8) - px(32);
    const float texto_x = desde.x + px(42);
    const ItemDeSonido& item = kItemsDeSonido[cual];
    const float texto_y = item.nota == kSinGrupo ? medio : medio - px(7);
    const float fin = escribir_en(texto_x, texto_y, t(item.nombre), color::texto, Letra::normal, medida::letra,
                                  probar_x - texto_x - px(8));
    if (item.nota != kSinGrupo) {
      escribir_en(texto_x, medio + px(10), t(item.nota), color::apagado, Letra::normal, medida::letra_chica,
                  probar_x - texto_x - px(8));
    }
    ImGui::SetCursorScreenPos(ImVec2(probar_x, medio - px(16)));
    if (boton("##probar_sonido", "", Estilo::contorneado, ImVec2(px(32), px(32)), Icono::reproducir)) {
      pedido_.probar_sonido = cual;
    }
    const std::string propio = cliente::ajuste_texto(actuales(), cliente::ajuste_de_propio(cual));
    std::vector<std::string> opciones = {s(Clave::ajustes_sonido_fabrica)};
    if (!propio.empty()) {
      opciones.push_back(con_datos(t(Clave::ajustes_sonido_propio), {propio}));
    }
    opciones.push_back(s(Clave::ajustes_sonido_elegir));
    if (const auto nuevo = desplegable("##origen_del_sonido", ImVec2(lista_x, medio - px(15)), ancho_lista, opciones,
                                       propio.empty() ? 0 : 1)) {
      if (static_cast<std::size_t>(*nuevo) + 1 == opciones.size()) {
        pedido_.elegir_sonido = cual;
      } else if (*nuevo == 0 && !propio.empty()) {
        pedido_.sonido_de_fabrica = cual;
      }
    }
    ImGui::EndDisabled();
    if (pronto && !en_desarrollo && fin + px(8) + ancho_chip(t(Clave::servidor_chip_pronto)) < probar_x) {
      chip(ImVec2(fin + px(8), texto_y - px(8)), t(Clave::servidor_chip_pronto));
    }
    ImGui::PopID();
  }

  bool a_la_izquierda(const char* id, std::string_view texto_boton, Estilo estilo, float& derecha, float y) {
    const float ancho = ancho_boton(texto_boton, estilo);
    derecha -= ancho;
    ImGui::SetCursorScreenPos(ImVec2(derecha, y));
    const bool tocado = boton(id, texto_boton, estilo, ImVec2(ancho, px(36)));
    derecha -= px(12);
    return tocado;
  }

  float fila_de_seguridad(float x, float y, float interior, Clave nombre, Clave ayuda, float derecha) {
    const float fila = px(61);
    const float ancho_texto = derecha - x - px(12);
    ImGui::GetWindowDrawList()->AddRect(ImVec2(x, y), ImVec2(x + interior, y + fila + 1), tono(color::linea));
    escribir_en(x + px(12), y + px(21), t(nombre), color::texto, Letra::fuerte, medida::letra, ancho_texto);
    escribir_en(x + px(12), y + px(41), t(ayuda), color::apagado, Letra::normal, medida::letra_chica, ancho_texto);
    return y + fila;
  }

  float seguridad(ImVec2 desde, float ancho) {
    const float x = desde.x + px(kRelleno);
    const float interior = ancho - 2 * px(kRelleno);
    const float fila = px(61);
    const EstadoDeIdentidad estado = pantalla_.estado_identidad;
    const bool sin_identidad = estado == EstadoDeIdentidad::danada || estado == EstadoDeIdentidad::error;
    const bool con_maestra = pantalla_.con_maestra;
    const std::string_view problema = t(estado == EstadoDeIdentidad::error ? Clave::seguridad_error : Clave::seguridad_danada);
    const float abajo = sin_identidad ? alto_envuelto(problema, interior, medida::letra_chica) + px(8) : px(44);
    const bool con_aviso = !pantalla_.aviso_identidad.empty();
    const float alto = px(64) + 3 * fila + px(16) + abajo + (con_aviso ? px(26) : 0) + px(kRelleno);
    panel(desde, ImVec2(desde.x + ancho, desde.y + alto));
    float y = titulo(desde, Clave::ajustes_seguridad);
    const float boton_y = (fila - px(36)) / 2;
    float derecha = x + interior - px(12);
    ImGui::BeginDisabled(estado != EstadoDeIdentidad::lista || pantalla_.calculando);
    if (con_maestra) {
      if (a_la_izquierda("##quitar_maestra", t(Clave::maestra_quitar), Estilo::contorneado, derecha, y + boton_y)) {
        abrir_dialogo(DialogoIdentidad::quitar);
      }
      if (a_la_izquierda("##cambiar_maestra", t(Clave::ajustes_cambiar), Estilo::contorneado, derecha, y + boton_y)) {
        abrir_dialogo(DialogoIdentidad::cambiar);
      }
    } else if (a_la_izquierda("##activar_maestra", t(Clave::maestra_activar), Estilo::contorneado, derecha,
                              y + boton_y)) {
      abrir_dialogo(DialogoIdentidad::activar);
    }
    ImGui::EndDisabled();
    y = fila_de_seguridad(x, y, interior, Clave::maestra, Clave::maestra_ayuda, derecha);
    const float ancho_lista = px(250);
    derecha = x + interior - px(12) - ancho_lista;
    ImGui::BeginDisabled(!con_maestra);
    if (const auto elegido = lista("##bloqueo", ImVec2(derecha, y + (fila - px(kAltoDesplegable)) / 2), ancho_lista,
                                   {Clave::bloqueo_nunca, Clave::bloqueo_windows, Clave::bloqueo_5, Clave::bloqueo_15,
                                    Clave::bloqueo_60},
                                   valor(Ajuste::bloqueo, 0))) {
      fijar(Ajuste::bloqueo, static_cast<std::uint8_t>(*elegido));
    }
    ImGui::EndDisabled();
    y = fila_de_seguridad(x, y, interior, Clave::bloqueo, con_maestra ? Clave::bloqueo_ayuda : Clave::bloqueo_sin_maestra,
                          derecha - px(12));
    derecha = x + interior - px(12);
    ImGui::BeginDisabled(estado == EstadoDeIdentidad::cargando || pantalla_.calculando);
    pedido_.importar_identidad = a_la_izquierda("##importar_identidad", t(Clave::seguridad_importar),
                                                Estilo::esquineros, derecha, y + boton_y);
    if (a_la_izquierda("##exportar_identidad",
                       t(sin_identidad ? Clave::seguridad_crear_otra : Clave::seguridad_exportar), Estilo::principal,
                       derecha, y + boton_y)) {
      if (sin_identidad) {
        abrir_dialogo(DialogoIdentidad::crear);
      } else {
        pedido_.exportar_identidad = true;
      }
    }
    ImGui::EndDisabled();
    y = fila_de_seguridad(x, y, interior, Clave::seguridad_copia, Clave::seguridad_copia_ayuda, derecha);
    y += px(16);
    if (sin_identidad) {
      escribir_envuelto(ImVec2(x, y), problema, color::rojo, interior, medida::letra_chica);
    } else if (estado == EstadoDeIdentidad::lista) {
      escribir_en(x, y + px(9), con_datos(t(Clave::seguridad_tu_identidad), {pantalla_.identidad}), color::texto2,
                  Letra::normal, medida::letra_chica, interior);
      escribir_en(x, y + px(31), t(pantalla_.copia_hecha ? Clave::seguridad_copia_hecha : Clave::seguridad_sin_copia),
                  pantalla_.copia_hecha ? color::verde : color::amarillo, Letra::normal, medida::letra_chica, interior);
    }
    y += abajo;
    if (con_aviso) {
      escribir_en(x, y + px(13), pantalla_.aviso_identidad, pantalla_.aviso_identidad_es_error ? color::rojo : color::verde,
                  Letra::normal, medida::letra_chica, interior);
    }
    return desde.y + alto;
  }

  void abrir_dialogo(DialogoIdentidad cual) {
    pantalla_.dialogo_identidad = cual;
    pantalla_.abrir_dialogo_identidad = true;
    pantalla_.error_identidad.clear();
  }

  bool contrasenas_validas() {
    const DatosDeDialogo& datos = kDialogos[static_cast<std::size_t>(pantalla_.dialogo_identidad)];
    const std::string_view primera(pantalla_.contrasena.data());
    const std::string_view segunda(pantalla_.repetir.data());
    const auto letras = static_cast<std::size_t>(
        std::count_if(primera.begin(), primera.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
    if (datos.actual && pantalla_.actual[0] == '\0') {
      return false;
    }
    if (!datos.repetir) {
      return !datos.nueva || !primera.empty();
    }
    pantalla_.error_identidad =
        letras < cliente::kMinimoContrasena
            ? con_datos(t(Clave::seguridad_corta), {std::to_string(cliente::kMinimoContrasena)})
            : primera != segunda ? s(Clave::seguridad_distintas) : std::string();
    return pantalla_.error_identidad.empty();
  }

  void dialogo_de_identidad() {
    const DialogoIdentidad cual = pantalla_.dialogo_identidad;
    if (cual == DialogoIdentidad::ninguno) {
      return;
    }
    if (pantalla_.abrir_dialogo_identidad) {
      ImGui::OpenPopup("##identidad");
      pantalla_.abrir_dialogo_identidad = false;
    }
    const DatosDeDialogo& datos = kDialogos[static_cast<std::size_t>(cual)];
    const int campos = static_cast<int>(datos.actual) + static_cast<int>(datos.nueva) + static_cast<int>(datos.repetir);
    const std::string texto_ayuda = con_datos(t(datos.ayuda), {pantalla_.identidad, pantalla_.identidad_nueva});
    const std::string_view estado =
        pantalla_.calculando ? t(datos.calculando) : std::string_view(pantalla_.error_identidad);
    const float ancho = px(440);
    const float interior = ancho - px(40);
    const float alto_ayuda = alto_envuelto(texto_ayuda, interior, medida::letra_chica);
    const float alto = px(56) + alto_ayuda + px(14) + static_cast<float>(campos) * px(46) +
                       (estado.empty() ? 0 : px(24)) + px(56);
    ImGui::SetNextWindowPos(ImVec2(ancho_ / 2, alto_ / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(ancho, alto));
    if (!ImGui::BeginPopupModal("##identidad", nullptr,
                                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoScrollbar)) {
      pantalla_.dialogo_identidad = DialogoIdentidad::ninguno;
      pedido_.cancelar_identidad = !pantalla_.calculando;
      return;
    }
    if (std::exchange(pantalla_.cerrar_dialogo_identidad, false)) {
      cerrar_ventana_emergente();
      ImGui::EndPopup();
      pantalla_.dialogo_identidad = DialogoIdentidad::ninguno;
      return;
    }
    const ImVec2 desde = ImGui::GetWindowPos();
    triangulos(*ImGui::GetWindowDrawList(), desde, ImVec2(desde.x + ancho, desde.y + alto));
    const float x = desde.x + px(20);
    float y = desde.y + px(20);
    escribir_cortado(ImVec2(x, y), interior, t(datos.titulo), color::texto, Letra::titulo, medida::letra_tarjeta);
    y += px(36);
    escribir_envuelto(ImVec2(x, y), texto_ayuda, color::texto2, interior, medida::letra_chica);
    y += alto_ayuda + px(14);
    ImGui::BeginDisabled(pantalla_.calculando);
    bool confirmar = false;
    bool enfocar = std::exchange(pantalla_.enfocar_contrasena, false) || ImGui::IsWindowAppearing();
    const auto escribir_contrasena = [&](const char* id, Clave pista, Campo& valor) {
      if (std::exchange(enfocar, false)) {
        ImGui::SetKeyboardFocusHere();
      }
      ImGui::SetCursorScreenPos(ImVec2(x, y));
      confirmar = campo(id, t(pista), valor, ImVec2(interior, px(36)), Icono::ninguno,
                        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_Password, color::pozo) ||
                  confirmar;
      y += px(46);
    };
    if (datos.actual) {
      escribir_contrasena("##actual", Clave::maestra_actual, pantalla_.actual);
    }
    if (datos.nueva) {
      escribir_contrasena("##contrasena_copia", datos.actual ? Clave::maestra_nueva : Clave::seguridad_contrasena,
                          pantalla_.contrasena);
    }
    if (datos.repetir) {
      escribir_contrasena("##repetir_copia", Clave::seguridad_repetir, pantalla_.repetir);
    }
    if (!estado.empty()) {
      escribir_cortado(ImVec2(x, y), interior, estado, pantalla_.calculando ? color::apagado : color::rojo,
                       Letra::normal, medida::letra_chica);
      y += px(24);
    }
    const std::string_view no = t(Clave::inicio_cancelar);
    const std::string_view texto_si = t(datos.si);
    const float ancho_no = ancho_boton(no, Estilo::contorneado);
    const float ancho_si = ancho_boton(texto_si, Estilo::principal);
    ImGui::SetCursorScreenPos(ImVec2(x + interior - ancho_no, y + px(6)));
    const bool cancelar = boton("##no_identidad", no, Estilo::contorneado, ImVec2(ancho_no, px(medida::alto_principal))) ||
                          (ImGui::IsKeyPressed(ImGuiKey_Escape) && !pantalla_.calculando);
    ImGui::SetCursorScreenPos(ImVec2(x + interior - ancho_no - px(10) - ancho_si, y + px(6)));
    confirmar = boton("##si_identidad", texto_si, Estilo::principal, ImVec2(ancho_si, px(medida::alto_principal))) ||
                confirmar;
    ImGui::EndDisabled();
    if (confirmar && !pantalla_.calculando && contrasenas_validas()) {
      pedido_.confirmar_identidad = true;
    }
    if (cancelar) {
      pedido_.cancelar_identidad = true;
      cerrar_ventana_emergente();
      pantalla_.dialogo_identidad = DialogoIdentidad::ninguno;
    }
    ImGui::EndPopup();
  }

  void avisos() {
    const auto& servidores = guardados_.servidores;
    const auto guardado = std::find_if(servidores.begin(), servidores.end(), [this](const auto& servidor) {
      return pantalla_.avisos_de && servidor.direccion == *pantalla_.avisos_de;
    });
    if (guardado == servidores.end()) {
      return;
    }
    EleccionDeAvisos eleccion = dialogo_avisos(pantalla_.idioma, red::a_texto(guardado->direccion), guardado->avisos,
                                               nullptr, cliente::ahora_ms(), pantalla_.abrir_avisos);
    if (eleccion.avisos) {
      pedido_.avisos.emplace(static_cast<std::size_t>(guardado - servidores.begin()), std::move(*eleccion.avisos));
    }
    pedido_.guardar = pedido_.guardar || eleccion.cerrado;
  }

  PantallaAjustes& pantalla_;
  const cliente::Guardados& guardados_;
  float ancho_;
  float alto_;
  std::optional<cliente::Guardados> cambiados_;
  PedidoAjustes pedido_;
};

}

PedidoAjustes dibujar_ajustes(PantallaAjustes& pantalla, const cliente::Guardados& guardados) {
  return Dibujo(pantalla, guardados).dibujar();
}

PedidoAjustes dibujar_bloqueo(PantallaAjustes& pantalla, const cliente::Guardados& guardados) {
  return Dibujo(pantalla, guardados).bloqueo();
}

}
