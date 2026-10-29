#include "client/avisos.h"

#include <algorithm>
#include <utility>

#include "client/horas.h"
#include "common/red/red.h"

namespace ventrik::cliente {
namespace {

std::string recortado(std::string texto) {
  std::replace(texto.begin(), texto.end(), '\n', ' ');
  if (texto.size() <= kTopeTextoAviso) {
    return texto;
  }
  std::size_t corte = kTopeTextoAviso;
  while (corte > 0 && (static_cast<unsigned char>(texto[corte]) & 0xC0) == 0x80) {
    --corte;
  }
  texto.resize(corte);
  return texto + "\xE2\x80\xA6";
}

void sumar_autor(std::vector<std::string>& autores, const std::string& autor) {
  if (autores.size() < kAutoresPorAviso && std::find(autores.begin(), autores.end(), autor) == autores.end()) {
    autores.push_back(autor);
  }
}

std::optional<std::string_view> campo(std::string_view& texto, std::string_view clave, bool ultimo) {
  const std::size_t fin = texto.find(';');
  if (texto.substr(0, clave.size()) != clave || (fin == std::string_view::npos) != ultimo) {
    return std::nullopt;
  }
  const std::string_view valor = texto.substr(clave.size(), fin == std::string_view::npos ? fin : fin - clave.size());
  texto.remove_prefix(ultimo ? texto.size() : fin + 1);
  return valor;
}

}

std::string argumentos_de_aviso(const Notificacion& aviso) {
  return "servidor=" + aviso.servidor + ";canal=" + std::to_string(aviso.canal) + ";voz=" + (aviso.voz ? "1" : "0");
}

std::optional<Notificacion> leer_argumentos_de_aviso(std::string_view texto) {
  if (texto.size() > kTopeArgumentosDeAviso) {
    return std::nullopt;
  }
  const auto servidor = campo(texto, "servidor=", false);
  const auto canal = campo(texto, "canal=", false);
  const auto voz = campo(texto, "voz=", true);
  const auto direccion = servidor ? red::leer_direccion(*servidor) : std::nullopt;
  if (!direccion || !canal || canal->empty() || canal->size() > 10 || (canal->size() > 1 && canal->front() == '0') ||
      !std::all_of(canal->begin(), canal->end(), [](char c) { return c >= '0' && c <= '9'; }) || !voz ||
      (*voz != "0" && *voz != "1")) {
    return std::nullopt;
  }
  std::uint64_t numero = 0;
  for (const char cifra : *canal) {
    numero = numero * 10 + static_cast<std::uint64_t>(cifra - '0');
  }
  if (numero > std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }
  Notificacion aviso;
  aviso.servidor = red::a_texto(*direccion);
  aviso.canal = static_cast<std::uint32_t>(numero);
  aviso.voz = *voz == "1";
  return aviso;
}

std::string explicar(Idioma idioma, const protocolo::AvisoDeSeguridad& aviso) {
  const std::string cuando = fecha_y_hora_local(aviso.hora_ms);
  switch (aviso.tipo) {
    case protocolo::Seguridad::canal_borrado:
      return con_datos(texto(idioma, Clave::seguridad_aviso_canal_borrado), {aviso.quien, aviso.detalle, cuando});
    case protocolo::Seguridad::equipo_nuevo:
      return con_datos(texto(idioma, Clave::seguridad_aviso_equipo_nuevo), {cuando});
    case protocolo::Seguridad::nombre_liberado:
      return con_datos(texto(idioma, Clave::seguridad_aviso_nombre_liberado), {aviso.detalle});
    default:
      return con_datos(texto(idioma, Clave::seguridad_aviso_otro), {cuando});
  }
}

void Avisos::conectado(std::string servidor, std::uint32_t tu_id, std::string tu_nombre) {
  servidor_ = std::move(servidor);
  tu_id_ = tu_id;
  tu_nombre_ = std::move(tu_nombre);
  desconectado();
}

void Avisos::desconectado() {
  pendientes_.clear();
  en_vuelo_ = 0;
  ++version_;
}

void Avisos::opciones(const OpcionesAvisos& opciones) {
  const bool otro = opciones.privado != opciones_.privado;
  opciones_ = opciones;
  for (auto& par : pendientes_) {
    if (otro && par.second.mostrar) {
      actualizar(par.second);
    }
  }
  if (otro && seguridad_) {
    rehacer_seguridad();
  }
}

void Avisos::fuera(bool si) {
  if (si == fuera_) {
    return;
  }
  fuera_ = si;
  if (!si) {
    pendientes_.clear();
    corte_.reset();
    seguridad_.reset();
    ++version_;
  }
}

Avisos::Pendiente* Avisos::buscar_o_crear(std::uint32_t canal) {
  const auto encontrado = pendientes_.find(canal);
  if (encontrado != pendientes_.end()) {
    return &encontrado->second;
  }
  if (canal == 0 || pendientes_.size() >= protocolo::kMaximoCanales) {
    return nullptr;
  }
  Pendiente& nuevo = pendientes_[canal];
  nuevo.aviso.canal = canal;
  return &nuevo;
}

void Avisos::mensaje(const protocolo::MensajeChat& mensaje, const std::string& canal, bool voz) {
  Pendiente* pendiente = activo(voz ? opciones_.voz : opciones_.texto) && mensaje.autor_id != tu_id_ &&
                                 avisa(ajustes_, mensaje.canal, mencion_en(mensaje.texto, tu_nombre_), ahora_ms_)
                             ? buscar_o_crear(mensaje.canal)
                             : nullptr;
  if (pendiente == nullptr) {
    return;
  }
  pendiente->aviso.voz = voz;
  pendiente->canal = canal;
  pendiente->mostrar = true;
  ++pendiente->cantidad;
  sumar_autor(pendiente->autores, mensaje.autor);
  pendiente->autor = mensaje.autor;
  pendiente->texto = recortado(mensaje.texto);
  actualizar(*pendiente);
}

void Avisos::nuevos(std::uint32_t canal, const std::string& nombre, std::uint32_t cantidad) {
  if (cantidad == 0) {
    visto(canal);
    return;
  }
  Pendiente* pendiente = activo(opciones_.texto) && avisa(ajustes_, canal, Mencion::directa, ahora_ms_)
                             ? buscar_o_crear(canal)
                             : nullptr;
  if (pendiente == nullptr || pendiente->total == cantidad) {
    return;
  }
  pendiente->canal = nombre;
  pendiente->total = cantidad;
  if (!avisa(ajustes_, canal, Mencion::ninguna, ahora_ms_)) {
    pendiente->falta_texto = true;
    return;
  }
  pendiente->cantidad = cantidad;
  pendiente->mostrar = true;
  pendiente->falta_texto = !opciones_.privado;
  actualizar(*pendiente);
}

std::optional<Orden> Avisos::pedir(Reloj::time_point ahora, bool sala_esperando, bool juego) {
  if (en_vuelo_ != 0 && ahora - en_vuelo_desde_ >= kEsperaDePedidoDeAviso) {
    en_vuelo_ = 0;
  }
  if (en_vuelo_ != 0 || sala_esperando || juego || !fuera_ || ahora < proximo_pedido_) {
    return std::nullopt;
  }
  Pendiente* elegido = nullptr;
  for (auto& par : pendientes_) {
    if (par.second.falta_texto && (elegido == nullptr || par.second.pedido_en < elegido->pedido_en)) {
      elegido = &par.second;
    }
  }
  if (elegido == nullptr) {
    return std::nullopt;
  }
  elegido->falta_texto = false;
  elegido->pedido_en = ahora;
  en_vuelo_ = elegido->aviso.canal;
  en_vuelo_desde_ = ahora;
  proximo_pedido_ = ahora + kEntrePedidosDeAviso;
  const std::uint32_t tope =
      avisa(ajustes_, en_vuelo_, Mencion::ninguna, ahora_ms_) ? kMensajesPorAviso : kMensajesParaMenciones;
  return protocolo::PedirHistorial{en_vuelo_, kDesdeAviso, protocolo::Sentido::viejos,
                                   static_cast<std::uint16_t>(std::min(elegido->total, tope))};
}

void Avisos::historial(const protocolo::Historial& historial, std::uint32_t leido) {
  if (!espera(historial.canal) || historial.desde != kDesdeAviso || historial.sentido != protocolo::Sentido::viejos) {
    return;
  }
  en_vuelo_ = 0;
  const auto encontrado = pendientes_.find(historial.canal);
  if (encontrado == pendientes_.end()) {
    return;
  }
  Pendiente& pendiente = encontrado->second;
  const bool todo = avisa(ajustes_, historial.canal, Mencion::ninguna, ahora_ms_);
  const std::uint64_t desde = todo ? leido : std::max<std::uint64_t>(leido, pendiente.revisado);
  std::vector<const protocolo::MensajeGuardado*> nuevos;
  std::size_t sin_leer = 0;
  for (const auto& mensaje : historial.mensajes) {
    pendiente.revisado = std::max<std::uint64_t>(pendiente.revisado, mensaje.numero);
    if (mensaje.numero > desde) {
      ++sin_leer;
      if (mensaje.autor != tu_nombre_ &&
          (todo || avisa(ajustes_, historial.canal, mencion_en(mensaje.texto, tu_nombre_), ahora_ms_))) {
        nuevos.push_back(&mensaje);
      }
    }
  }
  std::sort(nuevos.begin(), nuevos.end(), [](const auto* a, const auto* b) { return a->numero < b->numero; });
  if (!todo) {
    if (nuevos.empty()) {
      return;
    }
    pendiente.cantidad += static_cast<std::uint32_t>(nuevos.size());
    pendiente.mostrar = true;
    for (const auto* mensaje : nuevos) {
      sumar_autor(pendiente.autores, mensaje->autor);
    }
    const bool uno = pendiente.cantidad == 1;
    pendiente.autor = uno ? nuevos.front()->autor : std::string();
    pendiente.texto = uno ? recortado(nuevos.front()->texto) : std::string();
    actualizar(pendiente);
    return;
  }
  if (sin_leer < pendiente.total) {
    nuevos.clear();
  }
  pendiente.autores.clear();
  for (const auto* mensaje : nuevos) {
    sumar_autor(pendiente.autores, mensaje->autor);
  }
  const bool uno = nuevos.size() == 1;
  pendiente.autor = uno ? nuevos.front()->autor : std::string();
  pendiente.texto = uno ? recortado(nuevos.front()->texto) : std::string();
  actualizar(pendiente);
}

void Avisos::negado() {
  if (const auto encontrado = pendientes_.find(en_vuelo_); encontrado != pendientes_.end()) {
    encontrado->second.falta_texto = true;
  }
  en_vuelo_ = 0;
}

void Avisos::corte(const std::string& texto) {
  if (activo(opciones_.conexion)) {
    corte_ = Notificacion{0, false, servidor_, texto, ++version_, servidor_};
  }
}

void Avisos::seguridad(const std::string& texto) {
  if (activo(true)) {
    texto_seguridad_ = texto;
    seguridad_ = Notificacion{0, false, {}, {}, 0, servidor_};
    rehacer_seguridad();
  }
}

void Avisos::rehacer_seguridad() {
  seguridad_->titulo = con_datos(texto(idioma_, Clave::aviso_titulo_seguridad), {servidor_});
  seguridad_->cuerpo = opciones_.privado ? std::string(texto(idioma_, Clave::aviso_seguridad_privado)) : texto_seguridad_;
  seguridad_->version = ++version_;
}

void Avisos::visto(std::uint32_t canal) {
  if (pendientes_.erase(canal) != 0) {
    ++version_;
  }
}

std::vector<Notificacion> Avisos::lista() const {
  std::vector<Notificacion> lista;
  for (const auto& par : pendientes_) {
    if (par.second.mostrar) {
      lista.push_back(par.second.aviso);
    }
  }
  for (const auto* aparte : {&corte_, &seguridad_}) {
    if (*aparte) {
      lista.push_back(**aparte);
    }
  }
  std::sort(lista.begin(), lista.end(),
            [](const Notificacion& a, const Notificacion& b) { return a.version < b.version; });
  return lista;
}

void Avisos::actualizar(Pendiente& pendiente) {
  const auto t = [this](Clave clave) { return texto(idioma_, clave); };
  const std::string cantidad = std::to_string(pendiente.cantidad);
  const auto& autores = pendiente.autores;
  Notificacion& aviso = pendiente.aviso;
  aviso.servidor = servidor_;
  aviso.titulo = con_datos(t(aviso.voz ? Clave::aviso_titulo_voz : Clave::aviso_titulo_texto),
                           {servidor_, pendiente.canal});
  if (!opciones_.privado && pendiente.cantidad == 1 && !pendiente.texto.empty()) {
    aviso.cuerpo = con_datos(t(Clave::aviso_mensaje), {pendiente.autor, pendiente.texto});
  } else if (opciones_.privado || autores.empty()) {
    aviso.cuerpo = pendiente.cantidad == 1 ? std::string(t(Clave::aviso_un_mensaje))
                                           : con_datos(t(Clave::aviso_mensajes), {cantidad});
  } else if (autores.size() == 1) {
    aviso.cuerpo = con_datos(t(Clave::aviso_mensajes_de), {cantidad, autores[0]});
  } else if (autores.size() == 2) {
    aviso.cuerpo = con_datos(t(Clave::aviso_mensajes_de_dos), {cantidad, autores[0], autores[1]});
  } else {
    aviso.cuerpo = con_datos(t(Clave::aviso_mensajes_de_varios),
                             {cantidad, autores[0], autores[1], std::to_string(autores.size() - 2)});
  }
  aviso.version = ++version_;
}

}
