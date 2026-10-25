#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gsl/gsl>

namespace ventrik::protocolo {
class Lector {
 public:
  explicit Lector(gsl::span<const std::uint8_t> datos) : datos_(datos) {}
  explicit Lector(const std::vector<std::uint8_t>&&) = delete;

  std::uint8_t leer_u8();
  std::uint16_t leer_u16();
  std::uint32_t leer_u32();
  std::uint64_t leer_u64();
  bool leer_booleano();
  gsl::span<const std::uint8_t> leer_bytes(std::size_t cantidad);
  std::string leer_texto(std::size_t maximo_bytes);

  void marcar_error() { ok_ = false; }
  bool ok() const { return ok_; }
  bool terminado() const { return ok_ && restante() == 0; }
  std::size_t restante() const { return ok_ ? datos_.size() - posicion_ : 0; }

 private:
  std::uint64_t leer_entero(std::size_t bytes);

  gsl::span<const std::uint8_t> datos_;
  std::size_t posicion_ = 0;
  bool ok_ = true;
};

class Escritor {
 public:
  Escritor() = default;
  explicit Escritor(std::vector<std::uint8_t> reusar) : datos_(std::move(reusar)) { datos_.clear(); }

  void escribir_u8(std::uint8_t valor);
  void escribir_u16(std::uint16_t valor);
  void escribir_u32(std::uint32_t valor);
  void escribir_u64(std::uint64_t valor);
  void escribir_booleano(bool valor);
  void escribir_bytes(gsl::span<const std::uint8_t> bytes);
  void escribir_texto(std::string_view texto, std::size_t maximo_bytes);

  void marcar_error() { fallar(); }
  bool ok() const { return ok_; }
  const std::vector<std::uint8_t>& datos() const { return datos_; }
  std::vector<std::uint8_t> soltar() && { return std::move(datos_); }

 private:
  void escribir_entero(std::uint64_t valor, std::size_t bytes);
  void fallar();

  std::vector<std::uint8_t> datos_;
  bool ok_ = true;
};

bool es_texto_valido(std::string_view texto, std::size_t maximo_bytes);

}
