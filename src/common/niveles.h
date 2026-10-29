#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "common/version.h"

namespace ventrik {
enum class Nivel : std::uint8_t { basico, reforzado, maximo, personalizado };
constexpr std::size_t kNiveles = 4;

enum class Opcion : std::uint8_t {
  algoritmo,
  renovar_voz,
  aprobar_nuevos,
  segundo_paso,
  programa_minimo,
  historial_cifrado,
  sin_historial,
  reconocer_equipos,
  avisos_de_admins,
  voz_fija,
  antivirus,
  rechazar_comprimidos,
  invitaciones_de_un_uso
};
constexpr std::size_t kOpciones = 13;

enum class Algoritmo : std::uint8_t { automatico, solo_aes_gcm, solo_chacha20 };
enum class SegundoPaso : std::uint8_t { nadie, admins, todos };
enum class Minimo : std::uint8_t { compatible, con_arreglos };

using Valores = std::array<std::uint8_t, kOpciones>;

constexpr std::array<std::uint32_t, 4> kRenovarVozCada = {15 * 60, 30 * 60, 60 * 60, 4 * 60 * 60};
constexpr VersionPrograma kUltimaConArreglos{0, 5, 1};
constexpr Valores kValoresPorOpcion = {3, 4, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2};
constexpr Valores kEtapaDeOpcion = {0, 0, 9, 0, 0, 0, 0, 9, 0, 0, 5, 5, 6};
constexpr std::array<Valores, 3> kValoresDeNivel = {{{0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                                     {0, 2, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0},
                                                     {0, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1}}};

struct Opciones {
  Valores valores = kValoresDeNivel[0];
  std::uint8_t operator[](Opcion opcion) const { return valores[static_cast<std::size_t>(opcion)]; }
  std::uint8_t& operator[](Opcion opcion) { return valores[static_cast<std::size_t>(opcion)]; }
};

inline bool operator==(const Opciones& a, const Opciones& b) { return a.valores == b.valores; }
inline bool operator!=(const Opciones& a, const Opciones& b) { return !(a == b); }

struct Seguridad {
  Nivel nivel = Nivel::basico;
  Opciones opciones;
};

Opciones opciones_de(Nivel nivel);
Nivel nivel_de(const Opciones& opciones);
int etapa_de(Opcion opcion);
std::uint8_t valores_de(Opcion opcion);
bool es_valida(const Seguridad& seguridad);
void elegir_nivel(Seguridad& seguridad, Nivel nivel);
bool cambiar_opcion(Seguridad& seguridad, Opcion opcion, std::uint8_t valor);
VersionPrograma version_minima(const Opciones& opciones);
std::uint32_t segundos_de_renovacion(const Opciones& opciones);

}
