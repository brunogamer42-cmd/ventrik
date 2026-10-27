#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "client/sesion.h"
#include "client/silencios.h"
#include "common/protocol/pedidos.h"
#include "common/textos.h"

namespace ventrik::cliente {
constexpr std::uint64_t kDesdeAviso = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint16_t kMensajesPorAviso = 10;
constexpr std::uint16_t kMensajesParaMenciones = 25;
constexpr std::size_t kTopeTextoAviso = 200;
constexpr std::size_t kAutoresPorAviso = 32;
constexpr std::chrono::seconds kEntrePedidosDeAviso{2};
constexpr std::chrono::seconds kEsperaDePedidoDeAviso{10};

struct OpcionesAvisos {
  bool prendidos = true;
  bool texto = true;
  bool voz = true;
  bool conexion = true;
  bool privado = false;
  bool no_molestar = false;
};

struct Notificacion {
  std::uint32_t canal = 0;
  bool voz = false;
  std::string titulo;
  std::string cuerpo;
  std::uint64_t version = 0;
  std::string servidor;
};

constexpr std::size_t kTopeArgumentosDeAviso = 96;

std::string argumentos_de_aviso(const Notificacion& aviso);
std::optional<Notificacion> leer_argumentos_de_aviso(std::string_view texto);

class Avisos {
 public:
  using Reloj = std::chrono::steady_clock;

  explicit Avisos(Idioma idioma, OpcionesAvisos opciones = {}) : idioma_(idioma), opciones_(opciones) {}

  void conectado(std::string servidor, std::uint32_t tu_id, std::string tu_nombre);
  void desconectado();
  void fuera(bool si);
  void opciones(const OpcionesAvisos& opciones) { opciones_ = opciones; }
  void idioma(Idioma idioma) { idioma_ = idioma; }
  void ajustes(AvisosDelServidor ajustes) { ajustes_ = std::move(ajustes); }
  void reloj(std::uint64_t ahora_ms) { ahora_ms_ = ahora_ms; }
  void mensaje(const protocolo::MensajeChat& mensaje, const std::string& canal, bool voz);
  void nuevos(std::uint32_t canal, const std::string& nombre, std::uint32_t cantidad);
  std::optional<Orden> pedir(Reloj::time_point ahora, bool sala_esperando, bool juego);
  bool espera(std::uint32_t canal) const { return en_vuelo_ != 0 && en_vuelo_ == canal; }
  bool pidiendo() const { return en_vuelo_ != 0; }
  void historial(const protocolo::Historial& historial, std::uint32_t leido);
  void negado();
  void corte(const std::string& texto);
  void visto(std::uint32_t canal);

  std::vector<Notificacion> lista() const;
  std::uint64_t version() const { return version_; }
  std::size_t pendientes() const { return pendientes_.size(); }

 private:
  struct Pendiente {
    Notificacion aviso;
    std::string canal;
    std::uint32_t cantidad = 0;
    std::uint32_t total = 0;
    std::uint64_t revisado = 0;
    std::vector<std::string> autores;
    std::string autor;
    std::string texto;
    bool falta_texto = false;
    bool mostrar = false;
    Reloj::time_point pedido_en{};
  };

  bool activo(bool tipo) const { return fuera_ && opciones_.prendidos && tipo && !opciones_.no_molestar; }
  Pendiente* buscar_o_crear(std::uint32_t canal);
  void actualizar(Pendiente& pendiente);

  Idioma idioma_;
  OpcionesAvisos opciones_;
  std::string servidor_;
  std::uint32_t tu_id_ = 0;
  std::string tu_nombre_;
  AvisosDelServidor ajustes_;
  std::uint64_t ahora_ms_ = 0;
  bool fuera_ = false;
  std::map<std::uint32_t, Pendiente> pendientes_;
  std::optional<Notificacion> corte_;
  std::uint32_t en_vuelo_ = 0;
  Reloj::time_point en_vuelo_desde_{};
  Reloj::time_point proximo_pedido_{};
  std::uint64_t version_ = 0;
};

}
