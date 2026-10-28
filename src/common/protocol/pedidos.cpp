#include "common/protocol/pedidos.h"

#include <algorithm>
#include <functional>

#include "common/texto.h"

namespace ventrik::protocolo {
bool es_tipo_valido(TipoCanal tipo, bool solo_admins) {
  return tipo == TipoCanal::texto || (tipo == TipoCanal::voz && !solo_admins);
}

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

void escribir_tipo(Escritor& escritor, TipoCanal tipo, bool solo_admins) {
  if (!es_tipo_valido(tipo, solo_admins)) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_u8(static_cast<std::uint8_t>(tipo));
  escritor.escribir_booleano(solo_admins);
}

void leer_tipo(Lector& lector, TipoCanal& tipo, bool& solo_admins) {
  tipo = static_cast<TipoCanal>(lector.leer_u8());
  solo_admins = lector.leer_booleano();
  if (lector.ok() && !es_tipo_valido(tipo, solo_admins)) {
    lector.marcar_error();
  }
}

void escribir_canal(Escritor& escritor, const CanalInfo& canal) {
  escritor.escribir_u32(canal.id);
  escribir_nombre(escritor, canal.nombre);
  escribir_tipo(escritor, canal.tipo, canal.solo_admins);
  escritor.escribir_u32(canal.contador);
}

void leer_canal(Lector& lector, CanalInfo& canal) {
  canal.id = lector.leer_u32();
  leer_nombre(lector, canal.nombre);
  leer_tipo(lector, canal.tipo, canal.solo_admins);
  canal.contador = lector.leer_u32();
}

std::uint8_t leer_silencio(Lector& lector) {
  const std::uint8_t silencio = lector.leer_u8();
  if (!es_silencio_valido(silencio)) {
    lector.marcar_error();
  }
  return silencio;
}

void escribir_usuario(Escritor& escritor, const UsuarioInfo& usuario) {
  escritor.escribir_u32(usuario.id);
  escribir_nombre(escritor, usuario.nombre);
  escritor.escribir_u32(usuario.canal);
  escritor.escribir_u8(usuario.silencio);
  escritor.escribir_booleano(usuario.admin);
}

void leer_usuario(Lector& lector, UsuarioInfo& usuario) {
  usuario.id = lector.leer_u32();
  leer_nombre(lector, usuario.nombre);
  usuario.canal = lector.leer_u32();
  usuario.silencio = leer_silencio(lector);
  usuario.admin = lector.leer_booleano();
}

bool medio_valido(TipoFlujo tipo, std::uint8_t flujo, std::uint8_t nivel, std::size_t largo) {
  return tipo == TipoFlujo::voz && flujo <= kLugaresDeSusurro && nivel <= kNivelMaximo && largo > 0 && largo <= kMaximoOpus;
}

void escribir_medio(Escritor& escritor, const Medio& medio) {
  if (!medio_valido(medio.tipo, medio.flujo, medio.nivel, medio.datos.size())) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_u8(static_cast<std::uint8_t>(medio.tipo));
  escritor.escribir_u8(medio.flujo);
  escritor.escribir_u16(medio.numero);
  escritor.escribir_u16(medio.tiempo);
  escritor.escribir_u8(medio.nivel);
  escritor.escribir_u16(static_cast<std::uint16_t>(medio.datos.size()));
  escritor.escribir_bytes(medio.datos);
}

void leer_medio(Lector& lector, Medio& medio) {
  medio.tipo = static_cast<TipoFlujo>(lector.leer_u8());
  medio.flujo = lector.leer_u8();
  medio.numero = lector.leer_u16();
  medio.tiempo = lector.leer_u16();
  medio.nivel = lector.leer_u8();
  const std::size_t largo = lector.leer_u16();
  if (!medio_valido(medio.tipo, medio.flujo, medio.nivel, largo)) {
    lector.marcar_error();
    return;
  }
  const auto bytes = lector.leer_bytes(largo);
  medio.datos.assign(bytes.begin(), bytes.end());
}

bool es_sentido(Sentido sentido) {
  return sentido == Sentido::viejos || sentido == Sentido::nuevos;
}

void escribir_sentido(Escritor& escritor, Sentido sentido) {
  if (!es_sentido(sentido)) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_u8(static_cast<std::uint8_t>(sentido));
}

void leer_sentido(Lector& lector, Sentido& sentido) {
  sentido = static_cast<Sentido>(lector.leer_u8());
  if (lector.ok() && !es_sentido(sentido)) {
    lector.marcar_error();
  }
}

std::uint64_t antes_del_primero(Sentido sentido, std::uint64_t desde) {
  return sentido == Sentido::viejos && desde == 0 ? UINT64_MAX : desde;
}

bool sigue(Sentido sentido, std::uint64_t anterior, std::uint64_t id) {
  return sentido == Sentido::nuevos ? id > anterior : id < anterior;
}

bool es_pedido(std::uint16_t tipo) {
  return tipo >= static_cast<std::uint16_t>(TipoMensaje::presentarse) &&
         tipo <= static_cast<std::uint16_t>(TipoMensaje::destinos_de_susurro);
}

bool son_ids_validos(const std::vector<std::uint32_t>& usuarios) {
  return (usuarios.empty() || usuarios.front() != 0) &&
         std::adjacent_find(usuarios.begin(), usuarios.end(), std::greater_equal<>()) == usuarios.end();
}

bool es_lugar_de_susurro(std::uint8_t lugar) { return lugar >= 1 && lugar <= kLugaresDeSusurro; }

}

void escribir_sin_firma(Escritor& escritor, const Presentarse& mensaje) {
  escribir_nombre(escritor, mensaje.nombre);
  if (mensaje.codigo_dueno) {
    escritor.escribir_u8(static_cast<std::uint8_t>(mensaje.codigo_dueno->size()));
    escritor.escribir_bytes(*mensaje.codigo_dueno);
  } else {
    escritor.escribir_u8(0);
  }
  escritor.escribir_bytes(mensaje.clave);
  escritor.escribir_bytes(mensaje.equipo);
  escritor.escribir_texto(mensaje.contrasena, kMaximoBytesContrasena);
}

std::vector<std::uint8_t> lo_que_se_firma(const Presentarse& mensaje, std::uint16_t protocolo,
                                          gsl::span<const std::uint8_t> huella,
                                          gsl::span<const std::uint8_t> valor_de_conexion) {
  constexpr char kEtiqueta[] = "ventrik presentarse v5";
  Escritor escritor;
  escritor.escribir_bytes(gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(kEtiqueta),
                                                        sizeof(kEtiqueta) - 1));
  escritor.escribir_u16(protocolo);
  escritor.escribir_bytes(huella);
  escritor.escribir_bytes(valor_de_conexion);
  escribir_sin_firma(escritor, mensaje);
  return escritor.ok() ? std::move(escritor).soltar() : std::vector<std::uint8_t>();
}

void escribir(Escritor& escritor, const Presentarse& mensaje) {
  escribir_sin_firma(escritor, mensaje);
  if (mensaje.firma.empty() || mensaje.firma.size() > kMaximoFirma) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_u8(static_cast<std::uint8_t>(mensaje.firma.size()));
  escritor.escribir_bytes(mensaje.firma);
}

void leer(Lector& lector, Presentarse& mensaje) {
  leer_nombre(lector, mensaje.nombre);
  const std::size_t largo = lector.leer_u8();
  if (largo != 0) {
    std::array<std::uint8_t, 16> codigo{};
    const auto bytes = lector.leer_bytes(codigo.size());
    if (largo != codigo.size() || !lector.ok()) {
      lector.marcar_error();
      return;
    }
    std::copy(bytes.begin(), bytes.end(), codigo.begin());
    mensaje.codigo_dueno = codigo;
  }
  const auto clave = lector.leer_bytes(mensaje.clave.size());
  const auto equipo = lector.leer_bytes(mensaje.equipo.size());
  mensaje.contrasena = lector.leer_texto(kMaximoBytesContrasena);
  const std::size_t largo_firma = lector.leer_u8();
  const auto firma = lector.leer_bytes(largo_firma);
  if (!lector.ok() || largo_firma == 0 || largo_firma > kMaximoFirma) {
    lector.marcar_error();
    return;
  }
  std::copy(clave.begin(), clave.end(), mensaje.clave.begin());
  std::copy(equipo.begin(), equipo.end(), mensaje.equipo.begin());
  mensaje.firma.assign(firma.begin(), firma.end());
}

void escribir(Escritor& escritor, const Presentado& mensaje) {
  escritor.escribir_u32(mensaje.tu_id);
  escritor.escribir_booleano(mensaje.dueno);
  escribir_nombre(escritor, mensaje.servidor);
  escribir_lista(escritor, mensaje.canales, kMaximoCanalesEnLista,
                 [&escritor](const CanalInfo& canal) { escribir_canal(escritor, canal); });
  escribir_lista(escritor, mensaje.usuarios, kMaximoUsuariosEnLista,
                 [&escritor](const UsuarioInfo& usuario) { escribir_usuario(escritor, usuario); });
}

void leer(Lector& lector, Presentado& mensaje) {
  mensaje.tu_id = lector.leer_u32();
  mensaje.dueno = lector.leer_booleano();
  leer_nombre(lector, mensaje.servidor);
  leer_lista(lector, mensaje.canales, kMaximoCanalesEnLista,
             [&lector](CanalInfo& canal) { leer_canal(lector, canal); });
  leer_lista(lector, mensaje.usuarios, kMaximoUsuariosEnLista,
             [&lector](UsuarioInfo& usuario) { leer_usuario(lector, usuario); });
}

void escribir(Escritor& escritor, const PresentacionRechazada& mensaje) {
  escritor.escribir_u8(static_cast<std::uint8_t>(mensaje.motivo));
  escritor.escribir_u16(mensaje.espera);
}

void leer(Lector& lector, PresentacionRechazada& mensaje) {
  const std::uint8_t motivo = lector.leer_u8();
  if (motivo < static_cast<std::uint8_t>(MotivoPresentacion::nombre_invalido) ||
      motivo > static_cast<std::uint8_t>(MotivoPresentacion::esperar)) {
    lector.marcar_error();
    return;
  }
  mensaje.motivo = static_cast<MotivoPresentacion>(motivo);
  mensaje.espera = lector.leer_u16();
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

void escribir(Escritor& escritor, const CanalCambiado& mensaje) {
  escribir_canal(escritor, mensaje.canal);
}

void leer(Lector& lector, CanalCambiado& mensaje) {
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
  escritor.escribir_u32(mensaje.canal);
  escribir_chat(escritor, mensaje.texto);
}

void leer(Lector& lector, EnviarChat& mensaje) {
  mensaje.canal = lector.leer_u32();
  leer_chat(lector, mensaje.texto);
}

void escribir(Escritor& escritor, const MensajeChat& mensaje) {
  escritor.escribir_u64(mensaje.id);
  escritor.escribir_u32(mensaje.canal);
  escritor.escribir_u32(mensaje.numero);
  escritor.escribir_u32(mensaje.autor_id);
  escribir_nombre(escritor, mensaje.autor);
  escritor.escribir_u64(mensaje.hora_ms);
  escribir_chat(escritor, mensaje.texto);
}

void leer(Lector& lector, MensajeChat& mensaje) {
  mensaje.id = lector.leer_u64();
  mensaje.canal = lector.leer_u32();
  mensaje.numero = lector.leer_u32();
  mensaje.autor_id = lector.leer_u32();
  leer_nombre(lector, mensaje.autor);
  mensaje.hora_ms = lector.leer_u64();
  leer_chat(lector, mensaje.texto);
}

void escribir(Escritor& escritor, const PedirHistorial& mensaje) {
  escritor.escribir_u32(mensaje.canal);
  escritor.escribir_u64(mensaje.desde);
  escribir_sentido(escritor, mensaje.sentido);
  if (mensaje.cantidad == 0 || mensaje.cantidad > kMaximoMensajesEnHistorial) {
    escritor.marcar_error();
  }
  escritor.escribir_u16(mensaje.cantidad);
}

void leer(Lector& lector, PedirHistorial& mensaje) {
  mensaje.canal = lector.leer_u32();
  mensaje.desde = lector.leer_u64();
  leer_sentido(lector, mensaje.sentido);
  mensaje.cantidad = lector.leer_u16();
  if (mensaje.cantidad == 0 || mensaje.cantidad > kMaximoMensajesEnHistorial) {
    lector.marcar_error();
  }
}

void escribir(Escritor& escritor, const Historial& mensaje) {
  escritor.escribir_u32(mensaje.canal);
  escritor.escribir_u64(mensaje.desde);
  escribir_sentido(escritor, mensaje.sentido);
  escritor.escribir_booleano(mensaje.hay_mas);
  std::uint64_t anterior = antes_del_primero(mensaje.sentido, mensaje.desde);
  escribir_lista(escritor, mensaje.mensajes, kMaximoMensajesEnHistorial,
                 [&escritor, &mensaje, &anterior](const MensajeGuardado& guardado) {
                   if (!sigue(mensaje.sentido, anterior, guardado.id)) {
                     escritor.marcar_error();
                   }
                   anterior = guardado.id;
                   escritor.escribir_u64(guardado.id);
                   escritor.escribir_u32(guardado.numero);
                   escribir_nombre(escritor, guardado.autor);
                   escritor.escribir_u64(guardado.hora_ms);
                   escribir_chat(escritor, guardado.texto);
                 });
}

void leer(Lector& lector, Historial& mensaje) {
  mensaje.canal = lector.leer_u32();
  mensaje.desde = lector.leer_u64();
  leer_sentido(lector, mensaje.sentido);
  mensaje.hay_mas = lector.leer_booleano();
  std::uint64_t anterior = antes_del_primero(mensaje.sentido, mensaje.desde);
  leer_lista(lector, mensaje.mensajes, kMaximoMensajesEnHistorial,
             [&lector, &mensaje, &anterior](MensajeGuardado& guardado) {
               guardado.id = lector.leer_u64();
               if (lector.ok() && !sigue(mensaje.sentido, anterior, guardado.id)) {
                 lector.marcar_error();
               }
               anterior = guardado.id;
               guardado.numero = lector.leer_u32();
               leer_nombre(lector, guardado.autor);
               guardado.hora_ms = lector.leer_u64();
               leer_chat(lector, guardado.texto);
             });
}

void escribir(Escritor& escritor, const CrearCanal& mensaje) {
  escribir_nombre(escritor, mensaje.nombre);
  escribir_tipo(escritor, mensaje.tipo, mensaje.solo_admins);
}

void leer(Lector& lector, CrearCanal& mensaje) {
  leer_nombre(lector, mensaje.nombre);
  leer_tipo(lector, mensaje.tipo, mensaje.solo_admins);
}

void escribir(Escritor& escritor, const CambiarCanal& mensaje) {
  escritor.escribir_u32(mensaje.canal);
  escribir_nombre(escritor, mensaje.nombre);
  escritor.escribir_booleano(mensaje.solo_admins);
}

void leer(Lector& lector, CambiarCanal& mensaje) {
  mensaje.canal = lector.leer_u32();
  leer_nombre(lector, mensaje.nombre);
  mensaje.solo_admins = lector.leer_booleano();
}

void escribir(Escritor& escritor, const MirarCanal& mensaje) {
  escritor.escribir_u32(mensaje.canal);
}

void leer(Lector& lector, MirarCanal& mensaje) {
  mensaje.canal = lector.leer_u32();
}

void escribir(Escritor& escritor, const Silenciar& mensaje) {
  escritor.escribir_u8(mensaje.silencio);
}

void leer(Lector& lector, Silenciar& mensaje) {
  mensaje.silencio = leer_silencio(lector);
}

void escribir(Escritor& escritor, const SilenciarPersonas& mensaje) {
  if (!son_ids_validos(mensaje.usuarios)) {
    escritor.marcar_error();
    return;
  }
  escribir_lista(escritor, mensaje.usuarios, kMaximoSilenciados,
                 [&escritor](std::uint32_t usuario) { escritor.escribir_u32(usuario); });
}

void leer(Lector& lector, SilenciarPersonas& mensaje) {
  leer_lista(lector, mensaje.usuarios, kMaximoSilenciados,
             [&lector](std::uint32_t& usuario) { usuario = lector.leer_u32(); });
  if (!son_ids_validos(mensaje.usuarios)) {
    lector.marcar_error();
  }
}

void escribir(Escritor& escritor, const DestinosDeSusurro& mensaje) {
  if (!es_lugar_de_susurro(mensaje.lugar) || !son_ids_validos(mensaje.personas)) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_u8(mensaje.lugar);
  escritor.escribir_u32(mensaje.canal);
  escribir_lista(escritor, mensaje.personas, kMaximoPersonasEnSusurro,
                 [&escritor](std::uint32_t persona) { escritor.escribir_u32(persona); });
}

void leer(Lector& lector, DestinosDeSusurro& mensaje) {
  mensaje.lugar = lector.leer_u8();
  mensaje.canal = lector.leer_u32();
  leer_lista(lector, mensaje.personas, kMaximoPersonasEnSusurro,
             [&lector](std::uint32_t& persona) { persona = lector.leer_u32(); });
  if (!es_lugar_de_susurro(mensaje.lugar) || !son_ids_validos(mensaje.personas)) {
    lector.marcar_error();
  }
}

void escribir(Escritor& escritor, const UsuarioSilencio& mensaje) {
  escritor.escribir_u32(mensaje.id);
  escritor.escribir_u8(mensaje.silencio);
}

void leer(Lector& lector, UsuarioSilencio& mensaje) {
  mensaje.id = lector.leer_u32();
  mensaje.silencio = leer_silencio(lector);
}

void escribir(Escritor& escritor, const MensajesNuevos& mensaje) {
  escribir_lista(escritor, mensaje.canales, kMaximoCanalesEnLista, [&escritor](const Contador& contador) {
    escritor.escribir_u32(contador.canal);
    escritor.escribir_u32(contador.contador);
  });
}

void leer(Lector& lector, MensajesNuevos& mensaje) {
  leer_lista(lector, mensaje.canales, kMaximoCanalesEnLista, [&lector](Contador& contador) {
    contador.canal = lector.leer_u32();
    contador.contador = lector.leer_u32();
  });
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
      motivo > static_cast<std::uint8_t>(MotivoNegado::tipo_de_canal)) {
    lector.marcar_error();
    return;
  }
  mensaje.pedido = static_cast<TipoMensaje>(pedido);
  mensaje.motivo = static_cast<MotivoNegado>(motivo);
}

void escribir(Escritor&, const Desplazado&) {}

void leer(Lector&, Desplazado&) {}

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
