#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "common/cifrado/secreto.h"
#include "common/cifrado/sobre.h"
#include "common/niveles.h"
#include "common/protocol/pedidos.h"
#include "common/red/red.h"

struct sqlite3;
struct sqlite3_stmt;

namespace ventrik::servidor {
enum class Retencion : std::uint8_t { siempre = 0, dias_30 = 1, dias_7 = 2, no_guardar = 3 };

constexpr std::size_t kMaximoUsuariosGuardados = 20000;
constexpr std::size_t kMaximoEquiposPorUsuario = 16;
constexpr std::uint64_t kMsPorDia = 24ull * 60 * 60 * 1000;
constexpr std::uint64_t kDiasSinVolver = 180;
constexpr std::uint64_t kDiasDeUnaSolaVez = 30;
constexpr std::uint64_t kDiasParaDesalojar = 7;
constexpr std::size_t kMaximoAvisosPorUsuario = 100;
constexpr std::size_t kMaximoRegistroPorTipo = 1500;
constexpr std::size_t kMaximoPedidosEnCola = 4096;
constexpr std::size_t kMaximoGeneracionesSueltas = 10000;
using ClaveDeUsuario = std::array<std::uint8_t, protocolo::kBytesClavePublica>;
using Equipo = std::array<std::uint8_t, protocolo::kBytesEquipo>;

struct UsuarioGuardado {
  ClaveDeUsuario clave{};
  std::string nombre;
  bool dueno = false;
  bool admin = false;
  std::uint64_t creado_ms = 0;
  std::uint64_t visto_ms = 0;
  std::uint32_t entradas = 0;
};

constexpr std::size_t kBytesSegundoPasoSellado =
    cifrado::kBytesNonce + protocolo::kBytesSecretoSegundoPaso + cifrado::kBytesEtiqueta;
constexpr std::uint32_t kFallasDeSegundoPaso = 10;

struct SegundoPasoGuardado {
  ClaveDeUsuario clave{};
  std::vector<std::uint8_t> sellado;
  std::uint64_t paso = 0;
  std::uint32_t fallas = 0;
  std::uint64_t desde_ms = 0;
};

constexpr std::size_t kMaximoLlavesPorUsuario = 8;

struct LlaveGuardada {
  ClaveDeUsuario llave{};
  std::uint32_t contador = 0;
  bool confirmada = false;
};

struct LlavesDeUsuario {
  ClaveDeUsuario clave{};
  std::uint32_t generacion = 0;
  std::vector<LlaveGuardada> llaves;
};

struct AvisosPendientes {
  std::vector<protocolo::AvisoDeSeguridad> lista;
  std::int64_t hasta = 0;
};

bool esta_vencido(const UsuarioGuardado& usuario, std::uint64_t ahora_ms);
bool se_puede_desalojar(const UsuarioGuardado& usuario, std::uint64_t ahora_ms);
std::optional<bool> tiene_datos_guardados(const std::wstring& archivo);

using protocolo::kCanalGeneral;

class Base {
 public:
  static std::unique_ptr<Base> abrir(const std::wstring& archivo, const std::string& nombre_general,
                                     cifrado::Secreto clave_historial = {});
  ~Base();
  Base(const Base&) = delete;
  Base& operator=(const Base&) = delete;

  std::vector<protocolo::CanalInfo> canales();
  std::optional<std::uint32_t> crear_canal(const std::string& nombre,
                                           protocolo::TipoCanal tipo = protocolo::TipoCanal::voz,
                                           bool solo_admins = false);
  bool cambiar_canal(std::uint32_t id, const std::string& nombre, bool solo_admins);
  bool borrar_canal(std::uint32_t id);

  std::optional<std::uint64_t> guardar_mensaje(std::uint32_t canal, std::uint32_t numero,
                                               const std::string& autor, std::uint64_t hora_ms,
                                               const std::string& texto, bool guardar = true);
  protocolo::Historial historial(const protocolo::PedirHistorial& pedido, std::size_t maximo_bytes);

  Retencion retencion();
  bool cambiar_retencion(Retencion retencion);
  bool historial_cifrado();
  bool cifrar_historial(bool cifrar);
  void borrar_viejos(std::uint64_t ahora_ms);

  std::optional<std::vector<UsuarioGuardado>> usuarios(std::uint64_t ahora_ms);
  bool guardar_usuario(const UsuarioGuardado& usuario);
  bool borrar_usuarios(const std::vector<ClaveDeUsuario>& claves);
  bool liberar_nombre(const std::string& nombre);
  bool anotar_equipo(const ClaveDeUsuario& clave, const Equipo& equipo, std::uint64_t ahora_ms, bool es_nuevo = false);
  std::optional<bool> conoce_equipo(const ClaveDeUsuario& clave, const Equipo& equipo);
  bool anotar(const protocolo::AvisoDeSeguridad& aviso);
  std::vector<protocolo::AvisoDeSeguridad> registro(std::size_t maximo);
  bool guardar_aviso(const ClaveDeUsuario& clave, const std::optional<Equipo>& salvo,
                     const protocolo::AvisoDeSeguridad& aviso);
  AvisosPendientes avisos_para(const ClaveDeUsuario& clave, const Equipo& equipo);
  bool entregados(const ClaveDeUsuario& clave, const Equipo& equipo, std::int64_t hasta);
  std::optional<bool> es_de_confianza(const ClaveDeUsuario& clave, const Equipo& equipo);
  bool confiar_equipos(const ClaveDeUsuario& clave, const std::optional<Equipo>& solo = std::nullopt);
  std::optional<std::vector<SegundoPasoGuardado>> segundos_pasos();
  bool guardar_segundo_paso(const SegundoPasoGuardado& guardado);
  bool quitar_segundo_paso(const ClaveDeUsuario& clave);
  std::optional<std::vector<LlavesDeUsuario>> llaves();
  std::optional<std::vector<LlavesDeUsuario>> generaciones_sueltas();
  bool guardar_llaves(const LlavesDeUsuario& llaves);
  bool leer_seguridad(std::optional<ventrik::Seguridad>& leida);
  bool guardar_seguridad(const ventrik::Seguridad& seguridad);
  bool quitar_dueno();
  std::string texto_guardado(const std::string& clave);
  bool guardar_texto(const std::string& clave, const std::string& valor);

  bool empezar_tanda();
  bool en_tanda() const;
  bool confirmar_tanda();
  void deshacer_lo_abierto();
  bool asentar();
  bool hay_sin_asentar() const;

 private:
  struct Llave {
    explicit Llave() = default;
  };

 public:
  Base(Llave, sqlite3* base, cifrado::Secreto clave_historial);

 private:
  enum class Sentencia : std::size_t {
    empezar,
    confirmar,
    deshacer,
    marcar,
    soltar,
    volver,
    contador,
    insertar,
    viejos,
    nuevos,
    total
  };

  bool es_su_hilo() const;
  bool limpiar_equipos_nuevos(const ClaveDeUsuario& clave);
  sqlite3_stmt* preparada(Sentencia cual);
  bool correr(Sentencia cual);

  sqlite3* base_;
  cifrado::Secreto clave_historial_;
  unsigned long hilo_;
  std::array<sqlite3_stmt*, static_cast<std::size_t>(Sentencia::total)> preparadas_{};
  std::int64_t cambios_asentados_ = 0;
};

class ServicioBase {
 public:
  using Trabajo = std::function<std::function<void()>(Base&)>;

  ServicioBase() = default;
  ~ServicioBase();
  ServicioBase(const ServicioBase&) = delete;
  ServicioBase& operator=(const ServicioBase&) = delete;

  bool arrancar(std::wstring archivo, std::string nombre_general, const red::Despertador& despertador,
                cifrado::Secreto clave_historial = {});
  bool pedir(Trabajo trabajo, bool sola = false);
  void limitar(std::size_t maximo);
  void atender_respuestas();

 private:
  void trabajar(std::wstring archivo, std::string nombre_general, cifrado::Secreto clave_historial);

  const red::Despertador* despertador_ = nullptr;
  std::mutex candado_;
  std::condition_variable hay_pedidos_;
  std::condition_variable abierta_;
  std::deque<std::pair<Trabajo, bool>> pedidos_;
  std::deque<std::function<void()>> respuestas_;
  std::size_t maximo_ = kMaximoPedidosEnCola;
  std::optional<bool> pudo_abrir_;
  bool terminar_ = false;
  std::thread hilo_;
};

}
