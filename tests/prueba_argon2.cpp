#include <array>
#include <cstddef>
#include <string>

#include <argon2.h>
#include <doctest/doctest.h>

namespace {

template <std::size_t N>
std::string a_hexadecimal(const std::array<unsigned char, N>& bytes) {
  std::string texto;
  const char* const digitos = "0123456789abcdef";
  for (const unsigned char b : bytes) {
    texto.push_back(digitos[b >> 4]);
    texto.push_back(digitos[b & 0x0F]);
  }
  return texto;
}

// Ejemplo oficial de src/test.c de la implementación de referencia:
// Argon2id versión 19, t=2, m=256 KiB, p=1, contraseña "password", sal "somesalt".
const std::string kContrasena = "password";
const std::string kSal = "somesalt";

}  // namespace

TEST_CASE("Argon2id da el resultado del ejemplo oficial") {
  std::array<unsigned char, 32> resultado{};
  REQUIRE(argon2id_hash_raw(2, 1 << 8, 1, kContrasena.data(), kContrasena.size(),
                            kSal.data(), kSal.size(), resultado.data(),
                            resultado.size()) == ARGON2_OK);
  CHECK(a_hexadecimal(resultado) ==
        "9dfeb910e80bad0311fee20f9c0e2b12c17987b4cac90c2ef54d5b3021c68bfe");
}

TEST_CASE("Argon2id acepta la contraseña correcta y rechaza otra") {
  const char* const guardado =
      "$argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4";
  CHECK(argon2id_verify(guardado, kContrasena.data(), kContrasena.size()) == ARGON2_OK);
  const std::string otra = "passw0rd";
  CHECK(argon2id_verify(guardado, otra.data(), otra.size()) == ARGON2_VERIFY_MISMATCH);
}
