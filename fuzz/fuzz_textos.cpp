#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include <gsl/gsl>

#include "client/avisos.h"
#include "common/cifrado/base32.h"
#include "common/cifrado/codigo_dueno.h"
#include "common/cifrado/huella.h"
#include "common/cifrado/totp.h"
#include "common/red/red.h"
#include "common/texto.h"
#include "server/contrasena.h"

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

std::string sin_porcentajes(std::string_view texto) {
  constexpr std::string_view kHexadecimal = "0123456789ABCDEF";
  std::string resultado;
  for (std::size_t i = 0; i < texto.size(); ++i) {
    if (texto[i] != '%') {
      resultado.push_back(texto[i]);
      continue;
    }
    exigir(i + 2 < texto.size());
    const std::size_t alto = kHexadecimal.find(texto[i + 1]);
    const std::size_t bajo = kHexadecimal.find(texto[i + 2]);
    exigir(alto != std::string_view::npos && bajo != std::string_view::npos);
    resultado.push_back(static_cast<char>(alto * 16 + bajo));
    i += 2;
  }
  return resultado;
}

void probar_totp(const std::uint8_t* datos, std::string_view texto) {
  using namespace ventrik::cifrado;
  if (const auto codigo = leer_codigo_totp(texto)) {
    exigir(*codigo < kTopeCodigoTotp);
    std::string cifras(texto);
    cifras.erase(std::remove(cifras.begin(), cifras.end(), ' '), cifras.end());
    exigir(cifras == std::to_string(*codigo + kTopeCodigoTotp).substr(1));
    exigir(leer_codigo_totp(cifras) == codigo);
  }

  if (texto.size() < kBytesSecretoTotp) {
    return;
  }
  const gsl::span<const std::uint8_t> secreto(datos, kBytesSecretoTotp);
  const std::string_view nombre = texto.substr(kBytesSecretoTotp);
  const std::string uri = texto_otpauth(nombre, secreto);
  constexpr std::string_view kInicio = "otpauth://totp/Ventrik:";
  constexpr std::string_view kSecreto = "?secret=";
  constexpr std::string_view kFinal = "&issuer=Ventrik&digits=6&period=30";
  exigir(uri.compare(0, kInicio.size(), kInicio) == 0);
  exigir(std::all_of(uri.begin(), uri.end(), [](char letra) { return letra > ' ' && letra < 0x7F; }));
  const std::size_t pregunta = uri.find('?');
  exigir(sin_porcentajes(std::string_view(uri).substr(kInicio.size(), pregunta - kInicio.size())) == nombre);
  exigir(uri.compare(pregunta, kSecreto.size(), kSecreto) == 0);
  const std::size_t desde = pregunta + kSecreto.size();
  const std::size_t hasta = uri.find('&', desde);
  exigir(hasta != std::string::npos && uri.compare(hasta, std::string::npos, kFinal) == 0);
  std::array<std::uint8_t, kBytesSecretoTotp> leido{};
  exigir(de_base32(std::string_view(uri).substr(desde, hasta - desde), leido));
  exigir(std::equal(leido.begin(), leido.end(), secreto.begin()));

  std::uint64_t segundos = 0;
  for (const std::uint8_t byte : secreto.first(8)) {
    segundos = segundos << 8 | byte;
  }
  const std::uint64_t paso = segundos / kSegundosPasoTotp;
  const auto codigo = codigo_totp(secreto, paso);
  exigir(codigo && *codigo < kTopeCodigoTotp);
  const auto aceptado = verificar_totp(secreto, *codigo, segundos);
  exigir(aceptado && (*aceptado == paso || *aceptado == paso + 1));
  exigir(!verificar_totp(secreto, *codigo, segundos, paso + 1));
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  const std::string_view texto(reinterpret_cast<const char*>(datos), largo);
  probar_totp(datos, texto);

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
