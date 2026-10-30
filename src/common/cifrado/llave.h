#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/clave.h"
#include "common/protocol/bytes.h"

namespace ventrik::cifrado {

constexpr char kAplicacionDeLlave[] = "ventrik";
constexpr std::size_t kBytesDatosDeLlave = 37;
constexpr std::size_t kBytesDesafioDeLlave = 32;
constexpr std::size_t kMaximoIdDeLlave = 1023;
constexpr std::size_t kBytesModuloRsa = 256;
constexpr std::int32_t kCoseEs256 = -7;
constexpr std::int32_t kCoseRs256 = -257;
constexpr std::uint8_t kLlavePresente = 0x01;
constexpr std::uint8_t kLlaveVerificada = 0x04;
constexpr std::uint8_t kLlaveConCredencial = 0x40;
constexpr std::uint8_t kLlaveConExtensiones = 0x80;
constexpr std::uint32_t kAdelantoDeGeneracion = 2 * 24 * 60;

enum class AlgoritmoDeLlave : std::uint8_t { es256 = 1, rs256 = 2 };

struct LlavePublica {
  AlgoritmoDeLlave algoritmo = AlgoritmoDeLlave::es256;
  std::vector<std::uint8_t> clave;
};

inline bool operator==(const LlavePublica& a, const LlavePublica& b) {
  return a.algoritmo == b.algoritmo && a.clave == b.clave;
}

inline bool operator!=(const LlavePublica& a, const LlavePublica& b) {
  return !(a == b);
}

std::optional<LlavePublica> armar_llave(AlgoritmoDeLlave algoritmo, gsl::span<const std::uint8_t> clave);
bool firma_de_llave_valida(AlgoritmoDeLlave algoritmo, std::size_t largo);
void escribir_llave(protocolo::Escritor& escritor, const LlavePublica& llave);
std::optional<LlavePublica> leer_llave(protocolo::Lector& lector);

std::uint32_t generacion_de_la_hora(std::uint64_t ms_desde_1970);
std::uint32_t generacion_tope(std::uint64_t ms_desde_1970);

enum class Ceremonia { crear, firmar };

std::string texto_de_cliente(Ceremonia ceremonia, gsl::span<const std::uint8_t> desafio);

enum class ResultadoLlave { valida, datos_invalidos, firma_invalida, posible_llave_clonada };

struct LlaveVerificada {
  ResultadoLlave resultado = ResultadoLlave::datos_invalidos;
  bool segundo_paso = false;
  std::uint32_t contador = 0;
};

LlaveVerificada verificar_llave(const LlavePublica& llave, std::uint32_t contador_guardado,
                                gsl::span<const std::uint8_t> desafio, gsl::span<const std::uint8_t> datos,
                                gsl::span<const std::uint8_t> firma);

std::vector<std::uint8_t> lo_que_firma_el_permiso(const ClavePublica& identidad, const LlavePublica& llave,
                                                  std::uint32_t generacion);

bool verificar_permiso(const ClavePublica& identidad, const LlavePublica& llave, std::uint32_t generacion,
                       std::uint32_t generacion_minima, gsl::span<const std::uint8_t> firma);

std::optional<LlavePublica> leer_clave_cose(gsl::span<const std::uint8_t> cose);

struct LlaveCreada {
  std::vector<std::uint8_t> id;
  LlavePublica publica;
};

std::optional<LlaveCreada> leer_llave_creada(gsl::span<const std::uint8_t> datos);

}
