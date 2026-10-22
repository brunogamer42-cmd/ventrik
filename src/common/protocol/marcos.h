#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <gsl/gsl>

namespace ventrik::protocolo {

constexpr std::size_t kCabeceraMarco = 6;
// Con tope, la memoria por conexión también lo tiene: lo grande viaja en pedazos.
constexpr std::size_t kMaximoCuerpo = 64 * 1024;

struct Marco {
  std::uint16_t tipo = 0;
  std::vector<std::uint8_t> cuerpo;
};

[[nodiscard]] bool armar_marco(std::uint16_t tipo, gsl::span<const std::uint8_t> cuerpo,
                               std::vector<std::uint8_t>& salida);

class Ensamblador {
 public:
  explicit Ensamblador(std::size_t maximo_cuerpo = kMaximoCuerpo);

  // Se aplica desde el próximo marco; durante el saludo conviene un tope chico.
  void cambiar_maximo(std::size_t maximo_cuerpo);

  // Con error consume todo lo que queda, para que ningún bucle sobre datos quede trabado.
  [[nodiscard]] std::optional<Marco> siguiente(gsl::span<const std::uint8_t>& datos);

  bool error() const { return error_; }

 private:
  std::size_t maximo_cuerpo_;
  std::vector<std::uint8_t> cabecera_;
  Marco actual_;
  std::size_t falta_ = 0;
  bool en_cuerpo_ = false;
  bool error_ = false;
};

}  // namespace ventrik::protocolo
