#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/clave.h"

namespace ventrik::cifrado {

constexpr char kAplicacionDeLlave[] = "ventrik";
constexpr std::size_t kBytesDatosDeLlave = 37;
constexpr std::size_t kBytesDesafioDeLlave = 32;
constexpr std::size_t kMaximoIdDeLlave = 1023;
constexpr std::uint8_t kLlavePresente = 0x01;
constexpr std::uint8_t kLlaveVerificada = 0x04;
constexpr std::uint8_t kLlaveConCredencial = 0x40;
constexpr std::uint8_t kLlaveConExtensiones = 0x80;
constexpr std::uint32_t kAdelantoDeGeneracion = 2 * 24 * 60;

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

LlaveVerificada verificar_llave(const ClavePublica& llave, std::uint32_t contador_guardado,
                                gsl::span<const std::uint8_t> desafio, gsl::span<const std::uint8_t> datos,
                                gsl::span<const std::uint8_t> firma);

std::vector<std::uint8_t> lo_que_firma_el_permiso(const ClavePublica& identidad, const ClavePublica& llave,
                                                  std::uint32_t generacion);

bool verificar_permiso(const ClavePublica& identidad, const ClavePublica& llave, std::uint32_t generacion,
                       std::uint32_t generacion_minima, gsl::span<const std::uint8_t> firma);

std::optional<ClavePublica> leer_clave_cose(gsl::span<const std::uint8_t> cose);

struct LlaveCreada {
  std::vector<std::uint8_t> id;
  ClavePublica publica{};
};

std::optional<LlaveCreada> leer_llave_creada(gsl::span<const std::uint8_t> datos);

}
