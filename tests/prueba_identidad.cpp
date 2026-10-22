#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

#include <mbedtls/ecp.h>
#include <mbedtls/pk.h>

#include "common/archivos.h"
#include "common/cifrado/identidad.h"
#include "common/cifrado/inicio.h"
#include <doctest/doctest.h>

using ventrik::cifrado::IdentidadServidor;
using ventrik::cifrado::ResultadoIdentidad;

namespace {

class CarpetaTemporal {
 public:
  CarpetaTemporal() {
    std::array<wchar_t, MAX_PATH> base{};
    GetTempPathW(static_cast<DWORD>(base.size()), base.data());
    ruta_ = std::wstring(base.data()) + L"ventrik_prueba_" + std::to_wstring(GetCurrentProcessId()) +
            L"_" + std::to_wstring(GetTickCount());
    REQUIRE(ventrik::crear_carpetas(ruta_));
  }
  ~CarpetaTemporal() {
    for (const auto* nombre : {L"\\clave_servidor.der", L"\\clave_servidor.der.tmp"}) {
      DeleteFileW((ruta_ + nombre).c_str());
    }
    RemoveDirectoryW(ruta_.c_str());
  }
  CarpetaTemporal(const CarpetaTemporal&) = delete;
  CarpetaTemporal& operator=(const CarpetaTemporal&) = delete;

  std::wstring archivo() const { return ruta_ + L"\\clave_servidor.der"; }

 private:
  std::wstring ruta_;
};

std::vector<std::uint8_t> leer_todo(const std::wstring& archivo) {
  ventrik::cifrado::Secreto contenido;
  REQUIRE(ventrik::leer_archivo(archivo, 1 << 20, contenido) == ventrik::LecturaArchivo::leido);
  return std::vector<std::uint8_t>(contenido.bytes().begin(), contenido.bytes().end());
}

}  // namespace

TEST_CASE("La clave del servidor se guarda y se vuelve a cargar con la misma huella") {
  auto nueva = IdentidadServidor::crear_nueva();
  REQUIRE(nueva);
  const auto der = nueva->clave_der();
  REQUIRE(der);
  const auto cargada = IdentidadServidor::desde_der(der->bytes());
  REQUIRE(cargada);
  CHECK(cargada->huella() == nueva->huella());
}

TEST_CASE("Cada servidor nuevo tiene su propia huella") {
  auto una = IdentidadServidor::crear_nueva();
  auto otra = IdentidadServidor::crear_nueva();
  REQUIRE(una);
  REQUIRE(otra);
  CHECK(una->huella() != otra->huella());
}

TEST_CASE("Solo se aceptan claves privadas ECDSA P-256") {
  CHECK_FALSE(IdentidadServidor::desde_der({}));
  const std::vector<std::uint8_t> basura(200, 0x30);
  CHECK_FALSE(IdentidadServidor::desde_der(basura));

  mbedtls_pk_context p384;
  mbedtls_pk_init(&p384);
  REQUIRE(mbedtls_pk_setup(&p384, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) == 0);
  REQUIRE(mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP384R1, mbedtls_pk_ec(p384),
                              ventrik::cifrado::aleatorio, nullptr) == 0);
  std::array<unsigned char, 1024> der{};
  const int largo = mbedtls_pk_write_key_der(&p384, der.data(), der.size());
  mbedtls_pk_free(&p384);
  REQUIRE(largo > 0);
  CHECK_FALSE(IdentidadServidor::desde_der(
      gsl::span<const std::uint8_t>(der).last(static_cast<std::size_t>(largo))));
}

TEST_CASE("El servidor crea su clave una vez y después la reusa") {
  CarpetaTemporal carpeta;
  auto primera = ventrik::cifrado::cargar_o_crear_identidad(carpeta.archivo());
  REQUIRE(primera.resultado == ResultadoIdentidad::creada);
  REQUIRE(primera.identidad);
  auto segunda = ventrik::cifrado::cargar_o_crear_identidad(carpeta.archivo());
  REQUIRE(segunda.resultado == ResultadoIdentidad::cargada);
  REQUIRE(segunda.identidad);
  CHECK(segunda.identidad->huella() == primera.identidad->huella());
}

TEST_CASE("Con la clave dañada, el servidor no crea otra en silencio") {
  CarpetaTemporal carpeta;
  const std::vector<std::uint8_t> danada = {0x30, 0x82, 0x01, 0x00, 0x02};
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.archivo(), danada));
  const auto resultado = ventrik::cifrado::cargar_o_crear_identidad(carpeta.archivo());
  CHECK(resultado.resultado == ResultadoIdentidad::archivo_invalido);
  CHECK_FALSE(resultado.identidad);
  CHECK(leer_todo(carpeta.archivo()) == danada);
}

TEST_CASE("Al arrancar se borran los temporales de una escritura cortada") {
  CarpetaTemporal carpeta;
  const std::wstring temporal = carpeta.archivo() + L".999.1.tmp";
  const std::vector<std::uint8_t> clave_vieja = {0x30, 0x01, 0x00};
  REQUIRE(ventrik::escribir_archivo_completo(temporal, clave_vieja));
  REQUIRE(GetFileAttributesW(temporal.c_str()) != INVALID_FILE_ATTRIBUTES);
  CHECK(ventrik::cifrado::cargar_o_crear_identidad(carpeta.archivo()).resultado ==
        ResultadoIdentidad::creada);
  CHECK(GetFileAttributesW(temporal.c_str()) == INVALID_FILE_ATTRIBUTES);
}

TEST_CASE("Una clave enorme no se lee") {
  CarpetaTemporal carpeta;
  const std::vector<std::uint8_t> enorme(ventrik::cifrado::kMaximoArchivoClave + 1, 0x30);
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.archivo(), enorme));
  CHECK(ventrik::cifrado::cargar_o_crear_identidad(carpeta.archivo()).resultado ==
        ResultadoIdentidad::archivo_invalido);
}
