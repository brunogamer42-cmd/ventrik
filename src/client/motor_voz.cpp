#include "client/motor_voz.h"

#include <algorithm>
#include <vector>

#include <windows.h>

#include <opus.h>

#include "common/pendiente.h"

VENTRIK_PENDIENTE("una sola voz por vez y espera fija de 40 ms: mezcla y buffer que se ajusta solo en la etapa 3")

namespace ventrik::cliente {
namespace {
constexpr unsigned kQuietosParaCambiar = 50;
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

void MotorVoz::Borrar::operator()(OpusDecoder* decodificador) const {
  opus_decoder_destroy(decodificador);
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
  decodificador_.reset(opus_decoder_create(static_cast<opus_int32>(calidad_.frecuencia), 1, &error));
  if (error != OPUS_OK) {
    decodificador_.reset();
  }
}

void MotorVoz::procesar(gsl::span<const std::int16_t> entrada,
                        gsl::span<std::int16_t> salida) noexcept {
  if (!ok()) {
    std::fill(salida.begin(), salida.end(), std::int16_t{0});
    return;
  }
  if (!transmitiendo_) {
    capturadas_ = 0;
    entrada = {};
  }
  while (!entrada.empty()) {
    const auto parte = entrada.first(std::min(entrada.size(), por_paquete_ - capturadas_));
    std::copy(parte.begin(), parte.end(), capturado_.begin() + static_cast<std::ptrdiff_t>(capturadas_));
    capturadas_ += parte.size();
    entrada = entrada.subspan(parte.size());
    if (capturadas_ == por_paquete_) {
      codificar();
      capturadas_ = 0;
    }
  }
  for (std::int16_t& muestra : salida) {
    if (reproducidas_ == por_reproducir_) {
      decodificar();
    }
    muestra = reproducir_[reproducidas_++];
  }
}

void MotorVoz::codificar() {
  PaqueteVoz paquete;
  const opus_int32 largo =
      opus_encode(codificador_.get(), capturado_.data(), static_cast<int>(por_paquete_),
                  paquete.opus.data(), static_cast<opus_int32>(paquete.opus.size()));
  if (largo <= 0) {
    return;
  }
  paquete.numero = numero_++;
  paquete.largo = static_cast<std::uint16_t>(largo);
  if (puente_.subida.poner(paquete)) {
    puente_.despertador.despertar();
  }
}

void MotorVoz::decodificar() {
  PaqueteVoz llegado;
  while (puente_.bajada.sacar(llegado)) {
    if (hablante_ == 0) {
      hablante_ = llegado.hablante;
      buffer_.reiniciar();
      opus_decoder_ctl(decodificador_.get(), OPUS_RESET_STATE);
    }
    if (llegado.hablante == hablante_) {
      buffer_.llegar(llegado);
    }
  }
  PaqueteVoz paquete;
  const auto salida = buffer_.sacar(paquete);
  espera_ms_.store(static_cast<std::uint32_t>(buffer_.cantidad() * 20), std::memory_order_relaxed);
  const int muestras =
      salida == BufferVoz::Salida::paquete
          ? opus_decode(decodificador_.get(), paquete.opus.data(), paquete.largo,
                        reproducir_.data(), static_cast<int>(reproducir_.size()), 0)
      : salida == BufferVoz::Salida::perdido
          ? opus_decode(decodificador_.get(), nullptr, 0, reproducir_.data(),
                        static_cast<int>(por_paquete_), 0)
          : 0;
  reproducidas_ = 0;
  por_reproducir_ = muestras > 0 ? static_cast<std::size_t>(muestras) : por_paquete_;
  if (muestras <= 0) {
    std::fill(reproducir_.begin(), reproducir_.begin() + static_cast<std::ptrdiff_t>(por_paquete_),
              std::int16_t{0});
  }
  quietos_ = salida == BufferVoz::Salida::nada ? quietos_ + 1 : 0;
  if (quietos_ > kQuietosParaCambiar) {
    hablante_ = 0;
  }
  puente_.hablando = salida == BufferVoz::Salida::nada ? 0 : hablante_;
}

}
