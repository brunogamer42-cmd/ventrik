#include "common/protocol/hostear.h"

#include <algorithm>
#include <vector>

#include <windows.h>

#include <mbedtls/sha256.h>

#include "common/archivos.h"
#include "common/cifrado/sobre.h"
#include "common/texto.h"
#include "common/utf.h"

namespace ventrik::protocolo {
namespace {
constexpr std::uint8_t kVersionServidorNuevo = 1;
constexpr std::uint8_t kVersionEntrada = 1;
constexpr std::uint8_t kVersionPublicos = 1;

void escribir_secreto(Escritor& escritor, const cifrado::Secreto& secreto, std::size_t maximo) {
  if (!es_contrasena_valida(cifrado::texto_de(secreto), maximo)) {
    escritor.marcar_error();
    return;
  }
  escritor.escribir_u16(static_cast<std::uint16_t>(secreto.size()));
  escritor.escribir_bytes(secreto.bytes());
}

cifrado::Secreto leer_secreto(Lector& lector, std::size_t maximo) {
  const std::size_t largo = lector.leer_u16();
  if (largo > maximo) {
    lector.marcar_error();
    return {};
  }
  cifrado::Secreto secreto = cifrado::copiar(lector.leer_bytes(largo));
  if (lector.ok() && !es_contrasena_valida(cifrado::texto_de(secreto), maximo)) {
    lector.marcar_error();
  }
  return secreto;
}

bool es_arranque_valido(const cifrado::Secreto& arranque) {
  return arranque.size() == 0 || cantidad_de_letras(cifrado::texto_de(arranque)) >= cifrado::kMinimoContrasena;
}

std::wstring por_carpeta(const std::wstring& carpeta, std::wstring nombre) {
  std::wstring ruta = ruta_completa(carpeta);
  while (ruta.size() > 3 && (ruta.back() == L'\\' || ruta.back() == L'/')) {
    ruta.pop_back();
  }
  std::wstring mayusculas(ruta.size(), L'\0');
  const int hechas = ruta.empty() ? 0
                                  : LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, ruta.c_str(),
                                                 static_cast<int>(ruta.size()), &mayusculas[0],
                                                 static_cast<int>(mayusculas.size()));
  if (hechas == static_cast<int>(ruta.size())) {
    ruta = mayusculas;
  }
  std::array<unsigned char, 32> resumen{};
  mbedtls_sha256(reinterpret_cast<const unsigned char*>(ruta.data()), ruta.size() * sizeof(wchar_t), resumen.data(),
                 0);
  constexpr wchar_t kCifras[] = L"0123456789abcdef";
  for (std::size_t i = 0; i < 16; ++i) {
    nombre.push_back(kCifras[resumen[i] >> 4]);
    nombre.push_back(kCifras[resumen[i] & 0xF]);
  }
  return nombre;
}

template <typename Escribir>
cifrado::Secreto armar_secreto(Escribir&& escribir) {
  std::vector<std::uint8_t> lugar;
  lugar.reserve(kMaximoEntradaPorCano);
  Escritor escritor(std::move(lugar));
  escribir(escritor);
  const bool bien = escritor.ok();
  std::vector<std::uint8_t> datos = std::move(escritor).soltar();
  cifrado::Secreto armado =
      bien && datos.size() <= kMaximoEntradaPorCano ? cifrado::copiar(datos) : cifrado::Secreto();
  datos.resize(datos.capacity());
  mbedtls_platform_zeroize(datos.data(), datos.size());
  return armado;
}

}

bool es_contrasena_valida(std::string_view texto, std::size_t maximo) {
  return texto.size() <= maximo && es_utf8_valido(texto) &&
         std::none_of(texto.begin(), texto.end(),
                      [](char letra) { return static_cast<unsigned char>(letra) < 0x20 || letra == 0x7F; });
}

cifrado::Secreto armar_servidor_nuevo(const ServidorNuevo& nuevo) {
  return armar_secreto([&nuevo](Escritor& escritor) {
    if (!es_nombre_valido(nuevo.nombre) || !es_nombre_valido(nuevo.nombre_dueno) || !es_valida(nuevo.seguridad) ||
        !es_arranque_valido(nuevo.arranque)) {
      escritor.marcar_error();
      return;
    }
    escritor.escribir_u8(kVersionServidorNuevo);
    escritor.escribir_texto(nuevo.nombre, kMaximoBytesNombre);
    escritor.escribir_u8(static_cast<std::uint8_t>(nuevo.seguridad.nivel));
    escritor.escribir_bytes(nuevo.seguridad.opciones.valores);
    escritor.escribir_bytes(nuevo.dueno);
    escritor.escribir_texto(nuevo.nombre_dueno, kMaximoBytesNombre);
    escribir_secreto(escritor, nuevo.contrasena, kMaximoBytesContrasena);
    escribir_secreto(escritor, nuevo.arranque, kMaximoContrasenaDeArranque);
  });
}

std::optional<ServidorNuevo> leer_servidor_nuevo(gsl::span<const std::uint8_t> datos) {
  Lector lector(datos);
  ServidorNuevo nuevo;
  const std::uint8_t version = lector.leer_u8();
  nuevo.nombre = lector.leer_texto(kMaximoBytesNombre);
  nuevo.seguridad.nivel = static_cast<ventrik::Nivel>(lector.leer_u8());
  const auto valores = lector.leer_bytes(kOpciones);
  const auto dueno = lector.leer_bytes(nuevo.dueno.size());
  nuevo.nombre_dueno = lector.leer_texto(kMaximoBytesNombre);
  nuevo.contrasena = leer_secreto(lector, kMaximoBytesContrasena);
  nuevo.arranque = leer_secreto(lector, kMaximoContrasenaDeArranque);
  if (!lector.terminado() || version != kVersionServidorNuevo) {
    return std::nullopt;
  }
  std::copy(valores.begin(), valores.end(), nuevo.seguridad.opciones.valores.begin());
  std::copy(dueno.begin(), dueno.end(), nuevo.dueno.begin());
  if (!es_nombre_valido(nuevo.nombre) || !es_nombre_valido(nuevo.nombre_dueno) || !es_valida(nuevo.seguridad) ||
      nuevo.dueno[0] != 0x04 || !es_arranque_valido(nuevo.arranque)) {
    return std::nullopt;
  }
  return nuevo;
}

cifrado::Secreto armar_entrada(std::string_view arranque, std::string_view contrasena) {
  return armar_secreto([&](Escritor& escritor) {
    escritor.escribir_u8(kVersionEntrada);
    escribir_secreto(escritor, cifrado::secreto_de(arranque), kMaximoContrasenaDeArranque);
    escribir_secreto(escritor, cifrado::secreto_de(contrasena), kMaximoContrasenaDeArranque);
  });
}

std::optional<EntradaPorCano> leer_entrada(gsl::span<const std::uint8_t> datos) {
  Lector lector(datos);
  EntradaPorCano entrada;
  const std::uint8_t version = lector.leer_u8();
  entrada.arranque = leer_secreto(lector, kMaximoContrasenaDeArranque);
  entrada.contrasena = leer_secreto(lector, kMaximoContrasenaDeArranque);
  if (!lector.terminado() || version != kVersionEntrada) {
    return std::nullopt;
  }
  return entrada;
}

std::vector<std::uint8_t> armar_datos_publicos(const DatosPublicos& datos) {
  Escritor escritor;
  if (datos.puerto == 0 || !es_nombre_valido(datos.nombre)) {
    return {};
  }
  escritor.escribir_u8(kVersionPublicos);
  escritor.escribir_bytes(datos.huella);
  escritor.escribir_u16(datos.puerto);
  escritor.escribir_texto(datos.nombre, kMaximoBytesNombre);
  return escritor.ok() ? std::move(escritor).soltar() : std::vector<std::uint8_t>();
}

std::optional<DatosPublicos> leer_datos_publicos(gsl::span<const std::uint8_t> datos) {
  Lector lector(datos);
  DatosPublicos publicos;
  const std::uint8_t version = lector.leer_u8();
  const auto huella = lector.leer_bytes(publicos.huella.size());
  publicos.puerto = lector.leer_u16();
  publicos.nombre = lector.leer_texto(kMaximoBytesNombre);
  if (!lector.terminado() || version != kVersionPublicos || publicos.puerto == 0 ||
      !es_nombre_valido(publicos.nombre)) {
    return std::nullopt;
  }
  std::copy(huella.begin(), huella.end(), publicos.huella.begin());
  return publicos;
}

std::wstring evento_de(const std::wstring& carpeta) { return por_carpeta(carpeta, L"Local\\VentrikServidor-"); }

std::wstring candado_de(const std::wstring& carpeta) { return por_carpeta(carpeta, L"Local\\VentrikCandado-"); }

}
