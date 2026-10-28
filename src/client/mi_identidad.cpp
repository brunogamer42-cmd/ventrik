#include "client/mi_identidad.h"

#include <algorithm>
#include <array>

#include <windows.h>

#include "common/archivos.h"
#include "common/cifrado/dpapi.h"
#include "common/cifrado/inicio.h"
#include "common/protocol/bytes.h"

namespace ventrik::cliente {
namespace {

constexpr std::string_view kMarcaIdentidad = "VKMI";
constexpr std::string_view kMarcaCopia = "VKID";
constexpr std::string_view kMarcaEquipo = "VKEQ";
constexpr std::string_view kPropositoIdentidad = "ventrik identidad v1";
constexpr std::string_view kPropositoEquipo = "ventrik equipo v1";
constexpr std::uint8_t kVersion = 1;
constexpr std::uint8_t kSinContrasenaMaestra = 0;
constexpr std::size_t kMaximoDer = 1024;
constexpr std::size_t kLargoCorto = 19;

gsl::span<const std::uint8_t> bytes_de(std::string_view texto) {
  return gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(texto.data()), texto.size());
}

std::vector<std::uint8_t> con_marca(std::string_view marca, gsl::span<const std::uint8_t> cuerpo) {
  protocolo::Escritor escritor;
  escritor.escribir_bytes(bytes_de(marca));
  escritor.escribir_u8(kVersion);
  escritor.escribir_bytes(cuerpo);
  return std::move(escritor).soltar();
}

std::optional<gsl::span<const std::uint8_t>> sin_marca(std::string_view marca, gsl::span<const std::uint8_t> archivo) {
  protocolo::Lector lector(archivo);
  const auto leida = lector.leer_bytes(marca.size());
  const bool misma = lector.ok() && std::equal(leida.begin(), leida.end(), bytes_de(marca).begin());
  const std::uint8_t version = lector.leer_u8();
  const auto cuerpo = lector.leer_bytes(lector.restante());
  if (!lector.terminado() || !misma || version != kVersion || cuerpo.empty()) {
    return std::nullopt;
  }
  return cuerpo;
}

std::optional<cifrado::Secreto> empaquetar(std::uint8_t primero, const std::optional<cifrado::Secreto>& der) {
  if (!der || der->size() == 0 || der->size() > kMaximoDer) {
    return std::nullopt;
  }
  cifrado::Secreto paquete(3 + der->size());
  const auto destino = paquete.bytes();
  destino[0] = primero;
  destino[1] = static_cast<std::uint8_t>(der->size() & 0xFF);
  destino[2] = static_cast<std::uint8_t>(der->size() >> 8);
  std::copy(der->bytes().begin(), der->bytes().end(), destino.begin() + 3);
  return paquete;
}

std::optional<gsl::span<const std::uint8_t>> desempaquetar(std::uint8_t primero, gsl::span<const std::uint8_t> paquete) {
  protocolo::Lector lector(paquete);
  const std::uint8_t leido = lector.leer_u8();
  const std::size_t largo = lector.leer_u16();
  const auto der = lector.leer_bytes(largo);
  if (!lector.terminado() || leido != primero || largo == 0 || largo > kMaximoDer) {
    return std::nullopt;
  }
  return der;
}

std::optional<cifrado::Clave> clave_de(const std::optional<gsl::span<const std::uint8_t>>& der) {
  return der ? cifrado::Clave::desde_der(*der) : std::nullopt;
}

}  // namespace

MiIdentidad cargar_o_crear_mi_identidad(const std::wstring& carpeta) {
  const std::wstring archivo = carpeta + L"\\identidad.dat";
  borrar_temporales_de(archivo);
  for (int intento = 0; intento < 2; ++intento) {
    cifrado::Secreto contenido;
    switch (leer_archivo(archivo, kMaximoArchivoIdentidad, contenido)) {
      case LecturaArchivo::leido: {
        auto clave = leer_archivo_de_identidad(contenido.bytes());
        return clave ? MiIdentidad{EstadoIdentidad::lista, std::move(clave)} : MiIdentidad{EstadoIdentidad::danada, {}};
      }
      case LecturaArchivo::demasiado_grande:
        return {EstadoIdentidad::danada, {}};
      case LecturaArchivo::error:
        return {EstadoIdentidad::error, {}};
      case LecturaArchivo::no_existe:
        break;
    }
    auto nueva = cifrado::Clave::crear_nueva();
    const auto armado = nueva ? armar_archivo_de_identidad(*nueva) : std::nullopt;
    const EscrituraNueva escrito =
        armado && crear_carpetas(carpeta) ? escribir_archivo_nuevo(archivo, *armado) : EscrituraNueva::error;
    if (escrito == EscrituraNueva::escrito) {
      return {EstadoIdentidad::creada, std::move(nueva)};
    }
    if (escrito == EscrituraNueva::error) {
      return {EstadoIdentidad::error, {}};
    }
  }
  return {EstadoIdentidad::error, {}};
}

bool guardar_mi_identidad(const std::wstring& carpeta, const cifrado::Clave& clave) {
  const auto armado = armar_archivo_de_identidad(clave);
  return armado && crear_carpetas(carpeta) && escribir_archivo_completo(carpeta + L"\\identidad.dat", *armado);
}

std::optional<std::vector<std::uint8_t>> armar_archivo_de_identidad(const cifrado::Clave& clave) {
  const auto paquete = empaquetar(kSinContrasenaMaestra, clave.der());
  const auto protegido = paquete ? cifrado::proteger_con_windows(paquete->bytes(), kPropositoIdentidad) : std::nullopt;
  return protegido ? std::optional<std::vector<std::uint8_t>>(con_marca(kMarcaIdentidad, *protegido)) : std::nullopt;
}

std::optional<cifrado::Clave> leer_archivo_de_identidad(gsl::span<const std::uint8_t> archivo) {
  const auto cuerpo = sin_marca(kMarcaIdentidad, archivo);
  const auto abierto = cuerpo ? cifrado::abrir_con_windows(*cuerpo, kPropositoIdentidad) : std::nullopt;
  return abierto ? clave_de(desempaquetar(kSinContrasenaMaestra, abierto->bytes())) : std::nullopt;
}

std::optional<std::vector<std::uint8_t>> armar_copia(gsl::span<const std::uint8_t> der, std::string_view contrasena,
                                                     std::optional<cifrado::Argon2> fijos) {
  cifrado::Secreto copia_der(der.size());
  std::copy(der.begin(), der.end(), copia_der.bytes().begin());
  const auto paquete = empaquetar(kVersion, std::optional<cifrado::Secreto>(std::move(copia_der)));
  return paquete ? cifrado::cerrar_sobre(kMarcaCopia, paquete->bytes(), contrasena, fijos) : std::nullopt;
}

CopiaAbierta abrir_copia(gsl::span<const std::uint8_t> copia, std::string_view contrasena) {
  cifrado::SobreAbierto abierto = cifrado::abrir_sobre(kMarcaCopia, copia, contrasena);
  CopiaAbierta resultado{abierto.resultado, std::nullopt};
  if (abierto.contenido) {
    resultado.clave = leer_contenido_de_copia(abierto.contenido->bytes());
    resultado.resultado = resultado.clave ? cifrado::AperturaSobre::abierto : cifrado::AperturaSobre::danado;
  }
  return resultado;
}

std::optional<cifrado::Clave> leer_contenido_de_copia(gsl::span<const std::uint8_t> contenido) {
  return clave_de(desempaquetar(kVersion, contenido));
}

std::optional<cifrado::Secreto> secreto_de_equipo(const std::wstring& carpeta) {
  const std::wstring archivo = carpeta + L"\\equipo.dat";
  borrar_temporales_de(archivo);
  for (int intento = 0; intento < 2; ++intento) {
    cifrado::Secreto contenido;
    const LecturaArchivo lectura = leer_archivo(archivo, kMaximoArchivoIdentidad, contenido);
    if (lectura != LecturaArchivo::no_existe) {
      return lectura == LecturaArchivo::leido ? leer_archivo_de_equipo(contenido.bytes()) : std::nullopt;
    }
    cifrado::Secreto nuevo(kBytesSecretoDeEquipo);
    const bool hecho = cifrado::aleatorio(nullptr, nuevo.bytes().data(), nuevo.size()) == 0;
    const auto protegido = hecho ? cifrado::proteger_con_windows(nuevo.bytes(), kPropositoEquipo) : std::nullopt;
    const EscrituraNueva escrito = protegido && crear_carpetas(carpeta)
                                       ? escribir_archivo_nuevo(archivo, con_marca(kMarcaEquipo, *protegido))
                                       : EscrituraNueva::error;
    if (escrito != EscrituraNueva::ya_existe) {
      return escrito == EscrituraNueva::escrito ? std::optional<cifrado::Secreto>(std::move(nuevo)) : std::nullopt;
    }
  }
  return std::nullopt;
}

bool guardar_apartando_la_vieja(const std::wstring& carpeta, const cifrado::Clave& clave) {
  const std::wstring archivo = carpeta + L"\\identidad.dat";
  std::wstring vieja;
  if (GetFileAttributesW(archivo.c_str()) != INVALID_FILE_ATTRIBUTES) {
    for (int numero = 1; numero < 100 && vieja.empty(); ++numero) {
      const std::wstring candidata = carpeta + L"\\identidad-vieja-" + std::to_wstring(numero) + L".dat";
      if (MoveFileExW(archivo.c_str(), candidata.c_str(), MOVEFILE_WRITE_THROUGH)) {
        vieja = candidata;
      }
    }
    if (vieja.empty()) {
      return false;
    }
  } else if (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND) {
    return false;
  }
  if (guardar_mi_identidad(carpeta, clave)) {
    return true;
  }
  if (!vieja.empty()) {
    MoveFileExW(vieja.c_str(), archivo.c_str(), MOVEFILE_WRITE_THROUGH);
  }
  return false;
}

std::optional<cifrado::Secreto> leer_archivo_de_equipo(gsl::span<const std::uint8_t> archivo) {
  const auto cuerpo = sin_marca(kMarcaEquipo, archivo);
  auto abierto = cuerpo ? cifrado::abrir_con_windows(*cuerpo, kPropositoEquipo) : std::nullopt;
  return abierto && abierto->size() == kBytesSecretoDeEquipo ? std::move(abierto) : std::nullopt;
}

std::string identidad_corta(const cifrado::Huella& huella) {
  return huella.a_texto().substr(0, kLargoCorto);
}

}  // namespace ventrik::cliente
