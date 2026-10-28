#include "common/cifrado/dpapi.h"

#include <algorithm>
#include <limits>

#include <windows.h>
#include <wincrypt.h>

namespace ventrik::cifrado {
namespace {

DATA_BLOB blob_de(gsl::span<const std::uint8_t> datos) {
  return {static_cast<DWORD>(datos.size()), const_cast<BYTE*>(datos.data())};
}

DATA_BLOB blob_de(std::string_view texto) {
  return blob_de(gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(texto.data()), texto.size()));
}

constexpr bool cabe(std::size_t largo) {
  return largo > 0 && largo <= std::numeric_limits<DWORD>::max() / 2;
}

}  // namespace

std::optional<std::vector<std::uint8_t>> proteger_con_windows(gsl::span<const std::uint8_t> datos,
                                                              std::string_view proposito) {
  if (!cabe(datos.size())) {
    return std::nullopt;
  }
  DATA_BLOB entrada = blob_de(datos);
  DATA_BLOB entropia = blob_de(proposito);
  DATA_BLOB salida = {};
  if (!CryptProtectData(&entrada, L"Ventrik", &entropia, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &salida)) {
    return std::nullopt;
  }
  const auto liberar = gsl::finally([&salida] { LocalFree(salida.pbData); });
  return std::vector<std::uint8_t>(salida.pbData, salida.pbData + salida.cbData);
}

std::optional<Secreto> abrir_con_windows(gsl::span<const std::uint8_t> protegido, std::string_view proposito) {
  if (!cabe(protegido.size())) {
    return std::nullopt;
  }
  DATA_BLOB entrada = blob_de(protegido);
  DATA_BLOB entropia = blob_de(proposito);
  DATA_BLOB salida = {};
  if (!CryptUnprotectData(&entrada, nullptr, &entropia, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &salida)) {
    return std::nullopt;
  }
  const auto liberar = gsl::finally([&salida] {
    SecureZeroMemory(salida.pbData, salida.cbData);
    LocalFree(salida.pbData);
  });
  Secreto abierto(salida.cbData);
  std::copy(salida.pbData, salida.pbData + salida.cbData, abierto.bytes().begin());
  return abierto;
}

}  // namespace ventrik::cifrado
