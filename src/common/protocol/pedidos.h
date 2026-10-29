#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/cadena.h"
#include "common/protocol/bytes.h"
#include "common/protocol/marcos.h"
#include "common/protocol/mensajes.h"

namespace ventrik::protocolo {
constexpr std::size_t kMaximoCanalesEnLista = 256;
constexpr std::size_t kMaximoUsuariosEnLista = 1024;
constexpr std::size_t kMaximoMensajesEnHistorial = 256;
constexpr std::size_t kMaximoOpus = 400;
constexpr std::size_t kMaximoDatagrama = 2048;
constexpr std::size_t kMaximoCuerpoPresentacion = 512;
constexpr std::size_t kBytesClavePublica = 65;
constexpr std::size_t kBytesEquipo = 32;
constexpr std::size_t kMaximoFirma = 72;
constexpr std::size_t kBytesValorDeConexion = 32;
constexpr std::size_t kMaximoBytesContrasena = 127;
constexpr std::uint32_t kCanalGeneral = 1;
constexpr std::size_t kMaximoCanales = 60;
constexpr std::uint8_t kSilencioMicrofono = 1;
constexpr std::uint8_t kSilencioAudio = 2;
constexpr std::size_t kMaximoSilenciados = 32;
constexpr std::size_t kLugaresDeSusurro = 4;
constexpr std::size_t kMaximoPersonasEnSusurro = 8;
constexpr std::size_t kMaximoCuerpoCadena =
    1 + cifrado::kMaximoEslabones * (2 * cifrado::kBytesClavePublica + 4 + 1 + cifrado::kMaximoFirma);

constexpr bool es_silencio_valido(std::uint8_t silencio) {
  return silencio == 0 || silencio == kSilencioMicrofono || silencio == (kSilencioMicrofono | kSilencioAudio);
}

struct Presentarse {
  static constexpr TipoMensaje kTipo = TipoMensaje::presentarse;
  std::string nombre;
  std::optional<std::array<std::uint8_t, 16>> codigo_dueno;
  std::array<std::uint8_t, kBytesClavePublica> clave{};
  std::array<std::uint8_t, kBytesEquipo> equipo{};
  std::string contrasena;
  std::vector<std::uint8_t> firma;
};

std::vector<std::uint8_t> lo_que_se_firma(const Presentarse& mensaje, std::uint16_t protocolo,
                                          gsl::span<const std::uint8_t> huella,
                                          gsl::span<const std::uint8_t> valor_de_conexion);

enum class TipoCanal : std::uint8_t { voz = 1, texto = 2 };

bool es_tipo_valido(TipoCanal tipo, bool solo_admins);

struct CanalInfo {
  std::uint32_t id = 0;
  std::string nombre;
  TipoCanal tipo = TipoCanal::voz;
  bool solo_admins = false;
  std::uint32_t contador = 0;
};

enum class Rol : std::uint8_t { ninguno = 0, admin = 1, dueno = 2 };

struct UsuarioInfo {
  std::uint32_t id = 0;
  std::string nombre;
  std::uint32_t canal = 0;
  std::uint8_t silencio = 0;
  Rol rol = Rol::ninguno;
};

struct Presentado {
  static constexpr TipoMensaje kTipo = TipoMensaje::presentado;
  std::uint32_t tu_id = 0;
  Rol rol = Rol::ninguno;
  std::string servidor;
  std::vector<CanalInfo> canales;
  std::vector<UsuarioInfo> usuarios;
};

enum class MotivoPresentacion : std::uint8_t {
  nombre_invalido = 1,
  nombre_en_uso = 2,
  codigo_incorrecto = 3,
  lleno = 4,
  firma_invalida = 5,
  demasiadas = 6,
  falta_contrasena = 7,
  contrasena_incorrecta = 8,
  ocupado = 9,
  esperar = 10,
};

struct PresentacionRechazada {
  static constexpr TipoMensaje kTipo = TipoMensaje::presentacion_rechazada;
  MotivoPresentacion motivo = MotivoPresentacion::nombre_invalido;
  std::uint16_t espera = 0;
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

struct CanalCambiado {
  static constexpr TipoMensaje kTipo = TipoMensaje::canal_cambiado;
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

struct Silenciar {
  static constexpr TipoMensaje kTipo = TipoMensaje::silenciar;
  std::uint8_t silencio = 0;
};

struct SilenciarPersonas {
  static constexpr TipoMensaje kTipo = TipoMensaje::silenciar_personas;
  std::vector<std::uint32_t> usuarios;
};

struct DestinosDeSusurro {
  static constexpr TipoMensaje kTipo = TipoMensaje::destinos_de_susurro;
  std::uint8_t lugar = 1;
  std::uint32_t canal = 0;
  std::vector<std::uint32_t> personas;
};

struct UsuarioSilencio {
  static constexpr TipoMensaje kTipo = TipoMensaje::usuario_silencio;
  std::uint32_t id = 0;
  std::uint8_t silencio = 0;
};

struct EnviarChat {
  static constexpr TipoMensaje kTipo = TipoMensaje::enviar_chat;
  std::uint32_t canal = 0;
  std::string texto;
};

struct MensajeChat {
  static constexpr TipoMensaje kTipo = TipoMensaje::mensaje_chat;
  std::uint64_t id = 0;
  std::uint32_t canal = 0;
  std::uint32_t numero = 0;
  std::uint32_t autor_id = 0;
  std::string autor;
  std::uint64_t hora_ms = 0;
  std::string texto;
};

enum class Sentido : std::uint8_t { viejos = 1, nuevos = 2 };

struct PedirHistorial {
  static constexpr TipoMensaje kTipo = TipoMensaje::pedir_historial;
  std::uint32_t canal = 0;
  std::uint64_t desde = 0;
  Sentido sentido = Sentido::viejos;
  std::uint16_t cantidad = static_cast<std::uint16_t>(kMaximoMensajesEnHistorial);
};

struct MensajeGuardado {
  std::uint64_t id = 0;
  std::uint32_t numero = 0;
  std::string autor;
  std::uint64_t hora_ms = 0;
  std::string texto;
};

struct Historial {
  static constexpr TipoMensaje kTipo = TipoMensaje::historial;
  std::uint32_t canal = 0;
  std::uint64_t desde = 0;
  Sentido sentido = Sentido::viejos;
  bool hay_mas = false;
  std::vector<MensajeGuardado> mensajes;
};

struct CrearCanal {
  static constexpr TipoMensaje kTipo = TipoMensaje::crear_canal;
  std::string nombre;
  TipoCanal tipo = TipoCanal::voz;
  bool solo_admins = false;
};

struct CambiarCanal {
  static constexpr TipoMensaje kTipo = TipoMensaje::cambiar_canal;
  std::uint32_t canal = 0;
  std::string nombre;
  bool solo_admins = false;
};

struct MirarCanal {
  static constexpr TipoMensaje kTipo = TipoMensaje::mirar_canal;
  std::uint32_t canal = 0;
};

struct Contador {
  std::uint32_t canal = 0;
  std::uint32_t contador = 0;
};

struct MensajesNuevos {
  static constexpr TipoMensaje kTipo = TipoMensaje::mensajes_nuevos;
  std::vector<Contador> canales;
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
  tipo_de_canal = 7,
  nombre_libre = 8,
};

struct Negado {
  static constexpr TipoMensaje kTipo = TipoMensaje::negado;
  TipoMensaje pedido = TipoMensaje::presentarse;
  MotivoNegado motivo = MotivoNegado::sin_permiso;
};

struct Desplazado {
  static constexpr TipoMensaje kTipo = TipoMensaje::desplazado;
};

struct NombrarAdmin {
  static constexpr TipoMensaje kTipo = TipoMensaje::nombrar_admin;
  std::uint32_t usuario = 0;
  bool admin = false;
};

struct UsuarioRol {
  static constexpr TipoMensaje kTipo = TipoMensaje::usuario_rol;
  std::uint32_t id = 0;
  Rol rol = Rol::ninguno;
};

struct LiberarNombre {
  static constexpr TipoMensaje kTipo = TipoMensaje::liberar_nombre;
  std::string nombre;
};

enum class Seguridad : std::uint8_t {
  entrada_nueva = 1,
  equipo_nuevo = 2,
  admin_nombrado = 3,
  admin_quitado = 4,
  canal_borrado = 5,
  nombre_liberado = 6,
  ip_bloqueada = 7,
};

struct AvisoDeSeguridad {
  static constexpr TipoMensaje kTipo = TipoMensaje::aviso_de_seguridad;
  Seguridad tipo = Seguridad::entrada_nueva;
  std::uint64_t hora_ms = 0;
  std::string quien;
  std::string detalle;
};

struct CadenaDeClaves {
  static constexpr TipoMensaje kTipo = TipoMensaje::cadena_de_claves;
  cifrado::Cadena cadena;
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

enum class TipoFlujo : std::uint8_t { voz = 1 };

constexpr std::uint8_t kNivelMaximo = 127;

struct Medio {
  TipoFlujo tipo = TipoFlujo::voz;
  std::uint8_t flujo = 0;
  std::uint16_t numero = 0;
  std::vector<std::uint8_t> datos;
  std::uint16_t tiempo = 0;
  std::uint8_t nivel = 0;
};

struct MedioSubida {
  static constexpr TipoMensaje kTipo = TipoMensaje::medio_subida;
  Medio medio;
};

struct MedioBajada {
  static constexpr TipoMensaje kTipo = TipoMensaje::medio_bajada;
  std::uint32_t origen = 0;
  Medio medio;
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
VENTRIK_MENSAJE(CanalCambiado)
VENTRIK_MENSAJE(CanalBorrado)
VENTRIK_MENSAJE(EntrarACanal)
VENTRIK_MENSAJE(Silenciar)
VENTRIK_MENSAJE(SilenciarPersonas)
VENTRIK_MENSAJE(DestinosDeSusurro)
VENTRIK_MENSAJE(UsuarioSilencio)
VENTRIK_MENSAJE(EnviarChat)
VENTRIK_MENSAJE(MensajeChat)
VENTRIK_MENSAJE(PedirHistorial)
VENTRIK_MENSAJE(Historial)
VENTRIK_MENSAJE(CrearCanal)
VENTRIK_MENSAJE(CambiarCanal)
VENTRIK_MENSAJE(MirarCanal)
VENTRIK_MENSAJE(MensajesNuevos)
VENTRIK_MENSAJE(BorrarCanal)
VENTRIK_MENSAJE(Negado)
VENTRIK_MENSAJE(Desplazado)
VENTRIK_MENSAJE(NombrarAdmin)
VENTRIK_MENSAJE(UsuarioRol)
VENTRIK_MENSAJE(LiberarNombre)
VENTRIK_MENSAJE(AvisoDeSeguridad)
VENTRIK_MENSAJE(CadenaDeClaves)
VENTRIK_MENSAJE(Ping)
VENTRIK_MENSAJE(Pong)
VENTRIK_MENSAJE(ClaveVoz)
VENTRIK_MENSAJE(MedioSubida)
VENTRIK_MENSAJE(MedioBajada)

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
  Escritor escritor(std::move(salida));
  escritor.escribir_u16(static_cast<std::uint16_t>(Mensaje::kTipo));
  escribir(escritor, mensaje);
  if (!escritor.ok() || escritor.datos().size() > kMaximoDatagrama) {
    return false;
  }
  salida = std::move(escritor).soltar();
  return true;
}

std::optional<Marco> separar_datagrama(gsl::span<const std::uint8_t> datos);

}
