#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <imgui.h>

#include "client/sala.h"
#include "client/ui/piezas.h"
#include "common/texto.h"
#include "common/textos.h"

namespace ventrik::ui {
constexpr std::size_t kMuestrasDelGrafico = 12;
constexpr std::uint32_t kSegundosPorMuestra = 5;
constexpr std::size_t kTopeCaras = 2048;
constexpr std::size_t kTopeFechas = 512;

struct Muestra {
  std::uint32_t ping = 0;
  std::uint32_t perdidos = 0;
  std::uint32_t recibidos = 0;
};

struct Alturas {
  std::uint32_t canal = 0;
  float ancho = 0;
  bool al_final = false;
  std::vector<std::uint64_t> ids;
  std::vector<std::uint32_t> dias;
  std::vector<float> textos;
  std::vector<float> desde;
  std::vector<bool> nuevo_dia;
  std::vector<std::string> horas;
};

struct Cara {
  std::string iniciales;
  ImU32 color = 0;
};

struct TextoGuardado {
  std::size_t idioma = SIZE_MAX;
  std::uint64_t dato = UINT64_MAX;
  std::string nombre;
  std::string texto;
};

struct TextosGuardados {
  TextoGuardado conectados;
  TextoGuardado ping;
  TextoGuardado perdidos;
  TextoGuardado tecla;
  TextoGuardado subtitulo;
  TextoGuardado pista;
  TextoGuardado sin_voz;
  TextoGuardado en_canal;
  TextoGuardado ping_corto;
  TextoGuardado volver;
  std::deque<TextoGuardado> etapas;
};

enum class Dialogo { ninguno, crear_texto, crear_voz, cambiar, borrar };

struct DatosDiagnostico {
  bool voz_lista = false;
  std::uint32_t ping_voz = 0;
  double procesador = 0;
  std::uint64_t ram_bytes = 0;
  std::uint32_t colchon_ms = 0;
  std::uint32_t recortes = 0;
  std::uint32_t salida_ms = 0;
};

struct PantallaServidor {
  Idioma idioma{};
  ImTextureID logo{};
  std::string direccion;
  bool calidad_alta = true;
  bool hay_audio = true;
  std::uint8_t tecla = 0;
  bool eligiendo_tecla = false;
  bool por_voz = false;
  int sensibilidad = 0;
  bool ajustando = false;
  std::uint32_t ultima_voz = 0;
  bool transmitiendo = false;
  std::uint32_t hablando = 0;
  int nivel = 0;
  std::deque<Muestra> grafico;
  Campo buscar{};
  std::vector<char> mensaje = std::vector<char>(kMaximoBytesChat + 1);
  std::set<std::uint32_t> plegados;
  bool enfocar_mensaje = false;
  Alturas alturas;
  DatosDiagnostico diagnostico;
  Dialogo dialogo = Dialogo::ninguno;
  bool abrir_dialogo = false;
  std::uint32_t canal_dialogo = 0;
  Campo nombre_dialogo{};
  int marca_dialogo = 0;
  std::string error_dialogo;
  TextosGuardados textos;
  std::map<std::string, Cara, std::less<>> caras;
  std::map<std::uint32_t, std::string> fechas;
  std::uint64_t minuto = UINT64_MAX;
  std::uint32_t hoy = 0;
  std::uint32_t ayer = 0;
};

struct PedidoServidor {
  bool desconectar = false;
  bool elegir_tecla = false;
  std::optional<bool> por_voz;
  std::optional<int> sensibilidad;
  bool guardar = false;
  std::vector<cliente::Orden> ordenes;
};

std::string nombre_tecla(Idioma idioma, std::uint8_t tecla);
PedidoServidor dibujar_servidor(PantallaServidor& pantalla, cliente::Sala& sala);
void soltar_lejanos(PantallaServidor& pantalla);

}
