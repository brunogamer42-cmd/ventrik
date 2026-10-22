#pragma once

#include <cstdint>
#include <optional>

#include <gsl/gsl>

#include "common/protocol/bytes.h"
#include "common/version.h"

namespace ventrik::protocolo {

enum class TipoMensaje : std::uint16_t {
  hola = 1,
  bienvenida = 2,
  rechazo = 3,
};

using Funciones = std::uint64_t;

// Formato fijo para siempre: así cualquier versión puede pedirle a otra que actualice.
struct Hola {
  std::uint16_t protocolo_minimo = 0;
  std::uint16_t protocolo_maximo = 0;
  VersionPrograma programa;
  Funciones funciones = 0;
};

struct Bienvenida {
  std::uint16_t protocolo = 0;
  Funciones funciones = 0;
};

enum class MotivoRechazo : std::uint8_t {
  programa_viejo = 1,
  servidor_viejo = 2,
  saludo_invalido = 3,
};

struct Rechazo {
  MotivoRechazo motivo = MotivoRechazo::saludo_invalido;
  VersionPrograma version_minima;
};

constexpr bool operator==(const Hola& a, const Hola& b) {
  return a.protocolo_minimo == b.protocolo_minimo && a.protocolo_maximo == b.protocolo_maximo &&
         a.programa == b.programa && a.funciones == b.funciones;
}

constexpr bool operator==(const Bienvenida& a, const Bienvenida& b) {
  return a.protocolo == b.protocolo && a.funciones == b.funciones;
}

constexpr bool operator==(const Rechazo& a, const Rechazo& b) {
  return a.motivo == b.motivo && a.version_minima == b.version_minima;
}

void escribir(Escritor& escritor, const Hola& hola);
void escribir(Escritor& escritor, const Bienvenida& bienvenida);
void escribir(Escritor& escritor, const Rechazo& rechazo);

[[nodiscard]] std::optional<Hola> leer_hola(gsl::span<const std::uint8_t> cuerpo);
[[nodiscard]] std::optional<Bienvenida> leer_bienvenida(gsl::span<const std::uint8_t> cuerpo);
[[nodiscard]] std::optional<Rechazo> leer_rechazo(gsl::span<const std::uint8_t> cuerpo);

}  // namespace ventrik::protocolo
