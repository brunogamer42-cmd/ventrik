#include "common/cifrado/codigo_dueno.h"

#include "common/cifrado/base32.h"
#include "common/cifrado/inicio.h"

extern "C" {
#include <mbedtls/constant_time.h>
}

namespace ventrik::cifrado {
std::optional<CodigoDueno> CodigoDueno::generar() {
  Bytes bytes{};
  if (aleatorio(nullptr, bytes.data(), bytes.size()) != 0) {
    return std::nullopt;
  }
  return CodigoDueno(bytes);
}

std::string CodigoDueno::a_texto() const {
  return a_base32(bytes_);
}

bool operator==(const CodigoDueno& a, const CodigoDueno& b) {
  return mbedtls_ct_memcmp(a.bytes().data(), b.bytes().data(), CodigoDueno::kBytes) == 0;
}

std::optional<CodigoDueno> leer_codigo_dueno(std::string_view texto) {
  CodigoDueno::Bytes bytes{};
  if (!de_base32(texto, bytes)) {
    return std::nullopt;
  }
  return CodigoDueno(bytes);
}

}
