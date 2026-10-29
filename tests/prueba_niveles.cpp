#include <cstddef>
#include <cstdint>

#include <doctest/doctest.h>

#include "common/niveles.h"

using namespace ventrik;

TEST_CASE("cada nivel prende lo que dice la tabla del diseño y la captura del paso 4") {
  const Opciones basico = opciones_de(Nivel::basico);
  const Opciones reforzado = opciones_de(Nivel::reforzado);
  const Opciones maximo = opciones_de(Nivel::maximo);
  CHECK(basico == Opciones{});
  CHECK(opciones_de(Nivel::personalizado) == basico);
  for (const Opcion opcion : {Opcion::historial_cifrado, Opcion::avisos_de_admins, Opcion::aprobar_nuevos,
                              Opcion::antivirus, Opcion::reconocer_equipos, Opcion::programa_minimo}) {
    CHECK(basico[opcion] == 0);
    CHECK(reforzado[opcion] == 1);
    CHECK(maximo[opcion] == 1);
  }
  for (const Opcion opcion : {Opcion::sin_historial, Opcion::voz_fija, Opcion::invitaciones_de_un_uso,
                              Opcion::rechazar_comprimidos}) {
    CHECK(basico[opcion] == 0);
    CHECK(reforzado[opcion] == 0);
    CHECK(maximo[opcion] == 1);
  }
  CHECK(basico[Opcion::segundo_paso] == static_cast<std::uint8_t>(SegundoPaso::nadie));
  CHECK(reforzado[Opcion::segundo_paso] == static_cast<std::uint8_t>(SegundoPaso::admins));
  CHECK(maximo[Opcion::segundo_paso] == static_cast<std::uint8_t>(SegundoPaso::todos));
  for (const Opciones& opciones : {basico, reforzado, maximo}) {
    CHECK(opciones[Opcion::algoritmo] == static_cast<std::uint8_t>(Algoritmo::automatico));
    CHECK(segundos_de_renovacion(opciones) == 3600);
  }
  CHECK(version_minima(basico) == VersionPrograma{});
  CHECK(version_minima(reforzado) == kUltimaConArreglos);
  CHECK_FALSE(kVersionPrograma < kUltimaConArreglos);
}

TEST_CASE("las opciones de un nivel dicen ese nivel, y si no coinciden con ninguno, Personalizado") {
  for (const Nivel nivel : {Nivel::basico, Nivel::reforzado, Nivel::maximo}) {
    CHECK(nivel_de(opciones_de(nivel)) == nivel);
  }
  Opciones mezcla = opciones_de(Nivel::reforzado);
  mezcla[Opcion::voz_fija] = 1;
  CHECK(nivel_de(mezcla) == Nivel::personalizado);
}

TEST_CASE("cambiar una opción de otro nivel pasa solo a Personalizado, y volverla atrás vuelve al nivel") {
  Seguridad seguridad;
  elegir_nivel(seguridad, Nivel::reforzado);
  CHECK(seguridad.opciones == opciones_de(Nivel::reforzado));
  CHECK(cambiar_opcion(seguridad, Opcion::voz_fija, 1));
  CHECK(seguridad.nivel == Nivel::personalizado);
  CHECK(seguridad.opciones[Opcion::antivirus] == 1);
  CHECK(cambiar_opcion(seguridad, Opcion::voz_fija, 0));
  CHECK(seguridad.nivel == Nivel::reforzado);
  CHECK_FALSE(cambiar_opcion(seguridad, Opcion::voz_fija, 0));
  elegir_nivel(seguridad, Nivel::personalizado);
  CHECK(seguridad.nivel == Nivel::personalizado);
  CHECK(seguridad.opciones == opciones_de(Nivel::reforzado));
  elegir_nivel(seguridad, Nivel::basico);
  CHECK(seguridad.opciones == Opciones{});
}

TEST_CASE("las opciones de otras etapas no se cambian a mano ni aceptan valores de más") {
  Seguridad seguridad;
  for (std::size_t i = 0; i < kOpciones; ++i) {
    const auto opcion = static_cast<Opcion>(i);
    CAPTURE(i);
    CHECK(valores_de(opcion) >= 2);
    CHECK_FALSE(cambiar_opcion(seguridad, opcion, valores_de(opcion)));
    if (etapa_de(opcion) != 0) {
      CHECK_FALSE(cambiar_opcion(seguridad, opcion, 1));
    }
  }
  CHECK(seguridad.opciones == Opciones{});
  CHECK(etapa_de(Opcion::antivirus) == 5);
  CHECK(etapa_de(Opcion::rechazar_comprimidos) == 5);
  CHECK(etapa_de(Opcion::invitaciones_de_un_uso) == 6);
  CHECK(etapa_de(Opcion::aprobar_nuevos) == 9);
  CHECK(etapa_de(Opcion::reconocer_equipos) == 9);
  CHECK(etapa_de(Opcion::voz_fija) == 0);
}

TEST_CASE("una seguridad es válida con cada valor en su rango y un nivel fijo con sus opciones exactas") {
  Seguridad seguridad;
  CHECK(es_valida(seguridad));
  seguridad.opciones[Opcion::voz_fija] = 1;
  CHECK_FALSE(es_valida(seguridad));
  seguridad.nivel = Nivel::personalizado;
  CHECK(es_valida(seguridad));
  seguridad.opciones[Opcion::algoritmo] = 3;
  CHECK_FALSE(es_valida(seguridad));
  seguridad.opciones[Opcion::algoritmo] = 0;
  seguridad.nivel = static_cast<Nivel>(4);
  CHECK_FALSE(es_valida(seguridad));
}
