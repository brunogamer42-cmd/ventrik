#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/inicio.h"
#include "common/cifrado/tls.h"
#include "server/voz.h"

using namespace std::chrono_literals;
using ventrik::red::Direccion;

namespace {
void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

std::unique_ptr<ventrik::cifrado::Inicio> cifrado;

bool derivar(gsl::span<const std::uint8_t> identidad, gsl::span<std::uint8_t> clave) {
  for (std::size_t i = 0; i < clave.size(); ++i) {
    clave[i] = identidad[i % identidad.size()];
  }
  return true;
}
using Datagramas = std::vector<std::pair<Direccion, std::vector<std::uint8_t>>>;
const Direccion kPrograma{0x0A000001, 5000};
}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  cifrado = std::make_unique<ventrik::cifrado::Inicio>();
  exigir(cifrado->ok());
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  auto entrada = gsl::make_span(datos, largo);
  if (entrada.empty()) {
    return 0;
  }
  auto ahora = ventrik::servidor::Instante{} + 1000s;
  Datagramas al_programa;
  Datagramas al_servidor;
  ventrik::servidor::Voz voz({}, [&](const Direccion& a, gsl::span<const std::uint8_t> d) {
    al_programa.emplace_back(a, std::vector<std::uint8_t>(d.begin(), d.end()));
  });
  exigir(voz.ok());
  const auto clave = voz.alta(1, kPrograma.ipv4, 1, ahora, derivar);
  exigir(clave && voz.alta(2, 0x0A000002, 1, ahora, derivar));
  std::array<std::uint8_t, 32> secreto{};
  derivar(clave->identidad, secreto);
  const bool cifrar = (entrada[0] & 1) != 0;
  entrada = entrada.subspan(1);

  auto config = ventrik::cifrado::ConfigTls::para_voz_programa(clave->identidad, secreto);
  exigir(config != nullptr);
  const ventrik::cifrado::EnviarDatagrama enviar = [&](const Direccion&,
                                                       gsl::span<const std::uint8_t> d) {
    al_servidor.emplace_back(kPrograma, std::vector<std::uint8_t>(d.begin(), d.end()));
  };
  ventrik::cifrado::SesionTls programa(*config, enviar, Direccion{});
  bool listo = false;
  for (int vuelta = 0; cifrar && !listo && vuelta < 10; ++vuelta) {
    listo = programa.apreton() == ventrik::cifrado::Paso::listo;
    for (auto& [origen, d] : std::exchange(al_servidor, {})) {
      voz.recibir(origen, d, ahora);
    }
    for (auto& [destino, d] : std::exchange(al_programa, {})) {
      programa.entregar(d);
      listo = listo || programa.apreton() == ventrik::cifrado::Paso::listo;
    }
  }
  exigir(!cifrar || (listo && voz.tiene_voz(1)));
  const auto inicio = ahora;

  while (entrada.size() >= 3) {
    const std::size_t cuanto = std::min<std::size_t>(entrada[2] * 8u, entrada.size() - 3);
    const auto trozo = entrada.subspan(3, cuanto);
    if (cifrar && (entrada[0] & 0x80) != 0) {
      voz.recibir(kPrograma, trozo, ahora);
    } else if (cifrar) {
      std::size_t escritos = 0;
      programa.escribir(trozo, escritos);
      for (auto& [origen, d] : std::exchange(al_servidor, {})) {
        voz.recibir(origen, d, ahora);
      }
    } else {
      voz.recibir(Direccion{0x0A000000u | (entrada[0] & 3u), entrada[1]}, trozo, ahora);
    }
    ahora += std::chrono::milliseconds(entrada[0]);
    voz.revisar(ahora);
    entrada = entrada.subspan(3 + cuanto);
    exigir(voz.pendientes() <= 64);
    exigir(!cifrar || ahora - inicio > 50s || voz.tiene_voz(1));
  }
  voz.revisar(ahora + 61s);
  exigir(!voz.tiene_voz(1) && !voz.tiene_voz(2));
  return 0;
}
