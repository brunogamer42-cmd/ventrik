#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "client/guardados.h"
#include "client/sesion.h"

namespace ventrik::cliente {
constexpr std::size_t kVentanaChat = 300;
constexpr std::size_t kTopeBytesPorChat = 512 * 1024;
constexpr std::uint16_t kMensajesAlEntrar = 64;
constexpr std::uint16_t kMensajesAlMoverse = 128;
constexpr std::chrono::seconds kEsperaTrasRechazo{1};

struct MensajeDeChat {
  std::uint64_t id = 0;
  std::uint64_t hora_ms = 0;
  std::uint32_t numero = 0;
  std::uint32_t autor = 0;
  std::string texto;
};

struct Chat {
  std::uint32_t canal = 0;
  std::vector<MensajeDeChat> mensajes;
  std::vector<std::string> autores;
  std::size_t bytes = 0;
  bool hay_mas = false;
  bool hay_nuevos = false;
  bool pidiendo = false;
  protocolo::PedirHistorial pedido;
  std::uint64_t ancla = 0;

  const std::string& autor(const MensajeDeChat& mensaje) const { return autores[mensaje.autor]; }
};

class Sala {
 public:
  explicit Sala(const protocolo::Presentado& presentado, const std::vector<Leido>& leidos = {});
  Sala(const Sala&) = delete;
  Sala& operator=(const Sala&) = delete;
  Sala(Sala&&) = default;
  Sala& operator=(Sala&&) = default;

  std::vector<Orden> empezar();
  std::vector<Orden> aplicar(const Novedad& novedad);
  std::vector<Orden> elegir_voz(std::uint32_t canal);
  std::vector<Orden> elegir_texto(std::uint32_t canal);
  std::optional<Orden> enviar(const std::string& texto);
  std::optional<Orden> pedir_mas_viejos();
  std::optional<Orden> pedir_mas_nuevos();
  std::optional<Orden> ir_a_lo_ultimo();
  void a_la_vista(std::uint64_t id) { visible().ancla = id; }
  void soltar_lejanos();

  std::uint32_t tu_id() const { return tu_id_; }
  bool dueno() const { return dueno_; }
  const std::vector<protocolo::CanalInfo>& canales() const { return canales_; }
  const std::map<std::uint32_t, protocolo::UsuarioInfo>& usuarios() const { return usuarios_; }
  const std::vector<const protocolo::UsuarioInfo*>& en_canal(std::uint32_t canal) const;
  const protocolo::CanalInfo* canal(std::uint32_t id) const;
  std::uint32_t tu_canal() const { return voz_.canal; }
  std::uint32_t mirando() const { return texto_.canal; }
  const Chat& chat() const { return texto_.canal != 0 ? texto_ : voz_; }
  std::uint32_t nuevos(std::uint32_t canal) const;
  bool puede_escribir() const;
  std::optional<std::uint32_t> ping() const { return ping_; }
  const std::optional<protocolo::Negado>& negado() const { return negado_; }
  std::vector<Leido> leidos() const;

 private:
  template <typename Otra>
  std::vector<Orden> atender(const Otra&) {
    return {};
  }
  std::vector<Orden> atender(const protocolo::UsuarioEntro& entro);
  std::vector<Orden> atender(const protocolo::UsuarioSalio& salio);
  std::vector<Orden> atender(const protocolo::UsuarioCambioCanal& cambio);
  std::vector<Orden> atender(const protocolo::CanalCreado& creado);
  std::vector<Orden> atender(const protocolo::CanalCambiado& cambiado);
  std::vector<Orden> atender(const protocolo::CanalBorrado& borrado);
  std::vector<Orden> atender(const protocolo::MensajeChat& mensaje);
  std::vector<Orden> atender(const protocolo::MensajesNuevos& nuevos);
  std::vector<Orden> atender(const protocolo::Historial& historial);
  std::vector<Orden> atender(const protocolo::Negado& negado);
  std::vector<Orden> atender(const PingMedido& ping);

  std::vector<Orden> cambiar_voz(std::uint32_t canal);
  Chat& visible() { return texto_.canal != 0 ? texto_ : voz_; }
  Chat* chat_de(std::uint32_t canal);
  bool puede_pedir(const Chat& chat) const;
  void subir(std::uint32_t canal, std::uint32_t contador);
  void marcar_leido();

  std::uint32_t tu_id_ = 0;
  bool dueno_ = false;
  std::vector<protocolo::CanalInfo> canales_;
  std::map<std::uint32_t, protocolo::UsuarioInfo> usuarios_;
  mutable std::optional<std::map<std::uint32_t, std::vector<const protocolo::UsuarioInfo*>>> por_canal_;
  std::map<std::uint32_t, std::uint32_t> leidos_;
  Chat voz_;
  Chat texto_;
  std::optional<std::uint32_t> ping_;
  std::optional<protocolo::Negado> negado_;
  std::chrono::steady_clock::time_point historial_despues_{};
};

}
