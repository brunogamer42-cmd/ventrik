#include "client/avisos.h"

#include <algorithm>
#include <utility>

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

void Avisos::fuera(bool si) {
  if (si == fuera_) {
    return;
  }
  fuera_ = si;
  if (!si) {
    pendientes_.clear();
    corte_.reset();
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
  Pendiente* pendiente =
      activo(voz ? opciones_.voz : opciones_.texto) && mensaje.autor_id != tu_id_ ? buscar_o_crear(mensaje.canal)
                                                                                : nullptr;
  if (pendiente == nullptr) {
    return;
  }
  pendiente->aviso.voz = voz;
  pendiente->canal = canal;
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
  Pendiente* pendiente = activo(opciones_.texto) ? buscar_o_crear(canal) : nullptr;
  if (pendiente == nullptr || pendiente->cantidad == cantidad) {
    return;
  }
  pendiente->canal = nombre;
  pendiente->cantidad = cantidad;
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
  return protocolo::PedirHistorial{en_vuelo_, kDesdeAviso, protocolo::Sentido::viejos,
                                   static_cast<std::uint16_t>(std::min<std::uint32_t>(elegido->cantidad,
                                                                                      kMensajesPorAviso))};
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
  std::vector<const protocolo::MensajeGuardado*> nuevos;
  std::size_t sin_leer = 0;
  for (const auto& mensaje : historial.mensajes) {
    if (mensaje.numero > leido) {
      ++sin_leer;
      if (mensaje.autor != tu_nombre_) {
        nuevos.push_back(&mensaje);
      }
    }
  }
  std::sort(nuevos.begin(), nuevos.end(), [](const auto* a, const auto* b) { return a->numero < b->numero; });
  if (sin_leer < pendiente.cantidad) {
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
    corte_ = Notificacion{0, false, servidor_, texto, ++version_};
  }
}

void Avisos::visto(std::uint32_t canal) {
  if (pendientes_.erase(canal) != 0) {
    ++version_;
  }
}

std::vector<Notificacion> Avisos::lista() const {
  std::vector<Notificacion> lista;
  for (const auto& par : pendientes_) {
    lista.push_back(par.second.aviso);
  }
  if (corte_) {
    lista.push_back(*corte_);
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
