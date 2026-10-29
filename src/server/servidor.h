#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
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
#include "server/comprobador.h"
#include "server/contrasena.h"
#include "server/permisos.h"
#include "server/proteccion.h"
#include "server/voz.h"

namespace ventrik::servidor {
struct Opciones {
  red::Direccion escuchar{0, red::kPuertoPredeterminado};
  std::chrono::milliseconds tiempo_para_saludar{10000};
  std::chrono::milliseconds tiempo_para_la_llave{180000};
  std::size_t esperando_llave = 32;
  std::size_t esperando_llave_sin_conocer = 8;
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
  std::size_t maximo_identidades = kMaximoUsuariosGuardados;
  int identidades_nuevas_por_ip = 5;
  int identidades_nuevas_por_hora = 30;
  int reemplazos_por_identidad = 6;
  std::chrono::minutes ventana_de_reemplazos{10};
  std::size_t comprobaciones_en_cola = 4;
  std::chrono::milliseconds espera_tras_fallo{1000};
  std::chrono::seconds espera_maxima{300};
  OpcionesProteccion proteccion;
  OpcionesVoz voz;
  protocolo::ConfigServidor protocolo;
  std::wstring archivo_base;
  std::string nombre_general;
  std::optional<cifrado::CodigoDueno> codigo_dueno;
  bool quitar_dueno = false;
  std::optional<std::string> poner_contrasena;
  std::optional<bool> cifrar_historial;
  std::optional<SegundoPaso> segundo_paso;
  std::optional<Seguridad> seguridad;
  std::string quitar_segundo_paso;
  std::string permitir_segundo_paso;
  bool en_xp = false;
  std::string liberar_nombre;
  std::string nombre_servidor;
  std::string nombre_de_fabrica = "Ventrik";
  cifrado::Cadena cadena;
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
  firma_invalida,
  dueno_reclamado,
  desplazada,
  error_base,
  contrasena_incorrecta,
  no_se_pudo_comprobar,
  admin_nombrado,
  admin_quitado,
  canal_borrado,
  nombre_liberado,
  equipo_nuevo,
  segundo_paso_incorrecto,
  segundo_paso_bloqueado,
  segundo_paso_configurado,
  segundo_paso_quitado,
  config_cambiada,
  maximo_en_xp,
  llave_clonada,
  segundo_paso_permitido,
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
  bool tiene_dueno() const;
  void codigo_para_reclamar(std::optional<cifrado::CodigoDueno> codigo) { opciones_.codigo_dueno = codigo; }
  const std::string& nombre() const { return nombre_; }
  bool pide_contrasena() const { return contrasena_.has_value(); }
  void clave_del_historial(cifrado::Secreto clave) { clave_historial_ = std::move(clave); }
  bool historial_cifrado() const { return historial_cifrado_; }
  void clave_del_segundo_paso(cifrado::Secreto clave) { clave_segundo_paso_ = std::move(clave); }
  const std::string& segundo_paso_quitado() const { return segundo_paso_quitado_; }
  const std::string& segundo_paso_permitido() const { return segundo_paso_permitido_; }
  const Seguridad& seguridad() const { return seguridad_; }
  bool maximo_en_xp() const { return opciones_.en_xp && seguridad_.nivel == Nivel::maximo; }

  void correr(const std::atomic<bool>& seguir);

 private:
  struct Conexion;
  struct Espera {
    int fallas = 0;
    std::chrono::steady_clock::time_point hasta{};
  };
  struct Registrado {
    UsuarioGuardado guardado;
    std::optional<SegundoPasoGuardado> segundo_paso;
    std::wstring comparable;
    std::chrono::steady_clock::time_point desde_reemplazos{};
    int reemplazos = 0;
    std::uint32_t generacion = 0;
    std::vector<LlaveGuardada> llaves;
  };

  void aceptar_nuevas();
  void avanzar(Conexion& conexion);
  void atender(Conexion& conexion, const protocolo::Marco& marco);
  void atender_hola(Conexion& conexion, const protocolo::Marco& marco);
  void atender_presentacion(Conexion& conexion, const protocolo::Marco& marco);
  void atender_pedido(Conexion& conexion, const protocolo::Marco& marco);
  void entrar_a_canal(Conexion& conexion, const protocolo::Marco& marco);
  void silenciar(Conexion& conexion, const protocolo::Marco& marco);
  void silenciar_personas(Conexion& conexion, const protocolo::Marco& marco);
  void destinos_de_susurro(Conexion& conexion, const protocolo::Marco& marco);
  void enviar_chat(Conexion& conexion, const protocolo::Marco& marco);
  void pedir_historial(Conexion& conexion, const protocolo::Marco& marco);
  void crear_canal(Conexion& conexion, const protocolo::Marco& marco);
  void cambiar_canal(Conexion& conexion, const protocolo::Marco& marco);
  void mirar_canal(Conexion& conexion, const protocolo::Marco& marco);
  void borrar_canal(Conexion& conexion, const protocolo::Marco& marco);
  void nombrar_admin(Conexion& conexion, const protocolo::Marco& marco);
  void liberar_nombre(Conexion& conexion, const protocolo::Marco& marco);
  void segundo_paso(Conexion& conexion, const protocolo::Marco& marco);
  void quitar_segundo_paso(Conexion& conexion, const protocolo::Marco& marco);
  void permitir_segundo_paso(Conexion& conexion, const protocolo::Marco& marco);
  void pedir_config_seguridad(Conexion& conexion, const protocolo::Marco& marco);
  void cambiar_config_seguridad(Conexion& conexion, const protocolo::Marco& marco);
  void aplicar_seguridad(const Seguridad& nueva);
  void renovar(const std::vector<std::uint32_t>& ids);
  bool pide_segundo_paso(const UsuarioGuardado& usuario, bool reclama = false) const;
  bool comprobar_segundo_paso(Conexion& conexion, Registrado& yo, const protocolo::Presentarse& presentacion);
  void revisar_segundo_paso(Conexion& conexion);
  void revisar_y_difundir(Conexion& conexion);
  void ofrecer_segundo_paso(Conexion& conexion);
  bool guardar_segundo_paso(const SegundoPasoGuardado& guardado);
  bool restringida(const Conexion& conexion) const;
  std::optional<protocolo::MotivoPresentacion> revisar_llave(Conexion& conexion,
                                                             const protocolo::Presentarse& presentacion,
                                                             gsl::span<const std::uint8_t> valor);
  void confirmar_llave(Conexion& conexion);
  void guardar_llaves(const Registrado& registrado);
  void pedir_avisos(const Conexion& conexion);
  void entregar_avisos(Conexion& conexion, const AvisosPendientes& pendientes);
  std::vector<Registrado>::iterator registrado_de(const ClaveDeUsuario& clave);
  void avisar_a(const ClaveDeUsuario& clave, const protocolo::AvisoDeSeguridad& aviso);
  void avisar_al_dueno(const protocolo::AvisoDeSeguridad& aviso);
  bool anotar(const protocolo::AvisoDeSeguridad& aviso, const std::optional<UsuarioGuardado>& usuario = std::nullopt);
  bool guardar_entrada(std::uint32_t id, const UsuarioGuardado& usuario, const Equipo& equipo, bool nueva);
  void rechazar_presentacion(Conexion& conexion, protocolo::MotivoPresentacion motivo, std::uint16_t espera = 0);
  void continuar_presentacion(Conexion& conexion, protocolo::Presentarse presentacion,
                              std::optional<cifrado::Secreto> contrasena);
  void comprobar_contrasena(Conexion& conexion, protocolo::Presentarse presentacion, cifrado::Secreto contrasena);
  void terminar_comprobaciones();
  std::uint16_t segundos_de_espera(std::uint32_t ip, const ClaveDeUsuario& clave) const;
  std::uint16_t anotar_fallo(std::uint32_t ip, const ClaveDeUsuario& clave);
  bool admitir_identidad(Conexion& conexion, const protocolo::Presentarse& presentacion,
                         std::vector<Registrado>::iterator& yo, bool reclama, bool& nueva);
  bool desalojar_una();
  bool guardar(const UsuarioGuardado& usuario);
  void anotar_salida(const ClaveDeUsuario& clave);
  void vencer_identidades();
  bool conectada(const ClaveDeUsuario& clave) const;
  void desplazar(Conexion& conexion);
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
  void falla(const red::Direccion& origen);
  Conexion* buscar_conectada(std::uint32_t id);

  template <typename Mensaje>
  void encolar(Conexion& conexion, const Mensaje& mensaje);
  template <typename Mensaje>
  void difundir(const Mensaje& mensaje, std::uint32_t excepto = 0);
  void encolar_bytes(Conexion& conexion, const std::vector<std::uint8_t>& marco);

  cifrado::IdentidadServidor& identidad_;
  Opciones opciones_;
  Avisar avisar_;
  std::array<std::unique_ptr<cifrado::ConfigTls>, 3> configs_;
  std::vector<std::uint8_t> marco_cadena_;
  VersionPrograma programa_minimo_fijo_;
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
  std::vector<Registrado> registrados_;
  std::map<std::uint32_t, std::pair<std::chrono::steady_clock::time_point, int>> nuevas_por_ip_;
  std::pair<std::chrono::steady_clock::time_point, int> nuevas_en_total_{};
  std::chrono::steady_clock::time_point proxima_limpieza_{};
  std::optional<ContrasenaGuardada> contrasena_;
  cifrado::Secreto clave_historial_;
  bool historial_cifrado_ = false;
  cifrado::Secreto clave_segundo_paso_;
  std::string segundo_paso_quitado_;
  std::string segundo_paso_permitido_;
  Seguridad seguridad_;
  std::uint64_t proxima_marca_ = 0;
  std::map<std::uint32_t, Espera> esperas_por_ip_;
  std::map<ClaveDeUsuario, Espera> esperas_por_clave_;
  std::string nombre_;
  std::uint64_t proximo_mensaje_sin_guardar_ = 1;
  Comprobador comprobador_;
  ServicioBase base_;
};

}
