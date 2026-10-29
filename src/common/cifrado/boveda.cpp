#include "common/cifrado/boveda.h"

#include <algorithm>
#include <utility>

#include <mbedtls/hkdf.h>
#include <mbedtls/md.h>

#include "common/cifrado/dpapi.h"
#include "common/cifrado/inicio.h"
#include "common/protocol/bytes.h"

namespace ventrik::cifrado {
namespace {

constexpr std::string_view kMarcaBoveda = "VKBV";
constexpr std::string_view kMarcaArranque = "VKBA";
constexpr std::string_view kPropositoBoveda = "ventrik boveda v1";
constexpr std::uint8_t kVersionBoveda = 1;
constexpr std::uint8_t kCapaWindows = 0;
constexpr std::uint8_t kCapaContrasena = 1;

gsl::span<const std::uint8_t> bytes_de(std::string_view texto) {
  return gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(texto.data()), texto.size());
}

AperturaBoveda apertura_de(AperturaSobre resultado) {
  switch (resultado) {
    case AperturaSobre::abierto: return AperturaBoveda::abierta;
    case AperturaSobre::contrasena: return AperturaBoveda::contrasena;
    case AperturaSobre::sin_memoria: return AperturaBoveda::sin_memoria;
    case AperturaSobre::danado: break;
  }
  return AperturaBoveda::danada;
}

}

std::optional<Boveda> Boveda::crear_nueva() {
  Secreto clave(kBytesBoveda);
  if (aleatorio(nullptr, clave.bytes().data(), clave.size()) != 0) {
    return std::nullopt;
  }
  return Boveda(std::move(clave));
}

std::optional<Secreto> Boveda::derivar(std::string_view etiqueta) const {
  Secreto derivada(kBytesClaveSimetrica);
  const auto info = bytes_de(etiqueta);
  if (clave_.size() != kBytesBoveda ||
      mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), nullptr, 0, clave_.bytes().data(), clave_.size(),
                   info.data(), info.size(), derivada.bytes().data(), derivada.size()) != 0) {
    return std::nullopt;
  }
  return derivada;
}

std::optional<std::vector<std::uint8_t>> Boveda::armar_archivo(std::optional<std::string_view> contrasena,
                                                               std::optional<Argon2> fijos) const {
  const auto sobre = contrasena ? cerrar_sobre(kMarcaArranque, clave_.bytes(), *contrasena, fijos) : std::nullopt;
  const auto protegido =
      contrasena && !sobre
          ? std::nullopt
          : proteger_con_windows(sobre ? gsl::span<const std::uint8_t>(*sobre) : clave_.bytes(), kPropositoBoveda);
  if (!protegido || clave_.size() != kBytesBoveda) {
    return std::nullopt;
  }
  protocolo::Escritor escritor;
  escritor.escribir_bytes(bytes_de(kMarcaBoveda));
  escritor.escribir_u8(kVersionBoveda);
  escritor.escribir_u8(contrasena ? kCapaContrasena : kCapaWindows);
  escritor.escribir_bytes(*protegido);
  return std::move(escritor).soltar();
}

BovedaAbierta abrir_boveda(gsl::span<const std::uint8_t> archivo, std::optional<std::string_view> contrasena) {
  protocolo::Lector lector(archivo);
  const auto marca = lector.leer_bytes(kMarcaBoveda.size());
  const std::uint8_t version = lector.leer_u8();
  const std::uint8_t capa = lector.leer_u8();
  const auto protegido = lector.leer_bytes(lector.restante());
  BovedaAbierta resultado;
  if (!lector.terminado() || !std::equal(marca.begin(), marca.end(), bytes_de(kMarcaBoveda).begin()) ||
      version != kVersionBoveda || capa > kCapaContrasena || protegido.empty()) {
    return resultado;
  }
  auto abierto = abrir_con_windows(protegido, kPropositoBoveda);
  if (!abierto) {
    resultado.resultado = AperturaBoveda::otro_windows;
    return resultado;
  }
  if (capa == kCapaWindows) {
    if (abierto->size() == kBytesBoveda) {
      resultado.resultado = AperturaBoveda::abierta;
      resultado.boveda.emplace(std::move(*abierto));
    }
    return resultado;
  }
  if (!leer_cabecera_sobre(kMarcaArranque, abierto->bytes())) {
    return resultado;
  }
  if (!contrasena) {
    resultado.resultado = AperturaBoveda::falta_contrasena;
    return resultado;
  }
  SobreAbierto sobre = abrir_sobre(kMarcaArranque, abierto->bytes(), *contrasena);
  resultado.resultado = apertura_de(sobre.resultado);
  if (sobre.contenido && sobre.contenido->size() == kBytesBoveda) {
    resultado.boveda.emplace(std::move(*sobre.contenido));
  } else if (sobre.contenido) {
    resultado.resultado = AperturaBoveda::danada;
  }
  return resultado;
}

}
