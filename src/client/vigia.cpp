#include "client/vigia.h"

#include <algorithm>
#include <chrono>

#include <windows.h>

namespace ventrik::cliente {
namespace {
using Reloj = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr wchar_t kClaseOyente[] = L"VentrikVigia";
constexpr DWORD kConsultaRapida = 20;
constexpr DWORD kRespaldo = 250;
constexpr auto kCadaNivel = 250ms;
constexpr USHORT kTeclado = 0x06;
constexpr USHORT kEscritorio = 0x01;
constexpr USHORT kShiftDerecho = 0x36;

bool apretada(int tecla) {
  return (GetAsyncKeyState(tecla) & 0x8000) != 0;
}

bool es_boton_del_mouse(std::uint8_t tecla) {
  return tecla == VK_LBUTTON || tecla == VK_RBUTTON || tecla == VK_MBUTTON || tecla == VK_XBUTTON1 ||
         tecla == VK_XBUTTON2;
}

HWND crear_oyente() {
  WNDCLASSEXW clase = {};
  clase.cbSize = sizeof(clase);
  clase.lpfnWndProc = DefWindowProcW;
  clase.hInstance = GetModuleHandleW(nullptr);
  clase.lpszClassName = kClaseOyente;
  RegisterClassExW(&clase);
  return CreateWindowExW(0, kClaseOyente, nullptr, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, clase.hInstance, nullptr);
}

void escuchar_teclado(HWND oyente, bool si) {
  RAWINPUTDEVICE teclado = {kEscritorio, kTeclado, si ? DWORD{RIDEV_INPUTSINK} : DWORD{RIDEV_REMOVE}, si ? oyente : nullptr};
  if (oyente != nullptr) {
    RegisterRawInputDevices(&teclado, 1, sizeof(teclado));
  }
}

}

Teclas teclas_apretadas() {
  Teclas teclas;
  for (int tecla = 1; tecla < 0xFF; ++tecla) {
    teclas[static_cast<std::size_t>(tecla)] = apretada(tecla);
  }
  return teclas;
}

int tecla_nueva(const Teclas& antes, const Teclas& ahora) {
  if (ahora[VK_ESCAPE] && !antes[VK_ESCAPE]) {
    return 0;
  }
  for (int tecla = VK_MBUTTON; tecla < 0xFF; ++tecla) {
    const auto lugar = static_cast<std::size_t>(tecla);
    if (ahora[lugar] && !antes[lugar] && es_tecla_valida(static_cast<std::uint8_t>(tecla))) {
      return tecla;
    }
  }
  return -1;
}

bool es_la_tecla(std::uint8_t elegida, const RAWKEYBOARD& tecla) {
  const bool derecha = (tecla.Flags & RI_KEY_E0) != 0;
  const USHORT exacta = tecla.VKey == VK_SHIFT     ? (tecla.MakeCode == kShiftDerecho ? VK_RSHIFT : VK_LSHIFT)
                        : tecla.VKey == VK_CONTROL ? (derecha ? VK_RCONTROL : VK_LCONTROL)
                        : tecla.VKey == VK_MENU    ? (derecha ? VK_RMENU : VK_LMENU)
                                                   : tecla.VKey;
  return elegida == tecla.VKey || elegida == exacta;
}

Vigia::Vigia(MotorVoz& motor, const PuenteVoz& puente, std::uint8_t tecla, HWND ventana,
             std::function<void()> avisar)
    : motor_(motor), puente_(puente), ventana_(ventana), avisar_(std::move(avisar)), tecla_(tecla),
      despertar_(CreateEventW(nullptr, FALSE, FALSE, nullptr), &CloseHandle), hilo_([this] { correr(); }) {}

Vigia::~Vigia() {
  terminar_ = true;
  SetEvent(despertar_.get());
  hilo_.join();
  motor_.transmitir(false);
}

void Vigia::por_voz(bool si) {
  if (si && eligiendo_.exchange(false)) {
    elegida_ = 0;
  }
  por_voz_ = si;
  motor_.transmitir(false);
  SetEvent(despertar_.get());
}

void Vigia::medir(bool si) {
  medir_ = si;
  SetEvent(despertar_.get());
}

void Vigia::elegir_tecla() {
  elegida_ = -1;
  eligiendo_ = true;
  SetEvent(despertar_.get());
}

std::optional<std::uint8_t> Vigia::tomar_elegida() {
  const int elegida = elegida_.exchange(-1);
  return elegida < 0 ? std::nullopt : std::optional<std::uint8_t>(static_cast<std::uint8_t>(elegida));
}

void Vigia::correr() {
  const HWND oyente = crear_oyente();
  auto proximo_nivel = Reloj::now();
  Hablando hablando = puente_.quienes();
  EstadoVoz estado = puente_.estado;
  Teclas antes;
  bool empezando = true;
  bool esperar_soltar = false;
  bool transmitia = false;
  bool escucha_teclado = false;
  std::optional<Reloj::time_point> soltar_en;
  const HANDLE avisos[] = {despertar_.get(), puente_.cambio.evento()};
  while (!terminar_) {
    const bool con_tecla = !por_voz_;
    if (!con_tecla || eligiendo_) {
      soltar_en.reset();
    }
    if (con_tecla != escucha_teclado) {
      escucha_teclado = con_tecla;
      escuchar_teclado(oyente, con_tecla);
    }
    const DWORD hasta_soltar =
        soltar_en ? static_cast<DWORD>(std::max<long long>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(*soltar_en - Reloj::now()).count(), 0))
                  : INFINITE;
    const DWORD espera = std::min(eligiendo_ || (con_tecla && es_boton_del_mouse(tecla_)) ? kConsultaRapida
                                  : con_tecla || medir_                                   ? kRespaldo
                                                                                          : INFINITE,
                                  hasta_soltar);
    if (MsgWaitForMultipleObjects(2, avisos, FALSE, espera, QS_ALLINPUT) == WAIT_FAILED) {
      Sleep(espera == INFINITE ? kRespaldo : espera);
    }
    const bool por_voz = por_voz_;
    std::optional<bool> por_evento;
    MSG mensaje = {};
    while (PeekMessageW(&mensaje, nullptr, 0, 0, PM_REMOVE)) {
      RAWINPUT datos = {};
      UINT largo = sizeof(datos);
      if (mensaje.message == WM_INPUT &&
          GetRawInputData(reinterpret_cast<HRAWINPUT>(mensaje.lParam), RID_INPUT, &datos, &largo,
                          sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1) &&
          datos.header.dwType == RIM_TYPEKEYBOARD && es_la_tecla(tecla_, datos.data.keyboard)) {
        por_evento = (datos.data.keyboard.Flags & RI_KEY_BREAK) == 0;
      }
      DispatchMessageW(&mensaje);
    }
    bool cambio = false;
    if (eligiendo_) {
      if (motor_.transmitiendo()) {
        motor_.transmitir(false);
        cambio = true;
      }
      const Teclas ahora = teclas_apretadas();
      const int elegida = GetForegroundWindow() != ventana_ ? 0 : empezando ? -1 : tecla_nueva(antes, ahora);
      antes = ahora;
      empezando = false;
      if (elegida >= 0) {
        elegida_ = elegida;
        eligiendo_ = false;
        esperar_soltar = elegida != 0;
        cambio = true;
      }
    } else if (!por_voz) {
      empezando = true;
      const bool hablar =
          por_evento.value_or(apretada(tecla_)) && (!solo_con_foco_ || GetForegroundWindow() == ventana_);
      esperar_soltar = (esperar_soltar || soltar_primero_.exchange(false)) && hablar;
      if (hablar) {
        soltar_en.reset();
      } else if (motor_.transmitiendo() && !soltar_en) {
        soltar_en = Reloj::now() + std::chrono::milliseconds(seguir_al_soltar_.load());
      }
      const bool sigue = hablar || (soltar_en && Reloj::now() < *soltar_en);
      if (!sigue) {
        soltar_en.reset();
      }
      if (!esperar_soltar && sigue != motor_.transmitiendo()) {
        motor_.transmitir(sigue);
      }
    } else {
      empezando = true;
    }
    const bool transmite = motor_.transmitiendo();
    if (transmite != transmitia || puente_.quienes() != hablando || puente_.estado != estado) {
      transmitia = transmite;
      hablando = puente_.quienes();
      estado = puente_.estado;
      cambio = true;
    }
    const auto ahora = Reloj::now();
    if (medir_ && ahora >= proximo_nivel) {
      proximo_nivel = ahora + kCadaNivel;
      const int nuevo = motor_.nivel(kSegmentosDelMedidor);
      const int viejo = nivel_.exchange(nuevo);
      cambio = cambio || nuevo != viejo;
    }
    if (cambio) {
      avisar_();
    }
  }
  escuchar_teclado(oyente, false);
  if (oyente != nullptr) {
    DestroyWindow(oyente);
  }
}

}
