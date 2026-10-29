#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "client/conexion.h"
#include "common/cifrado/clave.h"
#include "common/cifrado/codigo_dueno.h"

namespace prueba {

inline std::shared_ptr<const ventrik::cifrado::Clave> identidad_nueva_de_prueba() {
  auto nueva = ventrik::cifrado::Clave::crear_nueva();
  if (!nueva) {
    std::abort();
  }
  return std::make_shared<const ventrik::cifrado::Clave>(std::move(*nueva));
}

inline std::shared_ptr<const ventrik::cifrado::Clave> identidad_de(const std::string& nombre) {
  static std::mutex candado;
  static std::map<std::string, std::shared_ptr<const ventrik::cifrado::Clave>> identidades;
  const std::lock_guard<std::mutex> cerrado(candado);
  auto& lugar = identidades[nombre];
  if (!lugar) {
    lugar = identidad_nueva_de_prueba();
  }
  return lugar;
}

inline std::shared_ptr<const ventrik::cifrado::Secreto> secreto_de(std::string_view texto) {
  auto secreto = std::make_shared<ventrik::cifrado::Secreto>(texto.size());
  std::transform(texto.begin(), texto.end(), secreto->bytes().begin(),
                 [](char letra) { return static_cast<std::uint8_t>(letra); });
  return secreto;
}

inline std::shared_ptr<const ventrik::cifrado::Secreto> equipo_de_prueba() { return secreto_de("equipo de prueba"); }

inline ventrik::cliente::Presentacion presentacion(std::string nombre,
                                                   std::optional<ventrik::cifrado::CodigoDueno> codigo = {}) {
  auto identidad = identidad_de(nombre);
  return {std::move(nombre), codigo, std::move(identidad), equipo_de_prueba()};
}

}  // namespace prueba
