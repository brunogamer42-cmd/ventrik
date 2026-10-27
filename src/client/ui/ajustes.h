#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "client/audio.h"
#include "client/guardados.h"
#include "client/ui/piezas.h"
#include "common/red/red.h"
#include "common/textos.h"

namespace ventrik::ui {
enum class Seccion : std::uint8_t {
  general,
  apariencia,
  teclas,
  audio,
  avisos,
  red,
  descargas,
  prestar,
  seguridad,
  amigos,
  copias,
  acerca
};
constexpr std::size_t kSecciones = 12;

struct PantallaAjustes {
  Idioma idioma{};
  Seccion seccion = Seccion::general;
  Campo buscar{};
  std::string buscado;
  std::array<bool, kSecciones> coinciden{true, true, true, true, true, true, true, true, true, true, true, true};
  cliente::ListaDeAudio dispositivos;
  bool arranca_con_windows = false;
  bool hay_bajar_otros = false;
  bool un_nucleo = false;
  bool eligiendo_tecla = false;
  bool probando_microfono = false;
  bool escuchandome = false;
  int nivel = 0;
  std::optional<red::Direccion> avisos_de;
  bool abrir_avisos = false;
  bool ocupado = false;
};

struct PedidoAjustes {
  bool volver = false;
  std::optional<cliente::Guardados> guardados;
  std::optional<bool> arrancar_con_windows;
  bool elegir_tecla = false;
  std::optional<bool> probar_microfono;
  std::optional<bool> escucharme;
  bool probar_salida = false;
  std::optional<std::pair<std::size_t, cliente::AvisosDelServidor>> avisos;
  bool guardar = false;
};

PedidoAjustes dibujar_ajustes(PantallaAjustes& pantalla, const cliente::Guardados& guardados);

}
