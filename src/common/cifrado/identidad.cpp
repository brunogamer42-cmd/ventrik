#include "common/cifrado/identidad.h"

#include <algorithm>
#include <array>
#include <utility>

#include <windows.h>

#include <mbedtls/ecp.h>
#include <mbedtls/x509_crt.h>

#include "common/archivos.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/sobre.h"

namespace ventrik::cifrado {

struct IdentidadServidor::Datos {
  explicit Datos(Clave nueva) : clave(std::move(nueva)) { mbedtls_x509_crt_init(&certificado); }
  ~Datos() { mbedtls_x509_crt_free(&certificado); }
  Datos(const Datos&) = delete;
  Datos& operator=(const Datos&) = delete;

  Clave clave;
  mbedtls_x509_crt certificado;
};

namespace {

bool armar_certificado(mbedtls_pk_context& clave, mbedtls_x509_crt& certificado) {
  mbedtls_x509write_cert escritor;
  mbedtls_x509write_crt_init(&escritor);
  const auto liberar = gsl::finally([&escritor] { mbedtls_x509write_crt_free(&escritor); });

  std::array<unsigned char, 16> serie{};
  if (aleatorio(nullptr, serie.data(), serie.size()) != 0) {
    return false;
  }
  serie[0] = static_cast<unsigned char>((serie[0] & 0x7F) | 0x40);

  mbedtls_x509write_crt_set_version(&escritor, MBEDTLS_X509_CRT_VERSION_3);
  mbedtls_x509write_crt_set_md_alg(&escritor, MBEDTLS_MD_SHA256);
  mbedtls_x509write_crt_set_subject_key(&escritor, &clave);
  mbedtls_x509write_crt_set_issuer_key(&escritor, &clave);
  if (mbedtls_x509write_crt_set_subject_name(&escritor, "CN=Ventrik") != 0 ||
      mbedtls_x509write_crt_set_issuer_name(&escritor, "CN=Ventrik") != 0 ||
      mbedtls_x509write_crt_set_serial_raw(&escritor, serie.data(), serie.size()) != 0 ||
      mbedtls_x509write_crt_set_validity(&escritor, "20000101000000", "20991231235959") != 0) {
    return false;
  }
  std::array<unsigned char, 2048> der{};
  const int largo = mbedtls_x509write_crt_der(&escritor, der.data(), der.size(), aleatorio, nullptr);
  if (largo <= 0 || static_cast<std::size_t>(largo) > der.size()) {
    return false;
  }
  const auto escrito = gsl::span<const unsigned char>(der).last(static_cast<std::size_t>(largo));
  return mbedtls_x509_crt_parse_der(&certificado, escrito.data(), escrito.size()) == 0;
}

}

IdentidadServidor::IdentidadServidor(std::unique_ptr<Datos> datos) : datos_(std::move(datos)) {}
IdentidadServidor::IdentidadServidor(IdentidadServidor&&) noexcept = default;
IdentidadServidor& IdentidadServidor::operator=(IdentidadServidor&&) noexcept = default;
IdentidadServidor::~IdentidadServidor() = default;

std::optional<IdentidadServidor> IdentidadServidor::completar(std::optional<Clave> clave) {
  if (!clave) {
    return std::nullopt;
  }
  auto datos = std::make_unique<Datos>(std::move(*clave));
  if (!armar_certificado(*datos->clave.contexto(), datos->certificado)) {
    return std::nullopt;
  }
  return IdentidadServidor(std::move(datos));
}

std::optional<IdentidadServidor> IdentidadServidor::crear_nueva() {
  return completar(Clave::crear_nueva());
}

std::optional<IdentidadServidor> IdentidadServidor::desde_der(gsl::span<const std::uint8_t> der) {
  return completar(Clave::desde_der(der));
}

std::optional<Secreto> IdentidadServidor::clave_der() const {
  return datos_->clave.der();
}

const Huella& IdentidadServidor::huella() const {
  return datos_->clave.huella();
}

mbedtls_pk_context* IdentidadServidor::clave() {
  return datos_->clave.contexto();
}

mbedtls_x509_crt* IdentidadServidor::certificado() {
  return &datos_->certificado;
}

namespace {

constexpr std::array<std::uint8_t, 5> kCabeceraClave = {'V', 'K', 'C', 'S', 1};

struct Leido {
  std::wstring ruta;
  LecturaArchivo lectura = LecturaArchivo::error;
  Secreto contenido;
  bool existe() const { return lectura != LecturaArchivo::no_existe; }
};

Leido leer(std::wstring ruta, std::size_t maximo) {
  borrar_temporales_de(ruta);
  Leido leido;
  leido.lectura = leer_archivo(ruta, maximo, leido.contenido);
  leido.ruta = std::move(ruta);
  return leido;
}

IdentidadCargada fallo(ResultadoIdentidad resultado, const std::wstring& archivo) {
  IdentidadCargada cargada;
  cargada.resultado = resultado;
  cargada.archivo = archivo;
  return cargada;
}

ResultadoIdentidad resultado_de(AperturaBoveda apertura) {
  switch (apertura) {
    case AperturaBoveda::falta_contrasena: return ResultadoIdentidad::falta_contrasena;
    case AperturaBoveda::contrasena: return ResultadoIdentidad::contrasena_incorrecta;
    case AperturaBoveda::sin_memoria: return ResultadoIdentidad::sin_memoria;
    case AperturaBoveda::otro_windows: return ResultadoIdentidad::boveda_de_otro_windows;
    case AperturaBoveda::abierta:
    case AperturaBoveda::danada: break;
  }
  return ResultadoIdentidad::boveda_danada;
}

bool misma_clave(const std::optional<IdentidadServidor>& una, const IdentidadServidor& otra) {
  return una && una->huella() == otra.huella();
}

}

std::optional<std::vector<std::uint8_t>> cerrar_clave(const IdentidadServidor& identidad, const Boveda& boveda) {
  const auto der = identidad.clave_der();
  const auto clave = boveda.derivar(kEtiquetaClaveServidor);
  auto sellado = der && clave ? sellar(clave->bytes(), kCabeceraClave, der->bytes()) : std::nullopt;
  if (sellado) {
    sellado->insert(sellado->begin(), kCabeceraClave.begin(), kCabeceraClave.end());
  }
  return sellado;
}

std::optional<IdentidadServidor> abrir_clave(gsl::span<const std::uint8_t> archivo, const Boveda& boveda) {
  const auto clave = boveda.derivar(kEtiquetaClaveServidor);
  if (!clave || archivo.size() < kCabeceraClave.size() ||
      !std::equal(kCabeceraClave.begin(), kCabeceraClave.end(), archivo.begin())) {
    return std::nullopt;
  }
  const auto der =
      abrir_sellado(clave->bytes(), archivo.first(kCabeceraClave.size()), archivo.subspan(kCabeceraClave.size()));
  return der ? IdentidadServidor::desde_der(der->bytes()) : std::nullopt;
}

IdentidadCargada cargar_o_crear_identidad(const std::wstring& carpeta, std::optional<std::string_view> contrasena,
                                          bool base_con_dueno) {
  const Leido boveda = leer(carpeta + kArchivoBoveda, kMaximoArchivoBoveda);
  const Leido clave = leer(carpeta + kArchivoClave, kMaximoArchivoClave);
  const Leido vieja = leer(carpeta + kArchivoClaveVieja, kMaximoArchivoClave);
  for (const Leido* leido : {&boveda, &clave, &vieja}) {
    if (leido->lectura == LecturaArchivo::error) {
      return fallo(ResultadoIdentidad::error_lectura, leido->ruta);
    }
  }
  IdentidadCargada cargada;
  if (boveda.existe()) {
    BovedaAbierta abierta = abrir_boveda(boveda.contenido.bytes(), contrasena);
    const ResultadoIdentidad resultado = resultado_de(abierta.resultado);
    if (!abierta.boveda) {
      const bool del_archivo =
          resultado == ResultadoIdentidad::boveda_danada || resultado == ResultadoIdentidad::boveda_de_otro_windows;
      return fallo(resultado, del_archivo ? boveda.ruta : std::wstring());
    }
    cargada.boveda = std::move(abierta.boveda);
  } else if (clave.existe()) {
    return fallo(ResultadoIdentidad::falta_boveda, boveda.ruta);
  } else if (!vieja.existe() && base_con_dueno) {
    return fallo(ResultadoIdentidad::falta_clave, clave.ruta);
  } else {
    cargada.boveda = Boveda::crear_nueva();
    const auto archivo = cargada.boveda ? cargada.boveda->armar_archivo(std::nullopt) : std::nullopt;
    if (!archivo || escribir_archivo_nuevo(boveda.ruta, *archivo) != EscrituraNueva::escrito) {
      return fallo(ResultadoIdentidad::error_escritura, boveda.ruta);
    }
  }
  if (clave.existe()) {
    cargada.identidad = abrir_clave(clave.contenido.bytes(), *cargada.boveda);
    if (!cargada.identidad) {
      return fallo(ResultadoIdentidad::archivo_invalido, clave.ruta);
    }
    if (vieja.existe() && !misma_clave(IdentidadServidor::desde_der(vieja.contenido.bytes()), *cargada.identidad)) {
      return fallo(ResultadoIdentidad::claves_distintas, vieja.ruta);
    }
    if (vieja.existe()) {
      borrar_con_ceros(vieja.ruta);
    }
    cargada.resultado = ResultadoIdentidad::cargada;
    return cargada;
  }
  if (boveda.existe() && !vieja.existe()) {
    return fallo(ResultadoIdentidad::falta_clave, clave.ruta);
  }
  const auto fallar = [&boveda](ResultadoIdentidad resultado, const std::wstring& archivo) {
    if (!boveda.existe()) {
      DeleteFileW(boveda.ruta.c_str());
    }
    return fallo(resultado, archivo);
  };
  cargada.identidad = vieja.existe() ? IdentidadServidor::desde_der(vieja.contenido.bytes()) : IdentidadServidor::crear_nueva();
  if (!cargada.identidad) {
    return vieja.existe() ? fallar(ResultadoIdentidad::archivo_invalido, vieja.ruta)
                          : fallar(ResultadoIdentidad::error_cifrado, std::wstring());
  }
  const auto cerrada = cerrar_clave(*cargada.identidad, *cargada.boveda);
  const EscrituraNueva escrita = cerrada ? escribir_archivo_nuevo(clave.ruta, *cerrada) : EscrituraNueva::error;
  Secreto releida;
  if (escrita != EscrituraNueva::escrito ||
      leer_archivo(clave.ruta, kMaximoArchivoClave, releida) != LecturaArchivo::leido ||
      !misma_clave(abrir_clave(releida.bytes(), *cargada.boveda), *cargada.identidad)) {
    if (escrita == EscrituraNueva::escrito) {
      DeleteFileW(clave.ruta.c_str());
    }
    return fallar(ResultadoIdentidad::error_escritura, clave.ruta);
  }
  if (vieja.existe()) {
    borrar_con_ceros(vieja.ruta);
  }
  cargada.resultado = vieja.existe() ? ResultadoIdentidad::convertida : ResultadoIdentidad::creada;
  return cargada;
}

}
