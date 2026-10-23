#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ventrik::cifrado {
class CodigoDueno {
 public:
  static constexpr std::size_t kBytes = 16;
  using Bytes = std::array<std::uint8_t, kBytes>;

  static std::optional<CodigoDueno> generar();
  explicit CodigoDueno(const Bytes& bytes) : bytes_(bytes) {}

  const Bytes& bytes() const { return bytes_; }
  std::string a_texto() const;

 private:
  Bytes bytes_;
};

bool operator==(const CodigoDueno& a, const CodigoDueno& b);

std::optional<CodigoDueno> leer_codigo_dueno(std::string_view texto);

}
