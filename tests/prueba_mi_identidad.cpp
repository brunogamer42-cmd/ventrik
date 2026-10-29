#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
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
         {L"\\identidad.dat", L"\\equipo.dat", L"\\identidad-vieja-1.dat", L"\\identidad-vieja-2.dat", L"\\llaves.dat"}) {
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
  CHECK(leida.estado == EstadoIdentidad::lista);
  REQUIRE(leida.clave);
  CHECK(leida.clave->huella() == clave->huella());
  for (const std::size_t i : {std::size_t{0}, std::size_t{4}, std::size_t{5}, archivo->size() / 2, archivo->size() - 1}) {
    auto alterado = *archivo;
    alterado[i] ^= 0x01;
    const auto rota = ventrik::cliente::leer_archivo_de_identidad(alterado);
    CHECK(rota.estado == EstadoIdentidad::danada);
    CHECK_FALSE(rota.clave);
  }
  CHECK(ventrik::cliente::leer_archivo_de_identidad(gsl::span<const std::uint8_t>(*archivo).first(5)).estado ==
        EstadoIdentidad::danada);
}

TEST_CASE("Con contraseña maestra la identidad se abre solo con ella, y cambiarla o quitarla no la pierde") {
  const Carpeta carpeta;
  const auto creada = ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta());
  REQUIRE(creada.clave);
  const auto huella = creada.clave->huella();
  const auto der = der_de(*creada.clave);
  const auto primera = ventrik::cliente::nueva_maestra("maestra uno", kRapido);
  REQUIRE(primera);
  REQUIRE(ventrik::cliente::guardar_mi_identidad(carpeta.ruta(), *creada.clave, &*primera));

  const auto bloqueada = ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta());
  REQUIRE(bloqueada.estado == EstadoIdentidad::con_contrasena);
  CHECK_FALSE(bloqueada.clave);
  CHECK(std::search(bloqueada.cerrada.begin(), bloqueada.cerrada.end(), der.begin(), der.end()) ==
        bloqueada.cerrada.end());
  const auto mal = ventrik::cliente::abrir_con_maestra(bloqueada.cerrada, "maestra dos");
  CHECK(mal.resultado == AperturaSobre::contrasena);
  CHECK_FALSE(mal.clave);
  CHECK_FALSE(mal.llave);
  const auto abierta = ventrik::cliente::abrir_con_maestra(bloqueada.cerrada, "maestra uno");
  REQUIRE(abierta.resultado == AperturaSobre::abierto);
  REQUIRE(abierta.clave);
  REQUIRE(abierta.llave);
  CHECK(abierta.clave->huella() == huella);

  const auto otra_vez = ventrik::cliente::cerrar_con_maestra(*abierta.clave, *abierta.llave);
  REQUIRE(otra_vez);
  const auto reabierta = ventrik::cliente::abrir_con_maestra(*otra_vez, "maestra uno");
  REQUIRE(reabierta.clave);
  CHECK(reabierta.clave->huella() == huella);
  CHECK(ventrik::cliente::abrir_con_maestra(*otra_vez, "maestra dos").resultado == AperturaSobre::contrasena);

  const auto segunda = ventrik::cliente::nueva_maestra("maestra dos", kRapido);
  REQUIRE(segunda);
  REQUIRE(ventrik::cliente::guardar_mi_identidad(carpeta.ruta(), *abierta.clave, &*segunda));
  const auto cambiada = ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta());
  REQUIRE(cambiada.estado == EstadoIdentidad::con_contrasena);
  CHECK(ventrik::cliente::abrir_con_maestra(cambiada.cerrada, "maestra uno").resultado == AperturaSobre::contrasena);
  const auto con_la_nueva = ventrik::cliente::abrir_con_maestra(cambiada.cerrada, "maestra dos");
  REQUIRE(con_la_nueva.clave);
  CHECK(con_la_nueva.clave->huella() == huella);

  REQUIRE(ventrik::cliente::guardar_mi_identidad(carpeta.ruta(), *abierta.clave));
  const auto sin_maestra = ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta());
  REQUIRE(sin_maestra.estado == EstadoIdentidad::lista);
  REQUIRE(sin_maestra.clave);
  CHECK(sin_maestra.clave->huella() == huella);
  CHECK(sin_maestra.cerrada.empty());
}

TEST_CASE("La capa de la contraseña maestra alterada en un byte, o una copia en su lugar, no se abre") {
  const auto clave = Clave::crear_nueva();
  REQUIRE(clave);
  const auto maestra = ventrik::cliente::nueva_maestra("maestra", kRapido);
  REQUIRE(maestra);
  const auto cerrada = ventrik::cliente::cerrar_con_maestra(*clave, *maestra);
  REQUIRE(cerrada);
  for (std::size_t i = 0; i < cerrada->size(); i += 7) {
    auto alterada = *cerrada;
    alterada[i] ^= 0x01;
    const auto abierta = ventrik::cliente::abrir_con_maestra(alterada, "maestra");
    CHECK(abierta.resultado != AperturaSobre::abierto);
    CHECK_FALSE(abierta.clave);
  }
  const auto copia = ventrik::cliente::armar_copia(der_de(*clave), "maestra", kRapido);
  REQUIRE(copia);
  CHECK(ventrik::cliente::abrir_con_maestra(*copia, "maestra").resultado == AperturaSobre::danado);
  CHECK(ventrik::cliente::abrir_copia(*cerrada, "maestra").resultado == AperturaSobre::danado);
}

TEST_CASE("El bloqueo automático decide con el bloqueo de Windows o con los minutos sin usar la PC") {
  using ventrik::cliente::hay_que_bloquear;
  constexpr std::uint32_t kMinuto = 60000;
  CHECK_FALSE(hay_que_bloquear(0, true, 24 * 60 * kMinuto));
  CHECK(hay_que_bloquear(1, true, 0));
  CHECK_FALSE(hay_que_bloquear(1, false, 24 * 60 * kMinuto));
  for (const auto& [eleccion, minutos] : {std::make_pair(2, 5u), std::make_pair(3, 15u), std::make_pair(4, 60u)}) {
    const auto cual = static_cast<std::uint8_t>(eleccion);
    CHECK_FALSE(hay_que_bloquear(cual, false, minutos * kMinuto - 1));
    CHECK(hay_que_bloquear(cual, false, minutos * kMinuto));
    CHECK_FALSE(hay_que_bloquear(cual, true, 0));
  }
  CHECK_FALSE(hay_que_bloquear(5, true, 24 * 60 * kMinuto));
  CHECK_FALSE(hay_que_bloquear(255, true, 24 * 60 * kMinuto));
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

TEST_CASE("La copia guarda la generación de las llaves, y una copia sin generación también se abre") {
  const auto clave = Clave::crear_nueva();
  REQUIRE(clave);
  const auto der = der_de(*clave);
  const auto copia = ventrik::cliente::armar_copia(der, "contraseña de la copia", kRapido, 0x01020304);
  REQUIRE(copia);
  const auto abierta = ventrik::cliente::abrir_copia(*copia, "contraseña de la copia");
  REQUIRE(abierta.clave);
  CHECK(abierta.clave->huella() == clave->huella());
  CHECK(abierta.generacion == 0x01020304);
  std::vector<std::uint8_t> sin_generacion = {1, static_cast<std::uint8_t>(der.size() & 0xFF),
                                              static_cast<std::uint8_t>(der.size() >> 8)};
  sin_generacion.insert(sin_generacion.end(), der.begin(), der.end());
  std::uint32_t generacion = 9;
  REQUIRE(ventrik::cliente::leer_contenido_de_copia(sin_generacion, &generacion));
  CHECK(generacion == 9);
  for (const std::size_t cola : {1, 3, 5}) {
    auto con_cola = sin_generacion;
    con_cola.insert(con_cola.end(), cola, 7);
    CHECK_FALSE(ventrik::cliente::leer_contenido_de_copia(con_cola));
  }
}

TEST_CASE("El archivo de llaves se guarda con DPAPI, vuelve igual y uno dañado o con topes pasados no se lee") {
  const Carpeta carpeta;
  const auto identidad = Clave::crear_nueva();
  const auto llave = Clave::crear_nueva();
  REQUIRE(identidad);
  REQUIRE(llave);
  CHECK_FALSE(ventrik::cliente::cargar_mis_llaves(carpeta.ruta()).llaves);
  CHECK_FALSE(ventrik::cliente::cargar_mis_llaves(carpeta.ruta()).danadas);
  ventrik::cliente::MisLlaves mias{7, identidad->publica(), {}};
  for (std::size_t i = 0; i < ventrik::cliente::kMaximoLlaves; ++i) {
    mias.llaves.push_back({llave->publica(), std::vector<std::uint8_t>(i == 0 ? 1023 : i + 1, 5),
                           std::vector<std::uint8_t>(i == 0 ? 72 : 1, 6)});
  }
  REQUIRE(ventrik::cliente::guardar_mis_llaves(carpeta.ruta(), mias));
  const auto archivo = leer_todo(carpeta.archivo(L"llaves.dat"));
  const auto contenido = ventrik::cliente::armar_contenido_de_llaves(mias);
  REQUIRE(contenido);
  CHECK(std::search(archivo.begin(), archivo.end(), contenido->begin(), contenido->end()) == archivo.end());
  const auto cargadas = ventrik::cliente::cargar_mis_llaves(carpeta.ruta());
  REQUIRE(cargadas.llaves);
  CHECK(cargadas.llaves->generacion == 7);
  CHECK(cargadas.llaves->identidad == identidad->publica());
  REQUIRE(cargadas.llaves->llaves.size() == ventrik::cliente::kMaximoLlaves);
  CHECK(cargadas.llaves->llaves[0].id.size() == 1023);
  CHECK(cargadas.llaves->llaves[0].permiso.size() == 72);
  CHECK(cargadas.llaves->llaves[3].publica == llave->publica());

  for (const auto& mala : {std::function<void(ventrik::cliente::MisLlaves&)>(
                               [](auto& llaves) { llaves.llaves.push_back(llaves.llaves.back()); }),
                           std::function<void(ventrik::cliente::MisLlaves&)>(
                               [](auto& llaves) { llaves.llaves[1].id.assign(1024, 1); }),
                           std::function<void(ventrik::cliente::MisLlaves&)>(
                               [](auto& llaves) { llaves.llaves[1].id.clear(); }),
                           std::function<void(ventrik::cliente::MisLlaves&)>(
                               [](auto& llaves) { llaves.llaves[1].permiso.assign(73, 1); }),
                           std::function<void(ventrik::cliente::MisLlaves&)>(
                               [](auto& llaves) { llaves.identidad.fill(1); })}) {
    auto cambiadas = mias;
    mala(cambiadas);
    CHECK_FALSE(ventrik::cliente::armar_contenido_de_llaves(cambiadas));
  }
  const auto entero = *contenido;
  for (std::size_t largo = 0; largo < entero.size(); largo += 37) {
    CHECK_FALSE(ventrik::cliente::leer_contenido_de_llaves(gsl::span<const std::uint8_t>(entero).first(largo)));
  }
  auto de_mas = entero;
  de_mas.push_back(0);
  CHECK_FALSE(ventrik::cliente::leer_contenido_de_llaves(de_mas));
  auto con_cantidad_de_mas = entero;
  con_cantidad_de_mas[4 + 65] = static_cast<std::uint8_t>(ventrik::cliente::kMaximoLlaves + 1);
  CHECK_FALSE(ventrik::cliente::leer_contenido_de_llaves(con_cantidad_de_mas));
  auto llave_fuera = entero;
  llave_fuera[4 + 65 + 1 + 10] ^= 0x01;
  CHECK_FALSE(ventrik::cliente::leer_contenido_de_llaves(llave_fuera));

  auto danado = archivo;
  danado[danado.size() / 2] ^= 0x01;
  REQUIRE(ventrik::escribir_archivo_completo(carpeta.archivo(L"llaves.dat"), danado));
  const auto con_dano = ventrik::cliente::cargar_mis_llaves(carpeta.ruta());
  CHECK_FALSE(con_dano.llaves);
  CHECK(con_dano.danadas);
}

TEST_CASE("Borrar la identidad de la PC la borra junto con sus copias apartadas y no se crea otra sola") {
  const Carpeta carpeta;
  const auto mia = Clave::crear_nueva();
  const auto otra = Clave::crear_nueva();
  REQUIRE(mia);
  REQUIRE(otra);
  REQUIRE(ventrik::cliente::guardar_apartando_la_vieja(carpeta.ruta(), *mia));
  REQUIRE(ventrik::cliente::guardar_apartando_la_vieja(carpeta.ruta(), *otra));
  REQUIRE(ventrik::cliente::guardar_apartando_la_vieja(carpeta.ruta(), *mia));
  REQUIRE(GetFileAttributesW(carpeta.archivo(L"identidad-vieja-1.dat").c_str()) != INVALID_FILE_ATTRIBUTES);
  REQUIRE(GetFileAttributesW(carpeta.archivo(L"identidad-vieja-2.dat").c_str()) != INVALID_FILE_ATTRIBUTES);
  REQUIRE(ventrik::cliente::borrar_identidad_de_la_pc(carpeta.ruta(), mia->huella()));
  CHECK(GetFileAttributesW(carpeta.archivo(L"identidad.dat").c_str()) == INVALID_FILE_ATTRIBUTES);
  CHECK(GetFileAttributesW(carpeta.archivo(L"identidad-vieja-1.dat").c_str()) == INVALID_FILE_ATTRIBUTES);
  CHECK(GetFileAttributesW(carpeta.archivo(L"identidad-vieja-2.dat").c_str()) != INVALID_FILE_ATTRIBUTES);
  const auto sin_crear = ventrik::cliente::cargar_o_crear_mi_identidad(carpeta.ruta(), false);
  CHECK(sin_crear.estado == EstadoIdentidad::danada);
  CHECK_FALSE(sin_crear.clave);
  CHECK(GetFileAttributesW(carpeta.archivo(L"identidad.dat").c_str()) == INVALID_FILE_ATTRIBUTES);
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
