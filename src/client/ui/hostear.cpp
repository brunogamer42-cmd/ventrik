#include "client/ui/hostear.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <windows.h>

#include <imgui_internal.h>

#include "client/mi_identidad.h"
#include "common/texto.h"
#include "common/utf.h"

namespace ventrik::ui {
namespace {
constexpr float kAltoBarra = 54;
constexpr float kMargen = 20;
constexpr float kAnchoLista = 271;
constexpr float kRelleno = 17;
constexpr float kAltoItem = 53;
constexpr float kAltoCampo = 38;
constexpr float kHueco = 16;
constexpr std::size_t kPasosDelResumen = 6;
constexpr Clave kNada = Clave::titulo_ventana;

struct DatosDePaso {
  Clave nombre;
  Clave ayuda;
  Clave abajo;
  Clave resumen;
  int etapa;
  std::array<std::pair<Clave, Clave>, 3> filas;
  Clave nota;
};

constexpr std::array<std::pair<Clave, Clave>, 3> kSinFilas = {{{kNada, kNada}, {kNada, kNada}, {kNada, kNada}}};

constexpr std::array<DatosDePaso, kPasos> kDatos = {{
    {Clave::hostear_paso_nombre, Clave::hostear_nombre_ayuda, kNada, kNada, 0, kSinFilas, kNada},
    {Clave::hostear_paso_conexion,
     Clave::hostear_conexion_ayuda,
     Clave::hostear_conexion_abajo,
     Clave::hostear_conexion_resumen,
     6,
     {{{Clave::hostear_puerto, Clave::hostear_puerto_valor},
       {Clave::hostear_placa, Clave::hostear_placa_valor},
       {Clave::hostear_modem, Clave::hostear_modem_valor}}},
     Clave::hostear_conexion_nota},
    {Clave::hostear_paso_capacidad,
     Clave::hostear_capacidad_ayuda,
     Clave::hostear_capacidad_abajo,
     Clave::hostear_capacidad_resumen,
     8,
     {{{Clave::hostear_maximo, Clave::hostear_maximo_valor},
       {Clave::hostear_calidad, Clave::hostear_calidad_valor},
       {kNada, kNada}}},
     Clave::hostear_capacidad_nota},
    {Clave::hostear_paso_seguridad, Clave::hostear_seguridad_ayuda, kNada, kNada, 0, kSinFilas, kNada},
    {Clave::hostear_paso_chat,
     Clave::hostear_chat_ayuda,
     Clave::hostear_chat_abajo,
     Clave::hostear_chat_resumen,
     5,
     {{{Clave::hostear_historial, Clave::hostear_historial_valor},
       {Clave::hostear_archivos, Clave::hostear_archivos_valor},
       {kNada, kNada}}},
     Clave::hostear_chat_nota},
    {Clave::hostear_paso_pantallas,
     Clave::hostear_pantallas_ayuda,
     Clave::hostear_pantallas_abajo,
     Clave::hostear_pantallas_resumen,
     12,
     {{{Clave::hostear_compartir, Clave::hostear_compartir_valor},
       {Clave::hostear_varias, Clave::hostear_varias_valor},
       {kNada, kNada}}},
     Clave::hostear_pantallas_nota},
    {Clave::hostear_paso_revisar, Clave::hostear_revisar_ayuda, Clave::hostear_revisar_abajo, kNada, 0, kSinFilas,
     kNada},
    {Clave::hostear_paso_invitar, kNada, Clave::hostear_invitar_abajo, kNada, 6, kSinFilas, kNada},
}};

constexpr std::array<Clave, kNiveles> kNombreDeNivel = {Clave::nivel_basico, Clave::nivel_reforzado,
                                                        Clave::nivel_maximo, Clave::nivel_personalizado};
constexpr std::array<Clave, kNiveles> kAyudaDeNivel = {Clave::nivel_basico_ayuda, Clave::nivel_reforzado_ayuda,
                                                       Clave::nivel_maximo_ayuda, Clave::nivel_personalizado_ayuda};

struct Lista {
  Opcion opcion;
  Clave nombre;
  std::array<Clave, 4> valores;
};

constexpr std::array<Lista, 5> kListas = {{
    {Opcion::algoritmo,
     Clave::opcion_algoritmo,
     {Clave::algoritmo_automatico, Clave::algoritmo_aes, Clave::algoritmo_chacha, kNada}},
    {Opcion::renovar_voz,
     Clave::opcion_renovar,
     {Clave::renovar_15, Clave::renovar_30, Clave::renovar_60, Clave::renovar_240}},
    {Opcion::aprobar_nuevos, Clave::opcion_nuevos, {Clave::nuevos_directo, Clave::nuevos_aprobacion, kNada, kNada}},
    {Opcion::segundo_paso,
     Clave::opcion_segundo_paso,
     {Clave::segundo_nadie, Clave::segundo_admins, Clave::segundo_todos, kNada}},
    {Opcion::programa_minimo, Clave::opcion_minimo, {Clave::minimo_compatible, Clave::minimo_arreglos, kNada, kNada}},
}};

constexpr std::array<std::pair<Opcion, Clave>, 8> kCasillas = {{
    {Opcion::historial_cifrado, Clave::opcion_historial},
    {Opcion::sin_historial, Clave::opcion_sin_historial},
    {Opcion::avisos_de_admins, Clave::opcion_avisos},
    {Opcion::voz_fija, Clave::opcion_voz_fija},
    {Opcion::reconocer_equipos, Clave::opcion_equipos},
    {Opcion::antivirus, Clave::opcion_antivirus},
    {Opcion::rechazar_comprimidos, Clave::opcion_comprimidos},
    {Opcion::invitaciones_de_un_uso, Clave::opcion_un_uso},
}};

struct ParteDelResumen {
  Opcion opcion;
  std::uint8_t valor;
  Clave clave;
};

constexpr std::array<ParteDelResumen, 6> kResumenDeSeguridad = {{
    {Opcion::historial_cifrado, 1, Clave::resumen_historial},
    {Opcion::sin_historial, 1, Clave::resumen_sin_historial},
    {Opcion::segundo_paso, 1, Clave::resumen_segundo_admins},
    {Opcion::segundo_paso, 2, Clave::resumen_segundo_todos},
    {Opcion::avisos_de_admins, 1, Clave::resumen_avisos},
    {Opcion::voz_fija, 1, Clave::resumen_voz_fija},
}};

struct DatosDeDialogo {
  Clave titulo;
  Clave ayuda;
  Clave si;
};

constexpr std::array<DatosDeDialogo, 5> kDialogos = {{
    {kNada, kNada, kNada},
    {Clave::dialogo_arranque_titulo, Clave::dialogo_arranque_ayuda, Clave::dialogo_arranque_si},
    {Clave::dialogo_exportar_titulo, Clave::dialogo_exportar_ayuda, Clave::dialogo_exportar_si},
    {Clave::dialogo_importar_titulo, Clave::dialogo_importar_ayuda, Clave::hostear_importar},
    {Clave::dialogo_cambiar_titulo, Clave::dialogo_cambiar_ayuda, Clave::hostear_cambiar_clave},
}};

cifrado::Secreto tomar(Campo& campo) {
  cifrado::Secreto secreto = cifrado::secreto_de(campo.data());
  SecureZeroMemory(campo.data(), campo.size());
  olvidar_lo_escrito();
  return secreto;
}

class Dibujo {
 public:
  explicit Dibujo(PantallaHostear& pantalla)
      : pantalla_(pantalla), ancho_(ImGui::GetIO().DisplaySize.x), alto_(ImGui::GetIO().DisplaySize.y) {}

  PedidoHostear dibujar() {
    barra();
    lista();
    contenido();
    pedido_.contrasenas = dibujar_dialogo(pantalla_.idioma, pantalla_.dialogo);
    if (pedido_.cerrar) {
      borrar_contrasenas(pantalla_);
    }
    return std::move(pedido_);
  }

 private:
  std::string_view t(Clave clave) const { return texto(pantalla_.idioma, clave); }
  std::string llega_en(int etapa) const { return con_datos(t(Clave::inicio_llega_en), {std::to_string(etapa)}); }
  static const DatosDePaso& datos(Paso paso) { return kDatos[static_cast<std::size_t>(paso)]; }
  static Paso junto(Paso paso, int cuanto) { return static_cast<Paso>(static_cast<int>(paso) + cuanto); }
  std::uint8_t valor(Opcion opcion) const { return pantalla_.seguridad.opciones[opcion]; }
  std::size_t nivel() const { return static_cast<std::size_t>(pantalla_.seguridad.nivel) % kNiveles; }
  bool con_contrasena() const {
    return pantalla_.configurando ? pantalla_.con_contrasena : pantalla_.contrasena[0] != '\0';
  }

  void etiqueta(ImVec2 donde, Clave clave) const {
    escribir(donde, t(clave), color::apagado, Letra::normal, medida::letra_chica);
  }

  static float nota(ImVec2 donde, float ancho, std::string_view texto_nota, ImU32 color_nota = color::apagado) {
    escribir_envuelto(donde, texto_nota, color_nota, ancho, medida::letra_chica);
    return donde.y + alto_envuelto(texto_nota, ancho, medida::letra_chica);
  }

  static void linea(float x, float y, float ancho) {
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + ancho, y + 1), tono(color::linea));
  }

  template <typename Dibujar>
  void con_etapa(int etapa, Dibujar&& dibujar) const {
    if (etapa == 0) {
      dibujar();
    } else {
      apagado(llega_en(etapa), dibujar);
    }
  }

  void ir(Paso paso) {
    pantalla_.paso = paso;
    pantalla_.error.clear();
  }

  void barra() {
    const bool configurando = pantalla_.configurando;
    const std::string paso =
        configurando ? std::string()
        : pantalla_.paso == Paso::revisar
            ? std::string(t(Clave::hostear_ultimo_paso))
            : con_datos(t(Clave::hostear_paso_de), {std::to_string(static_cast<int>(pantalla_.paso) + 1)});
    pedido_.cerrar =
        barra_con_volver(t(configurando ? Clave::ajustes_volver : Clave::hostear_servidores),
                         t(configurando ? Clave::avisos_configuracion : Clave::hostear_titulo),
                         configurando ? t(Clave::ajustes_se_guardan) : std::string_view(), paso) ||
        (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive() &&
         !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
  }

  std::string abajo(Paso paso) const {
    if (paso == Paso::nombre) {
      return con_datos(t(con_contrasena() ? Clave::hostear_abajo_con : Clave::hostear_abajo_sin),
                       {pantalla_.nombre.data()});
    }
    return std::string(t(paso != Paso::seguridad ? datos(paso).abajo
                         : pantalla_.cargando    ? Clave::hostear_cargando
                                                 : kNombreDeNivel[nivel()]));
  }

  void lista() {
    const float x = px(kMargen);
    const float arriba = px(kAltoBarra) + 1 + px(kMargen);
    const float ancho = px(kAnchoLista);
    const std::string_view ayuda =
        t(pantalla_.configurando ? Clave::hostear_lista_nota_config : Clave::hostear_lista_nota);
    const float fin = arriba + px(13) + static_cast<float>(kPasos) * px(kAltoItem);
    panel(ImVec2(x, arriba),
          ImVec2(x + ancho, fin + px(23) + alto_envuelto(ayuda, ancho - px(34), medida::letra_chica) + px(16)));
    for (std::size_t i = 0; i < kPasos; ++i) {
      item(static_cast<Paso>(i), ImVec2(x + px(13), arriba + px(13) + static_cast<float>(i) * px(kAltoItem)),
           ancho - px(25));
    }
    linea(x + px(17), fin + px(6), ancho - px(29));
    nota(ImVec2(x + px(17), fin + px(17)), ancho - px(34), ayuda);
  }

  void item(Paso paso, ImVec2 desde, float ancho) {
    const int numero = static_cast<int>(paso);
    const ImVec2 hasta(desde.x + ancho, desde.y + px(kAltoItem - 3));
    const bool elegido = pantalla_.paso == paso;
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    ImGui::PushID(numero);
    ImGui::BeginDisabled(paso == Paso::invitar);
    ImGui::SetCursorScreenPos(desde);
    if (ImGui::InvisibleButton("##paso", ImVec2(ancho, hasta.y - desde.y), ImGuiButtonFlags_EnableNav)) {
      ir(paso);
    }
    if (elegido || ImGui::IsItemHovered()) {
      dibujo.AddRectFilled(desde, hasta, tono(color::seleccion));
    }
    const ImVec2 caja(desde.x + px(8), desde.y + px(9));
    const ImVec2 fin_caja(caja.x + px(24), caja.y + px(24));
    if (elegido) {
      dibujo.AddRectFilled(desde, ImVec2(desde.x + px(3), hasta.y), tono(color::claro));
      const auto forma = corte_abajo_derecha(caja, fin_caja, px(6));
      dibujo.AddConvexPolyFilled(forma.data(), static_cast<int>(forma.size()), tono(color::claro));
    } else {
      dibujo.AddRectFilled(caja, fin_caja, tono(color::pozo));
      dibujo.AddRect(caja, fin_caja, tono(color::linea2));
    }
    if (!pantalla_.configurando && paso < pantalla_.paso) {
      ImGui::RenderCheckMark(&dibujo, ImVec2(caja.x + px(6), caja.y + px(6)), tono(color::verde), px(12));
    } else {
      const std::string cifra = std::to_string(numero + 1);
      const ImVec2 tamano = medir(cifra, Letra::fuerte, medida::letra_chica);
      escribir(ImVec2(caja.x + (px(24) - tamano.x) / 2, caja.y + (px(24) - tamano.y) / 2), cifra,
               elegido ? color::tinta : color::texto2, Letra::fuerte, medida::letra_chica);
    }
    const float texto_x = desde.x + px(42);
    escribir_en(texto_x, desde.y + px(17), t(datos(paso).nombre), color::texto, elegido ? Letra::fuerte : Letra::normal,
                medida::letra, hasta.x - texto_x - px(4));
    escribir_en(texto_x, desde.y + px(35), abajo(paso), color::apagado, Letra::normal, medida::letra_chica,
                hasta.x - texto_x - px(4));
    ImGui::EndDisabled();
    ImGui::PopID();
  }

  void contenido() {
    const float izquierda = px(kMargen + kAnchoLista);
    const float arriba = px(kAltoBarra) + 1;
    ImGui::SetCursorScreenPos(ImVec2(izquierda, arriba));
    if (std::exchange(pantalla_.dibujado, pantalla_.paso) != pantalla_.paso) {
      ImGui::SetNextWindowScroll(ImVec2(0, 0));
    }
    ImGui::BeginChild("##contenido_hostear", ImVec2(ancho_ - izquierda, alto_ - arriba));
    const float x = izquierda + px(21);
    const float ancho = ancho_ - x - px(kMargen);
    float y = ImGui::GetCursorScreenPos().y;
    const DatosDePaso& paso = datos(pantalla_.paso);
    if (pantalla_.configurando) {
      y -= px(26);
    } else {
      escribir_espaciado(ImVec2(x, y + px(23)),
                         con_datos(t(Clave::hostear_paso_n), {std::to_string(static_cast<int>(pantalla_.paso) + 1)}),
                         color::apagado, Letra::negrita, medida::letra_mini, px(1.2f));
    }
    escribir_en(x, y + px(57), t(paso.nombre), color::texto, Letra::titulo, medida::letra_servidor);
    escribir_en(x, y + px(87), t(paso.ayuda), color::texto2, Letra::normal, medida::letra, ancho);
    y += px(123);
    switch (pantalla_.paso) {
      case Paso::nombre:
        y = nombre_y_acceso(x, y, ancho);
        break;
      case Paso::seguridad:
        y = seguridad(x, y, ancho);
        break;
      case Paso::revisar:
        y = revisar(x, y, ancho);
        break;
      default:
        y = en_desarrollo(x, y, ancho, paso);
        break;
    }
    y = pie(x, y, ancho);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::Dummy(ImVec2(1, px(kMargen)));
    ImGui::EndChild();
  }

  template <typename Contenido>
  float seccion(float x, float y, float ancho, std::string_view titulo, bool en_desarrollo, Contenido&& dentro) {
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    dibujo.ChannelsSplit(2);
    dibujo.ChannelsSetCurrent(1);
    const float fin = dentro(x + px(kRelleno), y + px(30), ancho - 2 * px(kRelleno)) + px(kRelleno);
    const ImVec2 tamano = medir(titulo, Letra::titulo, medida::letra_bloque);
    const float izquierda = x + px(23);
    const std::string_view chip_texto = t(Clave::ajustes_en_desarrollo);
    const float derecha = izquierda + tamano.x + (en_desarrollo ? px(10) + ancho_chip(chip_texto) : 0);
    dibujo.AddRectFilled(ImVec2(izquierda - px(6), y - tamano.y / 2), ImVec2(derecha + px(6), y + tamano.y / 2),
                         tono(color::fondo));
    escribir(ImVec2(izquierda, y - tamano.y / 2), titulo, color::texto, Letra::titulo, medida::letra_bloque);
    if (en_desarrollo) {
      chip(ImVec2(izquierda + tamano.x + px(10), y - px(8)), chip_texto);
    }
    dibujo.ChannelsSetCurrent(0);
    panel(ImVec2(x, y), ImVec2(x + ancho, fin));
    if (caja_) {
      dibujo.AddRectFilled(caja_->first, caja_->second, tono(color::pozo));
      dibujo.AddRect(caja_->first, caja_->second, tono(color::linea));
      caja_.reset();
    }
    dibujo.ChannelsMerge();
    return fin;
  }

  float nombre_y_acceso(float x, float y, float ancho) {
    y = seccion(x, y, ancho, t(Clave::hostear_tu_servidor), false, [&](float cx, float cy, float interior) {
      const float mitad = (interior - px(kHueco)) / 2;
      const float derecha = cx + mitad + px(kHueco);
      etiqueta(ImVec2(cx, cy), Clave::hostear_nombre);
      etiqueta(ImVec2(derecha, cy), Clave::hostear_contrasena);
      ImGui::SetCursorScreenPos(ImVec2(cx, cy + px(20)));
      const bool listo = campo("##nombre_servidor", {}, pantalla_.nombre, ImVec2(mitad, px(kAltoCampo)), Icono::ninguno,
                               ImGuiInputTextFlags_EnterReturnsTrue, color::pozo);
      if (pantalla_.configurando && (listo || ImGui::IsItemDeactivatedAfterEdit())) {
        cambiar_nombre();
      }
      float fin = cx + interior;
      if (pantalla_.configurando && boton_a_la_izquierda("##guardar_contrasena", t(Clave::hostear_guardar_contrasena),
                                                         Estilo::contorneado, fin, cy + px(20), kAltoCampo)) {
        pedido_.contrasena = tomar(pantalla_.contrasena);
      }
      ImGui::SetCursorScreenPos(ImVec2(derecha, cy + px(20)));
      campo("##contrasena_servidor",
            t(pantalla_.configurando && pantalla_.con_contrasena ? Clave::hostear_contrasena_cambiar
                                                                 : Clave::hostear_contrasena_pista),
            pantalla_.contrasena, ImVec2(fin + px(pantalla_.configurando ? 4.0f : 0.0f) - derecha, px(kAltoCampo)),
            Icono::ninguno, ImGuiInputTextFlags_Password, color::pozo);
      nota(ImVec2(derecha, cy + px(66)), interior - mitad - px(kHueco), t(Clave::hostear_contrasena_nota));
      apagado(llega_en(6), [&] { casilla(t(Clave::hostear_solo_invitacion), ImVec2(cx, cy + px(106)), false); });
      return cy + px(135);
    });
    return nota(ImVec2(x, y + px(20)), ancho, t(Clave::hostear_personalizar_nota));
  }

  void cambiar_nombre() {
    const std::string nombre(pantalla_.nombre.data());
    if (!es_nombre_valido(nombre)) {
      pantalla_.error = std::string(t(Clave::hostear_nombre_malo));
      return;
    }
    pantalla_.error.clear();
    pedido_.nombre = nombre;
  }

  float en_desarrollo(float x, float y, float ancho, const DatosDePaso& paso) {
    return seccion(x, y, ancho, t(paso.nombre), true, [&](float cx, float cy, float interior) {
      escribir_en(cx, cy + px(8), con_datos(t(Clave::ajustes_llega_en), {std::to_string(paso.etapa)}), color::texto2,
                  Letra::normal, medida::letra, interior);
      cy += px(30);
      ImGui::BeginDisabled();
      for (const auto& [nombre, valor_fijo] : paso.filas) {
        if (nombre == kNada) {
          continue;
        }
        etiqueta(ImVec2(cx, cy), nombre);
        escribir_en(cx, cy + px(30), t(valor_fijo), color::texto, Letra::fuerte, medida::letra, interior);
        cy += px(52);
      }
      ImGui::EndDisabled();
      return paso.nota == kNada ? cy : nota(ImVec2(cx, cy + px(4)), interior, t(paso.nota));
    });
  }

  float seguridad(float x, float y, float ancho) {
    y = seccion(x, y, ancho, t(Clave::hostear_nivel), false, [&](float cx, float cy, float interior) {
      if (pantalla_.cargando) {
        cy = nota(ImVec2(cx, cy), interior, t(Clave::hostear_cargando));
      } else {
        cy = niveles(cx, cy - px(2), interior);
        cy = que_incluye(cx, cy + px(15), interior);
      }
      if (!pantalla_.cargando && pantalla_.seguridad.nivel == Nivel::maximo && pantalla_.xp) {
        cy = nota(ImVec2(cx, cy + px(12)), interior, t(Clave::hostear_xp_relay), color::amarillo);
      }
      if (!pantalla_.configurando && valor(Opcion::historial_cifrado) != 0) {
        cy = arranque(cx, cy + px(16), interior);
      }
      linea(cx, cy + px(14), interior);
      return identidad(cx, cy + px(25), interior);
    });
    if (pantalla_.configurando) {
      y = admins(x, y + px(28), ancho);
      y = registro(x, y + px(28), ancho);
    }
    return y;
  }

  void elegir(Nivel nuevo) {
    const Seguridad antes = pantalla_.seguridad;
    elegir_nivel(pantalla_.seguridad, nuevo);
    pantalla_.ver_opciones = pantalla_.ver_opciones || nuevo == Nivel::personalizado;
    if (pantalla_.configurando &&
        (antes.nivel != pantalla_.seguridad.nivel || antes.opciones != pantalla_.seguridad.opciones)) {
      pedido_.seguridad = pantalla_.seguridad;
    }
  }

  void cambiar(Opcion opcion, std::uint8_t nuevo) {
    if (cambiar_opcion(pantalla_.seguridad, opcion, nuevo) && pantalla_.configurando) {
      pedido_.seguridad = pantalla_.seguridad;
    }
  }

  float niveles(float cx, float cy, float interior) {
    const float hueco = px(9);
    const float ancho = (interior - 3 * hueco) / static_cast<float>(kNiveles);
    const float ancho_ayuda = ancho - px(46);
    float alto = px(70);
    for (const Clave ayuda : kAyudaDeNivel) {
      alto = std::max(alto, px(50) + alto_envuelto(t(ayuda), ancho_ayuda, medida::letra_chica));
    }
    ImDrawList& dibujo = *ImGui::GetWindowDrawList();
    for (std::size_t i = 0; i < kNiveles; ++i) {
      const auto este = static_cast<Nivel>(i);
      const ImVec2 desde(cx + static_cast<float>(i) * (ancho + hueco), cy);
      const ImVec2 hasta(desde.x + ancho, cy + alto);
      ImGui::SetCursorScreenPos(desde);
      ImGui::PushID(static_cast<int>(i));
      if (ImGui::InvisibleButton("##nivel", ImVec2(ancho, alto), ImGuiButtonFlags_EnableNav)) {
        elegir(este);
      }
      ImGui::PopID();
      const bool elegido = pantalla_.seguridad.nivel == este;
      dibujo.AddRectFilled(desde, hasta, tono(elegido ? color::elevado : color::pozo));
      dibujo.AddRect(desde, hasta, tono(elegido ? color::claro : color::linea), 0, 0, elegido ? px(1.5f) : 1);
      const ImVec2 centro(desde.x + px(19), desde.y + px(22));
      dibujo.AddCircleFilled(centro, px(6.5f), tono(color::claro));
      if (elegido) {
        dibujo.AddCircleFilled(centro, px(4.5f), tono(color::tinta));
        dibujo.AddCircleFilled(centro, px(2.5f), tono(color::claro));
      }
      const float lugar = ancho - px(42);
      const std::string_view corto = t(kNombreDeNivel[i]);
      const std::string largo = este == Nivel::basico ? con_datos(t(Clave::hostear_predeterminado), {corto}) : "";
      escribir_en(desde.x + px(36), centro.y,
                  !largo.empty() && medir(largo, Letra::fuerte, medida::letra).x <= lugar ? std::string_view(largo)
                                                                                          : corto,
                  color::texto, Letra::fuerte, medida::letra, lugar);
      escribir_envuelto(ImVec2(desde.x + px(36), desde.y + px(38)), t(kAyudaDeNivel[i]), color::apagado, ancho_ayuda,
                        medida::letra_chica);
    }
    return cy + alto;
  }

  float que_incluye(float cx, float cy, float interior) {
    const ImVec2 desde(cx, cy);
    const float medio = cy + px(20.5f);
    ImGui::SetCursorScreenPos(desde);
    if (ImGui::InvisibleButton("##que_incluye", ImVec2(interior, px(41)), ImGuiButtonFlags_EnableNav)) {
      pantalla_.ver_opciones = !pantalla_.ver_opciones;
    }
    dibujar_icono(*ImGui::GetWindowDrawList(), pantalla_.ver_opciones ? Icono::flecha_abajo : Icono::flecha_derecha,
                  ImVec2(cx + px(20), medio), px(11), tono(ImGui::IsItemHovered() ? color::texto : color::texto2));
    const float fin = escribir_en(cx + px(34), medio, t(Clave::hostear_que_incluye), color::texto, Letra::fuerte,
                                  medida::letra, interior - px(50));
    escribir_en(fin + px(8), medio, t(Clave::hostear_que_incluye_nota), color::apagado, Letra::normal, medida::letra,
                std::max(px(1), cx + interior - fin - px(16)));
    float fin_caja = cy + px(41);
    if (pantalla_.ver_opciones) {
      fin_caja = opciones(cx + px(13), fin_caja + px(4), interior - px(26)) + px(13);
    }
    caja_ = std::make_pair(desde, ImVec2(cx + interior, fin_caja));
    return fin_caja;
  }

  float opciones(float ox, float oy, float ancho) {
    const float mitad = (ancho - px(kHueco)) / 2;
    const float derecha = ox + mitad + px(kHueco);
    const std::string_view cifrado_nota = t(Clave::hostear_cifrado_nota);
    const float fin_cifrado = oy + px(84) + alto_envuelto(cifrado_nota, ancho - px(26), medida::letra_chica) + px(12);
    ImGui::GetWindowDrawList()->AddRect(ImVec2(ox, oy), ImVec2(ox + ancho, fin_cifrado), tono(color::linea));
    escribir_en(ox + px(13), oy + px(22), t(Clave::hostear_cifrado), color::texto, Letra::fuerte);
    etiqueta(ImVec2(ox + px(13), oy + px(38)), Clave::hostear_cifrado_chat);
    etiqueta(ImVec2(derecha, oy + px(38)), Clave::hostear_cifrado_voz);
    escribir(ImVec2(ox + px(13), oy + px(56)), t(Clave::hostear_tls), color::texto);
    escribir(ImVec2(derecha, oy + px(56)), t(Clave::hostear_dtls), color::texto);
    nota(ImVec2(ox + px(13), oy + px(84)), ancho - px(26), cifrado_nota);
    const std::string version = a_texto(kUltimaConArreglos);
    float y = fin_cifrado + px(22);
    for (std::size_t i = 0; i < kListas.size(); ++i) {
      lista_de_opcion(kListas[i], ImVec2(i % 2 == 0 ? ox : derecha, y), mitad, version);
      y += i % 2 == 1 || i + 1 == kListas.size() ? px(68) : 0;
    }
    for (const auto& [opcion, clave] : kCasillas) {
      con_etapa(etapa_de(opcion), [&, opcion = opcion, clave = clave] {
        if (casilla(t(clave), ImVec2(ox, y), valor(opcion) != 0)) {
          cambiar(opcion, valor(opcion) != 0 ? 0 : 1);
        }
      });
      y += px(40);
    }
    return nota(ImVec2(ox, y + px(4)), ancho, t(Clave::hostear_siempre_activo));
  }

  void lista_de_opcion(const Lista& cual, ImVec2 donde, float ancho, const std::string& version) {
    con_etapa(etapa_de(cual.opcion), [&] {
      etiqueta(donde, cual.nombre);
      std::vector<std::string> nombres;
      for (std::uint8_t i = 0; i < valores_de(cual.opcion); ++i) {
        nombres.push_back(con_datos(t(cual.valores[i]), {version}));
      }
      ImGui::PushID(static_cast<int>(cual.opcion));
      if (const auto nuevo =
              desplegable("##opcion", ImVec2(donde.x, donde.y + px(18)), ancho, nombres, valor(cual.opcion))) {
        cambiar(cual.opcion, static_cast<std::uint8_t>(*nuevo));
      }
      ImGui::PopID();
    });
  }

  float arranque(float cx, float cy, float interior) {
    const float mitad = (interior - px(kHueco)) / 2;
    const float derecha = cx + mitad + px(kHueco);
    etiqueta(ImVec2(cx, cy), Clave::hostear_arranque);
    etiqueta(ImVec2(derecha, cy), Clave::hostear_repetir);
    ImGui::SetCursorScreenPos(ImVec2(cx, cy + px(20)));
    campo("##arranque", t(Clave::hostear_arranque_pista), pantalla_.arranque, ImVec2(mitad, px(kAltoCampo)),
          Icono::ninguno, ImGuiInputTextFlags_Password, color::pozo);
    ImGui::SetCursorScreenPos(ImVec2(derecha, cy + px(20)));
    campo("##repetir_arranque", {}, pantalla_.repetir, ImVec2(mitad, px(kAltoCampo)), Icono::ninguno,
          ImGuiInputTextFlags_Password, color::pozo);
    return nota(ImVec2(cx, cy + px(66)), interior, t(Clave::hostear_arranque_nota));
  }

  float identidad(float cx, float cy, float interior) {
    const bool aca = !pantalla_.configurando || pantalla_.en_esta_pc;
    float derecha = cx + interior;
    if (aca) {
      ImGui::BeginDisabled(pantalla_.ocupado);
      const auto con_servidor = [&](Clave aviso, auto&& dibujar) {
        if (pantalla_.existe) {
          dibujar();
        } else {
          apagado(t(aviso), dibujar);
        }
      };
      con_servidor(Clave::hostear_cambiar_clave_despues, [&] {
        pedido_.cambiar_clave = boton_a_la_izquierda("##cambiar_clave", t(Clave::hostear_cambiar_clave),
                                                     Estilo::esquineros, derecha, cy, 40);
      });
      pedido_.importar =
          boton_a_la_izquierda("##importar_servidor", t(Clave::hostear_importar), Estilo::esquineros, derecha, cy, 40);
      con_servidor(Clave::hostear_exportar_despues, [&] {
        pedido_.exportar = boton_a_la_izquierda("##exportar_servidor", t(Clave::hostear_exportar), Estilo::contorneado,
                                                derecha, cy, 40);
      });
      ImGui::EndDisabled();
    }
    const bool sin_huella = pantalla_.huella.empty();
    ImGui::BeginDisabled(sin_huella);
    if (boton_a_la_izquierda("##copiar_huella",
                             t(pantalla_.huella_copiada ? Clave::hostear_huella_copiada : Clave::hostear_copiar_huella),
                             Estilo::contorneado, derecha, cy, 40)) {
      ImGui::SetClipboardText(pantalla_.huella.c_str());
      pantalla_.huella_copiada = true;
    }
    ImGui::EndDisabled();
    etiqueta(ImVec2(cx, cy + px(2)), Clave::hostear_huella);
    const std::string_view huella = sin_huella ? t(Clave::hostear_huella_al_crear) : std::string_view(pantalla_.huella);
    const float ancho_huella = derecha - cx;
    escribir_envuelto(ImVec2(cx, cy + px(20)), huella, sin_huella ? color::apagado : color::texto, ancho_huella,
                      medida::letra);
    const float y = std::max(cy + px(40), cy + px(20) + alto_envuelto(huella, ancho_huella, medida::letra)) + px(10);
    const Clave estado = !aca                  ? Clave::hostear_en_otra_pc
                         : pantalla_.exportada ? Clave::hostear_exportada
                                               : Clave::hostear_sin_exportar;
    escribir_en(cx, y + px(8), t(estado), !aca ? color::apagado : pantalla_.exportada ? color::verde : color::amarillo,
                Letra::normal, medida::letra_chica, interior);
    return nota(ImVec2(cx, y + px(26)), interior, t(Clave::hostear_identidad_nota));
  }

  float admins(float x, float y, float ancho) {
    return seccion(x, y, ancho, t(Clave::hostear_admins), false, [&](float cx, float cy, float interior) {
      if (pantalla_.admins.empty()) {
        return nota(ImVec2(cx, cy), interior, t(Clave::hostear_sin_admins));
      }
      for (std::size_t i = 0; i < pantalla_.admins.size(); ++i) {
        const AdminDelServidor& admin = pantalla_.admins[i];
        float derecha = cx + interior;
        ImGui::PushID(static_cast<int>(i));
        if (boton_a_la_izquierda("##sacar_admin", t(Clave::hostear_sacar_admin), Estilo::esquineros, derecha, cy,
                                 34)) {
          pedido_.sacar_admin = i;
        }
        ImGui::PopID();
        const float fin =
            escribir_en(cx, cy + px(17), admin.nombre, color::texto, Letra::fuerte, medida::letra, derecha - cx);
        escribir_en(fin + px(10), cy + px(17), admin.identidad, color::apagado, Letra::normal, medida::letra_chica,
                    std::max(px(1), derecha - fin - px(10)));
        cy += px(44);
      }
      return cy - px(10);
    });
  }

  float registro(float x, float y, float ancho) {
    return seccion(x, y, ancho, t(Clave::hostear_registro), false, [&](float cx, float cy, float interior) {
      if (pantalla_.registro.empty()) {
        return nota(ImVec2(cx, cy), interior, t(Clave::hostear_registro_vacio));
      }
      const std::size_t cuantas = std::min(pantalla_.registro.size(), kFilasDelRegistro);
      for (std::size_t i = 0; i < cuantas; ++i) {
        if (ImGui::IsRectVisible(ImVec2(cx, cy), ImVec2(cx + interior, cy + px(24)))) {
          const EntradaDelRegistro& entrada = pantalla_.registro[i];
          escribir_en(cx, cy + px(10), entrada.cuando, color::apagado, Letra::normal, medida::letra_chica, px(130));
          escribir_en(cx + px(140), cy + px(10), entrada.que, color::texto2, Letra::normal, medida::letra_chica,
                      interior - px(140));
        }
        cy += px(24);
      }
      return cy;
    });
  }

  std::string resumen(Paso paso) const {
    if (paso == Paso::nombre) {
      return con_datos(t(con_contrasena() ? Clave::hostear_resumen_con : Clave::hostear_resumen_sin),
                       {pantalla_.nombre.data()});
    }
    if (paso != Paso::seguridad) {
      return std::string(t(datos(paso).resumen));
    }
    std::string todo(t(kNombreDeNivel[nivel()]));
    for (const ParteDelResumen& parte : kResumenDeSeguridad) {
      if (valor(parte.opcion) == parte.valor) {
        todo = con_datos(t(Clave::hostear_y), {todo, t(parte.clave)});
      }
    }
    return todo;
  }

  float revisar(float x, float y, float ancho) {
    const float cabeza = px(47);
    const float fila = px(53);
    y -= px(10);
    const float fin = y + cabeza + static_cast<float>(kPasosDelResumen) * fila;
    panel(ImVec2(x, y), ImVec2(x + ancho, fin));
    escribir_en(x + px(13), y + cabeza / 2, t(Clave::hostear_resumen), color::texto, Letra::fuerte);
    for (std::size_t i = 0; i < kPasosDelResumen; ++i) {
      const auto paso = static_cast<Paso>(i);
      const float arriba = y + cabeza + static_cast<float>(i) * fila;
      const float medio = arriba + fila / 2;
      linea(x + 1, arriba, ancho - 2);
      float derecha = x + ancho - px(13);
      ImGui::PushID(static_cast<int>(i));
      if (boton_a_la_izquierda("##cambiar", t(Clave::ajustes_cambiar), Estilo::esquineros, derecha, medio - px(17),
                               34)) {
        ir(paso);
      }
      ImGui::PopID();
      escribir_en(x + px(13), medio, t(datos(paso).nombre), color::texto, Letra::fuerte, medida::letra, px(170));
      escribir_en(x + px(195), medio, resumen(paso), color::texto2, Letra::normal, medida::letra,
                  std::max(px(1), derecha - x - px(195)));
    }
    y = seccion(x, fin + px(28), ancho, t(Clave::hostear_al_iniciar), false, [&](float cx, float cy, float interior) {
      for (const auto& [clave, etapa] : {std::make_pair(Clave::hostear_red_local, 6),
                                         std::make_pair(Clave::hostear_lista_publica, 7),
                                         std::make_pair(Clave::hostear_con_windows, 11)}) {
        apagado(llega_en(etapa), [&, clave = clave] { casilla(t(clave), ImVec2(cx, cy), false); });
        cy += px(46);
      }
      return nota(ImVec2(cx, cy), interior, t(pantalla_.limitado ? Clave::hostear_limitado_si : Clave::hostear_limitado_no));
    });
    if (!pantalla_.configurando && !pantalla_.exportada) {
      y = nota(ImVec2(x, y + px(16)), ancho, t(Clave::hostear_sin_exportar_largo), color::amarillo);
    }
    return y;
  }

  bool siguiente(std::string_view etiqueta_boton, float& derecha, float y) {
    const float ancho = ancho_boton(etiqueta_boton, Estilo::principal, Icono::adelante);
    derecha -= ancho;
    ImGui::SetCursorScreenPos(ImVec2(derecha, y));
    const bool tocado = boton("##siguiente", {}, Estilo::principal, ImVec2(ancho, px(38)));
    const ImVec2 tamano = medir(etiqueta_boton, Letra::fuerte, medida::letra);
    const float medio = y + px(19);
    const float desde = derecha + px(medida::margen_principal);
    escribir(ImVec2(desde, medio - tamano.y / 2), etiqueta_boton, color::tinta, Letra::fuerte);
    dibujar_icono(*ImGui::GetWindowDrawList(), Icono::adelante,
                  ImVec2(desde + tamano.x + px(medida::separacion_icono + medida::icono / 2), medio), px(medida::icono),
                  tono(color::tinta));
    derecha -= px(12);
    return tocado;
  }

  float pie(float x, float y, float ancho) {
    if (!pantalla_.error.empty()) {
      y = nota(ImVec2(x, y + px(14)), ancho, pantalla_.error, color::rojo);
    }
    if (!pantalla_.aviso.empty()) {
      y = nota(ImVec2(x, y + px(14)), ancho, pantalla_.aviso,
               pantalla_.aviso_es_error ? color::rojo
               : pantalla_.ocupado      ? color::apagado
                                        : color::verde);
    }
    if (pantalla_.configurando) {
      return y;
    }
    linea(x, y + px(25), ancho);
    y += px(40);
    const Paso paso = pantalla_.paso;
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    if (paso == Paso::nombre) {
      const std::string_view cancelar = t(Clave::inicio_cancelar);
      pedido_.cerrar =
          boton("##cancelar", cancelar, Estilo::esquineros, ImVec2(ancho_boton(cancelar, Estilo::esquineros), px(38))) ||
          pedido_.cerrar;
    } else {
      const std::string_view atras = t(Clave::hostear_atras);
      if (boton("##atras", atras, Estilo::esquineros,
                ImVec2(ancho_boton(atras, Estilo::esquineros, Icono::atras), px(38)), Icono::atras)) {
        ir(junto(paso, -1));
      }
    }
    float derecha = x + ancho;
    if (paso == Paso::revisar) {
      ImGui::BeginDisabled(pantalla_.ocupado);
      if (boton_a_la_izquierda("##iniciar", t(Clave::hostear_iniciar), Estilo::principal, derecha, y, 38,
                               Icono::reproducir)) {
        iniciar();
      }
      ImGui::EndDisabled();
      return y + px(38);
    }
    const Paso proximo = junto(paso, 1);
    if (siguiente(con_datos(t(Clave::hostear_siguiente), {t(datos(proximo).nombre)}), derecha, y)) {
      ir(proximo);
    }
    if (proximo != Paso::revisar && boton_a_la_izquierda("##recomendado", t(Clave::hostear_recomendado),
                                                         Estilo::contorneado, derecha, y, 38)) {
      ir(Paso::revisar);
    }
    return y + px(38);
  }

  void iniciar() {
    const std::string nombre(pantalla_.nombre.data());
    if (!es_nombre_valido(nombre)) {
      ir(Paso::nombre);
      pantalla_.error = std::string(t(Clave::hostear_nombre_malo));
      return;
    }
    const bool con_arranque = valor(Opcion::historial_cifrado) != 0 && pantalla_.arranque[0] != '\0';
    if (con_arranque) {
      const std::string_view escrita(pantalla_.arranque.data());
      const std::string problema =
          cantidad_de_letras(escrita) < cliente::kMinimoContrasena
              ? con_datos(t(Clave::seguridad_corta), {std::to_string(cliente::kMinimoContrasena)})
          : escrita != std::string_view(pantalla_.repetir.data()) ? std::string(t(Clave::seguridad_distintas))
                                                                  : std::string();
      if (!problema.empty()) {
        ir(Paso::seguridad);
        pantalla_.error = problema;
        return;
      }
    }
    pedido_.crear = Creacion{nombre, tomar(pantalla_.contrasena),
                             con_arranque ? tomar(pantalla_.arranque) : cifrado::Secreto(), pantalla_.seguridad};
    borrar_contrasenas(pantalla_);
  }

  PantallaHostear& pantalla_;
  float ancho_;
  float alto_;
  std::optional<std::pair<ImVec2, ImVec2>> caja_;
  PedidoHostear pedido_;
};

}

PantallaHostear asistente(Idioma idioma) {
  PantallaHostear pantalla;
  pantalla.idioma = idioma;
  poner(pantalla.nombre, texto(idioma, Clave::servidor_nombre_de_fabrica));
  return pantalla;
}

PantallaHostear configuracion(Idioma idioma, std::string_view nombre, bool en_esta_pc) {
  PantallaHostear pantalla;
  pantalla.idioma = idioma;
  pantalla.configurando = true;
  pantalla.existe = true;
  pantalla.en_esta_pc = en_esta_pc;
  poner(pantalla.nombre, nombre);
  return pantalla;
}

void borrar_contrasenas(PantallaHostear& pantalla) {
  for (Campo* campo : {&pantalla.contrasena, &pantalla.arranque, &pantalla.repetir}) {
    SecureZeroMemory(campo->data(), campo->size());
  }
  borrar_contrasenas(pantalla.dialogo);
}

void borrar_contrasenas(DialogoDeServidor& dialogo) {
  for (Campo* campo : {&dialogo.contrasena, &dialogo.repetir, &dialogo.arranque}) {
    SecureZeroMemory(campo->data(), campo->size());
  }
  olvidar_lo_escrito();
}

void pedir_contrasenas(DialogoDeServidor& dialogo, PideContrasena cual, bool con_arranque, bool andando) {
  borrar_contrasenas(dialogo);
  dialogo.cual = cual;
  dialogo.abrir = true;
  dialogo.con_arranque = con_arranque || cual == PideContrasena::arranque;
  dialogo.andando = andando;
  dialogo.error.clear();
}

std::optional<Contrasenas> dibujar_dialogo(Idioma idioma, DialogoDeServidor& dialogo) {
  const PideContrasena cual = dialogo.cual;
  if (cual == PideContrasena::ninguna) {
    return std::nullopt;
  }
  const auto t = [idioma](Clave clave) { return texto(idioma, clave); };
  if (std::exchange(dialogo.abrir, false)) {
    ImGui::OpenPopup("##dialogo_servidor");
  }
  const DatosDeDialogo& datos = kDialogos[static_cast<std::size_t>(cual)];
  const bool con_contrasena = cual == PideContrasena::exportar || cual == PideContrasena::importar;
  const bool con_repetir = cual == PideContrasena::exportar;
  const bool arranque_opcional = cual == PideContrasena::importar;
  const bool con_arranque = dialogo.con_arranque || arranque_opcional;
  const std::string ayuda = std::string(t(datos.ayuda)) +
                            (cual == PideContrasena::cambiar_clave && dialogo.andando
                                 ? " " + std::string(t(Clave::dialogo_cambiar_andando))
                                 : std::string());
  const int campos = static_cast<int>(con_contrasena) + static_cast<int>(con_repetir) + static_cast<int>(con_arranque);
  const ImVec2 pantalla = ImGui::GetIO().DisplaySize;
  const float ancho = std::min(px(440), pantalla.x - px(20));
  const float interior = ancho - px(40);
  const float alto_ayuda = alto_envuelto(ayuda, interior, medida::letra_chica);
  const float alto = px(56) + alto_ayuda + px(14) + static_cast<float>(campos) * px(46) +
                     (dialogo.error.empty() ? 0 : px(24)) + px(56);
  ImGui::SetNextWindowPos(ImVec2(pantalla.x / 2, pantalla.y / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(ancho, alto));
  if (!ImGui::BeginPopupModal("##dialogo_servidor", nullptr,
                              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoScrollbar)) {
    dialogo.cual = PideContrasena::ninguna;
    borrar_contrasenas(dialogo);
    return std::nullopt;
  }
  const ImVec2 desde = ImGui::GetWindowPos();
  triangulos(*ImGui::GetWindowDrawList(), desde, ImVec2(desde.x + ancho, desde.y + alto));
  const float x = desde.x + px(20);
  float y = desde.y + px(20);
  escribir_cortado(ImVec2(x, y), interior, t(datos.titulo), color::texto, Letra::titulo, medida::letra_tarjeta);
  y += px(36);
  escribir_envuelto(ImVec2(x, y), ayuda, color::texto2, interior, medida::letra_chica);
  y += alto_ayuda + px(14);
  bool confirmar = false;
  bool enfocar = ImGui::IsWindowAppearing();
  const auto escribir_campo = [&](const char* id, Clave pista, Campo& valor) {
    if (std::exchange(enfocar, false)) {
      ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    confirmar = campo(id, t(pista), valor, ImVec2(interior, px(36)), Icono::ninguno,
                      ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_Password, color::pozo) ||
                confirmar;
    y += px(46);
  };
  if (con_contrasena) {
    escribir_campo("##contrasena_archivo", Clave::dialogo_contrasena_archivo, dialogo.contrasena);
  }
  if (con_repetir) {
    escribir_campo("##repetir_archivo", Clave::seguridad_repetir, dialogo.repetir);
  }
  if (con_arranque) {
    escribir_campo("##arranque_servidor", arranque_opcional ? Clave::dialogo_arranque_opcional : Clave::dialogo_arranque_pista,
                   dialogo.arranque);
  }
  if (!dialogo.error.empty()) {
    escribir_cortado(ImVec2(x, y), interior, dialogo.error, color::rojo, Letra::normal, medida::letra_chica);
    y += px(24);
  }
  const std::string_view no = t(Clave::inicio_cancelar);
  const std::string_view si = t(datos.si);
  const float ancho_no = ancho_boton(no, Estilo::contorneado);
  const float ancho_si = ancho_boton(si, Estilo::principal);
  ImGui::SetCursorScreenPos(ImVec2(x + interior - ancho_no, y + px(6)));
  const bool cancelar = boton("##no_servidor", no, Estilo::contorneado, ImVec2(ancho_no, px(medida::alto_principal))) ||
                        ImGui::IsKeyPressed(ImGuiKey_Escape);
  ImGui::SetCursorScreenPos(ImVec2(x + interior - ancho_no - px(10) - ancho_si, y + px(6)));
  confirmar = boton("##si_servidor", si, Estilo::principal, ImVec2(ancho_si, px(medida::alto_principal))) || confirmar;
  std::optional<Contrasenas> hechas;
  if (confirmar && !cancelar) {
    const std::string_view primera(dialogo.contrasena.data());
    const std::string_view arranque(dialogo.arranque.data());
    const auto corta = [](std::string_view escrita) { return cantidad_de_letras(escrita) < cliente::kMinimoContrasena; };
    dialogo.error = (con_repetir && corta(primera)) || (arranque_opcional && !arranque.empty() && corta(arranque))
                        ? con_datos(t(Clave::seguridad_corta), {std::to_string(cliente::kMinimoContrasena)})
                    : con_repetir && primera != std::string_view(dialogo.repetir.data())
                        ? std::string(t(Clave::seguridad_distintas))
                    : (con_contrasena && primera.empty()) || (dialogo.con_arranque && arranque.empty())
                        ? std::string(t(Clave::dialogo_falta))
                        : std::string();
    if (dialogo.error.empty()) {
      hechas = Contrasenas{cual, tomar(dialogo.contrasena), tomar(dialogo.arranque)};
    }
  }
  if (cancelar || hechas) {
    cerrar_ventana_emergente();
    dialogo.cual = PideContrasena::ninguna;
    borrar_contrasenas(dialogo);
  }
  ImGui::EndPopup();
  return hechas;
}

PedidoHostear dibujar_hostear(PantallaHostear& pantalla) { return Dibujo(pantalla).dibujar(); }

}
