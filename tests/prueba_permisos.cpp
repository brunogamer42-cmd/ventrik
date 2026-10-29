#include <string>
#include <vector>

#include "common/texto.h"
#include "server/base.h"
#include "server/permisos.h"
#include <doctest/doctest.h>

using namespace ventrik::servidor;
using ventrik::protocolo::CambiarCanal;
using ventrik::protocolo::MotivoNegado;
using ventrik::protocolo::TipoCanal;

namespace {
Canal canal(std::uint32_t id, const std::string& nombre, TipoCanal tipo = TipoCanal::voz,
            bool solo_admins = false) {
  return Canal{id, nombre, ventrik::nombre_para_comparar(nombre), tipo, solo_admins};
}

const std::vector<Canal> kCanales = {canal(kCanalGeneral, "General"), canal(2, "Juegos"),
                                     canal(3, "Charla", TipoCanal::texto),
                                     canal(4, "Anuncios", TipoCanal::texto, true)};
const Persona kDueno{true, false, kCanalGeneral};
const Persona kInvitado{false, false, 2};
}

TEST_CASE("Solo se entra a canales de voz, y se lee el propio o cualquiera de texto") {
  CHECK_FALSE(puede_entrar(kCanales, 2));
  CHECK(puede_entrar(kCanales, 3) == MotivoNegado::tipo_de_canal);
  CHECK(puede_entrar(kCanales, 9) == MotivoNegado::no_existe);
  CHECK_FALSE(puede_leer(kInvitado, kCanales, 2));
  CHECK_FALSE(puede_leer(kInvitado, kCanales, 3));
  CHECK_FALSE(puede_leer(kInvitado, kCanales, 4));
  CHECK(puede_leer(kInvitado, kCanales, kCanalGeneral) == MotivoNegado::sin_permiso);
  CHECK(puede_leer(kInvitado, kCanales, 9) == MotivoNegado::no_existe);
}

TEST_CASE("Solo se miran canales de texto, y 0 es dejar de mirar") {
  CHECK_FALSE(puede_mirar(kCanales, 3));
  CHECK_FALSE(puede_mirar(kCanales, 0));
  CHECK(puede_mirar(kCanales, 2) == MotivoNegado::tipo_de_canal);
  CHECK(puede_mirar(kCanales, 9) == MotivoNegado::no_existe);
}

TEST_CASE("Se escribe en el canal de voz propio y en los de texto, salvo donde solo escriben los admins") {
  CHECK_FALSE(puede_escribir(kInvitado, kCanales, 2));
  CHECK_FALSE(puede_escribir(kInvitado, kCanales, 3));
  CHECK(puede_escribir(kInvitado, kCanales, kCanalGeneral) == MotivoNegado::sin_permiso);
  CHECK(puede_escribir(kInvitado, kCanales, 4) == MotivoNegado::sin_permiso);
  CHECK_FALSE(puede_escribir(kDueno, kCanales, 4));
  CHECK(puede_escribir(kDueno, kCanales, 9) == MotivoNegado::no_existe);
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

TEST_CASE("Solo el dueño cambia un canal, sin usar el nombre de otro y con la marca solo en los de texto") {
  CHECK_FALSE(puede_cambiar(kDueno, kCanales, CambiarCanal{2, "Música", false}));
  CHECK_FALSE(puede_cambiar(kDueno, kCanales, CambiarCanal{2, "JUEGOS", false}));
  CHECK_FALSE(puede_cambiar(kDueno, kCanales, CambiarCanal{kCanalGeneral, "Sala", false}));
  CHECK_FALSE(puede_cambiar(kDueno, kCanales, CambiarCanal{3, "Charla", true}));
  CHECK(puede_cambiar(kInvitado, kCanales, CambiarCanal{2, "Música", false}) == MotivoNegado::sin_permiso);
  CHECK(puede_cambiar(kDueno, kCanales, CambiarCanal{9, "Música", false}) == MotivoNegado::no_existe);
  CHECK(puede_cambiar(kDueno, kCanales, CambiarCanal{2, "general", false}) == MotivoNegado::nombre_en_uso);
  CHECK(puede_cambiar(kDueno, kCanales, CambiarCanal{2, "anuncios", false}) == MotivoNegado::nombre_en_uso);
  CHECK(puede_cambiar(kDueno, kCanales, CambiarCanal{2, "Juegos", true}) == MotivoNegado::tipo_de_canal);
  CHECK(puede_cambiar(kDueno, kCanales, CambiarCanal{2, "a\xE2\x80\xAE" "b", false}) ==
        MotivoNegado::texto_invalido);
}

TEST_CASE("Un admin crea, cambia y borra canales y escribe donde solo escriben los admins") {
  const Persona admin{false, true, 2};
  CHECK_FALSE(puede_crear(admin, kCanales, "Nuevo"));
  CHECK_FALSE(puede_cambiar(admin, kCanales, CambiarCanal{3, "Charla 2", true}));
  CHECK_FALSE(puede_borrar(admin, kCanales, 2));
  CHECK(puede_borrar(admin, kCanales, kCanalGeneral) == MotivoNegado::sin_permiso);
  CHECK_FALSE(puede_escribir(admin, kCanales, 4));
  CHECK(puede_escribir(kInvitado, kCanales, 4) == MotivoNegado::sin_permiso);
}

TEST_CASE("Solo el dueño borra, y General no se borra") {
  CHECK_FALSE(puede_borrar(kDueno, kCanales, 2));
  CHECK(puede_borrar(kInvitado, kCanales, 2) == MotivoNegado::sin_permiso);
  CHECK(puede_borrar(kDueno, kCanales, kCanalGeneral) == MotivoNegado::sin_permiso);
  CHECK_FALSE(puede_borrar(kDueno, kCanales, 3));
  CHECK(puede_borrar(kDueno, kCanales, 9) == MotivoNegado::no_existe);
}
