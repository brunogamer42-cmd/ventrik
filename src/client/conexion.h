#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "common/cifrado/huella.h"
#include "common/cifrado/tls.h"
#include "common/protocol/mensajes.h"
#include "common/red/red.h"
#include "common/version.h"

namespace ventrik::cliente {

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
  error_interno,
};

// Lo que queda abierto después del saludo; al destruirse le avisa al servidor que se va.
class ConexionAbierta {
 public:
  ConexionAbierta(std::unique_ptr<cifrado::ConfigTls> config, red::Socket canal);
  ~ConexionAbierta();
  ConexionAbierta(const ConexionAbierta&) = delete;
  ConexionAbierta& operator=(const ConexionAbierta&) = delete;

  const red::Socket& canal() const { return canal_; }
  cifrado::SesionTls& tls() { return *tls_; }

 private:
  std::unique_ptr<cifrado::ConfigTls> config_;
  red::Socket canal_;
  std::unique_ptr<cifrado::SesionTls> tls_;
};

struct ResultadoConexion {
  EstadoConexion estado = EstadoConexion::error_interno;
  VersionPrograma version_pedida;
  std::uint16_t protocolo = 0;
  std::string cifrado;
  int codigo = 0;
  std::unique_ptr<ConexionAbierta> abierta;
};

// Espera hasta terminar o hasta vencer los tiempos: la llama el hilo de red, nunca la interfaz.
ResultadoConexion conectar(const red::Direccion& destino, const cifrado::Huella& huella,
                           const OpcionesConexion& opciones = {});

}  // namespace ventrik::cliente
