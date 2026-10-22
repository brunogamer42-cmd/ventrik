#include "common/protocol/marcos.h"

#include <algorithm>
#include <utility>

#include "common/protocol/bytes.h"

namespace ventrik::protocolo {

bool armar_marco(std::uint16_t tipo, gsl::span<const std::uint8_t> cuerpo,
                 std::vector<std::uint8_t>& salida) {
  if (cuerpo.size() > kMaximoCuerpo) {
    return false;
  }
  Escritor cabecera;
  cabecera.escribir_u32(static_cast<std::uint32_t>(cuerpo.size()));
  cabecera.escribir_u16(tipo);
  salida.insert(salida.end(), cabecera.datos().begin(), cabecera.datos().end());
  salida.insert(salida.end(), cuerpo.begin(), cuerpo.end());
  return true;
}

Ensamblador::Ensamblador(std::size_t maximo_cuerpo) : maximo_cuerpo_(maximo_cuerpo) {
  Expects(maximo_cuerpo <= kMaximoCuerpo);
}

void Ensamblador::cambiar_maximo(std::size_t maximo_cuerpo) {
  Expects(maximo_cuerpo <= kMaximoCuerpo);
  maximo_cuerpo_ = maximo_cuerpo;
}

std::optional<Marco> Ensamblador::siguiente(gsl::span<const std::uint8_t>& datos) {
  while (!error_ && !datos.empty()) {
    if (!en_cuerpo_) {
      const auto pedazo = datos.first(std::min(kCabeceraMarco - cabecera_.size(), datos.size()));
      cabecera_.insert(cabecera_.end(), pedazo.begin(), pedazo.end());
      datos = datos.subspan(pedazo.size());
      if (cabecera_.size() < kCabeceraMarco) {
        continue;
      }
      Lector lector(cabecera_);
      const std::uint32_t largo = lector.leer_u32();
      actual_.tipo = lector.leer_u16();
      if (!lector.terminado() || largo > maximo_cuerpo_) {
        error_ = true;
        break;
      }
      falta_ = largo;
      en_cuerpo_ = true;
    } else {
      const auto pedazo = datos.first(std::min(falta_, datos.size()));
      actual_.cuerpo.insert(actual_.cuerpo.end(), pedazo.begin(), pedazo.end());
      datos = datos.subspan(pedazo.size());
      falta_ -= pedazo.size();
    }
    if (en_cuerpo_ && falta_ == 0) {
      Marco listo = std::move(actual_);
      actual_ = Marco{};
      cabecera_.clear();
      en_cuerpo_ = false;
      return listo;
    }
  }
  if (error_) {
    datos = datos.subspan(datos.size());
  }
  return std::nullopt;
}

}  // namespace ventrik::protocolo
