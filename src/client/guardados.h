#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/huella.h"
#include "common/protocol/pedidos.h"
#include "common/red/red.h"

namespace ventrik::cliente {
constexpr std::size_t kMaximoServidoresGuardados = 32;
constexpr std::size_t kMaximoArchivoGuardados = 65536;
constexpr std::size_t kMaximoLeidosPorServidor = 64;
constexpr std::size_t kMaximoPersonasPorServidor = 16;
static_assert(kMaximoLeidosPorServidor >= protocolo::kMaximoCanales);
constexpr std::uint8_t kTeclaHablarPorDefecto = 0xA4;
constexpr std::uint8_t kSensibilidadMaxima = 100;
constexpr std::uint8_t kSensibilidadPorDefecto = 30;
constexpr std::uint8_t kVolumenMaximo = 200;
constexpr std::uint8_t kVolumenPorDefecto = 100;

struct Leido {
  std::uint32_t canal = 0;
  std::uint32_t numero = 0;
};

struct PersonaGuardada {
  std::string nombre;
  std::uint8_t volumen = kVolumenPorDefecto;
  bool silenciada = false;
};

struct ServidorGuardado {
  red::Direccion direccion;
  cifrado::Huella::Bytes huella{};
  std::string nombre;
  std::uint32_t texto = 0;
  std::string nombre_canal;
  std::uint64_t ultima_vez_ms = 0;
  std::vector<Leido> leidos;
  std::vector<PersonaGuardada> personas;
};

struct AjustesDeVoz {
  std::vector<std::pair<std::uint32_t, std::uint8_t>> volumenes;
  std::vector<std::uint32_t> silenciados;
};

struct Guardados {
  std::uint8_t tecla_hablar = 0;
  bool por_voz = false;
  std::uint8_t sensibilidad = kSensibilidadPorDefecto;
  std::uint8_t silencio = 0;
  std::uint8_t volumen_microfono = kVolumenPorDefecto;
  std::uint8_t volumen_salida = kVolumenPorDefecto;
  std::string nombre;
  std::vector<ServidorGuardado> servidores;
};

enum class LecturaGuardados { leidos, danados, version_nueva, error };

LecturaGuardados leer_guardados(gsl::span<const std::uint8_t> datos, Guardados& guardados);
std::vector<std::uint8_t> armar_guardados(const Guardados& guardados);
bool es_tecla_valida(std::uint8_t tecla);
std::uint8_t tecla_para_hablar(std::uint8_t guardada);
ServidorGuardado* buscar_guardado(Guardados& guardados, const red::Direccion& direccion);
bool recordar(Guardados& guardados, ServidorGuardado servidor);
void ajustar_persona(std::vector<PersonaGuardada>& personas, PersonaGuardada persona);
AjustesDeVoz ajustes_de_voz(const std::vector<PersonaGuardada>& personas,
                            const std::map<std::uint32_t, protocolo::UsuarioInfo>& usuarios);

LecturaGuardados cargar_guardados(Guardados& guardados, const std::wstring& carpeta);
bool guardar_guardados(const Guardados& guardados, const std::wstring& carpeta);

}
