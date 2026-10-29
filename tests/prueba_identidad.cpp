#include <algorithm>
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

using ventrik::cifrado::Argon2;
using ventrik::cifrado::Boveda;
using ventrik::cifrado::IdentidadServidor;
using ventrik::cifrado::ResultadoIdentidad;

namespace {

constexpr Argon2 kRapido{8 * 1024, 1};

class CarpetaTemporal {
 public:
  CarpetaTemporal() {
    std::array<wchar_t, MAX_PATH> base{};
    GetTempPathW(static_cast<DWORD>(base.size()), base.data());
    ruta_ = std::wstring(base.data()) + L"ventrik_prueba_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
            std::to_wstring(GetTickCount()) + L"_" + std::to_wstring(++creadas_);
    REQUIRE(ventrik::crear_carpetas(ruta_));
  }
  ~CarpetaTemporal() {
    WIN32_FIND_DATAW encontrado = {};
    const HANDLE busqueda = FindFirstFileW((ruta_ + L"\\*").c_str(), &encontrado);
    if (busqueda != INVALID_HANDLE_VALUE) {
      do {
        DeleteFileW((ruta_ + L"\\" + encontrado.cFileName).c_str());
      } while (FindNextFileW(busqueda, &encontrado) != 0);
      FindClose(busqueda);
    }
    RemoveDirectoryW(ruta_.c_str());
  }
  CarpetaTemporal(const CarpetaTemporal&) = delete;
  CarpetaTemporal& operator=(const CarpetaTemporal&) = delete;

  const std::wstring& ruta() const { return ruta_; }
  std::wstring boveda() const { return ruta_ + ventrik::cifrado::kArchivoBoveda; }
  std::wstring clave() const { return ruta_ + ventrik::cifrado::kArchivoClave; }
  std::wstring vieja() const { return ruta_ + ventrik::cifrado::kArchivoClaveVieja; }

 private:
  static inline int creadas_ = 0;
  std::wstring ruta_;
};

std::vector<std::uint8_t> leer_todo(const std::wstring& archivo) {
  ventrik::cifrado::Secreto contenido;
  REQUIRE(ventrik::leer_archivo(archivo, 1 << 20, contenido) == ventrik::LecturaArchivo::leido);
  return std::vector<std::uint8_t>(contenido.bytes().begin(), contenido.bytes().end());
}

bool existe(const std::wstring& archivo) {
  return GetFileAttributesW(archivo.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool contiene(const std::vector<std::uint8_t>& todo, gsl::span<const std::uint8_t> parte) {
  return std::search(todo.begin(), todo.end(), parte.begin(), parte.end()) != todo.end();
}

ventrik::cifrado::IdentidadCargada cargar(const CarpetaTemporal& carpeta, bool con_dueno = false,
                                          std::optional<std::string_view> contrasena = std::nullopt) {
  return ventrik::cifrado::cargar_o_crear_identidad(carpeta.ruta(), contrasena, con_dueno);
}

IdentidadServidor identidad_nueva() {
  auto nueva = IdentidadServidor::crear_nueva();
  REQUIRE(nueva);
  return std::move(*nueva);
}

}

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

TEST_CASE("La bóveda va y vuelve, con y sin contraseña de arranque, y da siempre las mismas claves") {
  const auto boveda = Boveda::crear_nueva();
  REQUIRE(boveda);
  const auto historial = boveda->derivar(ventrik::cifrado::kEtiquetaHistorial);
  const auto clave = boveda->derivar(ventrik::cifrado::kEtiquetaClaveServidor);
  REQUIRE(historial);
  REQUIRE(clave);
  CHECK(historial->size() == ventrik::cifrado::kBytesClaveSimetrica);
  CHECK_FALSE(std::equal(historial->bytes().begin(), historial->bytes().end(), clave->bytes().begin()));
  const auto otra = Boveda::crear_nueva();
  REQUIRE(otra);
  CHECK_FALSE(std::equal(historial->bytes().begin(), historial->bytes().end(),
                         otra->derivar(ventrik::cifrado::kEtiquetaHistorial)->bytes().begin()));
  const auto misma_clave = [&historial](const ventrik::cifrado::BovedaAbierta& abierta) {
    REQUIRE(abierta.boveda);
    const auto derivada = abierta.boveda->derivar(ventrik::cifrado::kEtiquetaHistorial);
    return std::equal(derivada->bytes().begin(), derivada->bytes().end(), historial->bytes().begin(),
                      historial->bytes().end());
  };

  const auto sin_contrasena = boveda->armar_archivo(std::nullopt);
  REQUIRE(sin_contrasena);
  CHECK(misma_clave(ventrik::cifrado::abrir_boveda(*sin_contrasena, std::nullopt)));
  CHECK(misma_clave(ventrik::cifrado::abrir_boveda(*sin_contrasena, std::string_view("no hace falta"))));

  const auto con_contrasena = boveda->armar_archivo(std::string_view("una contraseña larga"), kRapido);
  REQUIRE(con_contrasena);
  CHECK(ventrik::cifrado::abrir_boveda(*con_contrasena, std::nullopt).resultado ==
        ventrik::cifrado::AperturaBoveda::falta_contrasena);
  const auto equivocada = ventrik::cifrado::abrir_boveda(*con_contrasena, std::string_view("una contraseña larg"));
  CHECK(equivocada.resultado == ventrik::cifrado::AperturaBoveda::contrasena);
  CHECK_FALSE(equivocada.boveda);
  const auto buena = ventrik::cifrado::abrir_boveda(*con_contrasena, std::string_view("una contraseña larga"));
  CHECK(buena.resultado == ventrik::cifrado::AperturaBoveda::abierta);
  CHECK(misma_clave(buena));
}

TEST_CASE("Una bóveda alterada, cortada o de una capa desconocida no se abre") {
  const auto boveda = Boveda::crear_nueva();
  REQUIRE(boveda);
  for (const bool con_contrasena : {false, true}) {
    const auto archivo = boveda->armar_archivo(
        con_contrasena ? std::optional<std::string_view>("contraseña de arranque") : std::nullopt, kRapido);
    REQUIRE(archivo);
    const auto abrir = [](const std::vector<std::uint8_t>& datos) {
      const auto abierta = ventrik::cifrado::abrir_boveda(datos, std::string_view("contraseña de arranque"));
      CHECK(abierta.boveda.has_value() == (abierta.resultado == ventrik::cifrado::AperturaBoveda::abierta));
      return abierta.resultado;
    };
    CHECK(abrir(*archivo) == ventrik::cifrado::AperturaBoveda::abierta);
    for (std::size_t lugar = 0; lugar < 6; ++lugar) {
      auto alterado = *archivo;
      alterado[lugar] ^= 0x01;
      CHECK(abrir(alterado) == ventrik::cifrado::AperturaBoveda::danada);
    }
    for (const int capa : {0, 1, 2, 255}) {
      auto otra_capa = *archivo;
      otra_capa[5] = static_cast<std::uint8_t>(capa);
      CHECK(abrir(otra_capa) ==
            (capa == (con_contrasena ? 1 : 0) ? ventrik::cifrado::AperturaBoveda::abierta
                                              : ventrik::cifrado::AperturaBoveda::danada));
    }
    for (const std::size_t lugar : {std::size_t{6}, archivo->size() / 2, archivo->size() - 1}) {
      auto alterado = *archivo;
      alterado[lugar] ^= 0x80;
      CHECK(abrir(alterado) == ventrik::cifrado::AperturaBoveda::otro_windows);
    }
    CHECK(abrir(std::vector<std::uint8_t>(archivo->begin(), archivo->end() - 1)) ==
          ventrik::cifrado::AperturaBoveda::otro_windows);
    CHECK(abrir(std::vector<std::uint8_t>(archivo->begin(), archivo->begin() + 6)) ==
          ventrik::cifrado::AperturaBoveda::danada);
  }
  CHECK(ventrik::cifrado::abrir_boveda({}, std::nullopt).resultado == ventrik::cifrado::AperturaBoveda::danada);
}

TEST_CASE("La clave cifrada solo se abre con su bóveda y sin cambios") {
  const auto boveda = Boveda::crear_nueva();
  const auto otra = Boveda::crear_nueva();
  REQUIRE(boveda);
  REQUIRE(otra);
  const auto identidad = identidad_nueva();
  const auto cerrada = ventrik::cifrado::cerrar_clave(identidad, *boveda);
  REQUIRE(cerrada);
  const auto der = identidad.clave_der();
  REQUIRE(der);
  CHECK_FALSE(contiene(*cerrada, der->bytes()));
  const auto abierta = ventrik::cifrado::abrir_clave(*cerrada, *boveda);
  REQUIRE(abierta);
  CHECK(abierta->huella() == identidad.huella());
  CHECK_FALSE(ventrik::cifrado::abrir_clave(*cerrada, *otra));
  for (std::size_t lugar = 0; lugar < cerrada->size(); lugar += 7) {
    auto alterada = *cerrada;
    alterada[lugar] ^= 0x04;
    CHECK_FALSE(ventrik::cifrado::abrir_clave(alterada, *boveda));
  }
  CHECK_FALSE(ventrik::cifrado::abrir_clave(gsl::make_span(*cerrada).first(20), *boveda));
  CHECK_FALSE(ventrik::cifrado::abrir_clave({}, *boveda));
}

TEST_CASE("El servidor crea su bóveda y su clave cifrada una vez y después las reusa") {
  CarpetaTemporal carpeta;
  auto primera = cargar(carpeta);
  REQUIRE(primera.resultado == ResultadoIdentidad::creada);
  REQUIRE(primera.identidad);
  REQUIRE(primera.boveda);
  CHECK(existe(carpeta.boveda()));
  CHECK(existe(carpeta.clave()));
  CHECK_FALSE(existe(carpeta.vieja()));
  const auto der = primera.identidad->clave_der();
  REQUIRE(der);
  CHECK_FALSE(contiene(leer_todo(carpeta.clave()), der->bytes()));
  auto segunda = cargar(carpeta, true);
  REQUIRE(segunda.resultado == ResultadoIdentidad::cargada);
  REQUIRE(segunda.identidad);
  CHECK(segunda.identidad->huella() == primera.identidad->huella());
  const auto historial = segunda.boveda->derivar(ventrik::cifrado::kEtiquetaHistorial);
  const auto antes = primera.boveda->derivar(ventrik::cifrado::kEtiquetaHistorial);
  CHECK(std::equal(historial->bytes().begin(), historial->bytes().end(), antes->bytes().begin()));
}

TEST_CASE("La clave vieja sin cifrar se convierte sola, con la misma huella, y se borra") {
  for (const bool con_dueno : {false, true}) {
    CarpetaTemporal carpeta;
    const auto identidad = identidad_nueva();
    const auto der = identidad.clave_der();
    REQUIRE(der);
    REQUIRE(ventrik::escribir_archivo_completo(carpeta.vieja(), der->bytes()));
    const auto convertida = cargar(carpeta, con_dueno);
    REQUIRE(convertida.resultado == ResultadoIdentidad::convertida);
    CHECK(convertida.identidad->huella() == identidad.huella());
    CHECK_FALSE(existe(carpeta.vieja()));
    CHECK_FALSE(contiene(leer_todo(carpeta.clave()), der->bytes()));
    const auto despues = cargar(carpeta, con_dueno);
    REQUIRE(despues.resultado == ResultadoIdentidad::cargada);
    CHECK(despues.identidad->huella() == identidad.huella());
  }
}

TEST_CASE("Si la conversión quedó a medias, la clave vieja igual se borra; si es otra, no arranca") {
  CarpetaTemporal carpeta;
  const auto creada = cargar(carpeta);
  REQUIRE(creada.identidad);
  const auto der = creada.identidad->clave_der();
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.vieja(), der->bytes()));
  CHECK(cargar(carpeta).resultado == ResultadoIdentidad::cargada);
  CHECK_FALSE(existe(carpeta.vieja()));

  const auto otra = identidad_nueva();
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.vieja(), otra.clave_der()->bytes()));
  const auto distintas = cargar(carpeta);
  CHECK(distintas.resultado == ResultadoIdentidad::claves_distintas);
  CHECK(distintas.archivo == carpeta.vieja());
  CHECK_FALSE(distintas.identidad);
  CHECK(existe(carpeta.vieja()));
}

TEST_CASE("Una clave vieja ya pisada con ceros es resto de un borrado cortado: se borra y el servidor arranca") {
  CarpetaTemporal carpeta;
  const auto creada = cargar(carpeta);
  REQUIRE(creada.identidad);
  const auto der = creada.identidad->clave_der();
  REQUIRE(der);
  for (const std::size_t largo : {der->size(), std::size_t{1}}) {
    REQUIRE(ventrik::escribir_archivo_completo(carpeta.vieja(), std::vector<std::uint8_t>(largo)));
    const auto cargada = cargar(carpeta, true);
    CHECK(cargada.resultado == ResultadoIdentidad::cargada);
    CHECK(cargada.identidad->huella() == creada.identidad->huella());
    CHECK_FALSE(cargada.vieja_sin_borrar);
    CHECK_FALSE(existe(carpeta.vieja()));
  }
}

TEST_CASE("Si la clave vieja no se puede borrar, se avisa en vez de decir que se borró") {
  CarpetaTemporal carpeta;
  const auto identidad = identidad_nueva();
  const auto der = identidad.clave_der();
  REQUIRE(der);
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.vieja(), der->bytes()));
  REQUIRE(SetFileAttributesW(carpeta.vieja().c_str(), FILE_ATTRIBUTE_READONLY));
  const auto convertida = cargar(carpeta);
  CHECK(convertida.resultado == ResultadoIdentidad::convertida);
  CHECK(convertida.vieja_sin_borrar);
  CHECK(existe(carpeta.vieja()));
  const auto otra_vez = cargar(carpeta);
  CHECK(otra_vez.resultado == ResultadoIdentidad::cargada);
  CHECK(otra_vez.vieja_sin_borrar);
  REQUIRE(SetFileAttributesW(carpeta.vieja().c_str(), FILE_ATTRIBUTE_NORMAL));
  const auto borrada = cargar(carpeta);
  CHECK_FALSE(borrada.vieja_sin_borrar);
  CHECK_FALSE(existe(carpeta.vieja()));
}

TEST_CASE("Una clave nueva se crea solo si no hay nada: si falta un archivo, el servidor no arranca") {
  SUBCASE("base con dueño y sin clave") {
    CarpetaTemporal carpeta;
    const auto resultado = cargar(carpeta, true);
    CHECK(resultado.resultado == ResultadoIdentidad::falta_clave);
    CHECK(resultado.archivo == carpeta.clave());
    CHECK_FALSE(existe(carpeta.boveda()));
    CHECK_FALSE(existe(carpeta.clave()));
  }
  SUBCASE("bóveda sin clave") {
    CarpetaTemporal carpeta;
    REQUIRE(cargar(carpeta).identidad);
    REQUIRE(DeleteFileW(carpeta.clave().c_str()));
    for (const bool con_dueno : {false, true}) {
      const auto resultado = cargar(carpeta, con_dueno);
      CHECK(resultado.resultado == ResultadoIdentidad::falta_clave);
      CHECK_FALSE(resultado.identidad);
    }
    CHECK_FALSE(existe(carpeta.clave()));
  }
  SUBCASE("clave cifrada sin bóveda") {
    CarpetaTemporal carpeta;
    REQUIRE(cargar(carpeta).identidad);
    const auto clave = leer_todo(carpeta.clave());
    REQUIRE(DeleteFileW(carpeta.boveda().c_str()));
    const auto resultado = cargar(carpeta);
    CHECK(resultado.resultado == ResultadoIdentidad::falta_boveda);
    CHECK(resultado.archivo == carpeta.boveda());
    CHECK_FALSE(existe(carpeta.boveda()));
    CHECK(leer_todo(carpeta.clave()) == clave);
  }
  SUBCASE("clave cifrada con otra bóveda") {
    CarpetaTemporal carpeta;
    REQUIRE(cargar(carpeta).identidad);
    const auto otra = Boveda::crear_nueva()->armar_archivo(std::nullopt);
    REQUIRE(ventrik::escribir_archivo_completo(carpeta.boveda(), *otra));
    CHECK(cargar(carpeta).resultado == ResultadoIdentidad::archivo_invalido);
  }
}

TEST_CASE("Con la clave o la bóveda dañadas, el servidor no crea otra en silencio") {
  CarpetaTemporal carpeta;
  REQUIRE(cargar(carpeta).identidad);
  const auto clave = leer_todo(carpeta.clave());
  const auto boveda = leer_todo(carpeta.boveda());

  auto danada = clave;
  danada.back() ^= 0x10;
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.clave(), danada));
  const auto resultado = cargar(carpeta);
  CHECK(resultado.resultado == ResultadoIdentidad::archivo_invalido);
  CHECK(resultado.archivo == carpeta.clave());
  CHECK_FALSE(resultado.identidad);
  CHECK(leer_todo(carpeta.clave()) == danada);
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.clave(), clave));

  auto otro_windows = boveda;
  otro_windows[boveda.size() / 2] ^= 0x10;
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.boveda(), otro_windows));
  const auto de_otro = cargar(carpeta);
  CHECK(de_otro.resultado == ResultadoIdentidad::boveda_de_otro_windows);
  CHECK(de_otro.archivo == carpeta.boveda());

  auto otra_capa = boveda;
  otra_capa[5] = 7;
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.boveda(), otra_capa));
  CHECK(cargar(carpeta).resultado == ResultadoIdentidad::boveda_danada);

  const std::vector<std::uint8_t> enorme(ventrik::cifrado::kMaximoArchivoBoveda + 1, 0x56);
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.boveda(), enorme));
  CHECK(cargar(carpeta).resultado == ResultadoIdentidad::boveda_danada);
  CHECK(leer_todo(carpeta.boveda()) == enorme);

  REQUIRE(ventrik::escribir_archivo_completo(carpeta.boveda(), boveda));
  const std::vector<std::uint8_t> clave_enorme(ventrik::cifrado::kMaximoArchivoClave + 1, 0x30);
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.clave(), clave_enorme));
  CHECK(cargar(carpeta).resultado == ResultadoIdentidad::archivo_invalido);
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.clave(), clave));
  CHECK(cargar(carpeta).resultado == ResultadoIdentidad::cargada);
}

TEST_CASE("Con contraseña de arranque, sin ella o equivocada no abre, y con la buena sí") {
  CarpetaTemporal carpeta;
  const auto creada = cargar(carpeta);
  REQUIRE(creada.boveda);
  const auto archivo = creada.boveda->armar_archivo(std::string_view("pan con manteca"), kRapido);
  REQUIRE(archivo);
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.boveda(), *archivo));
  const auto sin = cargar(carpeta);
  CHECK(sin.resultado == ResultadoIdentidad::falta_contrasena);
  CHECK(sin.archivo.empty());
  const auto mala = cargar(carpeta, false, std::string_view("pan con mantec"));
  CHECK(mala.resultado == ResultadoIdentidad::contrasena_incorrecta);
  CHECK_FALSE(mala.identidad);
  CHECK(mala.archivo.empty());
  const auto buena = cargar(carpeta, false, std::string_view("pan con manteca"));
  REQUIRE(buena.resultado == ResultadoIdentidad::cargada);
  CHECK(buena.identidad->huella() == creada.identidad->huella());
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.boveda(), *buena.boveda->armar_archivo(std::nullopt)));
  CHECK(cargar(carpeta).resultado == ResultadoIdentidad::cargada);
}

TEST_CASE("Al arrancar se borran los temporales de una escritura cortada") {
  CarpetaTemporal carpeta;
  const std::wstring temporal = carpeta.clave() + L".999.1.tmp";
  const std::vector<std::uint8_t> clave_vieja = {0x30, 0x01, 0x00};
  REQUIRE(ventrik::escribir_archivo_completo(temporal, clave_vieja));
  REQUIRE(existe(temporal));
  CHECK(cargar(carpeta).resultado == ResultadoIdentidad::creada);
  CHECK_FALSE(existe(temporal));
}

TEST_CASE("Borrar con ceros pisa el contenido antes de borrar el archivo") {
  CarpetaTemporal carpeta;
  const std::wstring archivo = carpeta.ruta() + L"\\secreto.der";
  REQUIRE(ventrik::escribir_archivo_completo(archivo, std::vector<std::uint8_t>(300, 0x42)));
  CHECK(ventrik::borrar_con_ceros(archivo));
  CHECK_FALSE(existe(archivo));
  CHECK_FALSE(ventrik::borrar_con_ceros(archivo));
}
