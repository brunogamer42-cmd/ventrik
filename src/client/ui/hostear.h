#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "client/ui/piezas.h"
#include "common/cifrado/secreto.h"
#include "common/niveles.h"
#include "common/textos.h"

namespace ventrik::ui {
enum class Paso : std::uint8_t { nombre, conexion, capacidad, seguridad, chat, pantallas, revisar, invitar };
constexpr std::size_t kPasos = 8;
constexpr std::size_t kFilasDelRegistro = 100;

struct AdminDelServidor {
  std::string nombre;
  std::string identidad;
};

struct EntradaDelRegistro {
  std::string cuando;
  std::string que;
};

enum class PideContrasena : std::uint8_t { ninguna, arranque, exportar, importar, cambiar_clave };

struct DialogoDeServidor {
  PideContrasena cual = PideContrasena::ninguna;
  bool abrir = false;
  bool con_arranque = false;
  bool andando = false;
  std::string error;
  Campo contrasena{};
  Campo repetir{};
  Campo arranque{};
};

struct Contrasenas {
  PideContrasena cual = PideContrasena::ninguna;
  cifrado::Secreto contrasena;
  cifrado::Secreto arranque;
};

struct PantallaHostear {
  Idioma idioma{};
  bool configurando = false;
  bool en_esta_pc = true;
  bool existe = false;
  bool con_contrasena = false;
  bool xp = false;
  bool limitado = false;
  Paso paso = Paso::nombre;
  Paso dibujado = Paso::invitar;
  Campo nombre{};
  Campo contrasena{};
  Campo arranque{};
  Campo repetir{};
  Seguridad seguridad;
  bool ver_opciones = false;
  std::string huella;
  bool exportada = false;
  std::string aviso;
  bool aviso_es_error = false;
  std::string error;
  std::vector<AdminDelServidor> admins;
  std::vector<EntradaDelRegistro> registro;
  bool ocupado = false;
  DialogoDeServidor dialogo;
};

struct Creacion {
  std::string nombre;
  cifrado::Secreto contrasena;
  cifrado::Secreto arranque;
  Seguridad seguridad;
};

struct PedidoHostear {
  bool cerrar = false;
  std::optional<Creacion> crear;
  std::optional<std::string> nombre;
  std::optional<cifrado::Secreto> contrasena;
  std::optional<Seguridad> seguridad;
  bool exportar = false;
  bool importar = false;
  bool cambiar_clave = false;
  std::optional<std::size_t> sacar_admin;
  std::optional<Contrasenas> contrasenas;
};

PantallaHostear asistente(Idioma idioma);
PantallaHostear configuracion(Idioma idioma, std::string_view nombre, bool en_esta_pc);
void borrar_contrasenas(PantallaHostear& pantalla);
void borrar_contrasenas(DialogoDeServidor& dialogo);
void pedir_contrasenas(DialogoDeServidor& dialogo, PideContrasena cual, bool con_arranque, bool andando = false);
std::optional<Contrasenas> dibujar_dialogo(Idioma idioma, DialogoDeServidor& dialogo);
PedidoHostear dibujar_hostear(PantallaHostear& pantalla);

}
