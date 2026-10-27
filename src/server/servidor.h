#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/cifrado/codigo_dueno.h"
#include "common/cifrado/identidad.h"
#include "common/cifrado/tls.h"
#include "common/protocol/marcos.h"
#include "common/protocol/negociacion.h"
#include "common/protocol/pedidos.h"
#include "common/red/red.h"
#include "server/base.h"
#include "server/permisos.h"
#include "server/proteccion.h"
#include "server/voz.h"

namespace ventrik::servidor {
struct Opciones {
  red::Direccion escuchar{0, red::kPuertoPredeterminado};
  std::chrono::milliseconds tiempo_para_saludar{10000};
  std::size_t pendientes_por_ip = 4;
  std::size_t pendientes_en_total = 64;
  std::size_t conexiones_por_ip = 16;
  std::size_t conexiones_maximas = 512;
  std::chrono::milliseconds tiempo_para_cerrar{5000};
  std::chrono::milliseconds inactividad{30000};
  std::size_t maximo_salida = 1024 * 1024;
  double rafaga_chat = 10;
  double chat_por_segundo = 1;
  double rafaga_pedidos = 20;
  double pedidos_por_segundo = 5;
  double rafaga_historial = 3;
  double historial_por_segundo = 1;
  double rafaga_entrar_a_la_voz = 5;
  double entrar_a_la_voz_por_segundo = 0.5;
  int intentos_de_presentacion = 5;
  OpcionesProteccion proteccion;
  OpcionesVoz voz;
  protocolo::ConfigServidor protocolo;
  std::wstring archivo_base;
  std::string nombre_general;
  std::optional<cifrado::CodigoDueno> codigo_dueno;
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
  presentada,
  salio,
  sin_respuesta,
  no_lee,
  codigo_incorrecto,
  voz_conectada,
};

using Avisar = std::function<void(Evento, const red::Direccion&, const std::string&)>;

class Servidor {
 public:
  Servidor(cifrado::IdentidadServidor& identidad, Opciones opciones, Avisar avisar = {});
  ~Servidor();
  Servidor(const Servidor&) = delete;
  Servidor& operator=(const Servidor&) = delete;

  bool escuchar();
  std::uint16_t puerto() const { return puerto_; }
  bool base_abierta() const { return !canales_.empty(); }

  void correr(const std::atomic<bool>& seguir);

 private:
  struct Conexion;

  void aceptar_nuevas();
  void avanzar(Conexion& conexion);
  void atender(Conexion& conexion, const protocolo::Marco& marco);
  void atender_hola(Conexion& conexion, const protocolo::Marco& marco);
  void atender_presentacion(Conexion& conexion, const protocolo::Marco& marco);
  void atender_pedido(Conexion& conexion, const protocolo::Marco& marco);
  void entrar_a_canal(Conexion& conexion, const protocolo::Marco& marco);
  void silenciar(Conexion& conexion, const protocolo::Marco& marco);
  void silenciar_personas(Conexion& conexion, const protocolo::Marco& marco);
  void enviar_chat(Conexion& conexion, const protocolo::Marco& marco);
  void pedir_historial(Conexion& conexion, const protocolo::Marco& marco);
  void crear_canal(Conexion& conexion, const protocolo::Marco& marco);
  void cambiar_canal(Conexion& conexion, const protocolo::Marco& marco);
  void mirar_canal(Conexion& conexion, const protocolo::Marco& marco);
  void borrar_canal(Conexion& conexion, const protocolo::Marco& marco);
  void error_de_protocolo(Conexion& conexion);
  void negar(Conexion& conexion, protocolo::TipoMensaje pedido, protocolo::MotivoNegado motivo);
  void cerrar_con_respuesta(Conexion& conexion);
  void difundir_chat(const protocolo::MensajeChat& mensaje);
  void avisar_novedades();
  void quitar_terminadas();
  void atender_udp();
  void renovar_claves();
  bool enviar_pendiente(Conexion& conexion);
  void revisar_lo_recibido(Conexion& conexion);
  void avisar(Evento evento, const Conexion& conexion);
  void avisar(Evento evento, const red::Direccion& origen);
  void falla(const Conexion& conexion);
  Conexion* buscar_conectada(std::uint32_t id);

  template <typename Mensaje>
  void encolar(Conexion& conexion, const Mensaje& mensaje);
  template <typename Mensaje>
  void difundir(const Mensaje& mensaje, std::uint32_t excepto = 0);
  void encolar_bytes(Conexion& conexion, const std::vector<std::uint8_t>& marco);

  cifrado::IdentidadServidor& identidad_;
  Opciones opciones_;
  Avisar avisar_;
  std::unique_ptr<cifrado::ConfigTls> config_;
  red::Socket escucha_;
  red::Socket udp_;
  red::Despertador despertador_;
  std::unique_ptr<Voz> voz_;
  std::uint16_t puerto_ = 0;
  std::vector<std::unique_ptr<Conexion>> conexiones_;
  std::chrono::steady_clock::time_point aceptar_desde_{};
  std::chrono::steady_clock::time_point revisar_voz_en_{};
  Proteccion proteccion_;
  std::vector<Canal> canales_;
  std::size_t canales_pedidos_ = 0;
  Retencion retencion_ = Retencion::siempre;
  std::uint32_t proximo_id_ = 1;
  std::uint64_t proximo_mensaje_sin_guardar_ = 1;
  ServicioBase base_;
};

}
