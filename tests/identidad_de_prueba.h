#pragma once

#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
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

inline ventrik::cliente::Presentacion presentacion(std::string nombre,
                                                   std::optional<ventrik::cifrado::CodigoDueno> codigo = {}) {
  auto identidad = identidad_de(nombre);
  return {std::move(nombre), codigo, std::move(identidad), nullptr};
}

}  // namespace prueba
