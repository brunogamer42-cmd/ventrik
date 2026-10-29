#include "common/cifrado/sobre.h"

#include <algorithm>
#include <chrono>
#include <string>

#include <windows.h>

#include <argon2.h>
#include <mbedtls/gcm.h>

#include "common/cifrado/inicio.h"
#include "common/protocol/bytes.h"

namespace ventrik::cifrado {
namespace {

constexpr std::uint8_t kVersionSobre = 1;
constexpr std::size_t kBytesCabecera = kBytesMarca + 1 + 4 + 1 + kBytesSal;

class Gcm {
 public:
  explicit Gcm(gsl::span<const std::uint8_t> clave) {
    mbedtls_gcm_init(&gcm_);
    lista_ = clave.size() == kBytesClaveSimetrica &&
             mbedtls_gcm_setkey(&gcm_, MBEDTLS_CIPHER_ID_AES, clave.data(), 256) == 0;
  }
  ~Gcm() { mbedtls_gcm_free(&gcm_); }
  Gcm(const Gcm&) = delete;
  Gcm& operator=(const Gcm&) = delete;

  bool lista() const { return lista_; }
  mbedtls_gcm_context* contexto() { return &gcm_; }

 private:
  mbedtls_gcm_context gcm_;
  bool lista_ = false;
};

std::uint64_t memoria_libre() {
  MEMORYSTATUSEX estado = {};
  estado.dwLength = sizeof(estado);
  return GlobalMemoryStatusEx(&estado) ? std::min(estado.ullAvailPhys, estado.ullAvailVirtual)
                                        : std::uint64_t{kMaximoMemoriaArgon2} * 2048;
}

std::vector<std::uint8_t> cabecera(std::string_view marca, const Argon2& valores, gsl::span<const std::uint8_t> sal) {
  protocolo::Escritor escritor;
  escritor.escribir_bytes(gsl::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(marca.data()), marca.size()));
  escritor.escribir_u8(kVersionSobre);
  escritor.escribir_u32(valores.memoria_kib);
  escritor.escribir_u8(valores.vueltas);
  escritor.escribir_bytes(sal);
  return std::move(escritor).soltar();
}

}  // namespace

bool es_argon2_valido(const Argon2& valores) {
  return valores.memoria_kib >= kMinimoMemoriaArgon2 && valores.memoria_kib <= kMaximoMemoriaArgon2 &&
         valores.vueltas >= 1 && valores.vueltas <= kMaximoVueltasArgon2;
}

std::optional<Argon2> derivar_a_tiempo(std::string_view contrasena, gsl::span<const std::uint8_t> sal,
                                       std::uint32_t milisegundos, gsl::span<std::uint8_t> salida) {
  Argon2 valores = memoria_para_esta_pc();
  const auto antes = std::chrono::steady_clock::now();
  if (!derivar_argon2(contrasena, sal, valores, salida)) {
    return std::nullopt;
  }
  const auto tardo = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - antes);
  const auto ms = static_cast<std::uint32_t>(std::max<std::chrono::milliseconds::rep>(tardo.count(), 1));
  if (ms < milisegundos) {
    valores.vueltas = static_cast<std::uint8_t>(std::min<std::uint32_t>(kMaximoVueltasArgon2, (milisegundos + ms - 1) / ms));
    if (valores.vueltas > 1 && !derivar_argon2(contrasena, sal, valores, salida)) {
      return std::nullopt;
    }
  }
  return valores;
}

bool derivar_argon2(std::string_view contrasena, gsl::span<const std::uint8_t> sal, const Argon2& valores,
                    gsl::span<std::uint8_t> salida) {
  return es_argon2_valido(valores) && !salida.empty() &&
         argon2id_hash_raw(valores.vueltas, valores.memoria_kib, 1, contrasena.data(), contrasena.size(), sal.data(),
                           sal.size(), salida.data(), salida.size()) == ARGON2_OK;
}

Argon2 memoria_para_esta_pc() {
  const std::uint64_t libre = memoria_libre();
  const std::uint32_t memoria = libre >= (std::uint64_t{1} << 30)   ? 64 * 1024
                                : libre >= (std::uint64_t{1} << 29) ? 32 * 1024
                                                                    : 16 * 1024;
  return {memoria, 1};
}

std::optional<std::vector<std::uint8_t>> sellar(gsl::span<const std::uint8_t> clave,
                                                gsl::span<const std::uint8_t> asociados,
                                                gsl::span<const std::uint8_t> datos) {
  Gcm gcm(clave);
  std::vector<std::uint8_t> sellado(kBytesNonce + datos.size() + kBytesEtiqueta);
  const auto nonce = gsl::span<std::uint8_t>(sellado).first(kBytesNonce);
  const auto cifrado = gsl::span<std::uint8_t>(sellado).subspan(kBytesNonce, datos.size());
  const auto etiqueta = gsl::span<std::uint8_t>(sellado).last(kBytesEtiqueta);
  if (!gcm.lista() || aleatorio(nullptr, nonce.data(), nonce.size()) != 0 ||
      mbedtls_gcm_crypt_and_tag(gcm.contexto(), MBEDTLS_GCM_ENCRYPT, datos.size(), nonce.data(), nonce.size(),
                                asociados.data(), asociados.size(), datos.data(), cifrado.data(), etiqueta.size(),
                                etiqueta.data()) != 0) {
    return std::nullopt;
  }
  return sellado;
}

std::optional<Secreto> abrir_sellado(gsl::span<const std::uint8_t> clave, gsl::span<const std::uint8_t> asociados,
                                     gsl::span<const std::uint8_t> sellado) {
  if (sellado.size() < kBytesNonce + kBytesEtiqueta) {
    return std::nullopt;
  }
  Gcm gcm(clave);
  const auto nonce = sellado.first(kBytesNonce);
  const auto cifrado = sellado.subspan(kBytesNonce, sellado.size() - kBytesNonce - kBytesEtiqueta);
  const auto etiqueta = sellado.last(kBytesEtiqueta);
  Secreto abierto(cifrado.size());
  if (!gcm.lista() ||
      mbedtls_gcm_auth_decrypt(gcm.contexto(), cifrado.size(), nonce.data(), nonce.size(), asociados.data(),
                               asociados.size(), etiqueta.data(), etiqueta.size(), cifrado.data(),
                               abierto.bytes().data()) != 0) {
    return std::nullopt;
  }
  return abierto;
}

std::optional<CabeceraSobre> leer_cabecera_sobre(std::string_view marca, gsl::span<const std::uint8_t> sobre) {
  if (marca.size() != kBytesMarca || sobre.size() < kBytesCabecera + kBytesNonce + kBytesEtiqueta) {
    return std::nullopt;
  }
  protocolo::Lector lector(sobre);
  const auto leida = lector.leer_bytes(kBytesMarca);
  CabeceraSobre resultado;
  const bool misma_marca =
      lector.ok() && std::equal(leida.begin(), leida.end(), marca.begin(), marca.end(),
                                [](std::uint8_t byte, char letra) { return byte == static_cast<std::uint8_t>(letra); });
  const std::uint8_t version = lector.leer_u8();
  resultado.argon2.memoria_kib = lector.leer_u32();
  resultado.argon2.vueltas = lector.leer_u8();
  resultado.sal = lector.leer_bytes(kBytesSal);
  if (!lector.ok() || !misma_marca || version != kVersionSobre || !es_argon2_valido(resultado.argon2)) {
    return std::nullopt;
  }
  resultado.asociados = sobre.first(kBytesCabecera);
  resultado.sellado = sobre.subspan(kBytesCabecera);
  return resultado;
}

std::optional<LlaveDeSobre> llave_nueva(std::string_view marca, std::string_view contrasena,
                                        std::optional<Argon2> fijos) {
  std::array<std::uint8_t, kBytesSal> sal{};
  Secreto clave(kBytesClaveSimetrica);
  if (marca.size() != kBytesMarca || aleatorio(nullptr, sal.data(), sal.size()) != 0) {
    return std::nullopt;
  }
  const auto valores = fijos ? (derivar_argon2(contrasena, sal, *fijos, clave.bytes()) ? fijos : std::nullopt)
                             : derivar_a_tiempo(contrasena, sal, kMilisegundosArgon2, clave.bytes());
  if (!valores) {
    return std::nullopt;
  }
  return LlaveDeSobre{cabecera(marca, *valores, sal), std::move(clave)};
}

std::optional<std::vector<std::uint8_t>> cerrar_con_llave(const LlaveDeSobre& llave,
                                                          gsl::span<const std::uint8_t> contenido) {
  const auto sellado = sellar(llave.clave.bytes(), llave.cabecera, contenido);
  if (!sellado || llave.cabecera.size() != kBytesCabecera) {
    return std::nullopt;
  }
  std::vector<std::uint8_t> sobre = llave.cabecera;
  sobre.insert(sobre.end(), sellado->begin(), sellado->end());
  return sobre;
}

std::optional<std::vector<std::uint8_t>> cerrar_sobre(std::string_view marca, gsl::span<const std::uint8_t> contenido,
                                                      std::string_view contrasena, std::optional<Argon2> fijos) {
  const auto llave = llave_nueva(marca, contrasena, fijos);
  return llave ? cerrar_con_llave(*llave, contenido) : std::nullopt;
}

SobreAbierto abrir_sobre(std::string_view marca, gsl::span<const std::uint8_t> sobre, std::string_view contrasena) {
  SobreAbierto resultado;
  const auto leida = leer_cabecera_sobre(marca, sobre);
  if (!leida) {
    return resultado;
  }
  if (std::uint64_t{leida->argon2.memoria_kib} * 1024 > memoria_libre() / 2) {
    resultado.resultado = AperturaSobre::sin_memoria;
    return resultado;
  }
  Secreto clave(kBytesClaveSimetrica);
  if (!derivar_argon2(contrasena, leida->sal, leida->argon2, clave.bytes())) {
    resultado.resultado = AperturaSobre::sin_memoria;
    return resultado;
  }
  resultado.contenido = abrir_sellado(clave.bytes(), leida->asociados, leida->sellado);
  resultado.resultado = resultado.contenido ? AperturaSobre::abierto : AperturaSobre::contrasena;
  if (resultado.contenido) {
    resultado.llave = LlaveDeSobre{std::vector<std::uint8_t>(leida->asociados.begin(), leida->asociados.end()),
                                   std::move(clave)};
  }
  return resultado;
}

}  // namespace ventrik::cifrado
