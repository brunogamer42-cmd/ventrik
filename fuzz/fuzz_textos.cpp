#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include "client/avisos.h"
#include "common/cifrado/codigo_dueno.h"
#include "common/cifrado/huella.h"
#include "common/red/red.h"
#include "common/texto.h"
#include "server/contrasena.h"

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const std::string_view texto(reinterpret_cast<const char*>(datos), largo);

  if (const auto huella = ventrik::cifrado::leer_huella(texto)) {
    const std::string escrita = huella->a_texto();
    exigir(escrita.size() == 64);
    const auto otra_vez = ventrik::cifrado::leer_huella(escrita);
    exigir(otra_vez && *otra_vez == *huella);
  }

  if (const auto codigo = ventrik::cifrado::leer_codigo_dueno(texto)) {
    const auto otra_vez = ventrik::cifrado::leer_codigo_dueno(codigo->a_texto());
    exigir(otra_vez && *otra_vez == *codigo);
  }

  if (const auto contrasena = ventrik::servidor::leer_contrasena(texto)) {
    exigir(ventrik::cifrado::es_argon2_valido(contrasena->argon2));
  }

  if (const auto direccion = ventrik::red::leer_direccion(texto)) {
    exigir(direccion->puerto != 0);
    const auto otra_vez = ventrik::red::leer_direccion(ventrik::red::a_texto(*direccion));
    exigir(otra_vez && *otra_vez == *direccion);
  }

  if (const auto clic = ventrik::cliente::leer_argumentos_de_aviso(texto)) {
    const std::string escritos = ventrik::cliente::argumentos_de_aviso(*clic);
    exigir(escritos.size() <= ventrik::cliente::kTopeArgumentosDeAviso);
    const auto otra_vez = ventrik::cliente::leer_argumentos_de_aviso(escritos);
    exigir(otra_vez && otra_vez->servidor == clic->servidor && otra_vez->canal == clic->canal &&
           otra_vez->voz == clic->voz);
  }

  if (!texto.empty()) {
    const std::size_t corte = std::min<std::size_t>(texto.front() % 40u, texto.size() - 1);
    const std::string_view nombre = texto.substr(1, corte);
    const std::string_view mensaje = texto.substr(1 + corte);
    const auto mencion = ventrik::cliente::mencion_en(mensaje, nombre);
    exigir(mencion == ventrik::cliente::Mencion::ninguna || mensaje.find('@') != std::string_view::npos);
    if (!nombre.empty() && ventrik::es_nombre_valido(nombre)) {
      exigir(ventrik::cliente::mencion_en("@" + std::string(nombre), nombre) == ventrik::cliente::Mencion::directa);
    }
  }
  return 0;
}
