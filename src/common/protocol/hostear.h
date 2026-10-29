#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <gsl/gsl>

#include "common/cifrado/huella.h"
#include "common/cifrado/secreto.h"
#include "common/niveles.h"
#include "common/protocol/pedidos.h"

namespace ventrik::protocolo {
constexpr std::size_t kMaximoContrasenaDeArranque = 1024;
constexpr std::size_t kMaximoEntradaPorCano = 4096;
constexpr std::size_t kMaximoDatosPublicos = 256;
constexpr wchar_t kArchivoPublico[] = L"\\publico.dat";
constexpr wchar_t kArchivoExportada[] = L"\\exportada.dat";
constexpr wchar_t kArchivoRegistro[] = L"\\registro.txt";

enum class SalidaServidor : int {
  bien = 0,
  error = 1,
  uso = 2,
  arranque = 3,
  andando = 4,
  datos_rotos = 5,
  carpeta_ocupada = 6,
  contrasena = 7
};

struct ServidorNuevo {
  std::string nombre;
  ventrik::Seguridad seguridad;
  std::array<std::uint8_t, kBytesClavePublica> dueno{};
  std::string nombre_dueno;
  cifrado::Secreto contrasena;
  cifrado::Secreto arranque;
};

struct EntradaPorCano {
  cifrado::Secreto arranque;
  cifrado::Secreto contrasena;
};

struct DatosPublicos {
  cifrado::Huella::Bytes huella{};
  std::uint16_t puerto = 0;
  std::string nombre;
};

bool es_contrasena_valida(std::string_view texto, std::size_t maximo = kMaximoBytesContrasena);

cifrado::Secreto armar_servidor_nuevo(const ServidorNuevo& nuevo);
std::optional<ServidorNuevo> leer_servidor_nuevo(gsl::span<const std::uint8_t> datos);
cifrado::Secreto armar_entrada(std::string_view arranque, std::string_view contrasena);
std::optional<EntradaPorCano> leer_entrada(gsl::span<const std::uint8_t> datos);
std::vector<std::uint8_t> armar_datos_publicos(const DatosPublicos& datos);
std::optional<DatosPublicos> leer_datos_publicos(gsl::span<const std::uint8_t> datos);
std::wstring evento_de(const std::wstring& carpeta);
std::wstring candado_de(const std::wstring& carpeta);

}
