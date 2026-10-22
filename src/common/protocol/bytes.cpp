#include "common/protocol/bytes.h"

#include <limits>

#include "common/utf.h"

namespace ventrik::protocolo {

namespace {

constexpr std::size_t kMaximoTexto = std::numeric_limits<std::uint16_t>::max();

}  // namespace

bool es_texto_valido(std::string_view texto, std::size_t maximo_bytes) {
  // El byte 0 cortaría el texto al pasarlo a funciones de Windows.
  return texto.size() <= maximo_bytes && texto.size() <= kMaximoTexto &&
         texto.find('\0') == std::string_view::npos && es_utf8_valido(texto);
}

gsl::span<const std::uint8_t> Lector::leer_bytes(std::size_t cantidad) {
  if (!ok_ || cantidad > restante()) {
    ok_ = false;
    return {};
  }
  const auto pedazo = datos_.subspan(posicion_, cantidad);
  posicion_ += cantidad;
  return pedazo;
}

std::uint64_t Lector::leer_entero(std::size_t bytes) {
  const auto pedazo = leer_bytes(bytes);
  std::uint64_t valor = 0;
  for (std::size_t i = pedazo.size(); i > 0; --i) {
    valor = (valor << 8) | pedazo[i - 1];
  }
  return valor;
}

std::uint8_t Lector::leer_u8() {
  return static_cast<std::uint8_t>(leer_entero(1));
}

std::uint16_t Lector::leer_u16() {
  return static_cast<std::uint16_t>(leer_entero(2));
}

std::uint32_t Lector::leer_u32() {
  return static_cast<std::uint32_t>(leer_entero(4));
}

std::uint64_t Lector::leer_u64() {
  return leer_entero(8);
}

bool Lector::leer_booleano() {
  const std::uint8_t valor = leer_u8();
  if (valor > 1) {
    ok_ = false;
  }
  return ok_ && valor == 1;
}

std::string Lector::leer_texto(std::size_t maximo_bytes) {
  const std::size_t largo = leer_u16();
  if (largo > maximo_bytes) {
    ok_ = false;
  }
  const auto bytes = leer_bytes(largo);
  if (!ok_) {
    return {};
  }
  std::string texto(bytes.begin(), bytes.end());
  if (!es_texto_valido(texto, maximo_bytes)) {
    ok_ = false;
    return {};
  }
  return texto;
}

void Escritor::fallar() {
  ok_ = false;
  datos_.clear();
}

void Escritor::escribir_entero(std::uint64_t valor, std::size_t bytes) {
  if (!ok_) {
    return;
  }
  for (std::size_t i = 0; i < bytes; ++i) {
    datos_.push_back(static_cast<std::uint8_t>(valor >> (8 * i)));
  }
}

void Escritor::escribir_u8(std::uint8_t valor) {
  escribir_entero(valor, 1);
}

void Escritor::escribir_u16(std::uint16_t valor) {
  escribir_entero(valor, 2);
}

void Escritor::escribir_u32(std::uint32_t valor) {
  escribir_entero(valor, 4);
}

void Escritor::escribir_u64(std::uint64_t valor) {
  escribir_entero(valor, 8);
}

void Escritor::escribir_booleano(bool valor) {
  escribir_u8(valor ? 1 : 0);
}

void Escritor::escribir_bytes(gsl::span<const std::uint8_t> bytes) {
  if (ok_) {
    datos_.insert(datos_.end(), bytes.begin(), bytes.end());
  }
}

void Escritor::escribir_texto(std::string_view texto, std::size_t maximo_bytes) {
  if (!es_texto_valido(texto, maximo_bytes)) {
    fallar();
    return;
  }
  escribir_u16(static_cast<std::uint16_t>(texto.size()));
  if (ok_) {
    datos_.insert(datos_.end(), texto.begin(), texto.end());
  }
}

}  // namespace ventrik::protocolo
