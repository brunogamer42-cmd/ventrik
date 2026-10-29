#include "server/contrasena.h"

#include <algorithm>

#include "common/cifrado/base32.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/secreto.h"
#include "common/protocol/bytes.h"
#include "common/protocol/hostear.h"

extern "C" {
#include <mbedtls/constant_time.h>
}

namespace ventrik::servidor {
namespace {

constexpr std::uint8_t kVersion = 1;
constexpr std::size_t kBytes = 1 + 4 + 1 + cifrado::kBytesSal + cifrado::kBytesClaveSimetrica;

}

std::optional<std::string> guardar_contrasena(std::string_view contrasena, std::optional<cifrado::Argon2> fijos) {
  ContrasenaGuardada guardada;
  cifrado::Secreto resumen(cifrado::kBytesClaveSimetrica);
  if (contrasena.empty() || !protocolo::es_contrasena_valida(contrasena) ||
      cifrado::aleatorio(nullptr, guardada.sal.data(), guardada.sal.size()) != 0) {
    return std::nullopt;
  }
  const auto valores =
      fijos ? (cifrado::derivar_argon2(contrasena, guardada.sal, *fijos, resumen.bytes()) ? fijos : std::nullopt)
            : cifrado::derivar_a_tiempo(contrasena, guardada.sal, kMilisegundosArgon2Servidor, resumen.bytes());
  if (!valores) {
    return std::nullopt;
  }
  protocolo::Escritor escritor;
  escritor.escribir_u8(kVersion);
  escritor.escribir_u32(valores->memoria_kib);
  escritor.escribir_u8(valores->vueltas);
  escritor.escribir_bytes(guardada.sal);
  escritor.escribir_bytes(resumen.bytes());
  return escritor.ok() ? std::optional<std::string>(cifrado::a_base32(escritor.datos())) : std::nullopt;
}

std::optional<ContrasenaGuardada> leer_contrasena(std::string_view texto) {
  std::array<std::uint8_t, kBytes> bytes{};
  if (!cifrado::de_base32(texto, bytes)) {
    return std::nullopt;
  }
  protocolo::Lector lector(bytes);
  ContrasenaGuardada guardada;
  const std::uint8_t version = lector.leer_u8();
  guardada.argon2.memoria_kib = lector.leer_u32();
  guardada.argon2.vueltas = lector.leer_u8();
  const auto sal = lector.leer_bytes(guardada.sal.size());
  const auto resumen = lector.leer_bytes(guardada.resumen.size());
  if (!lector.terminado() || version != kVersion || !cifrado::es_argon2_valido(guardada.argon2)) {
    return std::nullopt;
  }
  std::copy(sal.begin(), sal.end(), guardada.sal.begin());
  std::copy(resumen.begin(), resumen.end(), guardada.resumen.begin());
  return guardada;
}

Comprobado comprobar(const ContrasenaGuardada& guardada, std::string_view contrasena) {
  cifrado::Secreto resumen(cifrado::kBytesClaveSimetrica);
  if (contrasena.empty() || contrasena.size() > protocolo::kMaximoBytesContrasena) {
    return Comprobado::distinta;
  }
  if (!cifrado::derivar_argon2(contrasena, guardada.sal, guardada.argon2, resumen.bytes())) {
    return Comprobado::fallo;
  }
  return mbedtls_ct_memcmp(resumen.bytes().data(), guardada.resumen.data(), guardada.resumen.size()) == 0
             ? Comprobado::coincide
             : Comprobado::distinta;
}

}
