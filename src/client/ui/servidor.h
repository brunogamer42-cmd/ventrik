#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <vector>

#include <imgui.h>

#include "client/sala.h"
#include "client/ui/piezas.h"
#include "common/texto.h"
#include "common/textos.h"

namespace ventrik::ui {
constexpr std::size_t kMuestrasDelGrafico = 60;

struct Muestra {
  std::uint32_t ping = 0;
  std::uint32_t perdidos = 0;
  std::uint32_t recibidos = 0;
};

struct Alturas {
  std::uint32_t canal = 0;
  float ancho = 0;
  std::vector<std::uint64_t> ids;
  std::vector<std::uint32_t> dias;
  std::vector<float> textos;
  std::vector<float> desde;
  std::vector<bool> nuevo_dia;
};

struct PantallaServidor {
  Idioma idioma{};
  ImTextureID logo{};
  std::string direccion;
  bool calidad_alta = true;
  bool hay_audio = true;
  std::uint8_t tecla = 0;
  bool eligiendo_tecla = false;
  bool transmitiendo = false;
  std::uint32_t hablando = 0;
  int nivel = 0;
  std::deque<Muestra> grafico;
  Campo buscar{};
  std::vector<char> mensaje = std::vector<char>(kMaximoBytesChat + 1);
  std::set<std::uint32_t> plegados;
  bool enfocar_mensaje = false;
  Alturas alturas;
};

struct PedidoServidor {
  bool desconectar = false;
  bool elegir_tecla = false;
  std::vector<cliente::Orden> ordenes;
};

std::string nombre_tecla(Idioma idioma, std::uint8_t tecla);
PedidoServidor dibujar_servidor(PantallaServidor& pantalla, cliente::Sala& sala);

}
