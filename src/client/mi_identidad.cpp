#include "client/mi_identidad.h"

#include <algorithm>
#include <array>

#include <windows.h>

#include <mbedtls/md.h>

#include "client/guardados.h"
#include "common/archivos.h"
#include "common/cifrado/dpapi.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/llave.h"
#include "common/protocol/bytes.h"

namespace ventrik::cliente {
namespace {

constexpr std::string_view kMarcaIdentidad = "VKMI";
constexpr std::string_view kMarcaCopia = "VKID";
constexpr std::string_view kMarcaEquipo = "VKEQ";
constexpr std::string_view kMarcaMaestra = "VKMM";
constexpr std::string_view kMarcaLlaves = "VKLL";
constexpr std::string_view kPropositoIdentidad = "ventrik identidad v1";
constexpr std::string_view kPropositoEquipo = "ventrik equipo v1";
constexpr std::string_view kPropositoLlaves = "ventrik llaves v1";
constexpr std::size_t kBytesGeneracion = 4;
constexpr std::uint8_t kVersion = 1;
constexpr std::uint8_t kSinContrasenaMaestra = 0;
constexpr std::uint8_t kConContrasenaMaestra = 1;
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

std::optional<cifrado::Secreto> empaquetar(std::uint8_t primero, gsl::span<const std::uint8_t> datos) {
  if (datos.empty() || datos.size() > kMaximoDer) {
    return std::nullopt;
  }
  cifrado::Secreto paquete(3 + datos.size());
  const auto destino = paquete.bytes();
  destino[0] = primero;
  destino[1] = static_cast<std::uint8_t>(datos.size() & 0xFF);
  destino[2] = static_cast<std::uint8_t>(datos.size() >> 8);
  std::copy(datos.begin(), datos.end(), destino.begin() + 3);
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

std::wstring apartar(const std::wstring& archivo, const std::wstring& carpeta, const wchar_t* nombre) {
  for (int numero = 1; numero < 100; ++numero) {
    const std::wstring candidata = carpeta + nombre + std::to_wstring(numero) + L".dat";
    if (MoveFileExW(archivo.c_str(), candidata.c_str(), MOVEFILE_WRITE_THROUGH)) {
      return candidata;
    }
  }
  return {};
}

std::optional<cifrado::Clave> clave_de(const std::optional<gsl::span<const std::uint8_t>>& der) {
  return der ? cifrado::Clave::desde_der(*der) : std::nullopt;
}

CopiaAbierta abrir(std::string_view marca, gsl::span<const std::uint8_t> sobre, std::string_view contrasena) {
  cifrado::SobreAbierto abierto = cifrado::abrir_sobre(marca, sobre, contrasena);
  CopiaAbierta resultado{abierto.resultado, std::nullopt, std::nullopt};
  if (abierto.contenido) {
    resultado.clave = leer_contenido_de_copia(abierto.contenido->bytes(), &resultado.generacion);
    resultado.resultado = resultado.clave ? cifrado::AperturaSobre::abierto : cifrado::AperturaSobre::danado;
  }
  if (resultado.clave) {
    resultado.llave = std::move(abierto.llave);
  }
  return resultado;
}

}  // namespace

MiIdentidad cargar_o_crear_mi_identidad(const std::wstring& carpeta, bool crear) {
  const std::wstring archivo = carpeta + L"\\identidad.dat";
  borrar_temporales_de(archivo);
  for (int intento = 0; intento < 2; ++intento) {
    cifrado::Secreto contenido;
    switch (leer_archivo(archivo, kMaximoArchivoIdentidad, contenido)) {
      case LecturaArchivo::leido:
        return leer_archivo_de_identidad(contenido.bytes());
      case LecturaArchivo::demasiado_grande:
        return {EstadoIdentidad::danada, {}, {}};
      case LecturaArchivo::error:
        return {EstadoIdentidad::error, {}, {}};
      case LecturaArchivo::no_existe:
        if (!crear) {
          return {EstadoIdentidad::danada, {}, {}};
        }
        break;
    }
    auto nueva = cifrado::Clave::crear_nueva();
    const auto armado = nueva ? armar_archivo_de_identidad(*nueva) : std::nullopt;
    const EscrituraNueva escrito =
        armado && crear_carpetas(carpeta) ? escribir_archivo_nuevo(archivo, *armado) : EscrituraNueva::error;
    if (escrito == EscrituraNueva::escrito) {
      return {EstadoIdentidad::creada, std::move(nueva), {}};
    }
    if (escrito == EscrituraNueva::error) {
      return {EstadoIdentidad::error, {}, {}};
    }
  }
  return {EstadoIdentidad::error, {}, {}};
}

bool guardar_mi_identidad(const std::wstring& carpeta, const cifrado::Clave& clave,
                          const cifrado::LlaveDeSobre* maestra) {
  const auto armado = armar_archivo_de_identidad(clave, maestra);
  return armado && crear_carpetas(carpeta) && escribir_archivo_completo(carpeta + L"\\identidad.dat", *armado);
}

bool borrar_identidad_de_la_pc(const std::wstring& carpeta, const cifrado::Huella& huella) {
  for (int numero = 1; numero < 100; ++numero) {
    const std::wstring vieja = carpeta + L"\\identidad-vieja-" + std::to_wstring(numero) + L".dat";
    cifrado::Secreto contenido;
    if (leer_archivo(vieja, kMaximoArchivoIdentidad, contenido) == LecturaArchivo::leido) {
      const MiIdentidad leida = leer_archivo_de_identidad(contenido.bytes());
      if (leida.clave && leida.clave->huella() == huella) {
        borrar_con_ceros(vieja);
      }
    }
  }
  const std::wstring archivo = carpeta + L"\\identidad.dat";
  borrar_temporales_de(archivo);
  return borrar_con_ceros(archivo);
}

std::optional<std::vector<std::uint8_t>> armar_archivo_de_identidad(const cifrado::Clave& clave,
                                                                    const cifrado::LlaveDeSobre* maestra) {
  std::optional<cifrado::Secreto> paquete;
  if (maestra != nullptr) {
    const auto cerrada = cerrar_con_maestra(clave, *maestra);
    paquete = cerrada ? empaquetar(kConContrasenaMaestra, *cerrada) : std::nullopt;
  } else if (const auto der = clave.der()) {
    paquete = empaquetar(kSinContrasenaMaestra, der->bytes());
  }
  const auto protegido = paquete ? cifrado::proteger_con_windows(paquete->bytes(), kPropositoIdentidad) : std::nullopt;
  return protegido ? std::optional<std::vector<std::uint8_t>>(con_marca(kMarcaIdentidad, *protegido)) : std::nullopt;
}

MiIdentidad leer_archivo_de_identidad(gsl::span<const std::uint8_t> archivo) {
  const auto cuerpo = sin_marca(kMarcaIdentidad, archivo);
  const auto abierto = cuerpo ? cifrado::abrir_con_windows(*cuerpo, kPropositoIdentidad) : std::nullopt;
  const std::uint8_t tipo = abierto && abierto->size() > 0 ? abierto->bytes()[0] : kSinContrasenaMaestra;
  const auto contenido = abierto ? desempaquetar(tipo, abierto->bytes()) : std::nullopt;
  if (contenido && tipo == kConContrasenaMaestra && cifrado::leer_cabecera_sobre(kMarcaMaestra, *contenido)) {
    return {EstadoIdentidad::con_contrasena, {}, std::vector<std::uint8_t>(contenido->begin(), contenido->end())};
  }
  auto clave = tipo == kSinContrasenaMaestra ? clave_de(contenido) : std::nullopt;
  return {clave ? EstadoIdentidad::lista : EstadoIdentidad::danada, std::move(clave), {}};
}

std::optional<std::vector<std::uint8_t>> armar_copia(gsl::span<const std::uint8_t> der, std::string_view contrasena,
                                                     std::optional<cifrado::Argon2> fijos, std::uint32_t generacion) {
  const auto paquete = empaquetar(kVersion, der);
  if (!paquete) {
    return std::nullopt;
  }
  cifrado::Secreto con_generacion(paquete->size() + kBytesGeneracion);
  const auto destino = con_generacion.bytes();
  std::copy(paquete->bytes().begin(), paquete->bytes().end(), destino.begin());
  for (std::size_t i = 0; i < kBytesGeneracion; ++i) {
    destino[paquete->size() + i] = static_cast<std::uint8_t>(generacion >> (8 * i));
  }
  return cifrado::cerrar_sobre(kMarcaCopia, con_generacion.bytes(), contrasena, fijos);
}

CopiaAbierta abrir_copia(gsl::span<const std::uint8_t> copia, std::string_view contrasena) {
  return abrir(kMarcaCopia, copia, contrasena);
}

std::optional<cifrado::Clave> leer_contenido_de_copia(gsl::span<const std::uint8_t> contenido,
                                                      std::uint32_t* generacion) {
  const bool con_generacion = !desempaquetar(kVersion, contenido) && contenido.size() > kBytesGeneracion;
  const auto der =
      desempaquetar(kVersion, con_generacion ? contenido.first(contenido.size() - kBytesGeneracion) : contenido);
  auto clave = clave_de(der);
  if (clave && con_generacion && generacion != nullptr) {
    protocolo::Lector cola(contenido.last(kBytesGeneracion));
    *generacion = cola.leer_u32();
  }
  return clave;
}

std::optional<std::vector<std::uint8_t>> armar_contenido_de_llaves(const MisLlaves& llaves) {
  if (llaves.llaves.size() > kMaximoLlaves || !cifrado::huella_de_publica(llaves.identidad)) {
    return std::nullopt;
  }
  protocolo::Escritor escritor;
  escritor.escribir_u32(llaves.generacion);
  escritor.escribir_bytes(llaves.identidad);
  escritor.escribir_u8(static_cast<std::uint8_t>(llaves.llaves.size()));
  for (const LlaveRegistrada& llave : llaves.llaves) {
    if (llave.id.empty() || llave.id.size() > cifrado::kMaximoIdDeLlave || llave.permiso.empty() ||
        llave.permiso.size() > cifrado::kMaximoFirma) {
      return std::nullopt;
    }
    escritor.escribir_bytes(llave.publica);
    escritor.escribir_u16(static_cast<std::uint16_t>(llave.id.size()));
    escritor.escribir_bytes(llave.id);
    escritor.escribir_u8(static_cast<std::uint8_t>(llave.permiso.size()));
    escritor.escribir_bytes(llave.permiso);
  }
  return escritor.ok() ? std::optional<std::vector<std::uint8_t>>(std::move(escritor).soltar()) : std::nullopt;
}

std::optional<MisLlaves> leer_contenido_de_llaves(gsl::span<const std::uint8_t> contenido) {
  protocolo::Lector lector(contenido);
  MisLlaves leidas;
  leidas.generacion = lector.leer_u32();
  const auto identidad = lector.leer_bytes(leidas.identidad.size());
  std::copy(identidad.begin(), identidad.end(), leidas.identidad.begin());
  const std::size_t cantidad = lector.leer_u8();
  for (std::size_t i = 0; i < cantidad && cantidad <= kMaximoLlaves && lector.ok(); ++i) {
    LlaveRegistrada llave;
    const auto publica = lector.leer_bytes(llave.publica.size());
    std::copy(publica.begin(), publica.end(), llave.publica.begin());
    const std::size_t largo_id = lector.leer_u16();
    const auto id = lector.leer_bytes(largo_id);
    const std::size_t largo_permiso = lector.leer_u8();
    const auto permiso = lector.leer_bytes(largo_permiso);
    if (largo_id == 0 || largo_id > cifrado::kMaximoIdDeLlave || largo_permiso == 0 ||
        largo_permiso > cifrado::kMaximoFirma || !cifrado::huella_de_publica(llave.publica)) {
      return std::nullopt;
    }
    llave.id.assign(id.begin(), id.end());
    llave.permiso.assign(permiso.begin(), permiso.end());
    leidas.llaves.push_back(std::move(llave));
  }
  if (!lector.terminado() || cantidad > kMaximoLlaves || !cifrado::huella_de_publica(leidas.identidad)) {
    return std::nullopt;
  }
  return leidas;
}

std::optional<MisLlaves> leer_archivo_de_llaves(gsl::span<const std::uint8_t> archivo) {
  const auto cuerpo = sin_marca(kMarcaLlaves, archivo);
  const auto abierto = cuerpo ? cifrado::abrir_con_windows(*cuerpo, kPropositoLlaves) : std::nullopt;
  return abierto ? leer_contenido_de_llaves(abierto->bytes()) : std::nullopt;
}

LlavesCargadas cargar_mis_llaves(const std::wstring& carpeta) {
  const std::wstring archivo = carpeta + L"\\llaves.dat";
  borrar_temporales_de(archivo);
  cifrado::Secreto contenido;
  const LecturaArchivo lectura = leer_archivo(archivo, kMaximoArchivoIdentidad, contenido);
  if (lectura == LecturaArchivo::no_existe) {
    return {};
  }
  auto llaves = lectura == LecturaArchivo::leido ? leer_archivo_de_llaves(contenido.bytes()) : std::nullopt;
  const bool danadas = !llaves;
  return {std::move(llaves), danadas};
}

bool guardar_mis_llaves(const std::wstring& carpeta, const MisLlaves& llaves) {
  const auto contenido = armar_contenido_de_llaves(llaves);
  const auto protegido = contenido ? cifrado::proteger_con_windows(*contenido, kPropositoLlaves) : std::nullopt;
  return protegido && crear_carpetas(carpeta) &&
         escribir_archivo_completo(carpeta + L"\\llaves.dat", con_marca(kMarcaLlaves, *protegido));
}

std::optional<cifrado::LlaveDeSobre> nueva_maestra(std::string_view contrasena, std::optional<cifrado::Argon2> fijos) {
  return cifrado::llave_nueva(kMarcaMaestra, contrasena, fijos);
}

std::optional<std::vector<std::uint8_t>> cerrar_con_maestra(const cifrado::Clave& clave,
                                                            const cifrado::LlaveDeSobre& maestra) {
  const auto der = clave.der();
  const auto paquete = der ? empaquetar(kVersion, der->bytes()) : std::nullopt;
  return paquete ? cifrado::cerrar_con_llave(maestra, paquete->bytes()) : std::nullopt;
}

CopiaAbierta abrir_con_maestra(gsl::span<const std::uint8_t> cerrada, std::string_view contrasena) {
  return abrir(kMarcaMaestra, cerrada, contrasena);
}

bool hay_que_bloquear(std::uint8_t eleccion, bool windows_bloqueado, std::uint32_t sin_usar_ms) {
  if (eleccion == kBloqueoConWindows) {
    return windows_bloqueado;
  }
  return eleccion < kMinutosDeBloqueo.size() && kMinutosDeBloqueo[eleccion] != 0 &&
         sin_usar_ms / 60000 >= kMinutosDeBloqueo[eleccion];
}

bool reloj_adelantado(std::uint64_t ahora_ms, std::uint64_t servidor_ms) {
  return cifrado::generacion_de_la_hora(ahora_ms) > cifrado::generacion_tope(servidor_ms);
}

std::optional<cifrado::Secreto> secreto_de_equipo(const std::wstring& carpeta) {
  const std::wstring archivo = carpeta + L"\\equipo.dat";
  borrar_temporales_de(archivo);
  for (int intento = 0; intento < 2; ++intento) {
    cifrado::Secreto contenido;
    const LecturaArchivo lectura = leer_archivo(archivo, kMaximoArchivoIdentidad, contenido);
    auto leido = lectura == LecturaArchivo::leido ? leer_archivo_de_equipo(contenido.bytes()) : std::nullopt;
    if (leido || lectura == LecturaArchivo::error ||
        (lectura != LecturaArchivo::no_existe && apartar(archivo, carpeta, L"\\equipo-viejo-").empty())) {
      return leido;
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
    vieja = apartar(archivo, carpeta, L"\\identidad-vieja-");
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

std::array<std::uint8_t, kBytesSecretoDeEquipo> codigo_de_equipo(const cifrado::Secreto& secreto,
                                                                 const cifrado::Huella& huella) {
  std::array<std::uint8_t, kBytesSecretoDeEquipo> codigo{};
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), secreto.bytes().data(), secreto.size(),
                      huella.bytes().data(), huella.bytes().size(), codigo.data()) != 0) {
    codigo.fill(0);
  }
  return codigo;
}

}  // namespace ventrik::cliente
