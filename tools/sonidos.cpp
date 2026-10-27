#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>

#include <gsl/gsl>
#include <miniaudio.h>

#include "client/sonidos.h"
#include "common/archivos.h"

namespace {
constexpr int kCasiSilencio = 64;
constexpr std::size_t kColaDeSilencio = ventrik::cliente::kFrecuenciaDeSonidos / 20;

std::wstring buscar(const std::wstring& carpeta, std::size_t numero) {
  const std::wstring patron = carpeta + L"\\" + (numero < 10 ? L"0" : L"") + std::to_wstring(numero) + L"_*.wav";
  WIN32_FIND_DATAW encontrado = {};
  const HANDLE busqueda = FindFirstFileW(patron.c_str(), &encontrado);
  if (busqueda == INVALID_HANDLE_VALUE) {
    return {};
  }
  FindClose(busqueda);
  return carpeta + L"\\" + encontrado.cFileName;
}

ventrik::cliente::Muestras leer(const std::wstring& ruta, std::size_t maximo, bool recortar) {
  ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 1, ventrik::cliente::kFrecuenciaDeSonidos);
  const auto decodificador = std::make_unique<ma_decoder>();
  if (ma_decoder_init_file_w(ruta.c_str(), &config, decodificador.get()) != MA_SUCCESS) {
    return {};
  }
  const auto cerrar = gsl::finally([&decodificador] { ma_decoder_uninit(decodificador.get()); });
  ventrik::cliente::Muestras muestras(maximo * 2);
  ma_uint64 leidas = 0;
  ma_decoder_read_pcm_frames(decodificador.get(), muestras.data(), muestras.size(), &leidas);
  muestras.resize(static_cast<std::size_t>(leidas));
  if (!recortar) {
    return muestras;
  }
  const auto se_oye = [](std::int16_t muestra) { return std::abs(static_cast<int>(muestra)) > kCasiSilencio; };
  const auto desde = std::find_if(muestras.begin(), muestras.end(), se_oye);
  const auto hasta = std::find_if(muestras.rbegin(), muestras.rend(), se_oye).base();
  if (desde >= hasta) {
    return {};
  }
  return ventrik::cliente::Muestras(desde, hasta + std::min<std::ptrdiff_t>(kColaDeSilencio, muestras.end() - hasta));
}

}

int wmain(int cantidad, wchar_t** argumentos) {
  if (cantidad != 3) {
    return 1;
  }
  const std::wstring entrada = argumentos[1];
  const std::wstring salida = argumentos[2];
  std::size_t total = 0;
  for (std::size_t cual = 0; cual < ventrik::cliente::kSonidos; ++cual) {
    const auto sonido = static_cast<ventrik::cliente::Sonido>(cual);
    const auto maximo = ventrik::cliente::maximo_de_muestras(sonido);
    const bool llamada = sonido == ventrik::cliente::Sonido::llamada_entrante || sonido == ventrik::cliente::Sonido::llamando;
    const auto muestras = leer(buscar(entrada, cual + 1), maximo, !llamada);
    const auto vksn = muestras.size() <= maximo ? ventrik::cliente::codificar_vksn(muestras) : std::vector<std::uint8_t>();
    const std::wstring destino = salida + L"\\" + (cual < 9 ? L"0" : L"") + std::to_wstring(cual + 1) + L".vksn";
    if (vksn.empty() || !ventrik::escribir_archivo_completo(destino, vksn)) {
      std::printf("%02zu: -\n", cual + 1);
      return 2;
    }
    total += vksn.size();
    std::printf("%02zu: %.2f s, %zu B\n", cual + 1, static_cast<double>(muestras.size()) / 48000, vksn.size());
  }
  std::printf("%zu B\n", total);
  return 0;
}
