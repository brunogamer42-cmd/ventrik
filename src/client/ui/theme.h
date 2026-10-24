#pragma once

#include <array>
#include <cstdint>

#include <imgui.h>

namespace ventrik::ui {
constexpr ImU32 rgb(std::uint32_t hex, std::uint32_t alfa = 255) {
  return alfa << 24 | (hex & 0xFFu) << 16 | (hex & 0xFF00u) | hex >> 16;
}

namespace color {
constexpr ImU32 fondo = rgb(0x161B1E);
constexpr ImU32 pozo = rgb(0x1A1F22);
constexpr ImU32 panel = rgb(0x2A3235);
constexpr ImU32 elevado = rgb(0x3A4448);
constexpr ImU32 linea = rgb(0x3B4549);
constexpr ImU32 linea2 = rgb(0x4B565B);
constexpr ImU32 borde_oscuro = rgb(0x111517);
constexpr ImU32 texto = rgb(0xDDE6E6);
constexpr ImU32 texto2 = rgb(0xB3BDBF);
constexpr ImU32 apagado = rgb(0x8B9598);
constexpr ImU32 claro = rgb(0xD3DEDE);
constexpr ImU32 tinta = rgb(0x14191B);
constexpr ImU32 naranja = rgb(0xF28B30);
constexpr ImU32 celeste = rgb(0x5CC2D6);
constexpr ImU32 verde = rgb(0x6FCB7E);
constexpr ImU32 amarillo = rgb(0xE7B850);
constexpr ImU32 rojo = rgb(0xE0705F);
constexpr ImU32 triangulo = rgb(0xC3D0CF);

constexpr ImU32 bisel_arriba = rgb(0x323C40);
constexpr ImU32 bisel_abajo = rgb(0x293236);
constexpr ImU32 bisel_arriba_encima = rgb(0x3A464A);
constexpr ImU32 bisel_abajo_encima = rgb(0x30393D);
constexpr ImU32 brillo = rgb(0xFFFFFF, 15);
constexpr ImU32 escuadra = rgb(0x9AA6A8);
constexpr ImU32 fondo_esquineros = rgb(0xFFFFFF, 4);
constexpr ImU32 borde_contorneado = rgb(0x5A666A);
constexpr ImU32 borde_modo = rgb(0x262E31);
constexpr ImU32 emblema = rgb(0x0E1B26);
constexpr ImU32 avatar_propio = rgb(0x4B4774);
constexpr ImU32 tinta_naranja = rgb(0x2A1405);
constexpr ImU32 campo_chat = rgb(0x222A2D);
constexpr ImU32 chip = rgb(0x2F393D);
constexpr ImU32 escuadra_chat = rgb(0x5E6A6E);
constexpr ImU32 cuadricula = rgb(0xDDE6E6, 11);
constexpr ImU32 seleccion = rgb(0xFFFFFF, 18);
constexpr std::array<ImU32, 7> avatares = {rgb(0x4B4774), rgb(0x3B5B51), rgb(0x34555B), rgb(0x6A5A2C),
                                           rgb(0x3B4766), rgb(0x5E3846), rgb(0x5F4B3C)};
}

namespace medida {
constexpr float em = 1.2f;
constexpr float letra = 13;
constexpr float letra_chica = 12;
constexpr float letra_mini = 11;
constexpr float letra_tarjeta = 16;
constexpr float letra_panel = 17;
constexpr float letra_servidor = 21;
constexpr float letra_pagina = 27;
constexpr float alto_biselado = 34;
constexpr float bisel = 6;
constexpr float margen_biselado = 10;
constexpr float alto_esquineros = 38;
constexpr float escuadra = 8;
constexpr float grosor_escuadra = 1.5f;
constexpr float margen_esquineros = 14;
constexpr float margen_contorneado = 14;
constexpr float alto_principal = 38;
constexpr float corte_principal = 7;
constexpr float margen_principal = 18;
constexpr float alto_campo = 38;
constexpr float margen_campo = 12;
constexpr float triangulo = 9;
constexpr float icono = 15;
constexpr float separacion_icono = 7;
constexpr float letra_bloque = 15;
constexpr float letra_transmitiendo = 12.5f;
constexpr float alto_barra = 59;
constexpr float ancho_lateral = 290;
constexpr float alto_estado = 24;
constexpr float fila_voz = 31;
constexpr float fila_texto = 30;
constexpr float avatar_lista = 26;
constexpr float avatar_chat = 28;
constexpr float alto_tecla = 26;
constexpr float margen_tecla = 9;
constexpr float alto_transmitiendo = 28;
constexpr float corte_transmitiendo = 8;
constexpr float contador = 18;
constexpr float barra_lista = 8;
constexpr float boton_mas = 22;
constexpr float segmento = 7;
constexpr float alto_segmento = 9;
constexpr float entre_segmentos = 2;
constexpr float alto_campo_chat = 37;
}

}
