#include "common/protocol/mensajes.h"

namespace ventrik::protocolo {

namespace {

void escribir_version(Escritor& escritor, const VersionPrograma& version) {
  escritor.escribir_u16(version.mayor);
  escritor.escribir_u16(version.menor);
  escritor.escribir_u16(version.parche);
}

VersionPrograma leer_version(Lector& lector) {
  VersionPrograma version;
  version.mayor = lector.leer_u16();
  version.menor = lector.leer_u16();
  version.parche = lector.leer_u16();
  return version;
}

}  // namespace

void escribir(Escritor& escritor, const Hola& hola) {
  escritor.escribir_u16(hola.protocolo_minimo);
  escritor.escribir_u16(hola.protocolo_maximo);
  escribir_version(escritor, hola.programa);
  escritor.escribir_u64(hola.funciones);
}

void escribir(Escritor& escritor, const Bienvenida& bienvenida) {
  escritor.escribir_u16(bienvenida.protocolo);
  escritor.escribir_u64(bienvenida.funciones);
}

void escribir(Escritor& escritor, const Rechazo& rechazo) {
  escritor.escribir_u8(static_cast<std::uint8_t>(rechazo.motivo));
  escribir_version(escritor, rechazo.version_minima);
}

std::optional<Hola> leer_hola(gsl::span<const std::uint8_t> cuerpo) {
  Lector lector(cuerpo);
  Hola hola;
  hola.protocolo_minimo = lector.leer_u16();
  hola.protocolo_maximo = lector.leer_u16();
  hola.programa = leer_version(lector);
  hola.funciones = lector.leer_u64();
  if (!lector.terminado()) {
    return std::nullopt;
  }
  return hola;
}

std::optional<Bienvenida> leer_bienvenida(gsl::span<const std::uint8_t> cuerpo) {
  Lector lector(cuerpo);
  Bienvenida bienvenida;
  bienvenida.protocolo = lector.leer_u16();
  bienvenida.funciones = lector.leer_u64();
  if (!lector.terminado()) {
    return std::nullopt;
  }
  return bienvenida;
}

std::optional<Rechazo> leer_rechazo(gsl::span<const std::uint8_t> cuerpo) {
  Lector lector(cuerpo);
  const std::uint8_t motivo = lector.leer_u8();
  Rechazo rechazo;
  rechazo.version_minima = leer_version(lector);
  if (!lector.terminado()) {
    return std::nullopt;
  }
  // Una sola forma válida por rechazo: la versión va solo cuando se pide actualizar.
  const bool hay_version = rechazo.version_minima != VersionPrograma{};
  switch (static_cast<MotivoRechazo>(motivo)) {
    case MotivoRechazo::programa_viejo:
      if (!hay_version) {
        return std::nullopt;
      }
      break;
    case MotivoRechazo::servidor_viejo:
    case MotivoRechazo::saludo_invalido:
      if (hay_version) {
        return std::nullopt;
      }
      break;
    default:
      return std::nullopt;
  }
  rechazo.motivo = static_cast<MotivoRechazo>(motivo);
  return rechazo;
}

}  // namespace ventrik::protocolo
