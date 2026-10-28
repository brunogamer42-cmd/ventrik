#include "client/conexion.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <gsl/gsl>

#include "client/mi_identidad.h"
#include "common/protocol/negociacion.h"
#include "common/texto.h"

namespace ventrik::cliente {
namespace {
using Reloj = std::chrono::steady_clock;

bool esperar(const red::Socket& canal, cifrado::Paso paso, Reloj::time_point limite,
             const red::Socket* cancelar) {
  if ((paso != cifrado::Paso::esperar_lectura && paso != cifrado::Paso::esperar_escritura) ||
      Reloj::now() >= limite) {
    return false;
  }
  const auto falta = std::max(std::chrono::milliseconds{0},
                              std::chrono::duration_cast<std::chrono::milliseconds>(
                                  limite - Reloj::now()));
  return red::esperar(canal, paso == cifrado::Paso::esperar_lectura,
                      paso == cifrado::Paso::esperar_escritura, falta, cancelar) == red::Espera::lista;
}

bool enviar_todo(ConexionAbierta& abierta, const std::vector<std::uint8_t>& salida,
                 Reloj::time_point limite, const red::Socket* cancelar, ResultadoConexion& resultado) {
  gsl::span<const std::uint8_t> resto(salida);
  while (!resto.empty()) {
    std::size_t escritos = 0;
    const auto paso = abierta.tls().escribir(resto, escritos);
    if (paso == cifrado::Paso::listo) {
      resto = resto.subspan(escritos);
    } else if (!esperar(abierta.canal(), paso, limite, cancelar)) {
      resultado.estado = EstadoConexion::cortada;
      resultado.codigo = abierta.tls().ultimo_error();
      return false;
    }
  }
  return true;
}

std::optional<protocolo::Marco> leer_marco(ConexionAbierta& abierta, Reloj::time_point limite,
                                           const red::Socket* cancelar, ResultadoConexion& resultado) {
  for (;;) {
    if (!abierta.sobrante().empty()) {
      gsl::span<const std::uint8_t> datos(abierta.sobrante());
      auto marco = abierta.ensamblador().siguiente(datos);
      std::vector<std::uint8_t> resto(datos.begin(), datos.end());
      abierta.sobrante().swap(resto);
      if (abierta.ensamblador().error()) {
        resultado.estado = EstadoConexion::rechazo_ilegible;
        return std::nullopt;
      }
      if (marco) {
        return marco;
      }
      continue;
    }
    std::array<std::uint8_t, 4096> recibido{};
    std::size_t leidos = 0;
    const auto paso = abierta.tls().leer(recibido, leidos);
    if (paso == cifrado::Paso::listo) {
      const auto llegado = gsl::span<const std::uint8_t>(recibido).first(leidos);
      abierta.sobrante().assign(llegado.begin(), llegado.end());
    } else if (!esperar(abierta.canal(), paso, limite, cancelar)) {
      resultado.estado = EstadoConexion::cortada;
      resultado.codigo = abierta.tls().ultimo_error();
      return std::nullopt;
    }
  }
}

bool interpretar_saludo(const protocolo::Marco& marco, const protocolo::Hola& hola,
                        const cifrado::SesionTls& tls, ResultadoConexion& resultado) {
  resultado.estado = EstadoConexion::rechazo_ilegible;
  if (marco.tipo == static_cast<std::uint16_t>(protocolo::TipoMensaje::bienvenida)) {
    const auto bienvenida = protocolo::leer_bienvenida(marco.cuerpo);
    if (!bienvenida || !protocolo::aceptar_bienvenida(hola, *bienvenida)) {
      return false;
    }
    resultado.protocolo = bienvenida->protocolo;
    resultado.cifrado = tls.cifrado();
    return true;
  }
  if (marco.tipo != static_cast<std::uint16_t>(protocolo::TipoMensaje::rechazo)) {
    return false;
  }
  const auto rechazo = protocolo::leer_rechazo(marco.cuerpo);
  if (!rechazo || !protocolo::aceptar_rechazo(hola, *rechazo)) {
    return false;
  }
  if (rechazo->motivo == protocolo::MotivoRechazo::programa_viejo) {
    resultado.estado = EstadoConexion::programa_viejo;
    resultado.version_pedida = rechazo->version_minima;
  } else if (rechazo->motivo == protocolo::MotivoRechazo::servidor_viejo) {
    resultado.estado = EstadoConexion::servidor_viejo;
  }
  return false;
}

void interpretar_presentacion(const protocolo::Marco& marco, ResultadoConexion& resultado) {
  resultado.estado = EstadoConexion::rechazo_ilegible;
  if (marco.tipo == static_cast<std::uint16_t>(protocolo::TipoMensaje::presentado)) {
    resultado.presentado = protocolo::leer_mensaje<protocolo::Presentado>(marco.cuerpo);
    if (resultado.presentado) {
      resultado.estado = EstadoConexion::conectado;
    }
    return;
  }
  if (marco.tipo != static_cast<std::uint16_t>(protocolo::TipoMensaje::presentacion_rechazada)) {
    return;
  }
  const auto rechazo = protocolo::leer_mensaje<protocolo::PresentacionRechazada>(marco.cuerpo);
  if (!rechazo) {
    return;
  }
  switch (rechazo->motivo) {
    case protocolo::MotivoPresentacion::nombre_invalido:
      resultado.estado = EstadoConexion::nombre_invalido;
      break;
    case protocolo::MotivoPresentacion::nombre_en_uso:
      resultado.estado = EstadoConexion::nombre_en_uso;
      break;
    case protocolo::MotivoPresentacion::codigo_incorrecto:
      resultado.estado = EstadoConexion::codigo_incorrecto;
      break;
    case protocolo::MotivoPresentacion::lleno:
      resultado.estado = EstadoConexion::servidor_lleno;
      break;
    case protocolo::MotivoPresentacion::firma_invalida:
      resultado.estado = EstadoConexion::firma_rechazada;
      break;
    case protocolo::MotivoPresentacion::demasiadas:
      resultado.estado = EstadoConexion::demasiadas;
      break;
  }
}

}

ResultadoConexion conectar(const red::Direccion& destino, const cifrado::Huella& huella,
                           const Presentacion& quien, const OpcionesConexion& opciones) {
  ResultadoConexion resultado;
  if (!es_nombre_valido(quien.nombre)) {
    resultado.estado = EstadoConexion::nombre_invalido;
    return resultado;
  }
  if (!quien.identidad) {
    resultado.estado = EstadoConexion::sin_identidad;
    return resultado;
  }
  red::Socket nuevo = red::conectar(destino, opciones.tiempo_para_conectar, opciones.cancelar,
                                    opciones.origen ? &*opciones.origen : nullptr);
  if (!nuevo.valido()) {
    resultado.estado = EstadoConexion::sin_respuesta;
    return resultado;
  }
  auto config = cifrado::ConfigTls::para_programa();
  if (!config) {
    return resultado;
  }
  auto abierta = std::make_unique<ConexionAbierta>(std::move(config), std::move(nuevo));
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
    if (esperar(abierta->canal(), paso, limite, opciones.cancelar)) {
      continue;
    }
    const bool vencido = paso == cifrado::Paso::esperar_lectura ||
                         paso == cifrado::Paso::esperar_escritura;
    resultado.estado = tls.huella_distinta()       ? EstadoConexion::huella_distinta
                       : vencido                   ? EstadoConexion::sin_respuesta
                       : paso == cifrado::Paso::cerrado ? EstadoConexion::cortada
                                                        : EstadoConexion::cifrado_rechazado;
    resultado.codigo = tls.ultimo_error();
    return resultado;
  }
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
                              cuerpo.datos(), salida) ||
      !enviar_todo(*abierta, salida, limite, opciones.cancelar, resultado)) {
    return resultado;
  }
  auto marco = leer_marco(*abierta, limite, opciones.cancelar, resultado);
  if (!marco || !interpretar_saludo(*marco, opciones.hola, tls, resultado)) {
    return resultado;
  }

  abierta->ensamblador().cambiar_maximo(protocolo::kMaximoCuerpo);
  protocolo::Presentarse presentarse;
  presentarse.nombre = quien.nombre;
  if (quien.codigo_dueno) {
    presentarse.codigo_dueno = quien.codigo_dueno->bytes();
  }
  presentarse.clave = quien.identidad->publica();
  if (quien.equipo) {
    presentarse.equipo = codigo_de_equipo(*quien.equipo, huella);
  }
  std::array<std::uint8_t, protocolo::kBytesValorDeConexion> valor{};
  const auto firmado = tls.valor_para_identidad(valor)
                           ? protocolo::lo_que_se_firma(presentarse, resultado.protocolo, huella.bytes(), valor)
                           : std::vector<std::uint8_t>();
  auto firma = firmado.empty() ? std::nullopt : quien.identidad->firmar(firmado);
  if (!firma) {
    return resultado;
  }
  presentarse.firma = std::move(*firma);
  salida.clear();
  if (!protocolo::armar_mensaje(presentarse, salida) ||
      !enviar_todo(*abierta, salida, limite, opciones.cancelar, resultado)) {
    return resultado;
  }
  marco = leer_marco(*abierta, limite, opciones.cancelar, resultado);
  if (!marco) {
    return resultado;
  }
  interpretar_presentacion(*marco, resultado);
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

}
