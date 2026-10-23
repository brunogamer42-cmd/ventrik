#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gsl/gsl>

#include "common/protocol/bytes.h"
#include "common/protocol/marcos.h"
#include "common/protocol/mensajes.h"

namespace ventrik::protocolo {
constexpr std::size_t kMaximoCanalesEnLista = 256;
constexpr std::size_t kMaximoUsuariosEnLista = 1024;
constexpr std::size_t kMaximoMensajesEnHistorial = 256;
constexpr std::size_t kMaximoOpus = 400;
constexpr std::size_t kMaximoDatagrama = 2048;
constexpr std::size_t kMaximoCuerpoPresentacion = 128;

struct Presentarse {
  static constexpr TipoMensaje kTipo = TipoMensaje::presentarse;
  std::string nombre;
  std::optional<std::array<std::uint8_t, 16>> codigo_dueno;
};

struct CanalInfo {
  std::uint32_t id = 0;
  std::string nombre;
};

struct UsuarioInfo {
  std::uint32_t id = 0;
  std::string nombre;
  std::uint32_t canal = 0;
};

struct Presentado {
  static constexpr TipoMensaje kTipo = TipoMensaje::presentado;
  std::uint32_t tu_id = 0;
  bool dueno = false;
  std::vector<CanalInfo> canales;
  std::vector<UsuarioInfo> usuarios;
};

enum class MotivoPresentacion : std::uint8_t {
  nombre_invalido = 1,
  nombre_en_uso = 2,
  codigo_incorrecto = 3,
  lleno = 4,
};

struct PresentacionRechazada {
  static constexpr TipoMensaje kTipo = TipoMensaje::presentacion_rechazada;
  MotivoPresentacion motivo = MotivoPresentacion::nombre_invalido;
};

struct UsuarioEntro {
  static constexpr TipoMensaje kTipo = TipoMensaje::usuario_entro;
  UsuarioInfo usuario;
};

struct UsuarioSalio {
  static constexpr TipoMensaje kTipo = TipoMensaje::usuario_salio;
  std::uint32_t id = 0;
};

struct UsuarioCambioCanal {
  static constexpr TipoMensaje kTipo = TipoMensaje::usuario_cambio_canal;
  std::uint32_t id = 0;
  std::uint32_t canal = 0;
};

struct CanalCreado {
  static constexpr TipoMensaje kTipo = TipoMensaje::canal_creado;
  CanalInfo canal;
};

struct CanalRenombrado {
  static constexpr TipoMensaje kTipo = TipoMensaje::canal_renombrado;
  CanalInfo canal;
};

struct CanalBorrado {
  static constexpr TipoMensaje kTipo = TipoMensaje::canal_borrado;
  std::uint32_t id = 0;
  std::uint32_t movidos_a = 0;
};

struct EntrarACanal {
  static constexpr TipoMensaje kTipo = TipoMensaje::entrar_a_canal;
  std::uint32_t canal = 0;
};

struct EnviarChat {
  static constexpr TipoMensaje kTipo = TipoMensaje::enviar_chat;
  std::string texto;
};

struct MensajeChat {
  static constexpr TipoMensaje kTipo = TipoMensaje::mensaje_chat;
  std::uint64_t id = 0;
  std::uint32_t canal = 0;
  std::uint32_t autor_id = 0;
  std::string autor;
  std::uint64_t hora_ms = 0;
  std::string texto;
};

struct PedirHistorial {
  static constexpr TipoMensaje kTipo = TipoMensaje::pedir_historial;
  std::uint32_t canal = 0;
  std::uint64_t antes_de = 0;
};

struct MensajeGuardado {
  std::uint64_t id = 0;
  std::string autor;
  std::uint64_t hora_ms = 0;
  std::string texto;
};

struct Historial {
  static constexpr TipoMensaje kTipo = TipoMensaje::historial;
  std::uint32_t canal = 0;
  bool hay_mas = false;
  std::vector<MensajeGuardado> mensajes;
};

struct CrearCanal {
  static constexpr TipoMensaje kTipo = TipoMensaje::crear_canal;
  std::string nombre;
};

struct RenombrarCanal {
  static constexpr TipoMensaje kTipo = TipoMensaje::renombrar_canal;
  std::uint32_t canal = 0;
  std::string nombre;
};

struct BorrarCanal {
  static constexpr TipoMensaje kTipo = TipoMensaje::borrar_canal;
  std::uint32_t canal = 0;
};

enum class MotivoNegado : std::uint8_t {
  sin_permiso = 1,
  no_existe = 2,
  texto_invalido = 3,
  demasiado_rapido = 4,
  lleno = 5,
  nombre_en_uso = 6,
};

struct Negado {
  static constexpr TipoMensaje kTipo = TipoMensaje::negado;
  TipoMensaje pedido = TipoMensaje::presentarse;
  MotivoNegado motivo = MotivoNegado::sin_permiso;
};

struct Ping {
  static constexpr TipoMensaje kTipo = TipoMensaje::ping;
  std::uint64_t marca = 0;
};

struct Pong {
  static constexpr TipoMensaje kTipo = TipoMensaje::pong;
  std::uint64_t marca = 0;
};

struct ClaveVoz {
  static constexpr TipoMensaje kTipo = TipoMensaje::clave_voz;
  std::array<std::uint8_t, 16> identidad{};
};

struct VozSubida {
  static constexpr TipoMensaje kTipo = TipoMensaje::voz_subida;
  std::uint16_t numero = 0;
  std::vector<std::uint8_t> opus;
};

struct VozBajada {
  static constexpr TipoMensaje kTipo = TipoMensaje::voz_bajada;
  std::uint32_t hablante = 0;
  std::uint16_t numero = 0;
  std::vector<std::uint8_t> opus;
};

#define VENTRIK_MENSAJE(Tipo)                     \
  void escribir(Escritor& escritor, const Tipo& mensaje); \
  void leer(Lector& lector, Tipo& mensaje);

VENTRIK_MENSAJE(Presentarse)
VENTRIK_MENSAJE(Presentado)
VENTRIK_MENSAJE(PresentacionRechazada)
VENTRIK_MENSAJE(UsuarioEntro)
VENTRIK_MENSAJE(UsuarioSalio)
VENTRIK_MENSAJE(UsuarioCambioCanal)
VENTRIK_MENSAJE(CanalCreado)
VENTRIK_MENSAJE(CanalRenombrado)
VENTRIK_MENSAJE(CanalBorrado)
VENTRIK_MENSAJE(EntrarACanal)
VENTRIK_MENSAJE(EnviarChat)
VENTRIK_MENSAJE(MensajeChat)
VENTRIK_MENSAJE(PedirHistorial)
VENTRIK_MENSAJE(Historial)
VENTRIK_MENSAJE(CrearCanal)
VENTRIK_MENSAJE(RenombrarCanal)
VENTRIK_MENSAJE(BorrarCanal)
VENTRIK_MENSAJE(Negado)
VENTRIK_MENSAJE(Ping)
VENTRIK_MENSAJE(Pong)
VENTRIK_MENSAJE(ClaveVoz)
VENTRIK_MENSAJE(VozSubida)
VENTRIK_MENSAJE(VozBajada)

#undef VENTRIK_MENSAJE

template <typename Mensaje>
std::optional<Mensaje> leer_mensaje(gsl::span<const std::uint8_t> cuerpo) {
  Lector lector(cuerpo);
  Mensaje mensaje{};
  leer(lector, mensaje);
  if (!lector.terminado()) {
    return std::nullopt;
  }
  return mensaje;
}

template <typename Mensaje>
[[nodiscard]] bool armar_mensaje(const Mensaje& mensaje, std::vector<std::uint8_t>& salida) {
  Escritor escritor;
  escribir(escritor, mensaje);
  return escritor.ok() &&
         armar_marco(static_cast<std::uint16_t>(Mensaje::kTipo), escritor.datos(), salida);
}

template <typename Mensaje>
[[nodiscard]] bool armar_datagrama(const Mensaje& mensaje, std::vector<std::uint8_t>& salida) {
  Escritor escritor;
  escritor.escribir_u16(static_cast<std::uint16_t>(Mensaje::kTipo));
  escribir(escritor, mensaje);
  if (!escritor.ok() || escritor.datos().size() > kMaximoDatagrama) {
    return false;
  }
  salida = escritor.datos();
  return true;
}

std::optional<Marco> separar_datagrama(gsl::span<const std::uint8_t> datos);

}
