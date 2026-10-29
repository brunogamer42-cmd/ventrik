#pragma once

#include <cstddef>
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
  presentarse = 10,
  entrar_a_canal = 11,
  enviar_chat = 12,
  pedir_historial = 13,
  crear_canal = 14,
  cambiar_canal = 15,
  borrar_canal = 16,
  ping = 17,
  mirar_canal = 18,
  silenciar = 19,
  silenciar_personas = 20,
  destinos_de_susurro = 21,
  nombrar_admin = 22,
  liberar_nombre = 23,
  presentado = 30,
  presentacion_rechazada = 31,
  usuario_entro = 32,
  usuario_salio = 33,
  usuario_cambio_canal = 34,
  canal_creado = 35,
  canal_cambiado = 36,
  canal_borrado = 37,
  mensaje_chat = 38,
  historial = 39,
  negado = 40,
  pong = 41,
  clave_voz = 42,
  mensajes_nuevos = 43,
  usuario_silencio = 44,
  desplazado = 45,
  usuario_rol = 46,
  aviso_de_seguridad = 47,
  medio_subida = 50,
  medio_bajada = 51,
};

using Funciones = std::uint64_t;

constexpr Funciones kFuncionPantalla = 1;

constexpr std::size_t kMaximoCuerpoSaludo = 64;

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

}
