#include "server/permisos.h"

#include <algorithm>

#include "common/texto.h"
#include "server/base.h"

namespace ventrik::servidor {
namespace {
using protocolo::MotivoNegado;
using protocolo::TipoCanal;

bool nombre_ocupado(const std::vector<Canal>& canales, const std::string& nombre,
                    std::uint32_t salvo) {
  const std::wstring buscado = nombre_para_comparar(nombre);
  return buscado.empty() ||
         std::any_of(canales.begin(), canales.end(), [&](const Canal& canal) {
           return canal.id != salvo && canal.comparable == buscado;
         });
}

}

Permiso puede_entrar(const std::vector<Canal>& canales, std::uint32_t canal) {
  const Canal* buscado = buscar_canal(canales, canal);
  if (buscado == nullptr) {
    return MotivoNegado::no_existe;
  }
  if (buscado->tipo != TipoCanal::voz) {
    return MotivoNegado::tipo_de_canal;
  }
  return std::nullopt;
}

Permiso puede_mirar(const std::vector<Canal>& canales, std::uint32_t canal) {
  const Canal* buscado = buscar_canal(canales, canal);
  if (canal != 0 && buscado == nullptr) {
    return MotivoNegado::no_existe;
  }
  if (buscado != nullptr && buscado->tipo != TipoCanal::texto) {
    return MotivoNegado::tipo_de_canal;
  }
  return std::nullopt;
}

Permiso puede_leer(const Persona& persona, const std::vector<Canal>& canales, std::uint32_t canal) {
  const Canal* buscado = buscar_canal(canales, canal);
  if (buscado == nullptr) {
    return MotivoNegado::no_existe;
  }
  if (buscado->tipo == TipoCanal::voz && persona.canal != canal) {
    return MotivoNegado::sin_permiso;
  }
  return std::nullopt;
}

Permiso puede_escribir(const Persona& persona, const std::vector<Canal>& canales, std::uint32_t canal) {
  const Permiso leer = puede_leer(persona, canales, canal);
  if (leer) {
    return leer;
  }
  if (buscar_canal(canales, canal)->solo_admins && !persona.dueno) {
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

Permiso puede_cambiar(const Persona& persona, const std::vector<Canal>& canales,
                      const protocolo::CambiarCanal& cambio) {
  if (!persona.dueno) {
    return MotivoNegado::sin_permiso;
  }
  const Canal* buscado = buscar_canal(canales, cambio.canal);
  if (buscado == nullptr) {
    return MotivoNegado::no_existe;
  }
  if (cambio.solo_admins && buscado->tipo != TipoCanal::texto) {
    return MotivoNegado::tipo_de_canal;
  }
  if (!es_nombre_valido(cambio.nombre)) {
    return MotivoNegado::texto_invalido;
  }
  if (nombre_ocupado(canales, cambio.nombre, cambio.canal)) {
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
