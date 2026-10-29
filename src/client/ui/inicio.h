#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "client/guardados.h"
#include "client/ui/hostear.h"
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

struct MiServidor {
  std::string nombre;
  std::uint16_t puerto = 7700;
  bool andando = false;
  bool iniciando = false;
  std::optional<cifrado::Huella::Bytes> huella;
  std::string nota;
  bool nota_es_error = false;
};

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
  Campo contrasena{};
  bool enfocar_codigo = false;
  bool enfocar_contrasena = false;
  bool pedir_segundo_paso = false;
  Campo segundo_paso{};
  std::string error_segundo_paso;
  std::optional<red::Direccion> avisos_de;
  bool abrir_avisos = false;
  Copia copia = Copia::sin_saber;
  std::vector<MiServidor> mis_servidores;
  DialogoDeServidor dialogo;
  std::optional<Contrasenas> contrasenas;
};

enum class Accion {
  ninguna,
  entrar_con_formulario,
  entrar_a_guardado,
  quitar,
  desconectar,
  hostear,
  iniciar_mio,
  detener_mio,
  configurar_mio,
  segundo_paso
};

struct Pedido {
  Accion accion = Accion::ninguna;
  std::size_t cual = 0;
  std::uint32_t codigo = 0;
  std::optional<cliente::AvisosDelServidor> avisos;
  bool guardar = false;
  bool ajustes = false;
  bool copia = false;
};

Pedido dibujar_inicio(PantallaInicio& pantalla, const cliente::Guardados& guardados);

}
