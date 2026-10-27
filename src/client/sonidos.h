#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gsl/gsl>

#include "client/audio.h"
#include "client/guardados.h"
#include "client/puente_voz.h"

namespace ventrik::cliente {
enum class Sonido : std::uint8_t {
  entrar_canal,
  salir_canal,
  alguien_entra,
  alguien_sale,
  microfono_silenciado,
  microfono_activado,
  audio_silenciado,
  audio_activado,
  transmitir_empieza,
  transmitir_termina,
  te_movieron,
  susurro,
  conexion_perdida,
  reconectado,
  mensaje,
  mencion,
  mensaje_amigo,
  archivo_listo,
  llamada_entrante,
  llamando,
  llamada_termina,
  solicitud_amistad,
  amigo_conectado,
  pedido_entrada,
  alerta_seguridad,
  pantalla_compartida,
  error
};

constexpr unsigned kFrecuenciaDeSonidos = 48000;
constexpr std::size_t kMuestrasPorPaqueteDeSonido = 960;
constexpr std::size_t kMaximoArchivoDeSonido = 1048576;
constexpr std::size_t kMaximoSegundosDeSonido = 5;
constexpr std::size_t kMaximoSegundosDeLlamada = 10;
constexpr std::size_t kTopeDeSonidosGuardados = 2 * 1024 * 1024;
constexpr std::size_t kSonidosALaVez = 4;
constexpr std::chrono::seconds kCerrarTrasSonar{2};
constexpr std::array<const char*, kSonidos> kNombresDeSonidos = {
    "entrar_canal",      "salir_canal",       "alguien_entra",   "alguien_sale",      "mic_silenciado",
    "mic_activado",      "audio_silenciado",  "audio_activado",  "transmitir_empieza", "transmitir_termina",
    "te_movieron",       "susurro",           "conexion_perdida", "reconectado",       "mensaje",
    "mencion",           "mensaje_amigo",     "archivo_listo",   "llamada_entrante",  "llamando",
    "llamada_termina",   "solicitud_amistad", "amigo_conectado", "pedido_entrada",    "alerta_seguridad",
    "pantalla_compartida", "error"};

using Muestras = std::vector<std::int16_t>;

std::size_t maximo_de_muestras(Sonido sonido);
std::vector<std::uint8_t> codificar_vksn(gsl::span<const std::int16_t> muestras);
std::optional<Muestras> decodificar_vksn(gsl::span<const std::uint8_t> datos, std::size_t maximo);
std::optional<Muestras> decodificar_archivo(gsl::span<const std::uint8_t> datos, std::size_t maximo);
std::vector<std::uint8_t> armar_wav(gsl::span<const std::int16_t> muestras);
gsl::span<const std::uint8_t> sonido_de_fabrica(Sonido sonido);
std::wstring archivo_propio(const std::wstring& carpeta, Sonido sonido);

class Sonidos {
 public:
  using Reloj = std::chrono::steady_clock;

  explicit Sonidos(std::wstring carpeta) : carpeta_(std::move(carpeta)) {}
  Sonidos(const Sonidos&) = delete;
  Sonidos& operator=(const Sonidos&) = delete;

  bool sonar(Sonido sonido, int volumen, bool propio, const std::string& salida, Reloj::time_point ahora);
  void atender(Reloj::time_point ahora);
  void olvidar(Sonido sonido);
  void soltar();
  bool abierta() const { return salida_ != nullptr; }
  std::optional<Reloj::time_point> cerrar_en() const { return cerrar_en_; }
  std::size_t guardados() const;
  bool poner(gsl::span<const std::int16_t> muestras, float volumen);
  static void procesar(void* yo, gsl::span<const std::int16_t> entrada, gsl::span<std::int16_t> salida);

 private:
  struct Sonando {
    const std::int16_t* datos = nullptr;
    std::size_t largo = 0;
    std::size_t posicion = 0;
    float volumen = 0;
  };

  std::shared_ptr<const Muestras> cargar(Sonido sonido, bool propio);
  void cerrar();

  std::wstring carpeta_;
  std::vector<std::pair<Sonido, std::shared_ptr<const Muestras>>> guardados_;
  std::vector<std::pair<Sonido, std::shared_ptr<const Muestras>>> en_uso_;
  ColaVoz<8, Sonando> cola_;
  std::array<Sonando, kSonidosALaVez> sonando_{};
  std::array<float, kMuestrasPorPaqueteDeSonido> mezcla_{};
  float limite_ = 1;
  std::unique_ptr<Audio> salida_;
  std::string abierta_en_;
  std::optional<Reloj::time_point> cerrar_en_;
};

}
