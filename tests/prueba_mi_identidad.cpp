#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

#include "client/mi_identidad.h"
#include "common/archivos.h"
#include "common/cifrado/clave.h"
#include "common/cifrado/identidad.h"
#include <doctest/doctest.h>

using ventrik::cifrado::AperturaSobre;
using ventrik::cifrado::Clave;
using ventrik::cliente::EstadoIdentidad;

namespace {

constexpr ventrik::cifrado::Argon2 kRapido{ventrik::cifrado::kMinimoMemoriaArgon2, 1};

class Carpeta {
 public:
  Carpeta() {
    std::array<wchar_t, MAX_PATH> base{};
    GetTempPathW(static_cast<DWORD>(base.size()), base.data());
    ruta_ = std::wstring(base.data()) + L"ventrik_mi_identidad_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
            std::to_wstring(GetTickCount());
  }
  ~Carpeta() {
    for (const auto* nombre :
         {L"\\identidad.dat", L"\\equipo.dat", L"\\identidad-vieja-1.dat", L"\\identidad-vieja-2.dat"}) {
      DeleteFileW((ruta_ + nombre).c_str());
    }
    RemoveDirectoryW(ruta_.c_str());
  }
  Carpeta(const Carpeta&) = delete;
  Carpeta& operator=(const Carpeta&) = delete;

  const std::wstring& ruta() const { return ruta_; }
  std::wstring archivo(const wchar_t* nombre) const { return ruta_ + L"\\" + nombre; }

 private:
  std::wstring ruta_;
};

std::vector<std::uint8_t> leer_todo(const std::wstring& archivo) {
  ventrik::cifrado::Secreto contenido;
  REQUIRE(ventrik::leer_archivo(archivo, 1 << 20, contenido) == ventrik::LecturaArchivo::leido);
  return std::vector<std::uint8_t>(contenido.bytes().begin(), contenido.bytes().end());
}

std::vector<std::uint8_t> der_de(const Clave& clave) {
  const auto der = clave.der();
  REQUIRE(der);
  return std::vector<std::uint8_t>(der->bytes().begin(), der->bytes().end());
}

}  // namespace

TEST_CASE("Una firma se verifica con su clave y solo sobre los mismos datos") {
  const auto clave = Clave::crear_nueva();
  const auto otra = Clave::crear_nueva();
  REQUIRE(clave);
  REQUIRE(otra);
  const std::vector<std::uint8_t> datos = {1, 2, 3, 4, 5};
  const auto firma = clave->firmar(datos);
  REQUIRE(firma);
  CHECK(firma->size() <= ventrik::cifrado::kMaximoFirma);
  CHECK(ventrik::cifrado::verificar_firma(clave->publica(), datos, *firma));
  CHECK_FALSE(ventrik::cifrado::verificar_firma(otra->publica(), datos, *firma));
  CHECK_FALSE(ventrik::cifrado::verificar_firma(clave->publica(), std::vector<std::uint8_t>{1, 2, 3, 4, 6}, *firma));
  auto alterada = *firma;
  alterada[alterada.size() - 1] ^= 0x01;
  CHECK_FALSE(ventrik::cifrado::verificar_firma(clave->publica(), datos, alterada));
  auto con_basura = *firma;
  con_basura.push_back(0);
  CHECK_FALSE(ventrik::cifrado::verificar_firma(clave->publica(), datos, con_basura));
  CHECK_FALSE(ventrik::cifrado::verificar_firma(clave->publica(), datos, {}));
}

TEST_CASE("La clave pública sirve para sacar la misma huella y se rechaza si no está en la curva") {
  const auto clave = Clave::crear_nueva();
  REQUIRE(clave);
  const auto huella = ventrik::cifrado::huella_de_publica(clave->publica());
  REQUIRE(huella);
  CHECK(*huella == clave->huella());
  auto fuera = clave->publica();
  fuera[64] ^= 0x01;
  CHECK_FALSE(ventrik::cifrado::huella_de_publica(fuera));
  auto comprimida = clave->publica();
  comprimida[0] = 0x02;
  CHECK_FALSE(ventrik::cifrado::huella_de_publica(comprimida));
  const std::vector<std::uint8_t> datos = {9};
  const auto firma = clave->firmar(datos);
  REQUIRE(firma);
  CHECK_FALSE(ventrik::cifrado::verificar_firma(fuera, datos, *firma));
}

TEST_CASE("La clave del servidor sigue teniendo la misma huella que su clave") {
  const auto servidor = ventrik::cifrado::IdentidadServidor::crear_nueva();
  REQUIRE(servidor);
  const auto der = servidor->clave_der();
  REQUIRE(der);
  const auto clave = Clave::desde_der(der->bytes());
  REQUIRE(clave);
  CHECK(clave->huella() == servidor->huella());
}

TEST_CASE("El archivo de identidad se arma con DPAPI y vuelve a dar la misma clave") {
  const auto clave = Clave::crear_nueva();
  REQUIRE(clave);
  const auto archivo = ventrik::cliente::armar_archivo_de_identidad(*clave);
  REQUIRE(archivo);
  const auto der = der_de(*clave);
  CHECK(std::search(archivo->begin(), archivo->end(), der.begin(), der.end()) == archivo->end());
  const auto leida = ventrik::cliente::leer_archivo_de_identidad(*archivo);
  REQUIRE(leida);
  CHECK(leida->huella() == clave->huella());
  for (const std::size_t i : {std::size_t{0}, std::size_t{4}, std::size_t{5}, archivo->size() / 2, archivo->size() - 1}) {
    auto alterado = *archivo;
    alterado[i] ^= 0x01;
    CHECK_FALSE(ventrik::cliente::leer_archivo_de_identidad(alterado));
  }
  CHECK_FALSE(ventrik::cliente::leer_archivo_de_identidad(gsl::span<const std::uint8_t>(*archivo).first(5)));
}

TEST_CASE("La identidad se crea una vez y después se reusa") {
  const Carpeta carpeta;
  const auto primera = ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta());
  REQUIRE(primera.estado == EstadoIdentidad::creada);
  REQUIRE(primera.clave);
  const auto segunda = ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta());
  REQUIRE(segunda.estado == EstadoIdentidad::lista);
  REQUIRE(segunda.clave);
  CHECK(segunda.clave->huella() == primera.clave->huella());
}

TEST_CASE("Con el archivo dañado no se crea otra identidad en silencio") {
  const Carpeta carpeta;
  REQUIRE(ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta()).clave);
  auto contenido = leer_todo(carpeta.archivo(L"identidad.dat"));
  contenido[contenido.size() / 2] ^= 0x01;
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.archivo(L"identidad.dat"), contenido));
  const auto cargada = ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta());
  CHECK(cargada.estado == EstadoIdentidad::danada);
  CHECK_FALSE(cargada.clave);
  CHECK(leer_todo(carpeta.archivo(L"identidad.dat")) == contenido);
}

TEST_CASE("Guardar otra identidad la reemplaza") {
  const Carpeta carpeta;
  REQUIRE(ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta()).clave);
  const auto otra = Clave::crear_nueva();
  REQUIRE(otra);
  REQUIRE(ventrik::cliente::guardar_mi_identidad(carpeta.ruta(), *otra));
  const auto cargada = ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta());
  REQUIRE(cargada.clave);
  CHECK(cargada.clave->huella() == otra->huella());
}

TEST_CASE("La copia de la identidad se abre con su contraseña y da la misma identidad") {
  const auto clave = Clave::crear_nueva();
  REQUIRE(clave);
  const auto der = der_de(*clave);
  const auto copia = ventrik::cliente::armar_copia(der, "contraseña de la copia", kRapido);
  REQUIRE(copia);
  CHECK(std::search(copia->begin(), copia->end(), der.begin(), der.end()) == copia->end());
  const auto abierta = ventrik::cliente::abrir_copia(*copia, "contraseña de la copia");
  REQUIRE(abierta.resultado == AperturaSobre::abierto);
  REQUIRE(abierta.clave);
  CHECK(abierta.clave->huella() == clave->huella());
  const auto mal = ventrik::cliente::abrir_copia(*copia, "otra contraseña");
  CHECK(mal.resultado == AperturaSobre::contrasena);
  CHECK_FALSE(mal.clave);
}

TEST_CASE("Un archivo que no es una copia de identidad se rechaza") {
  const auto archivo = ventrik::cifrado::cerrar_sobre("VKSV", std::vector<std::uint8_t>{1, 2, 3}, "clave", kRapido);
  REQUIRE(archivo);
  CHECK(ventrik::cliente::abrir_copia(*archivo, "clave").resultado == AperturaSobre::danado);
  const auto sin_clave = ventrik::cifrado::cerrar_sobre("VKID", std::vector<std::uint8_t>{1, 3, 0, 9, 9, 9}, "clave", kRapido);
  REQUIRE(sin_clave);
  CHECK(ventrik::cliente::abrir_copia(*sin_clave, "clave").resultado == AperturaSobre::danado);
  CHECK_FALSE(ventrik::cliente::leer_contenido_de_copia(std::vector<std::uint8_t>{1, 0, 0}));
  CHECK_FALSE(ventrik::cliente::leer_contenido_de_copia(std::vector<std::uint8_t>{1, 0xFF, 0xFF}));
}

TEST_CASE("El secreto del equipo se crea una vez y no cambia") {
  const Carpeta carpeta;
  const auto primero = ventrik::cliente::secreto_de_equipo(carpeta.ruta());
  REQUIRE(primero);
  CHECK(primero->size() == ventrik::cliente::kBytesSecretoDeEquipo);
  const auto segundo = ventrik::cliente::secreto_de_equipo(carpeta.ruta());
  REQUIRE(segundo);
  CHECK(std::equal(primero->bytes().begin(), primero->bytes().end(), segundo->bytes().begin()));
  DeleteFileW(carpeta.archivo(L"equipo.dat").c_str());
  const auto tercero = ventrik::cliente::secreto_de_equipo(carpeta.ruta());
  REQUIRE(tercero);
  CHECK_FALSE(std::equal(primero->bytes().begin(), primero->bytes().end(), tercero->bytes().begin()));
}

TEST_CASE("Un secreto del equipo dañado no se pisa con otro") {
  const Carpeta carpeta;
  REQUIRE(ventrik::cliente::secreto_de_equipo(carpeta.ruta()));
  auto contenido = leer_todo(carpeta.archivo(L"equipo.dat"));
  contenido[contenido.size() / 2] ^= 0x01;
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.archivo(L"equipo.dat"), contenido));
  CHECK_FALSE(ventrik::cliente::secreto_de_equipo(carpeta.ruta()));
  CHECK(leer_todo(carpeta.archivo(L"equipo.dat")) == contenido);
}

TEST_CASE("La identidad que no se pudo abrir se aparta antes de guardar otra") {
  const Carpeta carpeta;
  const auto primera = Clave::crear_nueva();
  const auto segunda = Clave::crear_nueva();
  const auto tercera = Clave::crear_nueva();
  REQUIRE(primera);
  REQUIRE(segunda);
  REQUIRE(tercera);
  REQUIRE(ventrik::cliente::guardar_apartando_la_vieja(carpeta.ruta(), *primera));
  CHECK(GetFileAttributesW(carpeta.archivo(L"identidad-vieja-1.dat").c_str()) == INVALID_FILE_ATTRIBUTES);
  const auto vieja = leer_todo(carpeta.archivo(L"identidad.dat"));
  REQUIRE(ventrik::cliente::guardar_apartando_la_vieja(carpeta.ruta(), *segunda));
  CHECK(leer_todo(carpeta.archivo(L"identidad-vieja-1.dat")) == vieja);
  REQUIRE(ventrik::cliente::guardar_apartando_la_vieja(carpeta.ruta(), *tercera));
  CHECK(leer_todo(carpeta.archivo(L"identidad-vieja-1.dat")) == vieja);
  CHECK(GetFileAttributesW(carpeta.archivo(L"identidad-vieja-2.dat").c_str()) != INVALID_FILE_ATTRIBUTES);
  const auto cargada = ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta());
  REQUIRE(cargada.clave);
  CHECK(cargada.clave->huella() == tercera->huella());
}

TEST_CASE("La identidad corta son los primeros cuatro grupos de la huella") {
  const auto clave = Clave::crear_nueva();
  REQUIRE(clave);
  const std::string corta = ventrik::cliente::identidad_corta(clave->huella());
  CHECK(corta.size() == 19);
  CHECK(corta == clave->huella().a_texto().substr(0, 19));
  CHECK(corta[4] == '-');
  CHECK(corta[9] == '-');
  CHECK(corta[14] == '-');
}

TEST_CASE("Cuánto cuesta verificar la firma de una presentación") {
  const auto clave = Clave::crear_nueva();
  REQUIRE(clave);
  const std::vector<std::uint8_t> datos(220, 7);
  const auto firma = clave->firmar(datos);
  REQUIRE(firma);
  constexpr int kVeces = 10;
  const auto antes = std::chrono::steady_clock::now();
  for (int vez = 0; vez < kVeces; ++vez) {
    REQUIRE(ventrik::cifrado::verificar_firma(clave->publica(), datos, *firma));
  }
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - antes).count() / kVeces;
  MESSAGE("verificar una firma de identidad: " << ms << " ms");
  CHECK(ms < 500);
}
