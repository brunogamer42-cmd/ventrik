#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <gsl/gsl>

#include "client/buffer_voz.h"
#include "client/limpieza.h"
#include "client/puente_voz.h"
#include "client/voz_primero.h"

struct OpusEncoder;
struct OpusDecoder;

namespace ventrik::cliente {
struct Calidad {
  unsigned frecuencia = 48000;
  int complejidad = 10;
  int kbps = 24;
};

Calidad calidad_automatica();

constexpr int kPedazosTrasCallarse = 20;
constexpr std::size_t kMaximoPersonasConVolumen = 16;

int umbral_de(int por_ciento);
int segmentos_de(int pico, int segmentos);
std::int16_t con_ganancia(std::int16_t muestra, int por_ciento);
std::uint8_t nivel_de(gsl::span<const std::int16_t> pedazo);
void limitar(gsl::span<const float> mezcla, gsl::span<std::int16_t> salida, float& limite);
std::size_t periodo(gsl::span<const std::int16_t> pcm, unsigned frecuencia);
std::size_t acortar(gsl::span<std::int16_t> pcm, unsigned frecuencia);
std::size_t estirar(gsl::span<std::int16_t> pcm, std::size_t largo, unsigned frecuencia);

class MotorVoz {
 public:
  MotorVoz(PuenteVoz& puente, const Calidad& calidad);
  MotorVoz(const MotorVoz&) = delete;
  MotorVoz& operator=(const MotorVoz&) = delete;

  bool ok() const { return codificador_ && !decodificadores_.empty() && limpieza_; }
  unsigned frecuencia() const { return calidad_.frecuencia; }
  void transmitir(bool si) { transmitiendo_ = si && !sin_microfono_; }
  bool transmitiendo() const { return transmitiendo_; }
  void susurrar(std::uint8_t lugar) {
    susurro_ = sin_microfono_ || lugar > protocolo::kLugaresDeSusurro ? std::uint8_t{0} : lugar;
  }
  std::uint8_t susurrando() const { return susurro_; }
  void silenciar(bool microfono, bool audio) {
    if (sin_microfono_.exchange(microfono) != microfono) {
      transmitiendo_ = false;
      susurro_ = 0;
    }
    sin_audio_ = audio;
  }
  void volumenes(int microfono, int salida) {
    ganancia_microfono_ = microfono;
    ganancia_salida_ = salida;
  }
  void personas(gsl::span<const std::pair<std::uint32_t, std::uint8_t>> volumenes);
  void solo_en_el_canal(bool si) { solo_ = si; }
  void por_voz(bool si) { por_voz_ = si; }
  void sensibilidad(int por_ciento) { umbral_ = umbral_de(por_ciento); }
  void limpiar(const Limpiar& limpiar) { limpieza_->usar(limpiar); }
  std::uint32_t tomar_limpieza_us();
  std::size_t memoria_fija() const { return fija_.fijada(); }
  std::uint32_t espera_ms() const { return espera_ms_.load(std::memory_order_relaxed); }
  int nivel(int segmentos) const;
  std::uint32_t recortes() const { return recortes_.load(std::memory_order_relaxed); }
  std::uint32_t vaciados() const { return vaciados_.load(std::memory_order_relaxed); }
  std::uint32_t faltas() const { return faltas_.load(std::memory_order_relaxed); }
  std::uint32_t sin_lugar() const { return sin_lugar_.load(std::memory_order_relaxed); }
  std::uint32_t objetivo_ms() const { return objetivo_ms_.load(std::memory_order_relaxed); }
  std::uint32_t acelerado_ms() const { return acelerado_ms_.load(std::memory_order_relaxed); }
  std::uint32_t estirado_ms() const { return estirado_ms_.load(std::memory_order_relaxed); }
  std::uint32_t tardes() const { return tardes_.load(std::memory_order_relaxed); }

  void procesar(gsl::span<const std::int16_t> entrada, gsl::span<std::int16_t> salida) noexcept;
  static void procesar_audio(void* motor, gsl::span<const std::int16_t> entrada,
                             gsl::span<std::int16_t> salida) {
    static_cast<MotorVoz*>(motor)->procesar(entrada, salida);
  }

 private:
  struct Borrar {
    void operator()(OpusEncoder* codificador) const;
  };

  struct Voz {
    OpusDecoder* decodificador = nullptr;
    BufferVoz buffer;
    std::array<std::int16_t, 5760> pcm{};
    std::size_t cuantas = 0;
    std::size_t leidas = 0;
    std::uint32_t hablante = 0;
    std::uint8_t flujo = 0;
    int nivel = 0;
    unsigned quietos = 0;
    int disimulos = 0;
    float objetivo = 40;
    bool con_base = false;
    std::uint16_t base = 0;
    float resto = 0;
    std::uint64_t ultima_llegada = 0;
  };

  using Pedazo = std::array<std::int16_t, 960>;

  void limpiar_lo_capturado();
  void escuchar_salida(gsl::span<const std::int16_t> parte);
  void pedazo_completo(bool por_voz, std::uint8_t flujo);
  void codificar(const Pedazo& pedazo, std::uint16_t tiempo, std::uint8_t flujo);
  void recibir();
  Voz* lugar_para(const PaqueteVoz& paquete);
  void medir(Voz& voz, std::uint16_t tiempo);
  void sumar(std::size_t cual, gsl::span<float> mezcla, float ganancia);
  void decodificar(std::size_t cual, std::uint64_t muestra);
  void ajustar(Voz& voz, std::uint16_t tiempo, std::uint64_t muestra);
  std::uint64_t en_ms(std::uint64_t muestras) const { return muestras * 1000 / calidad_.frecuencia; }
  void contar();
  int por_ciento_de(std::uint32_t hablante) const;

  PuenteVoz& puente_;
  Calidad calidad_;
  std::size_t por_paquete_;
  std::unique_ptr<OpusEncoder, Borrar> codificador_;
  std::vector<std::uint64_t> decodificadores_;
  std::unique_ptr<Limpieza> limpieza_;
  Pedazo capturado_{};
  Pedazo anterior_{};
  std::size_t capturadas_ = 0;
  std::size_t limpias_ = 0;
  int pico_limpio_ = 0;
  std::array<std::int16_t, 480> referencia_{};
  std::size_t en_referencia_ = 0;
  bool capturaba_ = false;
  std::uint64_t capturadas_total_ = 0;
  bool hay_anterior_ = false;
  bool abierto_ = false;
  int restante_ = 0;
  std::array<Voz, kVoces> voces_;
  std::array<float, 960> mezcla_{};
  float limite_ = 1;
  std::uint64_t reproducidas_total_ = 0;
  std::uint64_t acelerado_ = 0;
  std::uint64_t estirado_ = 0;
  std::array<std::uint16_t, protocolo::kLugaresDeSusurro + 1> numeros_{};
  std::atomic<bool> transmitiendo_{false};
  std::atomic<std::uint8_t> susurro_{0};
  std::uint8_t flujo_capturado_ = 0;
  std::atomic<bool> solo_{false};
  std::atomic<bool> por_voz_{false};
  std::atomic<bool> sin_microfono_{false};
  std::atomic<bool> sin_audio_{false};
  std::atomic<int> ganancia_microfono_{100};
  std::atomic<int> ganancia_salida_{100};
  std::atomic<int> umbral_{umbral_de(100)};
  std::atomic<std::uint32_t> espera_ms_{0};
  std::atomic<int> pico_{0};
  std::atomic<std::uint32_t> recortes_{0};
  std::atomic<std::uint32_t> vaciados_{0};
  std::atomic<std::uint32_t> faltas_{0};
  std::atomic<std::uint32_t> sin_lugar_{0};
  std::atomic<std::uint32_t> objetivo_ms_{40};
  std::atomic<std::uint32_t> acelerado_ms_{0};
  std::atomic<std::uint32_t> estirado_ms_{0};
  std::atomic<std::uint32_t> tardes_{0};
  std::atomic<std::uint64_t> limpieza_tics_{0};
  std::array<std::atomic<std::uint64_t>, kMaximoPersonasConVolumen> personas_{};
  MemoriaFija fija_;
};

}
