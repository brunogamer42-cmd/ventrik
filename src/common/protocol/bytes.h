#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <gsl/gsl>

namespace ventrik::protocolo {

// El error se mantiene hasta el final, así un mensaje roto nunca se usa a medias.
class Lector {
 public:
  explicit Lector(gsl::span<const std::uint8_t> datos) : datos_(datos) {}
  // Leería memoria ya liberada: los datos tienen que vivir más que el lector.
  explicit Lector(const std::vector<std::uint8_t>&&) = delete;

  std::uint8_t leer_u8();
  std::uint16_t leer_u16();
  std::uint32_t leer_u32();
  std::uint64_t leer_u64();
  bool leer_booleano();
  // Apunta a los datos de entrada: no vive más que ellos.
  gsl::span<const std::uint8_t> leer_bytes(std::size_t cantidad);
  std::string leer_texto(std::size_t maximo_bytes);

  bool ok() const { return ok_; }
  bool terminado() const { return ok_ && restante() == 0; }
  // Con error da 0, así un bucle que lee mientras quede algo siempre termina.
  std::size_t restante() const { return ok_ ? datos_.size() - posicion_ : 0; }

 private:
  std::uint64_t leer_entero(std::size_t bytes);

  gsl::span<const std::uint8_t> datos_;
  std::size_t posicion_ = 0;
  bool ok_ = true;
};

// Después de un error queda vacío, así nunca sale un mensaje a medias.
class Escritor {
 public:
  void escribir_u8(std::uint8_t valor);
  void escribir_u16(std::uint16_t valor);
  void escribir_u32(std::uint32_t valor);
  void escribir_u64(std::uint64_t valor);
  void escribir_booleano(bool valor);
  void escribir_bytes(gsl::span<const std::uint8_t> bytes);
  void escribir_texto(std::string_view texto, std::size_t maximo_bytes);

  bool ok() const { return ok_; }
  const std::vector<std::uint8_t>& datos() const { return datos_; }

 private:
  void escribir_entero(std::uint64_t valor, std::size_t bytes);
  void fallar();

  std::vector<std::uint8_t> datos_;
  bool ok_ = true;
};

bool es_texto_valido(std::string_view texto, std::size_t maximo_bytes);

}  // namespace ventrik::protocolo
