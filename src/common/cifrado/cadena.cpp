#include "common/cifrado/cadena.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace ventrik::cifrado {

namespace {

std::vector<std::uint8_t> lo_que_se_firma(const Eslabon& eslabon) {
  constexpr char kEtiqueta[] = "ventrik cambio de clave v1";
  protocolo::Escritor escritor;
  escritor.escribir_bytes(gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(kEtiqueta),
                                                        sizeof(kEtiqueta) - 1));
  escritor.escribir_bytes(eslabon.vieja);
  escritor.escribir_bytes(eslabon.nueva);
  escritor.escribir_u32(eslabon.numero);
  return std::move(escritor).soltar();
}

bool firma_valida(const Eslabon& eslabon) {
  return verificar_firma(eslabon.vieja, lo_que_se_firma(eslabon), eslabon.firma);
}

}

std::optional<Eslabon> firmar_eslabon(const Clave& vieja, const ClavePublica& nueva, std::uint32_t numero) {
  Eslabon eslabon{vieja.publica(), nueva, numero, {}};
  auto firma = vieja.firmar(lo_que_se_firma(eslabon));
  if (!firma) {
    return std::nullopt;
  }
  eslabon.firma = std::move(*firma);
  return eslabon;
}

bool agregar_eslabon(Cadena& cadena, Eslabon eslabon) {
  if (!firma_valida(eslabon) ||
      (!cadena.empty() && (eslabon.vieja != cadena.back().nueva || eslabon.numero <= cadena.back().numero))) {
    return false;
  }
  if (cadena.size() >= kMaximoEslabones) {
    cadena.erase(cadena.begin(), cadena.end() - static_cast<std::ptrdiff_t>(kMaximoEslabones - 1));
  }
  cadena.push_back(std::move(eslabon));
  return true;
}

void escribir_cadena(protocolo::Escritor& escritor, const Cadena& cadena, bool vacia_vale) {
  if ((cadena.empty() && !vacia_vale) || cadena.size() > kMaximoEslabones) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_u8(static_cast<std::uint8_t>(cadena.size()));
  for (const Eslabon& eslabon : cadena) {
    if (eslabon.firma.empty() || eslabon.firma.size() > kMaximoFirma) {
      escritor.marcar_error();
      return;
    }
    escritor.escribir_bytes(eslabon.vieja);
    escritor.escribir_bytes(eslabon.nueva);
    escritor.escribir_u32(eslabon.numero);
    escritor.escribir_u8(static_cast<std::uint8_t>(eslabon.firma.size()));
    escritor.escribir_bytes(eslabon.firma);
  }
}

void leer_cadena(protocolo::Lector& lector, Cadena& cadena, bool vacia_vale) {
  cadena.clear();
  const std::size_t cantidad = lector.leer_u8();
  if ((cantidad == 0 && !vacia_vale) || cantidad > kMaximoEslabones) {
    lector.marcar_error();
    return;
  }
  for (std::size_t i = 0; i < cantidad && lector.ok(); ++i) {
    Eslabon eslabon;
    const auto vieja = lector.leer_bytes(eslabon.vieja.size());
    const auto nueva = lector.leer_bytes(eslabon.nueva.size());
    eslabon.numero = lector.leer_u32();
    const std::size_t largo_firma = lector.leer_u8();
    const auto firma = lector.leer_bytes(largo_firma);
    if (!lector.ok() || largo_firma == 0 || largo_firma > kMaximoFirma) {
      lector.marcar_error();
      return;
    }
    std::copy(vieja.begin(), vieja.end(), eslabon.vieja.begin());
    std::copy(nueva.begin(), nueva.end(), eslabon.nueva.begin());
    eslabon.firma.assign(firma.begin(), firma.end());
    cadena.push_back(std::move(eslabon));
  }
}

std::vector<std::uint8_t> escribir_cadena(const Cadena& cadena) {
  protocolo::Escritor escritor;
  escribir_cadena(escritor, cadena);
  return std::move(escritor).soltar();
}

std::optional<Cadena> leer_cadena(gsl::span<const std::uint8_t> datos) {
  protocolo::Lector lector(datos);
  Cadena cadena;
  leer_cadena(lector, cadena);
  if (!lector.terminado()) {
    return std::nullopt;
  }
  return cadena;
}

std::optional<CadenaSeguida> seguir_cadena(const Cadena& cadena, const Huella& guardada, const Huella& de_la_conexion,
                                           gsl::span<const Huella> atras) {
  const auto inicio = std::find_if(cadena.begin(), cadena.end(), [&guardada](const Eslabon& eslabon) {
    const auto vieja = huella_de_publica(eslabon.vieja);
    return vieja && *vieja == guardada;
  });
  if (cadena.size() > kMaximoEslabones || inicio == cadena.end()) {
    return std::nullopt;
  }
  std::vector<Huella> vistas(atras.begin(), atras.end());
  Huella actual = guardada;
  for (auto eslabon = inicio; eslabon != cadena.end(); ++eslabon) {
    vistas.push_back(actual);
    const auto nueva = huella_de_publica(eslabon->nueva);
    if (!nueva || std::find(vistas.begin(), vistas.end(), *nueva) != vistas.end() ||
        (eslabon != inicio &&
         (eslabon->vieja != std::prev(eslabon)->nueva || eslabon->numero <= std::prev(eslabon)->numero)) ||
        !firma_valida(*eslabon)) {
      return std::nullopt;
    }
    actual = *nueva;
  }
  if (actual != de_la_conexion) {
    return std::nullopt;
  }
  return CadenaSeguida{actual, std::vector<Huella>(vistas.begin() + static_cast<std::ptrdiff_t>(atras.size()),
                                                   vistas.end())};
}

}
