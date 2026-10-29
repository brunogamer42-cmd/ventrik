#pragma once

#include <cstdint>
#include <iterator>
#include <string>

namespace ventrik {

struct VersionPrograma {
  std::uint16_t mayor = 0;
  std::uint16_t menor = 0;
  std::uint16_t parche = 0;
};

constexpr bool operator==(const VersionPrograma& a, const VersionPrograma& b) {
  return a.mayor == b.mayor && a.menor == b.menor && a.parche == b.parche;
}

constexpr bool operator!=(const VersionPrograma& a, const VersionPrograma& b) {
  return !(a == b);
}

constexpr bool operator<(const VersionPrograma& a, const VersionPrograma& b) {
  if (a.mayor != b.mayor) {
    return a.mayor < b.mayor;
  }
  if (a.menor != b.menor) {
    return a.menor < b.menor;
  }
  return a.parche < b.parche;
}

constexpr VersionPrograma kVersionPrograma{0, 5, 2};

inline std::string a_texto(const VersionPrograma& version) {
  return std::to_string(version.mayor) + "." + std::to_string(version.menor) + "." +
         std::to_string(version.parche);
}

constexpr std::uint16_t kProtocoloMinimo = 7;
constexpr std::uint16_t kProtocoloMaximo = 7;

constexpr VersionPrograma kPrimeraVersionDelProtocolo[] = {
    {0, 1, 0},
    {0, 2, 0},
    {0, 3, 0},
    {0, 4, 0},
    {0, 5, 0},
    {0, 5, 1},
    {0, 5, 2},
};
static_assert(std::size(kPrimeraVersionDelProtocolo) == kProtocoloMaximo,
              "Falta la primera version del protocolo nuevo");

}
