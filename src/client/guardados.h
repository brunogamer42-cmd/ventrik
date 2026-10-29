#pragma once

#include <cstddef>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gsl/gsl>

#include "client/limpieza.h"
#include "client/silencios.h"
#include "common/cifrado/cadena.h"
#include "common/cifrado/huella.h"
#include "common/protocol/pedidos.h"
#include "common/red/red.h"

namespace ventrik::cliente {
constexpr std::size_t kMaximoServidoresGuardados = 32;
constexpr std::size_t kMaximoArchivoGuardados = 163840;
constexpr std::size_t kMaximoLeidosPorServidor = 64;
constexpr std::size_t kMaximoPersonasPorServidor = 16;
constexpr std::size_t kMaximoHuellasAtras = 16;
static_assert(kMaximoLeidosPorServidor >= protocolo::kMaximoCanales);
constexpr std::uint8_t kTeclaHablarPorDefecto = 0xA4;
constexpr std::uint8_t kSensibilidadMaxima = 100;
constexpr std::uint8_t kSensibilidadPorDefecto = 30;
constexpr std::uint8_t kVolumenMaximo = 200;
constexpr std::uint8_t kVolumenPorDefecto = 100;
constexpr std::size_t kMaximoAjustes = 128;
constexpr std::size_t kMaximoLargoDeAjuste = 2048;
constexpr std::size_t kMaximoBloqueDeAjustes = 16384;
constexpr std::size_t kMaximoNombreDeDispositivo = 256;
constexpr std::size_t kSonidos = 27;
constexpr std::size_t kMaximoNombreDePropio = 128;
constexpr std::uint8_t kNoMolestarAlJugar = 1;
constexpr std::uint8_t kNoMolestarSiempre = 2;

enum class Ajuste : std::uint16_t {
  ruido = 1,
  eco = 2,
  volumen_automatico = 3,
  bajar_otros = 4,
  al_cerrar = 5,
  al_minimizar = 6,
  entrar_al_ultimo = 7,
  al_entrar = 8,
  idioma = 9,
  teclas_sin_foco = 10,
  seguir_al_soltar = 11,
  microfono = 12,
  salida = 13,
  volumen_avisos = 14,
  avisos_de_texto = 15,
  avisos_de_voz = 16,
  avisos_de_conexion = 17,
  avisos_privados = 18,
  avisos_minimizada = 19,
  susurro_1 = 20,
  susurro_2 = 21,
  susurro_3 = 22,
  susurro_4 = 23,
  sonar_por = 24,
  no_molestar = 25,
  copia_de_identidad = 26,
  bloqueo = 27,
  maestra = 28,
  sonido_1 = 30,
  propio_1 = 60
};
constexpr std::array<std::uint16_t, 4> kSeguirAlSoltarMs = {0, 200, 500, 1000};
constexpr std::uint8_t kBloqueoConWindows = 1;
constexpr std::array<std::uint32_t, 5> kMinutosDeBloqueo = {0, 0, 5, 15, 60};
enum class Eco : std::uint8_t { apagado, automatico, prendido };
enum class Bajar : std::uint8_t { nada, un_poco, bastante, mucho };

struct Susurro {
  std::uint8_t tecla = 0;
  std::string canal;
  std::vector<std::string> personas;
};

struct AjustesDeLimpieza {
  Ruido ruido = Ruido::apagado;
  Eco eco = Eco::apagado;
  bool volumen = true;
  Bajar bajar = Bajar::nada;
};

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
  AvisosDelServidor avisos;
  std::vector<cifrado::Huella::Bytes> atras;
  cifrado::Huella::Bytes original{};
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
  std::map<std::uint16_t, std::vector<std::uint8_t>> ajustes;
};

enum class LecturaGuardados { leidos, danados, version_nueva, error };

LecturaGuardados leer_guardados(gsl::span<const std::uint8_t> datos, Guardados& guardados);
std::vector<std::uint8_t> armar_guardados(const Guardados& guardados);
bool es_tecla_valida(std::uint8_t tecla);
std::optional<std::uint8_t> ajuste_u8(const Guardados& guardados, Ajuste ajuste);
std::uint8_t ajuste_o(const Guardados& guardados, Ajuste ajuste, std::uint8_t de_fabrica);
std::string ajuste_texto(const Guardados& guardados, Ajuste ajuste);
void poner_ajuste(Guardados& guardados, Ajuste ajuste, std::uint8_t valor);
void poner_texto(Guardados& guardados, Ajuste ajuste, std::string_view texto);
AjustesDeLimpieza limpieza_de(const Guardados& guardados, bool un_nucleo);
std::optional<Susurro> susurro_de(const Guardados& guardados, std::size_t lugar);
bool sonido_prendido(const Guardados& guardados, std::size_t sonido);
Ajuste ajuste_de_sonido(std::size_t sonido);
Ajuste ajuste_de_propio(std::size_t sonido);
void poner_susurro(Guardados& guardados, std::size_t lugar, const std::optional<Susurro>& susurro);
protocolo::DestinosDeSusurro destinos_de(const Susurro& susurro, std::size_t lugar, std::uint32_t yo,
                                         const std::vector<protocolo::CanalInfo>& canales,
                                         const std::map<std::uint32_t, protocolo::UsuarioInfo>& usuarios);
std::uint8_t tecla_para_hablar(std::uint8_t guardada);
ServidorGuardado* buscar_guardado(Guardados& guardados, const red::Direccion& direccion);
bool recordar(Guardados& guardados, ServidorGuardado servidor);
void cambiar_huella(ServidorGuardado& servidor, const cifrado::CadenaSeguida& cambio);
bool conoce_huella(const ServidorGuardado& servidor, const cifrado::Huella::Bytes& huella);
std::vector<cifrado::Huella> huellas_atras(const ServidorGuardado& servidor);
void ajustar_persona(std::vector<PersonaGuardada>& personas, PersonaGuardada persona);
AjustesDeVoz ajustes_de_voz(const std::vector<PersonaGuardada>& personas,
                            const std::map<std::uint32_t, protocolo::UsuarioInfo>& usuarios);

LecturaGuardados cargar_guardados(Guardados& guardados, const std::wstring& carpeta);
bool guardar_guardados(const Guardados& guardados, const std::wstring& carpeta);

}
