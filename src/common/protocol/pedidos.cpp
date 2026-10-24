#include "common/protocol/pedidos.h"

#include <algorithm>

#include "common/texto.h"

namespace ventrik::protocolo {
namespace {
void escribir_nombre(Escritor& escritor, const std::string& nombre) {
  if (!es_nombre_valido(nombre)) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_texto(nombre, kMaximoBytesNombre);
}

void leer_nombre(Lector& lector, std::string& nombre) {
  nombre = lector.leer_texto(kMaximoBytesNombre);
  if (lector.ok() && !es_nombre_valido(nombre)) {
    lector.marcar_error();
  }
}

void escribir_chat(Escritor& escritor, const std::string& texto) {
  if (!es_chat_valido(texto)) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_texto(texto, kMaximoBytesChat);
}

void leer_chat(Lector& lector, std::string& texto) {
  texto = lector.leer_texto(kMaximoBytesChat);
  if (lector.ok() && !es_chat_valido(texto)) {
    lector.marcar_error();
  }
}

template <typename Elemento, typename Escribir>
void escribir_lista(Escritor& escritor, const std::vector<Elemento>& lista, std::size_t maximo,
                    Escribir&& escribir_uno) {
  if (lista.size() > maximo) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_u16(static_cast<std::uint16_t>(lista.size()));
  for (const Elemento& elemento : lista) {
    escribir_uno(elemento);
  }
}

template <typename Elemento, typename Leer>
void leer_lista(Lector& lector, std::vector<Elemento>& lista, std::size_t maximo, Leer&& leer_uno) {
  const std::size_t cantidad = lector.leer_u16();
  if (cantidad > maximo) {
    lector.marcar_error();
    return;
  }
  for (std::size_t i = 0; i < cantidad && lector.ok(); ++i) {
    Elemento elemento{};
    leer_uno(elemento);
    lista.push_back(std::move(elemento));
  }
}

void escribir_canal(Escritor& escritor, const CanalInfo& canal) {
  escritor.escribir_u32(canal.id);
  escribir_nombre(escritor, canal.nombre);
}

void leer_canal(Lector& lector, CanalInfo& canal) {
  canal.id = lector.leer_u32();
  leer_nombre(lector, canal.nombre);
}

void escribir_usuario(Escritor& escritor, const UsuarioInfo& usuario) {
  escritor.escribir_u32(usuario.id);
  escribir_nombre(escritor, usuario.nombre);
  escritor.escribir_u32(usuario.canal);
}

void leer_usuario(Lector& lector, UsuarioInfo& usuario) {
  usuario.id = lector.leer_u32();
  leer_nombre(lector, usuario.nombre);
  usuario.canal = lector.leer_u32();
}

bool medio_valido(TipoFlujo tipo, std::uint8_t flujo, std::size_t largo) {
  return tipo == TipoFlujo::voz && flujo == 0 && largo > 0 && largo <= kMaximoOpus;
}

void escribir_medio(Escritor& escritor, const Medio& medio) {
  if (!medio_valido(medio.tipo, medio.flujo, medio.datos.size())) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_u8(static_cast<std::uint8_t>(medio.tipo));
  escritor.escribir_u8(medio.flujo);
  escritor.escribir_u16(medio.numero);
  escritor.escribir_u16(static_cast<std::uint16_t>(medio.datos.size()));
  escritor.escribir_bytes(medio.datos);
}

void leer_medio(Lector& lector, Medio& medio) {
  medio.tipo = static_cast<TipoFlujo>(lector.leer_u8());
  medio.flujo = lector.leer_u8();
  medio.numero = lector.leer_u16();
  const std::size_t largo = lector.leer_u16();
  if (!medio_valido(medio.tipo, medio.flujo, largo)) {
    lector.marcar_error();
    return;
  }
  const auto bytes = lector.leer_bytes(largo);
  medio.datos.assign(bytes.begin(), bytes.end());
}

bool es_pedido(std::uint16_t tipo) {
  return tipo >= static_cast<std::uint16_t>(TipoMensaje::presentarse) &&
         tipo <= static_cast<std::uint16_t>(TipoMensaje::ping);
}

}

void escribir(Escritor& escritor, const Presentarse& mensaje) {
  escribir_nombre(escritor, mensaje.nombre);
  if (mensaje.codigo_dueno) {
    escritor.escribir_u8(static_cast<std::uint8_t>(mensaje.codigo_dueno->size()));
    escritor.escribir_bytes(*mensaje.codigo_dueno);
  } else {
    escritor.escribir_u8(0);
  }
}

void leer(Lector& lector, Presentarse& mensaje) {
  leer_nombre(lector, mensaje.nombre);
  const std::size_t largo = lector.leer_u8();
  if (largo == 0) {
    return;
  }
  std::array<std::uint8_t, 16> codigo{};
  if (largo != codigo.size()) {
    lector.marcar_error();
    return;
  }
  const auto bytes = lector.leer_bytes(largo);
  std::copy(bytes.begin(), bytes.end(), codigo.begin());
  mensaje.codigo_dueno = codigo;
}

void escribir(Escritor& escritor, const Presentado& mensaje) {
  escritor.escribir_u32(mensaje.tu_id);
  escritor.escribir_booleano(mensaje.dueno);
  escribir_lista(escritor, mensaje.canales, kMaximoCanalesEnLista,
                 [&escritor](const CanalInfo& canal) { escribir_canal(escritor, canal); });
  escribir_lista(escritor, mensaje.usuarios, kMaximoUsuariosEnLista,
                 [&escritor](const UsuarioInfo& usuario) { escribir_usuario(escritor, usuario); });
}

void leer(Lector& lector, Presentado& mensaje) {
  mensaje.tu_id = lector.leer_u32();
  mensaje.dueno = lector.leer_booleano();
  leer_lista(lector, mensaje.canales, kMaximoCanalesEnLista,
             [&lector](CanalInfo& canal) { leer_canal(lector, canal); });
  leer_lista(lector, mensaje.usuarios, kMaximoUsuariosEnLista,
             [&lector](UsuarioInfo& usuario) { leer_usuario(lector, usuario); });
}

void escribir(Escritor& escritor, const PresentacionRechazada& mensaje) {
  escritor.escribir_u8(static_cast<std::uint8_t>(mensaje.motivo));
}

void leer(Lector& lector, PresentacionRechazada& mensaje) {
  const std::uint8_t motivo = lector.leer_u8();
  if (motivo < static_cast<std::uint8_t>(MotivoPresentacion::nombre_invalido) ||
      motivo > static_cast<std::uint8_t>(MotivoPresentacion::lleno)) {
    lector.marcar_error();
    return;
  }
  mensaje.motivo = static_cast<MotivoPresentacion>(motivo);
}

void escribir(Escritor& escritor, const UsuarioEntro& mensaje) {
  escribir_usuario(escritor, mensaje.usuario);
}

void leer(Lector& lector, UsuarioEntro& mensaje) {
  leer_usuario(lector, mensaje.usuario);
}

void escribir(Escritor& escritor, const UsuarioSalio& mensaje) {
  escritor.escribir_u32(mensaje.id);
}

void leer(Lector& lector, UsuarioSalio& mensaje) {
  mensaje.id = lector.leer_u32();
}

void escribir(Escritor& escritor, const UsuarioCambioCanal& mensaje) {
  escritor.escribir_u32(mensaje.id);
  escritor.escribir_u32(mensaje.canal);
}

void leer(Lector& lector, UsuarioCambioCanal& mensaje) {
  mensaje.id = lector.leer_u32();
  mensaje.canal = lector.leer_u32();
}

void escribir(Escritor& escritor, const CanalCreado& mensaje) {
  escribir_canal(escritor, mensaje.canal);
}

void leer(Lector& lector, CanalCreado& mensaje) {
  leer_canal(lector, mensaje.canal);
}

void escribir(Escritor& escritor, const CanalRenombrado& mensaje) {
  escribir_canal(escritor, mensaje.canal);
}

void leer(Lector& lector, CanalRenombrado& mensaje) {
  leer_canal(lector, mensaje.canal);
}

void escribir(Escritor& escritor, const CanalBorrado& mensaje) {
  escritor.escribir_u32(mensaje.id);
  escritor.escribir_u32(mensaje.movidos_a);
}

void leer(Lector& lector, CanalBorrado& mensaje) {
  mensaje.id = lector.leer_u32();
  mensaje.movidos_a = lector.leer_u32();
}

void escribir(Escritor& escritor, const EntrarACanal& mensaje) {
  escritor.escribir_u32(mensaje.canal);
}

void leer(Lector& lector, EntrarACanal& mensaje) {
  mensaje.canal = lector.leer_u32();
}

void escribir(Escritor& escritor, const EnviarChat& mensaje) {
  escribir_chat(escritor, mensaje.texto);
}

void leer(Lector& lector, EnviarChat& mensaje) {
  leer_chat(lector, mensaje.texto);
}

void escribir(Escritor& escritor, const MensajeChat& mensaje) {
  escritor.escribir_u64(mensaje.id);
  escritor.escribir_u32(mensaje.canal);
  escritor.escribir_u32(mensaje.autor_id);
  escribir_nombre(escritor, mensaje.autor);
  escritor.escribir_u64(mensaje.hora_ms);
  escribir_chat(escritor, mensaje.texto);
}

void leer(Lector& lector, MensajeChat& mensaje) {
  mensaje.id = lector.leer_u64();
  mensaje.canal = lector.leer_u32();
  mensaje.autor_id = lector.leer_u32();
  leer_nombre(lector, mensaje.autor);
  mensaje.hora_ms = lector.leer_u64();
  leer_chat(lector, mensaje.texto);
}

void escribir(Escritor& escritor, const PedirHistorial& mensaje) {
  escritor.escribir_u32(mensaje.canal);
  escritor.escribir_u64(mensaje.antes_de);
}

void leer(Lector& lector, PedirHistorial& mensaje) {
  mensaje.canal = lector.leer_u32();
  mensaje.antes_de = lector.leer_u64();
}

void escribir(Escritor& escritor, const Historial& mensaje) {
  escritor.escribir_u32(mensaje.canal);
  escritor.escribir_booleano(mensaje.hay_mas);
  escribir_lista(escritor, mensaje.mensajes, kMaximoMensajesEnHistorial,
                 [&escritor](const MensajeGuardado& guardado) {
                   escritor.escribir_u64(guardado.id);
                   escribir_nombre(escritor, guardado.autor);
                   escritor.escribir_u64(guardado.hora_ms);
                   escribir_chat(escritor, guardado.texto);
                 });
}

void leer(Lector& lector, Historial& mensaje) {
  mensaje.canal = lector.leer_u32();
  mensaje.hay_mas = lector.leer_booleano();
  leer_lista(lector, mensaje.mensajes, kMaximoMensajesEnHistorial,
             [&lector](MensajeGuardado& guardado) {
               guardado.id = lector.leer_u64();
               leer_nombre(lector, guardado.autor);
               guardado.hora_ms = lector.leer_u64();
               leer_chat(lector, guardado.texto);
             });
}

void escribir(Escritor& escritor, const CrearCanal& mensaje) {
  escribir_nombre(escritor, mensaje.nombre);
}

void leer(Lector& lector, CrearCanal& mensaje) {
  leer_nombre(lector, mensaje.nombre);
}

void escribir(Escritor& escritor, const RenombrarCanal& mensaje) {
  escritor.escribir_u32(mensaje.canal);
  escribir_nombre(escritor, mensaje.nombre);
}

void leer(Lector& lector, RenombrarCanal& mensaje) {
  mensaje.canal = lector.leer_u32();
  leer_nombre(lector, mensaje.nombre);
}

void escribir(Escritor& escritor, const BorrarCanal& mensaje) {
  escritor.escribir_u32(mensaje.canal);
}

void leer(Lector& lector, BorrarCanal& mensaje) {
  mensaje.canal = lector.leer_u32();
}

void escribir(Escritor& escritor, const Negado& mensaje) {
  escritor.escribir_u16(static_cast<std::uint16_t>(mensaje.pedido));
  escritor.escribir_u8(static_cast<std::uint8_t>(mensaje.motivo));
}

void leer(Lector& lector, Negado& mensaje) {
  const std::uint16_t pedido = lector.leer_u16();
  const std::uint8_t motivo = lector.leer_u8();
  if (!lector.ok()) {
    return;
  }
  if (!es_pedido(pedido) || motivo < static_cast<std::uint8_t>(MotivoNegado::sin_permiso) ||
      motivo > static_cast<std::uint8_t>(MotivoNegado::nombre_en_uso)) {
    lector.marcar_error();
    return;
  }
  mensaje.pedido = static_cast<TipoMensaje>(pedido);
  mensaje.motivo = static_cast<MotivoNegado>(motivo);
}

void escribir(Escritor& escritor, const Ping& mensaje) {
  escritor.escribir_u64(mensaje.marca);
}

void leer(Lector& lector, Ping& mensaje) {
  mensaje.marca = lector.leer_u64();
}

void escribir(Escritor& escritor, const Pong& mensaje) {
  escritor.escribir_u64(mensaje.marca);
}

void leer(Lector& lector, Pong& mensaje) {
  mensaje.marca = lector.leer_u64();
}

void escribir(Escritor& escritor, const ClaveVoz& mensaje) {
  escritor.escribir_bytes(mensaje.identidad);
}

void leer(Lector& lector, ClaveVoz& mensaje) {
  const auto identidad = lector.leer_bytes(mensaje.identidad.size());
  std::copy(identidad.begin(), identidad.end(), mensaje.identidad.begin());
}

void escribir(Escritor& escritor, const MedioSubida& mensaje) { escribir_medio(escritor, mensaje.medio); }

void leer(Lector& lector, MedioSubida& mensaje) { leer_medio(lector, mensaje.medio); }

void escribir(Escritor& escritor, const MedioBajada& mensaje) {
  escritor.escribir_u32(mensaje.origen);
  escribir_medio(escritor, mensaje.medio);
}

void leer(Lector& lector, MedioBajada& mensaje) {
  mensaje.origen = lector.leer_u32();
  leer_medio(lector, mensaje.medio);
}

std::optional<Marco> separar_datagrama(gsl::span<const std::uint8_t> datos) {
  if (datos.size() > kMaximoDatagrama) {
    return std::nullopt;
  }
  Lector lector(datos);
  Marco marco;
  marco.tipo = lector.leer_u16();
  const auto cuerpo = lector.leer_bytes(lector.restante());
  if (!lector.terminado()) {
    return std::nullopt;
  }
  marco.cuerpo.assign(cuerpo.begin(), cuerpo.end());
  return marco;
}

}
