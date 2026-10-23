#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/protocol/pedidos.h"

namespace ventrik::servidor {
constexpr std::size_t kMaximoCanales = 60;

struct Canal {
  std::uint32_t id = 0;
  std::string nombre;
  std::wstring comparable;
};

struct Persona {
  bool dueno = false;
  std::uint32_t canal = 0;
};

using Permiso = std::optional<protocolo::MotivoNegado>;

const Canal* buscar_canal(const std::vector<Canal>& canales, std::uint32_t id);

Permiso puede_entrar(const std::vector<Canal>& canales, std::uint32_t canal);
Permiso puede_leer_historial(const Persona& persona, std::uint32_t canal);
Permiso puede_crear(const Persona& persona, const std::vector<Canal>& canales,
                    const std::string& nombre);
Permiso puede_renombrar(const Persona& persona, const std::vector<Canal>& canales,
                        std::uint32_t canal, const std::string& nombre);
Permiso puede_borrar(const Persona& persona, const std::vector<Canal>& canales,
                     std::uint32_t canal);

}
