#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <windows.h>

#include <gsl/gsl>

#include "common/cifrado/llave.h"

namespace ventrik::cliente {

enum class Autenticadores { ninguno, llaves, llaves_y_windows_hello };

constexpr std::uint32_t kPrimerWindowsConEcc = 22621;

std::vector<std::int32_t> algoritmos_de_llave(std::optional<std::uint32_t> build_de_windows);

Autenticadores autenticadores_disponibles();

using IdDeLlave = std::vector<std::uint8_t>;

template <typename Valor>
struct RespuestaDeWebAuthn {
  std::optional<Valor> valor;
  HRESULT error = E_NOTIMPL;
};

RespuestaDeWebAuthn<cifrado::LlaveCreada> crear_llave(HWND ventana, const std::wstring& nombre,
                                                      const std::vector<IdDeLlave>& excluir);

struct FirmaDeLlave {
  IdDeLlave id;
  std::vector<std::uint8_t> datos;
  std::string texto_de_cliente;
  std::vector<std::uint8_t> firma;
};

RespuestaDeWebAuthn<FirmaDeLlave> firmar_con_llave(HWND ventana, const std::vector<IdDeLlave>& llaves,
                                                   gsl::span<const std::uint8_t> desafio);

}
