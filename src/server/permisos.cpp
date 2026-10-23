#include "server/permisos.h"

#include <algorithm>

#include "common/texto.h"
#include "server/base.h"

namespace ventrik::servidor {
namespace {
using protocolo::MotivoNegado;

bool nombre_ocupado(const std::vector<Canal>& canales, const std::string& nombre,
                    std::uint32_t salvo) {
  const std::wstring buscado = nombre_para_comparar(nombre);
  return buscado.empty() ||
         std::any_of(canales.begin(), canales.end(), [&](const Canal& canal) {
           return canal.id != salvo && canal.comparable == buscado;
         });
}

}

const Canal* buscar_canal(const std::vector<Canal>& canales, std::uint32_t id) {
  const auto encontrado = std::find_if(canales.begin(), canales.end(),
                                       [id](const Canal& canal) { return canal.id == id; });
  return encontrado == canales.end() ? nullptr : &*encontrado;
}

Permiso puede_entrar(const std::vector<Canal>& canales, std::uint32_t canal) {
  if (buscar_canal(canales, canal) == nullptr) {
    return MotivoNegado::no_existe;
  }
  return std::nullopt;
}

Permiso puede_leer_historial(const Persona& persona, std::uint32_t canal) {
  if (persona.canal != canal) {
    return MotivoNegado::sin_permiso;
  }
  return std::nullopt;
}

Permiso puede_crear(const Persona& persona, const std::vector<Canal>& canales,
                    const std::string& nombre) {
  if (!persona.dueno) {
    return MotivoNegado::sin_permiso;
  }
  if (!es_nombre_valido(nombre)) {
    return MotivoNegado::texto_invalido;
  }
  if (canales.size() >= kMaximoCanales) {
    return MotivoNegado::lleno;
  }
  if (nombre_ocupado(canales, nombre, 0)) {
    return MotivoNegado::nombre_en_uso;
  }
  return std::nullopt;
}

Permiso puede_renombrar(const Persona& persona, const std::vector<Canal>& canales,
                        std::uint32_t canal, const std::string& nombre) {
  if (!persona.dueno) {
    return MotivoNegado::sin_permiso;
  }
  if (buscar_canal(canales, canal) == nullptr) {
    return MotivoNegado::no_existe;
  }
  if (!es_nombre_valido(nombre)) {
    return MotivoNegado::texto_invalido;
  }
  if (nombre_ocupado(canales, nombre, canal)) {
    return MotivoNegado::nombre_en_uso;
  }
  return std::nullopt;
}

Permiso puede_borrar(const Persona& persona, const std::vector<Canal>& canales,
                     std::uint32_t canal) {
  if (!persona.dueno || canal == kCanalGeneral) {
    return MotivoNegado::sin_permiso;
  }
  if (buscar_canal(canales, canal) == nullptr) {
    return MotivoNegado::no_existe;
  }
  return std::nullopt;
}

}
