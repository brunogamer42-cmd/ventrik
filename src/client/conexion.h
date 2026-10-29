#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/cifrado/cadena.h"
#include "common/cifrado/clave.h"
#include "common/cifrado/codigo_dueno.h"
#include "common/cifrado/huella.h"
#include "common/cifrado/tls.h"
#include "common/protocol/marcos.h"
#include "common/protocol/mensajes.h"
#include "common/protocol/pedidos.h"
#include "common/red/red.h"
#include "common/version.h"

namespace ventrik::cliente {
struct Presentacion {
  std::string nombre;
  std::optional<cifrado::CodigoDueno> codigo_dueno;
  std::shared_ptr<const cifrado::Clave> identidad;
  std::shared_ptr<const cifrado::Secreto> equipo;
  std::shared_ptr<const cifrado::Secreto> contrasena;
  std::optional<std::uint32_t> segundo_paso;
};

struct OpcionesConexion {
  std::chrono::milliseconds tiempo_para_conectar{5000};
  std::chrono::milliseconds tiempo_para_saludar{10000};
  protocolo::Hola hola{kProtocoloMinimo, kProtocoloMaximo, kVersionPrograma, 0};
  const red::Socket* cancelar = nullptr;
  std::optional<red::Direccion> origen;
  std::vector<cifrado::Huella> atras;
};

enum class EstadoConexion {
  conectado,
  sin_respuesta,
  cifrado_rechazado,
  huella_distinta,
  programa_viejo,
  servidor_viejo,
  rechazo_ilegible,
  cortada,
  nombre_invalido,
  nombre_en_uso,
  codigo_incorrecto,
  servidor_lleno,
  firma_rechazada,
  demasiadas,
  falta_contrasena,
  contrasena_incorrecta,
  esperar,
  ocupado,
  sin_identidad,
  falta_segundo_paso,
  segundo_paso_incorrecto,
  segundo_paso_bloqueado,
  error_interno,
};

class ConexionAbierta {
 public:
  ConexionAbierta(std::unique_ptr<cifrado::ConfigTls> config, red::Socket canal);
  ~ConexionAbierta();
  ConexionAbierta(const ConexionAbierta&) = delete;
  ConexionAbierta& operator=(const ConexionAbierta&) = delete;

  const red::Socket& canal() const { return canal_; }
  cifrado::SesionTls& tls() { return *tls_; }
  protocolo::Ensamblador& ensamblador() { return ensamblador_; }
  std::vector<std::uint8_t>& sobrante() { return sobrante_; }
  void confirmar() { confirmada_ = true; }
  bool confirmada() const { return confirmada_; }

 private:
  std::unique_ptr<cifrado::ConfigTls> config_;
  red::Socket canal_;
  std::unique_ptr<cifrado::SesionTls> tls_;
  protocolo::Ensamblador ensamblador_{protocolo::kMaximoCuerpoSaludo};
  std::vector<std::uint8_t> sobrante_;
  bool confirmada_ = false;
};

struct ResultadoConexion {
  EstadoConexion estado = EstadoConexion::error_interno;
  VersionPrograma version_pedida;
  std::uint16_t protocolo = 0;
  std::string cifrado;
  int codigo = 0;
  std::uint16_t espera = 0;
  std::optional<protocolo::Presentado> presentado;
  std::optional<cifrado::CadenaSeguida> cambio;
  std::unique_ptr<ConexionAbierta> abierta;
};

ResultadoConexion conectar(const red::Direccion& destino, const cifrado::Huella& huella,
                           const Presentacion& quien, const OpcionesConexion& opciones = {});

}
