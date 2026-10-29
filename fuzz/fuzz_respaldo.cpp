#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <windows.h>

#include <gsl/gsl>

#include "common/archivos.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/sobre.h"
#include "common/cifrado/cadena.h"
#include "server/base.h"
#include "server/mudanza.h"
#include "server/respaldo.h"

using namespace ventrik::servidor;
using ventrik::cifrado::AperturaSobre;

namespace {

constexpr std::string_view kContrasena = "contraseña de prueba";
constexpr ventrik::cifrado::Argon2 kRapido{ventrik::cifrado::kMinimoMemoriaArgon2, 1};
constexpr std::string_view kCabeceraSqlite("SQLite format 3\0", 16);

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

std::unique_ptr<ventrik::cifrado::Inicio> cifrado;
std::optional<ventrik::cifrado::LlaveDeSobre> llave;

bool adentro(gsl::span<const std::uint8_t> parte, gsl::span<const std::uint8_t> todo) {
  return !parte.empty() && parte.data() >= todo.data() && parte.data() + parte.size() <= todo.data() + todo.size();
}

bool iguales(const PartesRespaldo& una, const PartesRespaldo& otra) {
  return std::equal(una.clave.begin(), una.clave.end(), otra.clave.begin(), otra.clave.end()) &&
         std::equal(una.boveda.begin(), una.boveda.end(), otra.boveda.begin(), otra.boveda.end()) &&
         std::equal(una.base.begin(), una.base.end(), otra.base.begin(), otra.base.end()) &&
         std::equal(una.cadena.begin(), una.cadena.end(), otra.cadena.begin(), otra.cadena.end());
}

void contenido(gsl::span<const std::uint8_t> entrada) {
  const auto partes = leer_contenido(entrada);
  if (!partes) {
    return;
  }
  exigir(1 + 3 * 5 + partes->clave.size() + partes->boveda.size() + partes->base.size() +
             (partes->cadena.empty() ? 0 : 5 + partes->cadena.size()) ==
         entrada.size());
  exigir(partes->clave.size() <= kMaximoClaveRespaldo && partes->boveda.size() <= kMaximoBovedaRespaldo &&
         partes->base.size() <= kMaximoBaseRespaldo && partes->cadena.size() <= kMaximoCadenaRespaldo);
  exigir(adentro(partes->clave, entrada) && adentro(partes->boveda, entrada) && adentro(partes->base, entrada));
  exigir(partes->cadena.empty() || (adentro(partes->cadena, entrada) && ventrik::cifrado::leer_cadena(partes->cadena)));
  const auto sobre = ventrik::cifrado::cerrar_con_llave(*llave, entrada);
  exigir(sobre.has_value());
  const auto abierto = abrir_respaldo(*sobre, kContrasena);
  exigir(abierto.resultado == AperturaSobre::abierto && abierto.contenido && iguales(abierto.partes, *partes));
}

void respaldo(gsl::span<const std::uint8_t> entrada) {
  const auto cabecera = ventrik::cifrado::leer_cabecera_sobre(kMarcaRespaldo, entrada);
  if (cabecera && (cabecera->argon2.memoria_kib != kRapido.memoria_kib || cabecera->argon2.vueltas != kRapido.vueltas)) {
    return;
  }
  const auto abierto = abrir_respaldo(entrada, kContrasena);
  exigir((abierto.resultado == AperturaSobre::abierto) == abierto.contenido.has_value());
  exigir(abierto.contenido || (abierto.partes.clave.empty() && abierto.partes.base.empty()));
  if (abierto.contenido) {
    const auto partes = leer_contenido(abierto.contenido->bytes());
    exigir(partes && iguales(*partes, abierto.partes) && adentro(abierto.partes.base, abierto.contenido->bytes()));
  }
}

std::wstring archivo_de_prueba() {
  wchar_t carpeta[MAX_PATH] = {};
  exigir(GetTempPathW(MAX_PATH, carpeta) > 0);
  return std::wstring(carpeta) + L"ventrik_fuzz_respaldo_" + std::to_wstring(GetCurrentProcessId()) + L".db";
}

void base_importada(gsl::span<const std::uint8_t> entrada) {
  if (entrada.size() < kCabeceraSqlite.size() ||
      !std::equal(kCabeceraSqlite.begin(), kCabeceraSqlite.end(), entrada.begin(),
                  [](char letra, std::uint8_t byte) { return static_cast<std::uint8_t>(letra) == byte; })) {
    return;
  }
  static const std::wstring archivo = archivo_de_prueba();
  ventrik::borrar_temporales_de(archivo);
  borrar_base(archivo);
  if (ventrik::escribir_archivo_completo(archivo, entrada) &&
      revisar_base_importada(archivo, "General", ventrik::cifrado::Secreto(ventrik::cifrado::kBytesClaveSimetrica)) ==
          Mudanza::hecha) {
    exigir(revisar_base(archivo) == RevisionBase::buena && Base::abrir(archivo, "General") != nullptr);
  }
}

}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  cifrado = std::make_unique<ventrik::cifrado::Inicio>();
  exigir(cifrado->ok());
  llave = ventrik::cifrado::llave_nueva(kMarcaRespaldo, kContrasena, kRapido);
  exigir(llave.has_value());
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const gsl::span<const std::uint8_t> entrada(datos, largo);
  contenido(entrada);
  respaldo(entrada);
  base_importada(entrada);
  return 0;
}
