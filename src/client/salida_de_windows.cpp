#include "client/salida_de_windows.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <mmdeviceapi.h>

namespace ventrik::cliente {
namespace {
constexpr PROPERTYKEY kFormaDeLaSalida = {{0x1da5d803, 0xd492, 0x4edd, {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e}},
                                          0};
constexpr std::array<float, 4> kFactores = {1.0f, 0.6f, 0.35f, 0.15f};

template <typename Interfaz>
using Com = std::unique_ptr<Interfaz, LiberarCom>;

template <typename Interfaz, typename Fuente>
Com<Interfaz> pedir(Fuente& fuente) {
  Interfaz* interfaz = nullptr;
  return Com<Interfaz>(SUCCEEDED(fuente.QueryInterface(__uuidof(Interfaz), reinterpret_cast<void**>(&interfaz)))
                           ? interfaz
                           : nullptr);
}

Com<IMMDevice> salida_principal() {
  IMMDeviceEnumerator* lista = nullptr;
  if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                              reinterpret_cast<void**>(&lista)))) {
    return nullptr;
  }
  const Com<IMMDeviceEnumerator> dispositivos(lista);
  IMMDevice* salida = nullptr;
  return Com<IMMDevice>(SUCCEEDED(dispositivos->GetDefaultAudioEndpoint(eRender, eConsole, &salida)) ? salida
                                                                                                    : nullptr);
}

}

bool con_auriculares() {
  const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool hay = false;
  {
    const auto salida = salida_principal();
    IPropertyStore* lista = nullptr;
    if (salida && SUCCEEDED(salida->OpenPropertyStore(STGM_READ, &lista))) {
      const Com<IPropertyStore> propiedades(lista);
      PROPVARIANT forma;
      PropVariantInit(&forma);
      hay = SUCCEEDED(propiedades->GetValue(kFormaDeLaSalida, &forma)) && forma.vt == VT_UI4 &&
            (forma.ulVal == Headphones || forma.ulVal == Headset);
      PropVariantClear(&forma);
    }
  }
  if (SUCCEEDED(com)) {
    CoUninitialize();
  }
  return hay;
}

float factor_de_bajada(std::uint8_t cuanto) { return kFactores[std::min<std::size_t>(cuanto, kFactores.size() - 1)]; }

OtrosProgramas::~OtrosProgramas() {
  devolver();
  if (com_) {
    CoUninitialize();
  }
}

void OtrosProgramas::bajar(float factor) {
  if (!com_) {
    com_ = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
  }
  const auto salida = salida_principal();
  IAudioSessionManager2* gestor = nullptr;
  if (!bajados_.empty() || !salida ||
      FAILED(salida->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&gestor)))) {
    return;
  }
  const Com<IAudioSessionManager2> sesiones(gestor);
  IAudioSessionEnumerator* lista = nullptr;
  if (FAILED(sesiones->GetSessionEnumerator(&lista))) {
    return;
  }
  const Com<IAudioSessionEnumerator> todas(lista);
  int cuantas = 0;
  todas->GetCount(&cuantas);
  for (int i = 0; i < cuantas; ++i) {
    IAudioSessionControl* control = nullptr;
    if (FAILED(todas->GetSession(i, &control))) {
      continue;
    }
    const Com<IAudioSessionControl> sesion(control);
    const auto datos = pedir<IAudioSessionControl2>(*sesion);
    DWORD proceso = 0;
    if (!datos || datos->GetProcessId(&proceso) != S_OK || proceso == 0 || proceso == GetCurrentProcessId() ||
        datos->IsSystemSoundsSession() == S_OK) {
      continue;
    }
    auto volumen = pedir<ISimpleAudioVolume>(*sesion);
    float antes = 1;
    if (volumen && SUCCEEDED(volumen->GetMasterVolume(&antes)) &&
        SUCCEEDED(volumen->SetMasterVolume(antes * factor, nullptr))) {
      bajados_.push_back({std::move(volumen), antes, antes * factor});
    }
  }
}

void OtrosProgramas::devolver() {
  for (const Bajado& bajado : bajados_) {
    float ahora = 0;
    if (SUCCEEDED(bajado.volumen->GetMasterVolume(&ahora)) && std::abs(ahora - bajado.puesto) < 0.01f) {
      bajado.volumen->SetMasterVolume(bajado.antes, nullptr);
    }
  }
  bajados_.clear();
}

}
