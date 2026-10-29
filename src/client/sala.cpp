#include "client/sala.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "common/texto.h"

namespace ventrik::cliente {
namespace {
using protocolo::Sentido;
using protocolo::TipoCanal;

template <typename Lista>
auto buscar(Lista& canales, std::uint32_t id) -> decltype(canales.data()) {
  const auto encontrado = std::find_if(canales.begin(), canales.end(),
                                       [id](const protocolo::CanalInfo& info) { return info.id == id; });
  return encontrado == canales.end() ? nullptr : &*encontrado;
}

auto lugar_de(Chat& chat, std::uint64_t id) {
  return std::lower_bound(chat.mensajes.begin(), chat.mensajes.end(), id,
                          [](const MensajeDeChat& guardado, std::uint64_t buscado) { return guardado.id < buscado; });
}

std::uint32_t indice_de(Chat& chat, const std::string& autor) {
  const auto conocido = std::find(chat.autores.begin(), chat.autores.end(), autor);
  if (conocido == chat.autores.end()) {
    chat.autores.push_back(autor);
    return static_cast<std::uint32_t>(chat.autores.size() - 1);
  }
  return static_cast<std::uint32_t>(conocido - chat.autores.begin());
}

void insertar(Chat& chat, const protocolo::MensajeChat& mensaje) {
  const auto lugar = lugar_de(chat, mensaje.id);
  if (lugar != chat.mensajes.end() && lugar->id == mensaje.id) {
    return;
  }
  const std::uint32_t autor = indice_de(chat, mensaje.autor);
  chat.bytes += mensaje.texto.size();
  chat.mensajes.insert(lugar, MensajeDeChat{mensaje.id, mensaje.hora_ms, mensaje.numero, autor, mensaje.texto});
}

void juntar(Chat& chat, const std::vector<protocolo::MensajeGuardado>& llegados) {
  const auto antes = static_cast<std::ptrdiff_t>(chat.mensajes.size());
  for (const auto& mensaje : llegados) {
    chat.mensajes.push_back(
        MensajeDeChat{mensaje.id, mensaje.hora_ms, mensaje.numero, indice_de(chat, mensaje.autor), mensaje.texto});
  }
  const auto por_id = [](const MensajeDeChat& uno, const MensajeDeChat& otro) { return uno.id < otro.id; };
  std::sort(chat.mensajes.begin() + antes, chat.mensajes.end(), por_id);
  std::inplace_merge(chat.mensajes.begin(), chat.mensajes.begin() + antes, chat.mensajes.end(), por_id);
  chat.mensajes.erase(std::unique(chat.mensajes.begin(), chat.mensajes.end(),
                                  [](const MensajeDeChat& uno, const MensajeDeChat& otro) { return uno.id == otro.id; }),
                      chat.mensajes.end());
  chat.bytes = 0;
  for (const MensajeDeChat& mensaje : chat.mensajes) {
    chat.bytes += mensaje.texto.size();
  }
}

void compactar(Chat& chat, std::size_t hasta) {
  if (chat.autores.size() <= hasta) {
    return;
  }
  std::vector<std::string> usados;
  std::vector<std::uint32_t> nuevo(chat.autores.size(), UINT32_MAX);
  for (MensajeDeChat& mensaje : chat.mensajes) {
    std::uint32_t& indice = nuevo[mensaje.autor];
    if (indice == UINT32_MAX) {
      indice = static_cast<std::uint32_t>(usados.size());
      usados.push_back(std::move(chat.autores[mensaje.autor]));
    }
    mensaje.autor = indice;
  }
  chat.autores = std::move(usados);
}

void recortar(Chat& chat, std::size_t ventana) {
  std::size_t inicio = 0;
  std::size_t fin = chat.mensajes.size();
  const std::size_t ancla =
      chat.ancla == 0 ? fin : static_cast<std::size_t>(lugar_de(chat, chat.ancla) - chat.mensajes.begin());
  while (fin > inicio && (fin - inicio > ventana || chat.bytes > kTopeBytesPorChat)) {
    if (fin - ancla > ancla - inicio) {
      chat.bytes -= chat.mensajes[--fin].texto.size();
      chat.hay_nuevos = true;
    } else {
      chat.bytes -= chat.mensajes[inicio++].texto.size();
      chat.hay_mas = true;
    }
  }
  chat.mensajes.erase(chat.mensajes.begin() + static_cast<std::ptrdiff_t>(fin), chat.mensajes.end());
  chat.mensajes.erase(chat.mensajes.begin(), chat.mensajes.begin() + static_cast<std::ptrdiff_t>(inicio));
  compactar(chat, 2 * kVentanaChat);
}

Orden pedir(Chat& chat, std::uint64_t desde, Sentido sentido, std::uint16_t cantidad) {
  chat.pidiendo = true;
  chat.pedido = {chat.canal, desde, sentido, cantidad};
  return chat.pedido;
}

}

Sala::Sala(const protocolo::Presentado& presentado, const std::vector<Leido>& leidos, std::uint32_t texto)
    : tu_id_(presentado.tu_id), rol_(presentado.rol), canales_(presentado.canales), ultimo_texto_(texto) {
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
  return voz_.canal != 0 ? cambiar_voz(voz_.canal) : revisar();
}

std::vector<Orden> Sala::revisar() {
  const auto es_texto = [](const protocolo::CanalInfo& info) { return info.tipo == TipoCanal::texto; };
  const auto primero = std::find_if(canales_.begin(), canales_.end(), es_texto);
  if (entrando_ || voz_.canal != 0 || texto_.canal != 0 || primero == canales_.end() ||
      std::chrono::steady_clock::now() < historial_despues_) {
    return {};
  }
  const std::wstring general = nombre_para_comparar("general");
  const auto* ultimo = canal(ultimo_texto_);
  const auto llamado_general = std::find_if(canales_.begin(), canales_.end(), [&](const protocolo::CanalInfo& info) {
    return es_texto(info) && nombre_para_comparar(info.nombre) == general;
  });
  return elegir_texto(ultimo != nullptr && es_texto(*ultimo) ? ultimo->id
                      : llamado_general != canales_.end()    ? llamado_general->id
                                                             : primero->id);
}

std::vector<Orden> Sala::salir_de_la_voz() {
  return voz_.canal == 0 ? std::vector<Orden>{} : std::vector<Orden>{protocolo::EntrarACanal{0}};
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
  const std::uint32_t hasta = leido(id);
  return info != nullptr && info->contador > hasta ? info->contador - hasta : 0;
}

std::uint32_t Sala::leido(std::uint32_t id) const {
  const auto encontrado = leidos_.find(id);
  return encontrado == leidos_.end() ? 0 : encontrado->second;
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
  return info != nullptr && (info->tipo == TipoCanal::voz || !info->solo_admins || admin());
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
    entrando_ = true;
  }
  negado_.reset();
  marcar_leido(chat().canal);
  return ordenes;
}

std::vector<Orden> Sala::elegir_texto(std::uint32_t id) {
  const auto* info = canal(id);
  if (info == nullptr || info->tipo != TipoCanal::texto || id == texto_.canal) {
    return {};
  }
  texto_ = Chat{id};
  ultimo_texto_ = id;
  negado_.reset();
  marcar_leido(chat().canal);
  return {protocolo::MirarCanal{id}, pedir(texto_, 0, Sentido::viejos, kMensajesAlEntrar)};
}

std::vector<Orden> Sala::volver_a_pedir() {
  const std::uint32_t canal = texto_.canal;
  texto_ = Chat{};
  return canal != 0 ? elegir_texto(canal) : revisar();
}

std::optional<Orden> Sala::enviar(const std::string& texto) {
  if (!puede_escribir() || !es_chat_valido(texto)) {
    return std::nullopt;
  }
  negado_.reset();
  return protocolo::EnviarChat{chat().canal, texto};
}

bool Sala::puede_pedir(const Chat& chat) const {
  return chat.canal != 0 && !chat.pidiendo && std::chrono::steady_clock::now() >= historial_despues_;
}

std::optional<Orden> Sala::pedir_mas_viejos() {
  Chat& actual = visible();
  if (!puede_pedir(actual) || !actual.hay_mas) {
    return std::nullopt;
  }
  return actual.mensajes.empty() ? pedir(actual, 0, Sentido::viejos, kMensajesAlEntrar)
                                 : pedir(actual, actual.mensajes.front().id, Sentido::viejos, kMensajesAlMoverse);
}

std::optional<Orden> Sala::pedir_mas_nuevos() {
  Chat& actual = visible();
  if (!puede_pedir(actual) || !actual.hay_nuevos || actual.mensajes.empty()) {
    return std::nullopt;
  }
  return pedir(actual, actual.mensajes.back().id, Sentido::nuevos, kMensajesAlMoverse);
}

std::optional<Orden> Sala::ir_a_lo_ultimo() {
  Chat& actual = visible();
  if (actual.canal == 0 || !actual.hay_nuevos) {
    return std::nullopt;
  }
  actual = Chat{actual.canal};
  return pedir(actual, 0, Sentido::viejos, kMensajesAlEntrar);
}

void Sala::soltar_lejanos() {
  for (Chat* chat : {&voz_, &texto_}) {
    recortar(*chat, kMensajesAlEntrar);
    compactar(*chat, 0);
    chat->mensajes.shrink_to_fit();
    chat->autores.shrink_to_fit();
  }
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
  entrando_ = entrando_ && cambio.id != tu_id_;
  return cambio.id == tu_id_ && cambio.canal != voz_.canal ? cambiar_voz(cambio.canal)
                                                            : std::vector<Orden>{};
}

std::vector<Orden> Sala::atender(const protocolo::UsuarioSilencio& silencio) {
  if (const auto usuario = usuarios_.find(silencio.id); usuario != usuarios_.end()) {
    usuario->second.silencio = silencio.silencio;
  }
  return {};
}

std::vector<Orden> Sala::atender(const protocolo::UsuarioRol& rol) {
  if (const auto usuario = usuarios_.find(rol.id); usuario != usuarios_.end()) {
    usuario->second.rol = rol.rol;
  }
  rol_ = rol.id == tu_id_ ? rol.rol : rol_;
  return {};
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
    marcar_leido(chat().canal);
  }
  return voz_.canal == borrado.id ? cambiar_voz(borrado.movidos_a) : std::vector<Orden>{};
}

std::vector<Orden> Sala::atender(const protocolo::MensajeChat& mensaje) {
  Chat* destino = chat_de(mensaje.canal);
  if (destino != nullptr && !destino->hay_nuevos) {
    insertar(*destino, mensaje);
    recortar(*destino, kVentanaChat);
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
  if (destino == nullptr || !destino->pidiendo || historial.desde != destino->pedido.desde ||
      historial.sentido != destino->pedido.sentido) {
    return {};
  }
  const bool nuevos = historial.sentido == Sentido::nuevos;
  if (historial.desde != 0 &&
      (destino->mensajes.empty() ||
       (nuevos ? destino->mensajes.back().id : destino->mensajes.front().id) != historial.desde)) {
    destino->pidiendo = false;
    (nuevos ? destino->hay_nuevos : destino->hay_mas) = true;
    return {};
  }
  juntar(*destino, historial.mensajes);
  (historial.sentido == Sentido::nuevos ? destino->hay_nuevos : destino->hay_mas) = historial.hay_mas;
  destino->pidiendo = false;
  recortar(*destino, kVentanaChat);
  return {};
}

std::vector<Orden> Sala::atender(const protocolo::Negado& negado) {
  if (negado.pedido != protocolo::TipoMensaje::silenciar_personas &&
      negado.pedido != protocolo::TipoMensaje::destinos_de_susurro) {
    negado_ = negado;
  }
  entrando_ = entrando_ && negado.pedido != protocolo::TipoMensaje::entrar_a_canal;
  if (negado.pedido == protocolo::TipoMensaje::mirar_canal) {
    texto_ = Chat{};
    marcar_leido(chat().canal);
    historial_despues_ = std::chrono::steady_clock::now() + kEsperaTrasRechazo;
  }
  if (negado.pedido == protocolo::TipoMensaje::pedir_historial) {
    for (Chat* chat : {&voz_, &texto_}) {
      if (chat->pidiendo) {
        (chat->pedido.sentido == Sentido::nuevos ? chat->hay_nuevos : chat->hay_mas) = true;
      }
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
  marcar_leido(chat().canal);
  return id == 0 ? std::vector<Orden>{} : std::vector<Orden>{pedir(voz_, 0, Sentido::viejos, kMensajesAlEntrar)};
}

Chat* Sala::chat_de(std::uint32_t id) {
  return id == 0 ? nullptr : id == texto_.canal ? &texto_ : id == voz_.canal ? &voz_ : nullptr;
}

void Sala::subir(std::uint32_t id, std::uint32_t contador) {
  if (auto* info = buscar(canales_, id)) {
    info->contador = std::max(info->contador, contador);
    if (id == chat().canal) {
      marcar_leido(chat().canal);
    }
  }
}

void Sala::marcar_leido(std::uint32_t id) {
  if (const auto* info = canal(id)) {
    leidos_[info->id] = info->contador;
  }
}

}
