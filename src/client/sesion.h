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
};

struct PingMedido {
  std::uint32_t milisegundos = 0;
};

enum class MotivoFin { el_servidor_corto, sin_respuesta, mensaje_ilegible, desbordada };

struct Fin {
  MotivoFin motivo = MotivoFin::el_servidor_corto;
};

using Novedad = std::variant<IntentoDeConexion, protocolo::UsuarioEntro, protocolo::UsuarioSalio,
                             protocolo::UsuarioCambioCanal, protocolo::CanalCreado,
                             protocolo::CanalCambiado, protocolo::CanalBorrado,
                             protocolo::MensajeChat, protocolo::MensajesNuevos,
                             protocolo::Historial, protocolo::Negado, PingMedido, Fin>;

using Orden = std::variant<protocolo::EntrarACanal, protocolo::EnviarChat,
                           protocolo::PedirHistorial, protocolo::CrearCanal,
                           protocolo::CambiarCanal, protocolo::MirarCanal,
                           protocolo::BorrarCanal>;

struct OpcionesSesion {
  OpcionesConexion conexion;
  std::chrono::milliseconds cada_cuanto_ping{5000};
  std::chrono::milliseconds sin_respuesta{30000};
  std::shared_ptr<PuenteVoz> voz;
  std::function<void()> al_haber_novedades;
};

std::string explicar(Idioma idioma, const IntentoDeConexion& intento);
std::string explicar(Idioma idioma, const Fin& fin);

class Sesion {
 public:
  Sesion(const red::Direccion& destino, const cifrado::Huella& huella, Presentacion quien,
         OpcionesSesion opciones = {});
  ~Sesion();
  Sesion(const Sesion&) = delete;
  Sesion& operator=(const Sesion&) = delete;

  void ordenar(Orden orden);
  std::vector<Novedad> esperar_novedades(std::chrono::milliseconds tiempo);

 private:
  void correr();
  bool publicar(Novedad novedad);
  std::vector<Orden> tomar_ordenes();
  bool procesar(const protocolo::Marco& marco);

  const red::Direccion destino_;
  const cifrado::Huella huella_;
  const Presentacion quien_;
  const OpcionesSesion opciones_;
  red::Despertador despertador_;
  red::Despertador cancelar_;
  std::mutex candado_;
  std::condition_variable hay_novedades_;
  std::deque<Orden> ordenes_;
  std::deque<Novedad> novedades_;
  bool terminar_ = false;
  bool desbordada_ = false;
  std::unique_ptr<CanalVoz> canal_voz_;
  cifrado::SesionTls* tls_ = nullptr;
  std::thread hilo_;
};

}
