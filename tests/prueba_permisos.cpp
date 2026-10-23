#include <string>
#include <vector>

#include "common/texto.h"
#include "server/base.h"
#include "server/permisos.h"
#include <doctest/doctest.h>

using namespace ventrik::servidor;
using ventrik::protocolo::MotivoNegado;

namespace {
Canal canal(std::uint32_t id, const std::string& nombre) {
  return Canal{id, nombre, ventrik::nombre_para_comparar(nombre)};
}

const std::vector<Canal> kCanales = {canal(kCanalGeneral, "General"), canal(2, "Juegos")};
const Persona kDueno{true, kCanalGeneral};
const Persona kInvitado{false, 2};
}

TEST_CASE("Entrar y leer el historial") {
  CHECK_FALSE(puede_entrar(kCanales, 2));
  CHECK(puede_entrar(kCanales, 3) == MotivoNegado::no_existe);
  CHECK_FALSE(puede_leer_historial(kInvitado, 2));
  CHECK(puede_leer_historial(kInvitado, kCanalGeneral) == MotivoNegado::sin_permiso);
}

TEST_CASE("Solo el dueño crea canales, con nombre válido, libre y sin pasarse del tope") {
  CHECK_FALSE(puede_crear(kDueno, kCanales, "Música"));
  CHECK(puede_crear(kInvitado, kCanales, "Música") == MotivoNegado::sin_permiso);
  CHECK(puede_crear(kDueno, kCanales, " Música") == MotivoNegado::texto_invalido);
  CHECK(puede_crear(kDueno, kCanales, "juegos") == MotivoNegado::nombre_en_uso);
  std::vector<Canal> muchos;
  for (std::uint32_t i = 1; i <= kMaximoCanales; ++i) {
    muchos.push_back(canal(i, "Canal " + std::to_string(i)));
  }
  CHECK(puede_crear(kDueno, muchos, "Uno más") == MotivoNegado::lleno);
}

TEST_CASE("Solo el dueño renombra, y no a un nombre de otro canal") {
  CHECK_FALSE(puede_renombrar(kDueno, kCanales, 2, "Música"));
  CHECK_FALSE(puede_renombrar(kDueno, kCanales, 2, "JUEGOS"));
  CHECK_FALSE(puede_renombrar(kDueno, kCanales, kCanalGeneral, "Charla"));
  CHECK(puede_renombrar(kInvitado, kCanales, 2, "Música") == MotivoNegado::sin_permiso);
  CHECK(puede_renombrar(kDueno, kCanales, 3, "Música") == MotivoNegado::no_existe);
  CHECK(puede_renombrar(kDueno, kCanales, 2, "general") == MotivoNegado::nombre_en_uso);
  CHECK(puede_renombrar(kDueno, kCanales, 2, "a\xE2\x80\xAE" "b") == MotivoNegado::texto_invalido);
}

TEST_CASE("Solo el dueño borra, y General no se borra") {
  CHECK_FALSE(puede_borrar(kDueno, kCanales, 2));
  CHECK(puede_borrar(kInvitado, kCanales, 2) == MotivoNegado::sin_permiso);
  CHECK(puede_borrar(kDueno, kCanales, kCanalGeneral) == MotivoNegado::sin_permiso);
  CHECK(puede_borrar(kDueno, kCanales, 3) == MotivoNegado::no_existe);
}
