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
  protocolo::Lector cabecera(salida);
  const std::size_t largo = cabecera.leer_u32();
  const bool solo_hola = cabecera.leer_u16() == static_cast<std::uint16_t>(protocolo::TipoMensaje::hola) &&
                         salida.size() == protocolo::kCabeceraMarco + largo;
  if (!abierta.confirmada() && !solo_hola) {
    resultado.estado = EstadoConexion::huella_distinta;
    return false;
  }
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
    case protocolo::MotivoPresentacion::falta_contrasena:
      resultado.estado = EstadoConexion::falta_contrasena;
      break;
    case protocolo::MotivoPresentacion::contrasena_incorrecta:
      resultado.estado = EstadoConexion::contrasena_incorrecta;
      resultado.espera = rechazo->espera;
      break;
    case protocolo::MotivoPresentacion::ocupado:
      resultado.estado = EstadoConexion::ocupado;
      break;
    case protocolo::MotivoPresentacion::esperar:
      resultado.estado = EstadoConexion::esperar;
      resultado.espera = rechazo->espera;
      break;
    case protocolo::MotivoPresentacion::falta_segundo_paso:
      resultado.estado = EstadoConexion::falta_segundo_paso;
      break;
    case protocolo::MotivoPresentacion::segundo_paso_incorrecto:
      resultado.estado = EstadoConexion::segundo_paso_incorrecto;
      resultado.espera = rechazo->espera;
      break;
    case protocolo::MotivoPresentacion::segundo_paso_bloqueado:
      resultado.estado = EstadoConexion::segundo_paso_bloqueado;
      break;
    case protocolo::MotivoPresentacion::codigo_usado:
      resultado.estado = EstadoConexion::segundo_paso_usado;
      resultado.espera = rechazo->espera;
      break;
    case protocolo::MotivoPresentacion::llave_anulada:
      resultado.estado = EstadoConexion::llave_anulada;
      break;
    case protocolo::MotivoPresentacion::llave_clonada:
      resultado.estado = EstadoConexion::llave_clonada;
      break;
  }
}

bool tocar_llaves(protocolo::Presentarse& presentarse, const Presentacion& quien, std::uint16_t version,
                  gsl::span<const std::uint8_t> huella, gsl::span<const std::uint8_t> valor) {
  for (const LlaveRegistrada& llave : quien.llaves->llaves) {
    presentarse.llave = protocolo::EntradaConLlave{llave.publica, llave.permiso, {}, {}};
    const auto desafio = protocolo::desafio_de_llave(presentarse, version, huella, valor);
    const auto tocada = desafio ? quien.tocar_llave(llave.id, *desafio) : std::nullopt;
    if (tocada && tocada->datos.size() == protocolo::kBytesDatosDeLlave && !tocada->firma.empty() &&
        tocada->firma.size() <= protocolo::kMaximoFirma) {
      std::copy(tocada->datos.begin(), tocada->datos.end(), presentarse.llave->datos.begin());
      presentarse.llave->firma = tocada->firma;
      return true;
    }
  }
  return false;
}

}

ResultadoConexion conectar(const red::Direccion& destino, const cifrado::Huella& huella,
                           const Presentacion& quien, const OpcionesConexion& opciones) {
  ResultadoConexion resultado;
  if (!es_nombre_valido(quien.nombre)) {
    resultado.estado = EstadoConexion::nombre_invalido;
    return resultado;
  }
  const bool con_llave = !quien.identidad && quien.llaves && !quien.llaves->llaves.empty() && quien.tocar_llave;
  if (!quien.identidad && !con_llave) {
    resultado.estado = EstadoConexion::sin_identidad;
    return resultado;
  }
  if (!quien.equipo) {
    resultado.estado = EstadoConexion::sin_equipo;
    return resultado;
  }
  protocolo::Hola hola = opciones.hola;
  hola.funciones |= con_llave ? protocolo::kFuncionLlave : 0;
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
  auto limite = Reloj::now() + opciones.tiempo_para_saludar;

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
  if (!tls.es_tls13()) {
    resultado.estado = EstadoConexion::cifrado_rechazado;
    return resultado;
  }
  const auto de_la_conexion = tls.huella_del_otro();
  if (!de_la_conexion) {
    resultado.estado = EstadoConexion::huella_distinta;
    return resultado;
  }
  if (tls.huella_confirmada()) {
    abierta->confirmar();
  }
  const auto fallar = [&abierta, &resultado]() {
    ResultadoConexion distinta;
    distinta.estado = EstadoConexion::huella_distinta;
    return abierta->confirmada() ? std::move(resultado) : std::move(distinta);
  };

  protocolo::Escritor cuerpo;
  protocolo::escribir(cuerpo, hola);
  std::vector<std::uint8_t> salida;
  if (!cuerpo.ok() ||
      !protocolo::armar_marco(static_cast<std::uint16_t>(protocolo::TipoMensaje::hola),
                              cuerpo.datos(), salida) ||
      !enviar_todo(*abierta, salida, limite, opciones.cancelar, resultado)) {
    return fallar();
  }
  auto marco = leer_marco(*abierta, limite, opciones.cancelar, resultado);
  if (!marco || !interpretar_saludo(*marco, hola, tls, resultado)) {
    return fallar();
  }
  abierta->ensamblador().cambiar_maximo(protocolo::kMaximoCuerpoCadena);
  marco = leer_marco(*abierta, limite, opciones.cancelar, resultado);
  const auto cadena = marco && marco->tipo == static_cast<std::uint16_t>(protocolo::TipoMensaje::cadena_de_claves)
                          ? protocolo::leer_mensaje<protocolo::CadenaDeClaves>(marco->cuerpo)
                          : std::nullopt;
  if (!abierta->confirmada()) {
    resultado.cambio = cadena ? cifrado::seguir_cadena(cadena->cadena, huella, *de_la_conexion, opciones.atras)
                              : std::nullopt;
    if (!resultado.cambio) {
      return fallar();
    }
    abierta->confirmar();
  } else if (!cadena) {
    resultado.estado = marco ? EstadoConexion::rechazo_ilegible : resultado.estado;
    return resultado;
  }
  resultado.original = opciones.original ? *opciones.original
                                         : cifrado::origen_de_la_cadena(cadena->cadena, *de_la_conexion).value_or(huella);

  abierta->ensamblador().cambiar_maximo(protocolo::kMaximoCuerpo);
  protocolo::Presentarse presentarse;
  presentarse.nombre = quien.nombre;
  if (quien.codigo_dueno) {
    presentarse.codigo_dueno = quien.codigo_dueno->bytes();
  }
  presentarse.clave = con_llave ? quien.llaves->identidad : quien.identidad->publica();
  presentarse.generacion = quien.generacion;
  presentarse.equipo = codigo_de_equipo(*quien.equipo, *resultado.original);
  if (quien.contrasena) {
    presentarse.contrasena.assign(reinterpret_cast<const char*>(quien.contrasena->bytes().data()),
                                  quien.contrasena->size());
  }
  presentarse.segundo_paso = quien.segundo_paso;
  std::array<std::uint8_t, protocolo::kBytesValorDeConexion> valor{};
  const bool hay_valor = tls.valor_para_identidad(valor, con_llave);
  auto firmado = hay_valor && !con_llave
                     ? protocolo::lo_que_se_firma(presentarse, resultado.protocolo, de_la_conexion->bytes(), valor)
                     : std::vector<std::uint8_t>();
  const auto borrar = gsl::finally([&presentarse, &firmado, &salida] {
    mbedtls_platform_zeroize(&presentarse.contrasena[0], presentarse.contrasena.size());
    mbedtls_platform_zeroize(firmado.data(), firmado.size());
    mbedtls_platform_zeroize(salida.data(), salida.size());
  });
  if (con_llave) {
    if (!hay_valor || !tocar_llaves(presentarse, quien, resultado.protocolo, de_la_conexion->bytes(), valor)) {
      resultado.estado = EstadoConexion::sin_llave;
      return resultado;
    }
    limite = Reloj::now() + opciones.tiempo_para_saludar;
  } else if (auto firma = firmado.empty() ? std::nullopt : quien.identidad->firmar(firmado)) {
    presentarse.firma = std::move(*firma);
  } else {
    return resultado;
  }
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
