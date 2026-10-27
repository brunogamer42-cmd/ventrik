#include "client/motor_voz.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include <windows.h>

#include <opus.h>

namespace ventrik::cliente {
namespace {
constexpr unsigned kQuietosParaPrestar = 10;
constexpr int kVentajaParaEntrar = 6;
constexpr opus_int32 kSoloCabecera = 2;
constexpr float kObjetivoMinimo = 40;
constexpr float kObjetivoMaximo = 200;
constexpr float kMargenDelObjetivo = 20;
constexpr float kHolgura = 20;
constexpr float kBajadaDelObjetivo = 0.002f;
constexpr float kSubidaDeLaBase = 0.01f;
constexpr int kRetrasoImposible = 1000;
constexpr int kCasiCallado = 300;
constexpr double kParecido = 0.8;
constexpr int kNivelFuerte = 77;
constexpr int kDisimulosAlVaciarse = 3;

void fundir(gsl::span<const std::int16_t> desde, gsl::span<const std::int16_t> hacia,
            gsl::span<std::int16_t> salida) {
  const auto largo = static_cast<float>(salida.size());
  for (std::size_t i = 0; i < salida.size(); ++i) {
    const float peso = static_cast<float>(i) / largo;
    salida[i] = static_cast<std::int16_t>(desde[i] + (hacia[i] - desde[i]) * peso);
  }
}
}

std::size_t periodo(gsl::span<const std::int16_t> pcm, unsigned frecuencia) {
  const std::size_t minimo = frecuencia / 400;
  const std::size_t maximo = std::min<std::size_t>(frecuencia / 100, pcm.size() / 2);
  int pico = 0;
  for (const std::int16_t muestra : pcm) {
    pico = std::max(pico, std::abs(static_cast<int>(muestra)));
  }
  if (maximo <= minimo) {
    return 0;
  }
  if (pico < kCasiCallado) {
    return pcm.size() / 2;
  }
  const std::size_t paso = std::max(1u, frecuencia / 16000);
  double mejor = kParecido;
  std::size_t elegido = 0;
  for (std::size_t corrimiento = minimo; corrimiento <= maximo; corrimiento += paso) {
    std::int64_t cruzada = 0;
    std::int64_t uno = 0;
    std::int64_t otro = 0;
    for (std::size_t i = 0; i + maximo < pcm.size(); i += paso) {
      const std::int64_t a = pcm[i];
      const std::int64_t b = pcm[i + corrimiento];
      cruzada += a * b;
      uno += a * a;
      otro += b * b;
    }
    const double parecido = static_cast<double>(cruzada) / std::sqrt(static_cast<double>(uno) * otro + 1);
    if (parecido > mejor) {
      mejor = parecido;
      elegido = corrimiento;
    }
  }
  return elegido;
}

std::size_t acortar(gsl::span<std::int16_t> pcm, unsigned frecuencia) {
  const std::size_t corrimiento = periodo(pcm, frecuencia);
  if (corrimiento == 0) {
    return pcm.size();
  }
  fundir(pcm.first(corrimiento), pcm.subspan(corrimiento, corrimiento), pcm.first(corrimiento));
  std::copy(pcm.begin() + static_cast<std::ptrdiff_t>(2 * corrimiento), pcm.end(),
            pcm.begin() + static_cast<std::ptrdiff_t>(corrimiento));
  return pcm.size() - corrimiento;
}

std::size_t estirar(gsl::span<std::int16_t> pcm, std::size_t largo, unsigned frecuencia) {
  const std::size_t corrimiento = periodo(pcm.first(largo), frecuencia);
  if (corrimiento == 0 || largo + corrimiento > pcm.size()) {
    return largo;
  }
  std::copy_backward(pcm.begin() + static_cast<std::ptrdiff_t>(corrimiento),
                     pcm.begin() + static_cast<std::ptrdiff_t>(largo),
                     pcm.begin() + static_cast<std::ptrdiff_t>(largo + corrimiento));
  fundir(pcm.subspan(2 * corrimiento, corrimiento), pcm.first(corrimiento), pcm.subspan(corrimiento, corrimiento));
  return largo + corrimiento;
}

std::int16_t con_ganancia(std::int16_t muestra, int por_ciento) {
  return static_cast<std::int16_t>(std::clamp(muestra * por_ciento / 100, -32768, 32767));
}

std::uint8_t nivel_de(gsl::span<const std::int16_t> pedazo) {
  double suma = 0;
  for (const std::int16_t muestra : pedazo) {
    suma += static_cast<double>(muestra) * muestra;
  }
  if (suma <= 0) {
    return 0;
  }
  const double decibeles = 10 * std::log10(suma / static_cast<double>(pedazo.size()) / (32768.0 * 32768.0));
  return static_cast<std::uint8_t>(std::clamp<long>(std::lround(protocolo::kNivelMaximo + decibeles), 0,
                                                    protocolo::kNivelMaximo));
}

void limitar(gsl::span<const float> mezcla, gsl::span<std::int16_t> salida, float& limite) {
  float pico = 0;
  for (const float valor : mezcla) {
    pico = std::max(pico, std::abs(valor));
  }
  const float ganancia = pico * limite > 32767 ? 32767 / pico : limite;
  std::transform(mezcla.begin(), mezcla.end(), salida.begin(), [ganancia](float valor) {
    return static_cast<std::int16_t>(std::clamp(valor * ganancia, -32768.0f, 32767.0f));
  });
  limite = ganancia + (1 - ganancia) / 16;
}

Calidad calidad_automatica() {
  using Consultar = BOOL(WINAPI*)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
  const auto consultar = reinterpret_cast<Consultar>(
      GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetLogicalProcessorInformation"));
  SYSTEM_INFO sistema = {};
  GetSystemInfo(&sistema);
  std::size_t nucleos = sistema.dwNumberOfProcessors;
  DWORD largo = 0;
  if (consultar != nullptr && !consultar(nullptr, &largo) &&
      GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> datos(largo / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
    if (consultar(datos.data(), &largo)) {
      nucleos = static_cast<std::size_t>(std::count_if(datos.begin(), datos.end(), [](const auto& dato) {
        return dato.Relationship == RelationProcessorCore;
      }));
    }
  }
  return nucleos > 1 ? Calidad{} : Calidad{16000, 0, 20};
}

void MotorVoz::Borrar::operator()(OpusEncoder* codificador) const {
  opus_encoder_destroy(codificador);
}

MotorVoz::MotorVoz(PuenteVoz& puente, const Calidad& calidad)
    : puente_(puente),
      calidad_(calidad),
      por_paquete_(std::min<std::size_t>(calidad.frecuencia / 50, 960)) {
  int error = OPUS_OK;
  codificador_.reset(opus_encoder_create(static_cast<opus_int32>(calidad_.frecuencia), 1,
                                         OPUS_APPLICATION_VOIP, &error));
  if (error != OPUS_OK) {
    codificador_.reset();
    return;
  }
  opus_encoder_ctl(codificador_.get(), OPUS_SET_BITRATE(calidad_.kbps * 1000));
  opus_encoder_ctl(codificador_.get(), OPUS_SET_COMPLEXITY(calidad_.complejidad));
  opus_encoder_ctl(codificador_.get(), OPUS_SET_VBR_CONSTRAINT(1));
  opus_encoder_ctl(codificador_.get(), OPUS_SET_DTX(1));
  const std::size_t paso = (static_cast<std::size_t>(opus_decoder_get_size(1)) + 15) / 16 * 2;
  std::vector<std::uint64_t> bloque(paso * kVoces);
  for (std::size_t cual = 0; cual < kVoces; ++cual) {
    voces_[cual].decodificador = reinterpret_cast<OpusDecoder*>(bloque.data() + cual * paso);
    if (opus_decoder_init(voces_[cual].decodificador, static_cast<opus_int32>(calidad_.frecuencia), 1) != OPUS_OK) {
      return;
    }
  }
  decodificadores_ = std::move(bloque);
  fija_.fijar({{this, sizeof(*this)},
               {&puente_, sizeof(puente_)},
               {codificador_.get(), static_cast<std::size_t>(opus_encoder_get_size(1))},
               {decodificadores_.data(), decodificadores_.size() * sizeof(std::uint64_t)}});
}

void MotorVoz::procesar(gsl::span<const std::int16_t> entrada,
                        gsl::span<std::int16_t> salida) noexcept {
  if (!ok()) {
    std::fill(salida.begin(), salida.end(), std::int16_t{0});
    return;
  }
  const bool por_voz = por_voz_;
  const bool escuchar = !sin_microfono_ && (por_voz || transmitiendo_);
  const int ganancia = ganancia_microfono_;
  int pico = 0;
  for (const std::int16_t muestra : escuchar ? entrada : gsl::span<const std::int16_t>()) {
    pico = std::max(pico, std::abs(static_cast<int>(muestra)));
  }
  pico_.store(std::min(pico * ganancia / 100, 32767), std::memory_order_relaxed);
  if (!por_voz || !escuchar) {
    abierto_ = hay_anterior_ = false;
    restante_ = 0;
  }
  if (!escuchar || (solo_ && !por_voz)) {
    capturadas_ = 0;
    capturadas_total_ += entrada.size();
    entrada = {};
  }
  while (!entrada.empty()) {
    const auto parte = entrada.first(std::min(entrada.size(), por_paquete_ - capturadas_));
    std::transform(parte.begin(), parte.end(), capturado_.begin() + static_cast<std::ptrdiff_t>(capturadas_),
                   [ganancia](std::int16_t muestra) { return con_ganancia(muestra, ganancia); });
    capturadas_ += parte.size();
    capturadas_total_ += parte.size();
    entrada = entrada.subspan(parte.size());
    if (capturadas_ == por_paquete_) {
      pedazo_completo(por_voz);
      capturadas_ = 0;
    }
  }
  const float volumen = sin_audio_ ? 0.0f : static_cast<float>(ganancia_salida_.load()) / 100;
  while (!salida.empty()) {
    const auto parte = salida.first(std::min(salida.size(), mezcla_.size()));
    const auto mezcla = gsl::make_span(mezcla_).first(parte.size());
    recibir();
    std::fill(mezcla.begin(), mezcla.end(), 0.0f);
    for (std::size_t cual = 0; cual < kVoces; ++cual) {
      sumar(cual, mezcla, volumen);
    }
    limitar(mezcla, parte, limite_);
    reproducidas_total_ += parte.size();
    for (Voz& voz : voces_) {
      voz.objetivo = std::max(kObjetivoMinimo, voz.objetivo - static_cast<float>(en_ms(parte.size())) * kBajadaDelObjetivo);
    }
    salida = salida.subspan(parte.size());
  }
  contar();
}

int MotorVoz::nivel(int segmentos) const {
  const int pico = pico_.load(std::memory_order_relaxed);
  if (pico <= 0) {
    return 0;
  }
  const double decibeles = 20 * std::log10(static_cast<double>(pico) / 32768);
  return std::clamp(static_cast<int>(std::lround((decibeles + 50) / 50 * segmentos)), 0, segmentos);
}

int umbral_de(int por_ciento) {
  const double decibeles = std::clamp(por_ciento, 0, 100) / 2.0 - 50;
  return static_cast<int>(std::lround(32768 * std::pow(10.0, decibeles / 20)));
}

void MotorVoz::pedazo_completo(bool por_voz) {
  const auto tiempo = static_cast<std::uint16_t>(capturadas_total_ * 1000 / calidad_.frecuencia);
  if (!por_voz) {
    codificar(capturado_, tiempo);
    return;
  }
  const int umbral = umbral_;
  const bool pasa = std::any_of(capturado_.begin(), capturado_.begin() + static_cast<std::ptrdiff_t>(por_paquete_),
                                [umbral](std::int16_t muestra) { return std::abs(static_cast<int>(muestra)) >= umbral; });
  const bool abrir = pasa || restante_ > 0;
  restante_ = pasa ? kPedazosTrasCallarse : std::max(restante_ - 1, 0);
  if (abrir && !solo_) {
    if (!abierto_ && hay_anterior_) {
      codificar(anterior_, static_cast<std::uint16_t>(tiempo - por_paquete_ * 1000 / calidad_.frecuencia));
    }
    codificar(capturado_, tiempo);
  }
  if (!abrir) {
    anterior_ = capturado_;
  }
  hay_anterior_ = !abrir;
  abierto_ = abrir;
  if (transmitiendo_.exchange(abrir) != abrir) {
    puente_.cambio.avisar();
  }
}

void MotorVoz::codificar(const Pedazo& pedazo, std::uint16_t tiempo) {
  PaqueteVoz paquete;
  const opus_int32 largo =
      opus_encode(codificador_.get(), pedazo.data(), static_cast<int>(por_paquete_),
                  paquete.opus.data(), static_cast<opus_int32>(paquete.opus.size()));
  opus_int32 en_silencio = 0;
  opus_encoder_ctl(codificador_.get(), OPUS_GET_IN_DTX(&en_silencio));
  if (largo <= kSoloCabecera || en_silencio != 0) {
    return;
  }
  paquete.numero = numero_++;
  paquete.tiempo = tiempo;
  paquete.nivel = nivel_de(gsl::make_span(pedazo).first(por_paquete_));
  paquete.largo = static_cast<std::uint16_t>(largo);
  if (puente_.subida.poner(paquete)) {
    puente_.despertador.despertar();
  }
}

void MotorVoz::recibir() {
  PaqueteVoz llegado;
  while (puente_.bajada.sacar(llegado)) {
    if (Voz* voz = lugar_para(llegado)) {
      voz->nivel = std::max<int>(voz->nivel, llegado.nivel);
      if (voz->buffer.llegar(llegado)) {
        medir(*voz, llegado.tiempo);
      }
    } else {
      sin_lugar_.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

MotorVoz::Voz* MotorVoz::lugar_para(const PaqueteVoz& paquete) {
  const auto buscar = [this](auto condicion) {
    const auto encontrada = std::find_if(voces_.begin(), voces_.end(), condicion);
    return encontrada == voces_.end() ? nullptr : &*encontrada;
  };
  if (paquete.hablante == 0 || por_ciento_de(paquete.hablante) == 0) {
    return nullptr;
  }
  if (Voz* suya = buscar([&paquete](const Voz& voz) { return voz.hablante == paquete.hablante; })) {
    return suya;
  }
  Voz* libre = buscar([](const Voz& voz) {
    return voz.hablante == 0 || (voz.quietos >= kQuietosParaPrestar && voz.buffer.cantidad() == 0);
  });
  Voz& debil = *std::min_element(voces_.begin(), voces_.end(),
                                 [](const Voz& una, const Voz& otra) { return una.nivel < otra.nivel; });
  Voz* elegida = libre != nullptr ? libre : paquete.nivel >= debil.nivel + kVentajaParaEntrar ? &debil : nullptr;
  if (elegida != nullptr) {
    elegida->hablante = paquete.hablante;
    elegida->buffer.reiniciar();
    opus_decoder_ctl(elegida->decodificador, OPUS_RESET_STATE);
    elegida->cuantas = elegida->leidas = 0;
    elegida->nivel = 0;
    elegida->quietos = 0;
    elegida->disimulos = 0;
    elegida->objetivo = kObjetivoMinimo;
    elegida->con_base = false;
  }
  return elegida;
}

void MotorVoz::medir(Voz& voz, std::uint16_t tiempo) {
  const std::uint64_t ahora = en_ms(reproducidas_total_);
  const auto llegada = static_cast<std::uint16_t>(static_cast<std::uint16_t>(ahora) - tiempo);
  voz.resto += static_cast<float>(ahora - voz.ultima_llegada) * kSubidaDeLaBase;
  voz.ultima_llegada = ahora;
  voz.base = static_cast<std::uint16_t>(voz.base + static_cast<int>(voz.resto));
  voz.resto -= std::floor(voz.resto);
  const auto retraso = static_cast<std::int16_t>(static_cast<std::uint16_t>(llegada - voz.base));
  if (voz.con_base && retraso > voz.objetivo) {
    tardes_.fetch_add(1, std::memory_order_relaxed);
  }
  if (voz.con_base && retraso >= 0) {
    voz.objetivo = std::min(std::max(voz.objetivo, retraso + kMargenDelObjetivo), kObjetivoMaximo);
  }
  if (!voz.con_base || retraso < 0 || retraso > kObjetivoMaximo) {
    voz.con_base = true;
    voz.base = llegada;
  }
}

void MotorVoz::personas(gsl::span<const std::pair<std::uint32_t, std::uint8_t>> volumenes) {
  for (std::size_t i = 0; i < personas_.size(); ++i) {
    personas_[i].store(i < volumenes.size() ? std::uint64_t{volumenes[i].first} << 8 | volumenes[i].second : 0,
                       std::memory_order_relaxed);
  }
}

int MotorVoz::por_ciento_de(std::uint32_t hablante) const {
  for (const auto& persona : personas_) {
    const std::uint64_t valor = persona.load(std::memory_order_relaxed);
    if (valor >> 8 == hablante) {
      return static_cast<int>(valor & 0xFF);
    }
  }
  return 100;
}

void MotorVoz::sumar(std::size_t cual, gsl::span<float> mezcla, float ganancia) {
  Voz& voz = voces_[cual];
  const int por_ciento = voz.hablante != 0 ? por_ciento_de(voz.hablante) : 100;
  if (por_ciento == 0) {
    voz.hablante = 0;
    puente_.cambiar(puente_.hablando[cual], std::uint32_t{0});
  }
  ganancia = ganancia * static_cast<float>(por_ciento) / 100;
  for (auto hueco = mezcla; voz.hablante != 0 && !hueco.empty();) {
    if (voz.leidas == voz.cuantas) {
      decodificar(cual, reproducidas_total_ + (mezcla.size() - hueco.size()));
    }
    const std::size_t tramo = std::min(hueco.size(), voz.cuantas - voz.leidas);
    const auto desde = voz.pcm.begin() + static_cast<std::ptrdiff_t>(voz.leidas);
    std::transform(desde, desde + static_cast<std::ptrdiff_t>(tramo), hueco.begin(), hueco.begin(),
                   [ganancia](std::int16_t muestra, float suma) { return suma + muestra * ganancia; });
    voz.leidas += tramo;
    hueco = hueco.subspan(tramo);
  }
}

void MotorVoz::decodificar(std::size_t cual, std::uint64_t muestra) {
  Voz& voz = voces_[cual];
  voz.buffer.esperar(static_cast<std::size_t>(std::lround(voz.objetivo / static_cast<float>(en_ms(por_paquete_)))));
  PaqueteVoz paquete;
  const auto salida = voz.buffer.sacar(paquete);
  const bool disimular = salida == BufferVoz::Salida::perdido || (salida == BufferVoz::Salida::nada && voz.disimulos > 0);
  const int muestras =
      salida == BufferVoz::Salida::paquete
          ? opus_decode(voz.decodificador, paquete.opus.data(), paquete.largo, voz.pcm.data(),
                        static_cast<int>(voz.pcm.size()), 0)
      : disimular ? opus_decode(voz.decodificador, nullptr, 0, voz.pcm.data(), static_cast<int>(por_paquete_), 0)
                  : 0;
  voz.disimulos = salida == BufferVoz::Salida::paquete ? (paquete.nivel >= kNivelFuerte ? kDisimulosAlVaciarse : 0)
                  : salida == BufferVoz::Salida::nada ? std::max(voz.disimulos - 1, 0)
                                                      : voz.disimulos;
  voz.leidas = 0;
  voz.cuantas = muestras > 0 ? static_cast<std::size_t>(muestras) : por_paquete_;
  if (muestras <= 0) {
    std::fill(voz.pcm.begin(), voz.pcm.begin() + static_cast<std::ptrdiff_t>(por_paquete_), std::int16_t{0});
  }
  if (salida == BufferVoz::Salida::paquete && muestras > 0) {
    ajustar(voz, paquete.tiempo, muestra);
  }
  voz.nivel = std::max(voz.nivel - 1, 0);
  voz.quietos = salida == BufferVoz::Salida::nada ? std::min(voz.quietos + 1, kQuietosParaPrestar) : 0;
  puente_.cambiar(puente_.hablando[cual], salida == BufferVoz::Salida::nada ? std::uint32_t{0} : voz.hablante);
}

void MotorVoz::ajustar(Voz& voz, std::uint16_t tiempo, std::uint64_t muestra) {
  const auto demora = static_cast<std::int16_t>(
      static_cast<std::uint16_t>(static_cast<std::uint16_t>(en_ms(muestra)) - tiempo - voz.base));
  const std::size_t antes = voz.cuantas;
  if (demora > voz.objetivo + kHolgura && demora < kRetrasoImposible) {
    voz.cuantas = acortar(gsl::make_span(voz.pcm).first(voz.cuantas), calidad_.frecuencia);
    acelerado_ += antes - voz.cuantas;
  } else if (demora < voz.objetivo - kHolgura && demora > -kRetrasoImposible) {
    voz.cuantas = estirar(voz.pcm, voz.cuantas, calidad_.frecuencia);
    estirado_ += voz.cuantas - antes;
  }
}

void MotorVoz::contar() {
  const auto mayor = std::max_element(voces_.begin(), voces_.end(), [](const Voz& una, const Voz& otra) {
    return una.objetivo < otra.objetivo;
  });
  objetivo_ms_.store(static_cast<std::uint32_t>(mayor->objetivo), std::memory_order_relaxed);
  acelerado_ms_.store(static_cast<std::uint32_t>(en_ms(acelerado_)), std::memory_order_relaxed);
  estirado_ms_.store(static_cast<std::uint32_t>(en_ms(estirado_)), std::memory_order_relaxed);
  std::size_t espera = 0;
  std::size_t recortes = 0;
  std::size_t vaciados = 0;
  std::size_t faltas = 0;
  for (const Voz& voz : voces_) {
    espera = std::max(espera, voz.buffer.cantidad());
    recortes += voz.buffer.recortes();
    vaciados += voz.buffer.vaciados();
    faltas += voz.buffer.faltas();
  }
  espera_ms_.store(static_cast<std::uint32_t>(espera * 20), std::memory_order_relaxed);
  recortes_.store(static_cast<std::uint32_t>(recortes), std::memory_order_relaxed);
  vaciados_.store(static_cast<std::uint32_t>(vaciados), std::memory_order_relaxed);
  faltas_.store(static_cast<std::uint32_t>(faltas), std::memory_order_relaxed);
}

}
