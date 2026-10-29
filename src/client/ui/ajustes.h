#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

enum class DialogoIdentidad : std::uint8_t { ninguno, exportar, importar, reemplazar, crear, activar, cambiar, quitar };
enum class EstadoDeIdentidad : std::uint8_t { cargando, lista, danada, error };

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
  std::size_t tecla_de = 0;
  std::size_t destinos_de = 0;
  std::vector<std::string> personas;
  std::vector<std::string> canales;
  bool probando_microfono = false;
  bool escuchandome = false;
  int nivel = 0;
  std::optional<red::Direccion> avisos_de;
  bool abrir_avisos = false;
  bool ocupado = false;
  std::size_t grupo_de_sonidos = 0;
  std::string aviso_de_sonidos;
  bool aviso_es_error = false;
  EstadoDeIdentidad estado_identidad = EstadoDeIdentidad::cargando;
  std::string identidad;
  std::string identidad_nueva;
  bool copia_hecha = false;
  bool con_maestra = false;
  bool con_conexion = false;
  DialogoIdentidad dialogo_identidad = DialogoIdentidad::ninguno;
  bool abrir_dialogo_identidad = false;
  bool cerrar_dialogo_identidad = false;
  bool enfocar_contrasena = false;
  Campo actual{};
  Campo contrasena{};
  Campo repetir{};
  std::string error_identidad;
  bool calculando = false;
  std::string aviso_identidad;
  bool aviso_identidad_es_error = false;
};

struct PedidoAjustes {
  bool volver = false;
  std::optional<cliente::Guardados> guardados;
  std::optional<bool> arrancar_con_windows;
  bool elegir_tecla = false;
  std::size_t tecla_de = 0;
  std::optional<bool> probar_microfono;
  std::optional<bool> escucharme;
  bool probar_salida = false;
  std::optional<std::pair<std::size_t, cliente::AvisosDelServidor>> avisos;
  bool guardar = false;
  std::optional<std::size_t> probar_sonido;
  std::optional<std::size_t> elegir_sonido;
  std::optional<std::size_t> sonido_de_fabrica;
  bool importar_sonidos = false;
  bool exportar_sonidos = false;
  bool sonidos_de_fabrica = false;
  bool exportar_identidad = false;
  bool importar_identidad = false;
  bool confirmar_identidad = false;
  bool cancelar_identidad = false;
  bool desbloquear = false;
};

PedidoAjustes dibujar_ajustes(PantallaAjustes& pantalla, const cliente::Guardados& guardados);
PedidoAjustes dibujar_bloqueo(PantallaAjustes& pantalla, const cliente::Guardados& guardados);

}
