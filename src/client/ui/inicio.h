#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>

#include "client/guardados.h"
#include "client/ui/piezas.h"
#include "common/red/red.h"
#include "common/textos.h"

namespace ventrik::ui {
enum class Fase { quieta, conectando, error };
enum class Copia : std::uint8_t { sin_saber, hecha, falta, problema };

struct Intento {
  red::Direccion direccion;
  bool desde_formulario = false;
  Fase fase = Fase::quieta;
  std::string mensaje;
};

void poner(Campo& campo, std::string_view texto);

struct PantallaInicio {
  Idioma idioma;
  ImTextureID logo{};
  Intento intento;
  std::string aviso;
  std::string error_formulario;
  Campo buscar{};
  Campo direccion{};
  Campo huella{};
  Campo nombre{};
  Campo codigo{};
  bool enfocar_codigo = false;
  std::optional<red::Direccion> avisos_de;
  bool abrir_avisos = false;
  Copia copia = Copia::sin_saber;
};

enum class Accion { ninguna, entrar_con_formulario, entrar_a_guardado, quitar, desconectar };

struct Pedido {
  Accion accion = Accion::ninguna;
  std::size_t cual = 0;
  std::optional<cliente::AvisosDelServidor> avisos;
  bool guardar = false;
  bool ajustes = false;
  bool copia = false;
};

Pedido dibujar_inicio(PantallaInicio& pantalla, const cliente::Guardados& guardados);

}
