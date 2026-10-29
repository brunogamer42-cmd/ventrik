#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "client/canal_voz.h"
#include "client/conexion.h"
#include "client/puente_voz.h"
#include "common/protocol/pedidos.h"
#include "common/red/red.h"
#include "common/textos.h"

namespace ventrik::cliente {
struct IntentoDeConexion {
  EstadoConexion estado = EstadoConexion::error_interno;
  VersionPrograma version_pedida;
  std::string cifrado;
  std::optional<protocolo::Presentado> presentado;
  std::uint16_t espera = 0;
  std::optional<cifrado::CadenaSeguida> cambio;
};

struct PingMedido {
  std::uint32_t milisegundos = 0;
};

enum class MotivoFin { el_servidor_corto, sin_respuesta, mensaje_ilegible, desbordada, desplazada };

struct Fin {
  MotivoFin motivo = MotivoFin::el_servidor_corto;
};

using Novedad = std::variant<IntentoDeConexion, protocolo::UsuarioEntro, protocolo::UsuarioSalio,
                             protocolo::UsuarioCambioCanal, protocolo::UsuarioSilencio, protocolo::CanalCreado,
                             protocolo::CanalCambiado, protocolo::CanalBorrado,
                             protocolo::MensajeChat, protocolo::MensajesNuevos,
                             protocolo::Historial, protocolo::Negado, protocolo::UsuarioRol,
                             protocolo::AvisoDeSeguridad, protocolo::ConfigurarSegundoPaso,
                             protocolo::ConfigSeguridad, protocolo::EstadoSegundoPaso, PingMedido, Fin>;

using Orden = std::variant<protocolo::EntrarACanal, protocolo::EnviarChat,
                           protocolo::PedirHistorial, protocolo::CrearCanal,
                           protocolo::CambiarCanal, protocolo::MirarCanal,
                           protocolo::BorrarCanal, protocolo::Silenciar, protocolo::SilenciarPersonas,
                           protocolo::DestinosDeSusurro, protocolo::NombrarAdmin, protocolo::LiberarNombre,
                           protocolo::SegundoPaso, protocolo::QuitarSegundoPaso, protocolo::PedirConfigSeguridad,
                           protocolo::CambiarConfigSeguridad, protocolo::PermitirSegundoPaso>;

struct OpcionesSesion {
  OpcionesConexion conexion;
  std::chrono::milliseconds cada_cuanto_ping{5000};
  std::chrono::milliseconds cada_cuanto_ping_ahorrando{15000};
  std::chrono::milliseconds sin_respuesta{30000};
  std::function<void()> al_haber_novedades;
};

std::string explicar(Idioma idioma, const IntentoDeConexion& intento);
std::string explicar(Idioma idioma, const Fin& fin);
std::string explicar(Idioma idioma, protocolo::MotivoNegado motivo);

class Sesion {
 public:
  Sesion(const red::Direccion& destino, const cifrado::Huella& huella, Presentacion quien,
         OpcionesSesion opciones = {});
  ~Sesion();
  Sesion(const Sesion&) = delete;
  Sesion& operator=(const Sesion&) = delete;

  void ordenar(Orden orden);
  void ordenar(std::vector<Orden> ordenes);
  void ahorrar(bool si);
  void voz(std::shared_ptr<PuenteVoz> puente);
  std::vector<Novedad> esperar_novedades(std::chrono::milliseconds tiempo);
  std::uint32_t despertares() const { return despertares_.load(std::memory_order_relaxed); }
  bool tocando_llave() const { return tocando_.load(); }
  void abandonar();

 private:
  void correr();
  bool publicar(Novedad novedad);
  std::vector<Orden> tomar_ordenes();
  bool procesar(const protocolo::Marco& marco);
  void cambiar_voz();
  void usar_clave();

  const red::Direccion destino_;
  cifrado::Huella huella_;
  Presentacion quien_;
  const OpcionesSesion opciones_;
  red::Despertador despertador_;
  red::Despertador cancelar_;
  std::mutex candado_;
  std::condition_variable hay_novedades_;
  std::deque<Orden> ordenes_;
  std::deque<Novedad> novedades_;
  bool terminar_ = false;
  bool desbordada_ = false;
  bool desplazada_ = false;
  std::optional<std::shared_ptr<PuenteVoz>> voz_pedida_;
  std::shared_ptr<PuenteVoz> puente_;
  std::optional<protocolo::ClaveVoz> clave_;
  std::unique_ptr<CanalVoz> canal_voz_;
  cifrado::SesionTls* tls_ = nullptr;
  std::atomic<bool> ahorrando_{false};
  std::atomic<std::uint32_t> despertares_{0};
  std::atomic<bool> tocando_{false};
  std::thread hilo_;
};

}
