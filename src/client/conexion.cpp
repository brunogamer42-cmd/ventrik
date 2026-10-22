#include "client/conexion.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/tls.h"
#include "common/protocol/marcos.h"
#include "common/protocol/negociacion.h"

namespace ventrik::cliente {

namespace {

using Reloj = std::chrono::steady_clock;

bool esperar(const red::Socket& canal, cifrado::Paso paso, Reloj::time_point limite) {
  // Con datos esperando, select dice "listo" aunque le quede 0 de tiempo: el reloj se mira acá.
  if ((paso != cifrado::Paso::esperar_lectura && paso != cifrado::Paso::esperar_escritura) ||
      Reloj::now() >= limite) {
    return false;
  }
  const auto falta = std::max(std::chrono::milliseconds{0},
                              std::chrono::duration_cast<std::chrono::milliseconds>(
                                  limite - Reloj::now()));
  return red::esperar(canal, paso == cifrado::Paso::esperar_lectura,
                      paso == cifrado::Paso::esperar_escritura, falta) == red::Espera::lista;
}

ResultadoConexion interpretar(const protocolo::Marco& marco, const protocolo::Hola& hola,
                              const cifrado::SesionTls& tls) {
  ResultadoConexion resultado;
  resultado.estado = EstadoConexion::rechazo_ilegible;
  if (marco.tipo == static_cast<std::uint16_t>(protocolo::TipoMensaje::bienvenida)) {
    const auto bienvenida = protocolo::leer_bienvenida(marco.cuerpo);
    if (bienvenida && protocolo::aceptar_bienvenida(hola, *bienvenida)) {
      resultado.estado = EstadoConexion::conectado;
      resultado.protocolo = bienvenida->protocolo;
      resultado.cifrado = tls.cifrado();
    }
    return resultado;
  }
  if (marco.tipo != static_cast<std::uint16_t>(protocolo::TipoMensaje::rechazo)) {
    return resultado;
  }
  const auto rechazo = protocolo::leer_rechazo(marco.cuerpo);
  if (!rechazo || !protocolo::aceptar_rechazo(hola, *rechazo)) {
    return resultado;
  }
  if (rechazo->motivo == protocolo::MotivoRechazo::programa_viejo) {
    resultado.estado = EstadoConexion::programa_viejo;
    resultado.version_pedida = rechazo->version_minima;
  } else if (rechazo->motivo == protocolo::MotivoRechazo::servidor_viejo) {
    resultado.estado = EstadoConexion::servidor_viejo;
  }
  return resultado;
}

}  // namespace

ResultadoConexion conectar(const red::Direccion& destino, const cifrado::Huella& huella,
                           const OpcionesConexion& opciones) {
  ResultadoConexion resultado;
  red::Socket nuevo = red::conectar(destino, opciones.tiempo_para_conectar);
  if (!nuevo.valido()) {
    resultado.estado = EstadoConexion::sin_respuesta;
    return resultado;
  }
  auto config = cifrado::ConfigTls::para_programa();
  if (!config) {
    return resultado;
  }
  auto abierta = std::make_unique<ConexionAbierta>(std::move(config), std::move(nuevo));
  const red::Socket& canal = abierta->canal();
  cifrado::SesionTls& tls = abierta->tls();
  if (!tls.ok()) {
    return resultado;
  }
  tls.exigir_huella(huella);
  const auto limite = Reloj::now() + opciones.tiempo_para_saludar;

  for (;;) {
    const auto paso = tls.apreton();
    if (paso == cifrado::Paso::listo) {
      break;
    }
    if (esperar(canal, paso, limite)) {
      continue;
    }
    const bool vencido = paso == cifrado::Paso::esperar_lectura ||
                         paso == cifrado::Paso::esperar_escritura;
    // Un servidor lleno o que se apaga corta sin más: no es un cifrado inseguro.
    resultado.estado = tls.huella_distinta()       ? EstadoConexion::huella_distinta
                       : vencido                   ? EstadoConexion::sin_respuesta
                       : paso == cifrado::Paso::cerrado ? EstadoConexion::cortada
                                                        : EstadoConexion::cifrado_rechazado;
    resultado.codigo = tls.ultimo_error();
    return resultado;
  }
  // Aunque Mbed TLS dé el visto bueno, sin la huella confirmada no sale nada.
  if (!tls.huella_confirmada()) {
    resultado.estado = EstadoConexion::huella_distinta;
    return resultado;
  }
  if (!tls.es_tls13()) {
    resultado.estado = EstadoConexion::cifrado_rechazado;
    return resultado;
  }

  protocolo::Escritor cuerpo;
  protocolo::escribir(cuerpo, opciones.hola);
  std::vector<std::uint8_t> salida;
  if (!cuerpo.ok() ||
      !protocolo::armar_marco(static_cast<std::uint16_t>(protocolo::TipoMensaje::hola),
                              cuerpo.datos(), salida)) {
    return resultado;
  }
  gsl::span<const std::uint8_t> resto(salida);
  while (!resto.empty()) {
    std::size_t escritos = 0;
    const auto paso = tls.escribir(resto, escritos);
    if (paso == cifrado::Paso::listo) {
      resto = resto.subspan(escritos);
    } else if (!esperar(canal, paso, limite)) {
      resultado.estado = EstadoConexion::cortada;
      resultado.codigo = tls.ultimo_error();
      return resultado;
    }
  }

  protocolo::Ensamblador ensamblador(protocolo::kMaximoCuerpoSaludo);
  std::array<std::uint8_t, 512> recibido{};
  std::optional<protocolo::Marco> respuesta;
  while (!respuesta) {
    std::size_t leidos = 0;
    const auto paso = tls.leer(recibido, leidos);
    if (paso == cifrado::Paso::listo) {
      auto datos = gsl::span<const std::uint8_t>(recibido).first(leidos);
      respuesta = ensamblador.siguiente(datos);
      if (ensamblador.error()) {
        resultado.estado = EstadoConexion::rechazo_ilegible;
        return resultado;
      }
    } else if (!esperar(canal, paso, limite)) {
      resultado.estado = EstadoConexion::cortada;
      resultado.codigo = tls.ultimo_error();
      return resultado;
    }
  }
  resultado = interpretar(*respuesta, opciones.hola, tls);
  if (resultado.estado == EstadoConexion::conectado) {
    resultado.abierta = std::move(abierta);
  }
  return resultado;
}

ConexionAbierta::ConexionAbierta(std::unique_ptr<cifrado::ConfigTls> config, red::Socket canal)
    : config_(std::move(config)),
      canal_(std::move(canal)),
      tls_(std::make_unique<cifrado::SesionTls>(*config_, canal_.get())) {}

ConexionAbierta::~ConexionAbierta() {
  tls_->cerrar();
}

}  // namespace ventrik::cliente
