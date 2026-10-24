#include "client/buffer_voz.h"

namespace ventrik::cliente {
namespace {
std::int16_t distancia(std::uint16_t desde, std::uint16_t hasta) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(hasta - desde));
}

}

void BufferVoz::vaciar() {
  for (Lugar& lugar : lugares_) {
    lugar.ocupado = false;
  }
  cantidad_ = 0;
  sincronizado_ = false;
  llenando_ = true;
}

void BufferVoz::reiniciar() {
  vaciar();
  hay_ultimo_ = false;
}

void BufferVoz::llegar(const PaqueteVoz& paquete) {
  constexpr auto kVentana = static_cast<std::int16_t>(kLugares);
  if (paquete.largo == 0 || paquete.largo > paquete.opus.size()) {
    return;
  }
  if (hay_ultimo_) {
    const std::int16_t desde_ultimo = distancia(ultimo_, paquete.numero);
    if (desde_ultimo <= 0 && desde_ultimo > -kVentana) {
      return;
    }
    if (desde_ultimo <= 0) {
      reiniciar();
    }
  }
  if (!sincronizado_) {
    sincronizado_ = true;
    siguiente_ = paquete.numero;
  }
  const std::int16_t adelanto = distancia(siguiente_, paquete.numero);
  if (adelanto < 0) {
    return;
  }
  if (adelanto >= kVentana) {
    vaciar();
    sincronizado_ = true;
    siguiente_ = paquete.numero;
  }
  Lugar& lugar = lugares_[paquete.numero % kLugares];
  if (lugar.ocupado && lugar.paquete.numero == paquete.numero) {
    return;
  }
  cantidad_ += lugar.ocupado ? 0 : 1;
  lugar.ocupado = true;
  lugar.paquete = paquete;
}

BufferVoz::Salida BufferVoz::sacar(PaqueteVoz& paquete) {
  if (!sincronizado_ || (llenando_ && cantidad_ < espera_)) {
    return Salida::nada;
  }
  llenando_ = false;
  if (cantidad_ == 0) {
    ++vaciados_;
    vaciar();
    return Salida::nada;
  }
  if (cantidad_ > espera_ + kMargen) {
    ++recortes_;
    for (std::size_t i = 0; i < kLugares && cantidad_ > espera_; ++i, ++siguiente_) {
      Lugar& viejo = lugares_[siguiente_ % kLugares];
      if (viejo.ocupado && viejo.paquete.numero == siguiente_) {
        viejo.ocupado = false;
        --cantidad_;
      }
    }
  }
  Lugar& lugar = lugares_[siguiente_ % kLugares];
  ultimo_ = siguiente_++;
  hay_ultimo_ = true;
  paquete.numero = ultimo_;
  if (!lugar.ocupado || lugar.paquete.numero != ultimo_) {
    ++faltas_;
    return Salida::perdido;
  }
  lugar.ocupado = false;
  --cantidad_;
  paquete = lugar.paquete;
  return Salida::paquete;
}

}
