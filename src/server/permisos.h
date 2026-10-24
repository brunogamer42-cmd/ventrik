#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/protocol/pedidos.h"

namespace ventrik::servidor {
using protocolo::kMaximoCanales;

struct Canal {
  std::uint32_t id = 0;
  std::string nombre;
  std::wstring comparable;
  protocolo::TipoCanal tipo = protocolo::TipoCanal::voz;
  bool solo_admins = false;
  std::uint32_t contador = 0;
};

struct Persona {
  bool dueno = false;
  std::uint32_t canal = 0;
};

using Permiso = std::optional<protocolo::MotivoNegado>;

template <typename Canales>
auto buscar_canal(Canales& canales, std::uint32_t id) -> decltype(canales.data()) {
  const auto encontrado = std::find_if(canales.begin(), canales.end(),
                                       [id](const Canal& canal) { return canal.id == id; });
  return encontrado == canales.end() ? nullptr : &*encontrado;
}

Permiso puede_entrar(const std::vector<Canal>& canales, std::uint32_t canal);
Permiso puede_mirar(const std::vector<Canal>& canales, std::uint32_t canal);
Permiso puede_leer(const Persona& persona, const std::vector<Canal>& canales, std::uint32_t canal);
Permiso puede_escribir(const Persona& persona, const std::vector<Canal>& canales, std::uint32_t canal);
Permiso puede_crear(const Persona& persona, const std::vector<Canal>& canales,
                    const std::string& nombre);
Permiso puede_cambiar(const Persona& persona, const std::vector<Canal>& canales,
                      const protocolo::CambiarCanal& cambio);
Permiso puede_borrar(const Persona& persona, const std::vector<Canal>& canales,
                     std::uint32_t canal);

}
