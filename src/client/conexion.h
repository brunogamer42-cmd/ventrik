#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
};

struct OpcionesConexion {
  std::chrono::milliseconds tiempo_para_conectar{5000};
  std::chrono::milliseconds tiempo_para_saludar{10000};
  protocolo::Hola hola{kProtocoloMinimo, kProtocoloMaximo, kVersionPrograma, 0};
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

 private:
  std::unique_ptr<cifrado::ConfigTls> config_;
  red::Socket canal_;
  std::unique_ptr<cifrado::SesionTls> tls_;
  protocolo::Ensamblador ensamblador_{protocolo::kMaximoCuerpoSaludo};
  std::vector<std::uint8_t> sobrante_;
};

struct ResultadoConexion {
  EstadoConexion estado = EstadoConexion::error_interno;
  VersionPrograma version_pedida;
  std::uint16_t protocolo = 0;
  std::string cifrado;
  int codigo = 0;
  std::optional<protocolo::Presentado> presentado;
  std::unique_ptr<ConexionAbierta> abierta;
};

ResultadoConexion conectar(const red::Direccion& destino, const cifrado::Huella& huella,
                           const Presentacion& quien, const OpcionesConexion& opciones = {});

}
