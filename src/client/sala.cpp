#include "client/sala.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "common/texto.h"

namespace ventrik::cliente {
namespace {
using protocolo::TipoCanal;

template <typename Lista>
auto buscar(Lista& canales, std::uint32_t id) -> decltype(canales.data()) {
  const auto encontrado = std::find_if(canales.begin(), canales.end(),
                                       [id](const protocolo::CanalInfo& info) { return info.id == id; });
  return encontrado == canales.end() ? nullptr : &*encontrado;
}

void insertar(Chat& chat, protocolo::MensajeGuardado mensaje) {
  const auto lugar = std::lower_bound(
      chat.mensajes.begin(), chat.mensajes.end(), mensaje.id,
      [](const protocolo::MensajeGuardado& guardado, std::uint64_t id) { return guardado.id < id; });
  if (lugar != chat.mensajes.end() && lugar->id == mensaje.id) {
    return;
  }
  chat.bytes += mensaje.autor.size() + mensaje.texto.size();
  chat.mensajes.insert(lugar, std::move(mensaje));
}

void recortar(Chat& chat) {
  while (chat.mensajes.size() > kTopeMensajesPorChat || chat.bytes > kTopeBytesPorChat) {
    chat.bytes -= chat.mensajes.front().autor.size() + chat.mensajes.front().texto.size();
    chat.mensajes.pop_front();
    chat.hay_mas = true;
  }
}

}

Sala::Sala(const protocolo::Presentado& presentado, const std::vector<Leido>& leidos)
    : tu_id_(presentado.tu_id), dueno_(presentado.dueno), canales_(presentado.canales) {
  canales_.erase(std::remove_if(canales_.begin(), canales_.end(),
                                [](const protocolo::CanalInfo& info) { return info.id == 0; }),
                 canales_.end());
  for (const auto& usuario : presentado.usuarios) {
    usuarios_[usuario.id] = usuario;
  }
  if (leidos.empty()) {
    for (const auto& info : canales_) {
      leidos_[info.id] = info.contador;
    }
  }
  for (const Leido& leido : leidos) {
    if (const auto* info = canal(leido.canal)) {
      leidos_[leido.canal] = std::min(leido.numero, info->contador);
    }
  }
  const auto yo = usuarios_.find(tu_id_);
  voz_.canal = yo != usuarios_.end() ? yo->second.canal : 0;
}

std::vector<Orden> Sala::empezar() {
  return cambiar_voz(voz_.canal);
}

std::vector<Orden> Sala::aplicar(const Novedad& novedad) {
  por_canal_.reset();
  return std::visit([this](const auto& cual) { return atender(cual); }, novedad);
}

const std::vector<const protocolo::UsuarioInfo*>& Sala::en_canal(std::uint32_t canal) const {
  static const std::vector<const protocolo::UsuarioInfo*> kNadie;
  if (!por_canal_) {
    por_canal_.emplace();
    for (const auto& par : usuarios_) {
      (*por_canal_)[par.second.canal].push_back(&par.second);
    }
  }
  const auto encontrado = por_canal_->find(canal);
  return encontrado != por_canal_->end() ? encontrado->second : kNadie;
}

const protocolo::CanalInfo* Sala::canal(std::uint32_t id) const {
  return buscar(canales_, id);
}

std::uint32_t Sala::nuevos(std::uint32_t id) const {
  const auto* info = canal(id);
  const auto leido = leidos_.find(id);
  const std::uint32_t hasta = leido == leidos_.end() ? 0 : leido->second;
  return info != nullptr && info->contador > hasta ? info->contador - hasta : 0;
}

std::vector<Leido> Sala::leidos() const {
  std::vector<Leido> lista;
  for (const auto& [id, numero] : leidos_) {
    if (lista.size() < kMaximoLeidosPorServidor) {
      lista.push_back({id, numero});
    }
  }
  return lista;
}

bool Sala::puede_escribir() const {
  const auto* info = canal(chat().canal);
  return info != nullptr && (info->tipo == TipoCanal::voz || !info->solo_admins || dueno_);
}

std::vector<Orden> Sala::elegir_voz(std::uint32_t id) {
  const auto* info = canal(id);
  if (info == nullptr || info->tipo != TipoCanal::voz) {
    return {};
  }
  std::vector<Orden> ordenes;
  if (texto_.canal != 0) {
    texto_ = Chat{};
    ordenes.emplace_back(protocolo::MirarCanal{0});
  }
  if (id != voz_.canal) {
    ordenes.emplace_back(protocolo::EntrarACanal{id});
  }
  negado_.reset();
  marcar_leido();
  return ordenes;
}

std::vector<Orden> Sala::elegir_texto(std::uint32_t id) {
  const auto* info = canal(id);
  if (info == nullptr || info->tipo != TipoCanal::texto || id == texto_.canal) {
    return {};
  }
  texto_ = Chat{id};
  texto_.pidiendo = true;
  negado_.reset();
  marcar_leido();
  return {protocolo::MirarCanal{id}, protocolo::PedirHistorial{id, 0}};
}

std::optional<Orden> Sala::enviar(const std::string& texto) {
  if (!puede_escribir() || !es_chat_valido(texto)) {
    return std::nullopt;
  }
  negado_.reset();
  return protocolo::EnviarChat{chat().canal, texto};
}

std::optional<Orden> Sala::pedir_mas_viejos() {
  Chat& actual = visible();
  if (actual.canal == 0 || actual.pidiendo || !actual.hay_mas || actual.mensajes.size() >= kTopeMensajesPorChat ||
      actual.bytes >= kTopeBytesPorChat || std::chrono::steady_clock::now() < historial_despues_) {
    return std::nullopt;
  }
  actual.pidiendo = true;
  return protocolo::PedirHistorial{actual.canal, actual.mensajes.empty() ? 0 : actual.mensajes.front().id};
}

std::vector<Orden> Sala::atender(const protocolo::UsuarioEntro& entro) {
  if (usuarios_.size() < protocolo::kMaximoUsuariosEnLista || usuarios_.count(entro.usuario.id) != 0) {
    usuarios_[entro.usuario.id] = entro.usuario;
  }
  return {};
}

std::vector<Orden> Sala::atender(const protocolo::UsuarioSalio& salio) {
  usuarios_.erase(salio.id);
  return {};
}

std::vector<Orden> Sala::atender(const protocolo::UsuarioCambioCanal& cambio) {
  const auto usuario = usuarios_.find(cambio.id);
  if (usuario == usuarios_.end()) {
    return {};
  }
  usuario->second.canal = cambio.canal;
  return cambio.id == tu_id_ && cambio.canal != voz_.canal ? cambiar_voz(cambio.canal)
                                                            : std::vector<Orden>{};
}

std::vector<Orden> Sala::atender(const protocolo::CanalCreado& creado) {
  if (creado.canal.id != 0 && canal(creado.canal.id) == nullptr &&
      canales_.size() < protocolo::kMaximoCanalesEnLista) {
    canales_.push_back(creado.canal);
  }
  return {};
}

std::vector<Orden> Sala::atender(const protocolo::CanalCambiado& cambiado) {
  if (auto* info = buscar(canales_, cambiado.canal.id)) {
    const std::uint32_t contador = std::max(info->contador, cambiado.canal.contador);
    *info = cambiado.canal;
    info->contador = contador;
  }
  return {};
}

std::vector<Orden> Sala::atender(const protocolo::CanalBorrado& borrado) {
  canales_.erase(std::remove_if(canales_.begin(), canales_.end(),
                                [&borrado](const protocolo::CanalInfo& info) { return info.id == borrado.id; }),
                 canales_.end());
  leidos_.erase(borrado.id);
  for (auto& par : usuarios_) {
    if (par.second.canal == borrado.id) {
      par.second.canal = borrado.movidos_a;
    }
  }
  if (texto_.canal == borrado.id) {
    texto_ = Chat{};
    marcar_leido();
  }
  return voz_.canal == borrado.id ? cambiar_voz(borrado.movidos_a) : std::vector<Orden>{};
}

std::vector<Orden> Sala::atender(const protocolo::MensajeChat& mensaje) {
  if (Chat* destino = chat_de(mensaje.canal)) {
    insertar(*destino, {mensaje.id, mensaje.numero, mensaje.autor, mensaje.hora_ms, mensaje.texto});
    recortar(*destino);
  }
  subir(mensaje.canal, mensaje.numero);
  return {};
}

std::vector<Orden> Sala::atender(const protocolo::MensajesNuevos& nuevos) {
  for (const auto& contador : nuevos.canales) {
    subir(contador.canal, contador.contador);
  }
  return {};
}

std::vector<Orden> Sala::atender(const protocolo::Historial& historial) {
  Chat* destino = chat_de(historial.canal);
  if (destino == nullptr || !destino->pidiendo) {
    return {};
  }
  for (const auto& mensaje : historial.mensajes) {
    insertar(*destino, mensaje);
  }
  destino->hay_mas = historial.hay_mas;
  destino->pidiendo = false;
  recortar(*destino);
  return {};
}

std::vector<Orden> Sala::atender(const protocolo::Negado& negado) {
  negado_ = negado;
  if (negado.pedido == protocolo::TipoMensaje::mirar_canal) {
    texto_ = Chat{};
    marcar_leido();
  }
  if (negado.pedido == protocolo::TipoMensaje::pedir_historial) {
    for (Chat* chat : {&voz_, &texto_}) {
      chat->hay_mas = chat->hay_mas || chat->pidiendo;
      chat->pidiendo = false;
    }
    historial_despues_ = std::chrono::steady_clock::now() + kEsperaTrasRechazo;
  }
  return {};
}

std::vector<Orden> Sala::atender(const PingMedido& ping) {
  ping_ = ping.milisegundos;
  return {};
}

std::vector<Orden> Sala::cambiar_voz(std::uint32_t id) {
  voz_ = Chat{id};
  voz_.pidiendo = id != 0;
  marcar_leido();
  return id == 0 ? std::vector<Orden>{} : std::vector<Orden>{protocolo::PedirHistorial{id, 0}};
}

Chat* Sala::chat_de(std::uint32_t id) {
  return id == 0 ? nullptr : id == texto_.canal ? &texto_ : id == voz_.canal ? &voz_ : nullptr;
}

void Sala::subir(std::uint32_t id, std::uint32_t contador) {
  if (auto* info = buscar(canales_, id)) {
    info->contador = std::max(info->contador, contador);
    if (id == chat().canal) {
      marcar_leido();
    }
  }
}

void Sala::marcar_leido() {
  if (const auto* info = canal(chat().canal)) {
    leidos_[info->id] = info->contador;
  }
}

}
