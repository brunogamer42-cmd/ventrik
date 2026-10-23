#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>

#include <imgui.h>

#include "client/ui/theme.h"

namespace ventrik::ui {
enum class Letra { normal, fuerte, negrita, titulo };
enum class Icono { ninguno, lupa, pantalla, mas, reportar, ajustes, puntos, servidor };

using Campo = std::array<char, 128>;
enum class Estilo { biselado, esquineros, contorneado, principal };

bool preparar_interfaz(float escala);
void pantalla_completa(const std::function<void()>& dibujar);

float px(float medida);
ImU32 tono(ImU32 color);

class ConLetra {
 public:
  ConLetra(Letra letra, float tamano);
  ~ConLetra();
  ConLetra(const ConLetra&) = delete;
  ConLetra& operator=(const ConLetra&) = delete;
};

std::array<ImVec2, 8> octogono(ImVec2 desde, ImVec2 hasta, float corte);
std::array<ImVec2, 6> cortes_opuestos(ImVec2 desde, ImVec2 hasta, float corte, bool arriba_izquierda);
std::array<ImVec2, 5> corte_abajo_derecha(ImVec2 desde, ImVec2 hasta, float corte);
std::string iniciales(std::string_view nombre);

ImVec2 medir(std::string_view texto, Letra letra, float tamano);
void escribir(ImVec2 donde, std::string_view texto, ImU32 color, Letra letra = Letra::normal,
              float tamano = medida::letra);
void escribir_cortado(ImVec2 donde, float ancho, std::string_view texto, ImU32 color,
                      Letra letra = Letra::normal, float tamano = medida::letra);
void escribir_parrafo(std::string_view texto, ImU32 color, float ancho,
                      float tamano = medida::letra);
void cartel(std::string_view texto);

void dibujar_icono(ImDrawList& dibujo, Icono icono, ImVec2 centro, float lado, ImU32 color);
void escuadras(ImDrawList& dibujo, ImVec2 desde, ImVec2 hasta, float largo, float grosor, ImU32 color);
void triangulos(ImDrawList& dibujo, ImVec2 desde, ImVec2 hasta);
void panel(ImVec2 desde, ImVec2 hasta);
void avatar(ImVec2 desde, float lado, std::string_view letras, ImU32 fondo, ImU32 color_letra,
            float tamano_letra);
void emblema(ImVec2 desde, float lado, ImTextureID logo);

float ancho_boton(std::string_view texto, Estilo estilo, Icono icono = Icono::ninguno);
bool boton(const char* id, std::string_view texto, Estilo estilo, ImVec2 tamano,
           Icono icono = Icono::ninguno);
bool campo(const char* id, std::string_view pista, Campo& texto, ImVec2 tamano,
           Icono icono = Icono::ninguno, ImGuiInputTextFlags opciones = 0, ImU32 relleno = color::fondo);
float ancho_selector_modo(std::string_view rendimiento, std::string_view visual);
void selector_modo(std::string_view rendimiento, std::string_view visual, std::string_view cartel_visual);

}
