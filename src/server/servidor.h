#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "common/cifrado/identidad.h"
#include "common/cifrado/tls.h"
#include "common/protocol/marcos.h"
#include "common/protocol/negociacion.h"
#include "common/red/red.h"
#include "server/proteccion.h"

namespace ventrik::servidor {

struct Opciones {
  red::Direccion escuchar{0, red::kPuertoPredeterminado};
  // Diseño: "Conexiones que nunca terminan de saludar".
  std::chrono::milliseconds tiempo_para_saludar{10000};
  std::size_t pendientes_por_ip = 4;
  std::size_t pendientes_en_total = 64;
  // Detrás de un mismo módem puede haber varias personas: alto, pero con techo.
  std::size_t conexiones_por_ip = 16;
  std::size_t conexiones_maximas = 512;
  std::chrono::milliseconds tiempo_para_cerrar{5000};
  OpcionesProteccion proteccion;
  protocolo::ConfigServidor protocolo;
};

enum class Evento {
  conectada,
  desconectada,
  rechazo_cifrado,
  rechazo_saludo,
  tiempo_vencido,
  demasiadas_pendientes,
  demasiadas_por_ip,
  lleno,
  demasiado_rapido,
  bloqueada,
  rechazo_bloqueada,
  ocupado,
};

using Avisar = std::function<void(Evento, const red::Direccion&)>;

class Servidor {
 public:
  // La identidad tiene que vivir más que el servidor.
  Servidor(cifrado::IdentidadServidor& identidad, Opciones opciones, Avisar avisar = {});
  ~Servidor();
  Servidor(const Servidor&) = delete;
  Servidor& operator=(const Servidor&) = delete;

  bool escuchar();
  std::uint16_t puerto() const { return puerto_; }

  // Atiende hasta que `seguir` sea false; lo revisa al menos dos veces por segundo.
  void correr(const std::atomic<bool>& seguir);

 private:
  struct Conexion;

  void aceptar_nuevas();
  void avanzar(Conexion& conexion);
  void atender(Conexion& conexion, const protocolo::Marco& marco);
  bool enviar_pendiente(Conexion& conexion);
  void encolar(Conexion& conexion, protocolo::TipoMensaje tipo, const protocolo::Escritor& cuerpo);
  void avisar(Evento evento, const red::Direccion& origen);
  void falla(const Conexion& conexion);
  void revisar_lo_recibido(Conexion& conexion);

  cifrado::IdentidadServidor& identidad_;
  Opciones opciones_;
  Avisar avisar_;
  std::unique_ptr<cifrado::ConfigTls> config_;
  red::Socket escucha_;
  std::uint16_t puerto_ = 0;
  std::vector<std::unique_ptr<Conexion>> conexiones_;
  std::chrono::steady_clock::time_point aceptar_desde_{};
  Proteccion proteccion_;
};

}  // namespace ventrik::servidor
