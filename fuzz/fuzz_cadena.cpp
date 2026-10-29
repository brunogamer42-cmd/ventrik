#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/cadena.h"
#include "common/cifrado/clave.h"
#include "common/cifrado/huella.h"
#include "common/cifrado/inicio.h"

namespace {

using ventrik::cifrado::Cadena;
using ventrik::cifrado::Clave;
using ventrik::cifrado::Eslabon;
using ventrik::cifrado::Huella;

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

struct Genuinos {
  std::vector<Clave> claves;
  Cadena eslabones;
  std::array<std::vector<std::uint8_t>, 4> cadenas;
};

std::unique_ptr<ventrik::cifrado::Inicio> cifrado;
std::unique_ptr<Genuinos> genuinos;

bool es_genuino(const Eslabon& eslabon) {
  return std::any_of(genuinos->eslabones.begin(), genuinos->eslabones.end(), [&eslabon](const Eslabon& bueno) {
    return bueno.vieja == eslabon.vieja && bueno.nueva == eslabon.nueva && bueno.numero == eslabon.numero;
  });
}

bool contiene(gsl::span<const Huella> huellas, const Huella& huella) {
  return std::find(huellas.begin(), huellas.end(), huella) != huellas.end();
}

void seguir(const Cadena& cadena, const Huella& guardada, const Huella& conexion, gsl::span<const Huella> atras) {
  const auto seguida = ventrik::cifrado::seguir_cadena(cadena, guardada, conexion, atras);
  if (!seguida) {
    return;
  }
  const auto inicio = std::find_if(cadena.begin(), cadena.end(), [&guardada](const Eslabon& eslabon) {
    const auto vieja = ventrik::cifrado::huella_de_publica(eslabon.vieja);
    return vieja && *vieja == guardada;
  });
  exigir(seguida->nueva == conexion && !contiene(atras, conexion) && !seguida->atras.empty() &&
         seguida->atras.front() == guardada);
  exigir(static_cast<std::size_t>(cadena.end() - inicio) == seguida->atras.size() &&
         std::all_of(inicio, cadena.end(), es_genuino));
  exigir(std::none_of(seguida->atras.begin() + 1, seguida->atras.end(),
                      [&](const Huella& huella) { return huella == conexion || contiene(atras, huella); }));
}

void cadena_al_azar(gsl::span<const std::uint8_t> datos) {
  const auto cadena = ventrik::cifrado::leer_cadena(datos);
  if (!cadena) {
    return;
  }
  exigir(!cadena->empty() && cadena->size() <= ventrik::cifrado::kMaximoEslabones);
  const auto escrita = ventrik::cifrado::escribir_cadena(*cadena);
  exigir(std::equal(escrita.begin(), escrita.end(), datos.begin(), datos.end()));
  const auto guardada = ventrik::cifrado::huella_de_publica(cadena->front().vieja);
  const auto conexion = ventrik::cifrado::huella_de_publica(cadena->back().nueva);
  if (guardada && conexion) {
    seguir(*cadena, *guardada, *conexion, {});
  }
}

void cadena_genuina_cambiada(gsl::span<const std::uint8_t> datos) {
  if (datos.size() < 2) {
    return;
  }
  auto bytes = genuinos->cadenas[(datos[1] >> 4) % genuinos->cadenas.size()];
  for (std::size_t i = 2; i + 2 < datos.size(); i += 3) {
    auto& byte = bytes[static_cast<std::size_t>(datos[i] | (datos[i + 1] << 8)) % bytes.size()];
    byte = static_cast<std::uint8_t>(byte ^ datos[i + 2]);
  }
  const auto cadena = ventrik::cifrado::leer_cadena(bytes);
  if (!cadena) {
    return;
  }
  const auto& claves = genuinos->claves;
  std::vector<Huella> atras;
  for (std::size_t i = 0; i < claves.size(); ++i) {
    if ((datos[1] >> i) & 1) {
      atras.push_back(claves[i].huella());
    }
  }
  seguir(*cadena, claves[datos[0] & 3].huella(), claves[(datos[0] >> 2) & 3].huella(), atras);
}

}  // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  cifrado = std::make_unique<ventrik::cifrado::Inicio>();
  exigir(cifrado->ok());
  genuinos = std::make_unique<Genuinos>();
  for (int i = 0; i < 4; ++i) {
    auto clave = Clave::crear_nueva();
    exigir(clave.has_value());
    genuinos->claves.push_back(std::move(*clave));
  }
  constexpr std::array<std::array<std::uint32_t, 3>, 4> kPasos{{{0, 1, 1}, {1, 2, 2}, {2, 3, 3}, {3, 1, 4}}};
  for (const auto& [vieja, nueva, numero] : kPasos) {
    auto eslabon = ventrik::cifrado::firmar_eslabon(genuinos->claves[vieja], genuinos->claves[nueva].publica(), numero);
    exigir(eslabon.has_value());
    genuinos->eslabones.push_back(std::move(*eslabon));
  }
  using ventrik::cifrado::escribir_cadena;
  const auto& e = genuinos->eslabones;
  genuinos->cadenas = {escribir_cadena({e[0], e[1], e[2]}), escribir_cadena({e[0], e[1], e[2], e[3]}),
                       escribir_cadena({e[1], e[2]}), escribir_cadena({e[2], e[3]})};
  for (const auto& cadena : genuinos->cadenas) {
    exigir(!cadena.empty());
  }
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const gsl::span<const std::uint8_t> entrada(datos, largo);
  cadena_al_azar(entrada);
  cadena_genuina_cambiada(entrada);
  return 0;
}
