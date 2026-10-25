#include "client/ui/ventana.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

struct IUnknown;
#include <d3d9.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

#include "client/ui/piezas.h"
#include "client/ui/theme.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND ventana, UINT mensaje, WPARAM wparam,
                                                             LPARAM lparam);

namespace ventrik::ui {
namespace {
constexpr wchar_t kClase[] = L"VentrikVentanaPrincipal";
constexpr int kAnchoReferencia = 1376;
constexpr int kAltoReferencia = 768;
constexpr int kAnchoMinimo = 1100;
constexpr int kAltoMinimo = 640;
constexpr int kCuadrosTrasEvento = 3;
constexpr DWORD kMilisegundosPorCuadro = 15;
constexpr DWORD kEsperaConMouseEncima = 100;
constexpr DWORD kTiempoParaCarteles = 1500;
constexpr DWORD kVecesElCuadroEntreCuadros = 4;

using Reloj = std::chrono::steady_clock;

double ms_desde(Reloj::time_point desde) {
  return std::chrono::duration<double, std::milli>(Reloj::now() - desde).count();
}

template <typename T>
std::string_view como_bytes(const ImVector<T>& datos) {
  return std::string_view(reinterpret_cast<const char*>(datos.Data), static_cast<std::size_t>(datos.size_in_bytes()));
}

struct Soltar {
  void operator()(IUnknown* objeto) const { objeto->Release(); }
};

float escala_de_pantalla() {
  using Avisar = BOOL(WINAPI*)();
  const HMODULE user32 = GetModuleHandleW(L"user32.dll");
  const auto avisar =
      user32 != nullptr ? reinterpret_cast<Avisar>(GetProcAddress(user32, "SetProcessDPIAware")) : nullptr;
  if (avisar != nullptr) {
    avisar();
  }
  const HDC pantalla = GetDC(nullptr);
  const int puntos = pantalla != nullptr ? GetDeviceCaps(pantalla, LOGPIXELSX) : 0;
  ReleaseDC(nullptr, pantalla);
  return puntos > 0 ? static_cast<float>(puntos) / 96.0f : 1.0f;
}

}

struct Ventana::Grafica {
  std::unique_ptr<IDirect3D9, Soltar> d3d;
  std::unique_ptr<IDirect3DDevice9, Soltar> dispositivo;
  std::unique_ptr<IDirect3DTexture9, Soltar> logo;
  D3DPRESENT_PARAMETERS presentar{};
  bool win32 = false;
  bool dx9 = false;
  bool por_procesador = false;
};

Ventana::Ventana(const std::wstring& titulo, int como_mostrar)
    : grafica_(std::make_unique<Grafica>()), como_mostrar_(como_mostrar) {
  const float escala = escala_ = escala_de_pantalla();
  WNDCLASSEXW clase = {};
  clase.cbSize = sizeof(clase);
  clase.lpfnWndProc = procesar;
  clase.hInstance = GetModuleHandleW(nullptr);
  clase.hIcon = LoadIconW(clase.hInstance, L"VENTRIK");
  clase.hIconSm = static_cast<HICON>(LoadImageW(clase.hInstance, L"VENTRIK", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                                GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
  clase.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  clase.hbrBackground = CreateSolidBrush(RGB(0x16, 0x1B, 0x1E));
  clase.lpszClassName = kClase;
  RegisterClassExW(&clase);

  RECT area = {0, 0, static_cast<LONG>(kAnchoReferencia * escala), static_cast<LONG>(kAltoReferencia * escala)};
  AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
  RECT trabajo = {};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &trabajo, 0);
  const int ancho = area.right - area.left;
  const int alto = area.bottom - area.top;
  if (ancho > trabajo.right - trabajo.left || alto > trabajo.bottom - trabajo.top) {
    como_mostrar_ = SW_SHOWMAXIMIZED;
  }
  ventana_ = CreateWindowExW(0, kClase, titulo.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             ancho, alto, nullptr, nullptr, clase.hInstance, this);
  if (ventana_ == nullptr || !preparar_dispositivo() || !preparar_interfaz(escala)) {
    return;
  }
  ImGui::GetIO().ConfigDebugHighlightIdConflicts = false;
  cargar_logo(escala);
  grafica_->win32 = ImGui_ImplWin32_Init(ventana_);
  grafica_->dx9 = grafica_->win32 && ImGui_ImplDX9_Init(grafica_->dispositivo.get());
  lista_ = grafica_->dx9;
}

Ventana::~Ventana() {
  if (grafica_->dx9) {
    ImGui_ImplDX9_Shutdown();
  }
  if (grafica_->win32) {
    ImGui_ImplWin32_Shutdown();
  }
  if (ImGui::GetCurrentContext() != nullptr) {
    ImGui::DestroyContext();
  }
  grafica_->logo.reset();
  grafica_->dispositivo.reset();
  if (ventana_ != nullptr && IsWindow(ventana_)) {
    DestroyWindow(ventana_);
  }
}

bool Ventana::preparar_dispositivo() {
  grafica_->d3d.reset(Direct3DCreate9(D3D_SDK_VERSION));
  D3DCAPS9 capacidades = {};
  if (!grafica_->d3d ||
      FAILED(grafica_->d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &capacidades)) ||
      (capacidades.Caps2 & D3DCAPS2_DYNAMICTEXTURES) == 0) {
    return false;
  }
  D3DPRESENT_PARAMETERS& presentar = grafica_->presentar;
  presentar.Windowed = TRUE;
  presentar.SwapEffect = D3DSWAPEFFECT_DISCARD;
  presentar.BackBufferFormat = D3DFMT_UNKNOWN;
  presentar.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
  for (const DWORD vertices : {D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DCREATE_SOFTWARE_VERTEXPROCESSING}) {
    IDirect3DDevice9* dispositivo = nullptr;
    if (SUCCEEDED(grafica_->d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, ventana_,
                                              vertices | D3DCREATE_FPU_PRESERVE, &presentar, &dispositivo))) {
      grafica_->dispositivo.reset(dispositivo);
      grafica_->por_procesador = vertices == D3DCREATE_SOFTWARE_VERTEXPROCESSING;
      return true;
    }
  }
  return false;
}

ImTextureID Ventana::logo() const {
  return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(grafica_->logo.get()));
}

std::string Ventana::placa() const {
  D3DADAPTER_IDENTIFIER9 placa = {};
  grafica_->d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &placa);
  return placa.Description;
}

bool Ventana::por_procesador() const { return grafica_->por_procesador; }

void Ventana::cargar_logo(float escala) {
  const int lado = 42 * escala <= 64 ? 64 : 128;
  const auto icono = static_cast<HICON>(
      LoadImageW(GetModuleHandleW(nullptr), L"VENTRIK", IMAGE_ICON, lado, lado, LR_DEFAULTCOLOR));
  ICONINFO partes = {};
  if (icono == nullptr) {
    return;
  }
  if (!GetIconInfo(icono, &partes)) {
    DestroyIcon(icono);
    return;
  }
  BITMAPINFO formato = {};
  formato.bmiHeader.biSize = sizeof(formato.bmiHeader);
  formato.bmiHeader.biWidth = lado;
  formato.bmiHeader.biHeight = -lado;
  formato.bmiHeader.biPlanes = 1;
  formato.bmiHeader.biBitCount = 32;
  formato.bmiHeader.biCompression = BI_RGB;
  std::vector<std::uint32_t> pixeles(static_cast<std::size_t>(lado) * lado);
  const HDC pantalla = GetDC(nullptr);
  const bool leido = GetDIBits(pantalla, partes.hbmColor, 0, lado, pixeles.data(), &formato, DIB_RGB_COLORS) == lado;
  ReleaseDC(nullptr, pantalla);
  DeleteObject(partes.hbmColor);
  DeleteObject(partes.hbmMask);
  DestroyIcon(icono);
  IDirect3DTexture9* textura = nullptr;
  if (!leido || FAILED(grafica_->dispositivo->CreateTexture(lado, lado, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                                            &textura, nullptr))) {
    return;
  }
  grafica_->logo.reset(textura);
  D3DLOCKED_RECT bloque = {};
  if (FAILED(textura->LockRect(0, &bloque, nullptr, 0))) {
    grafica_->logo.reset();
    return;
  }
  for (int fila = 0; fila < lado; ++fila) {
    std::copy_n(pixeles.begin() + fila * lado, lado,
                reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(bloque.pBits) + fila * bloque.Pitch));
  }
  textura->UnlockRect(0);
}

bool Ventana::recuperar_dispositivo() {
  IDirect3DDevice9& dispositivo = *grafica_->dispositivo;
  const HRESULT estado = dispositivo.TestCooperativeLevel();
  if (estado == D3DERR_DEVICELOST) {
    return false;
  }
  if (estado == D3DERR_DEVICENOTRESET || ancho_nuevo_ != 0) {
    if (ancho_nuevo_ != 0) {
      grafica_->presentar.BackBufferWidth = ancho_nuevo_;
      grafica_->presentar.BackBufferHeight = alto_nuevo_;
      ancho_nuevo_ = alto_nuevo_ = 0;
    }
    grafica_->presentar.BackBufferFormat = D3DFMT_UNKNOWN;
    ImGui_ImplDX9_InvalidateDeviceObjects();
    const bool reiniciado = SUCCEEDED(dispositivo.Reset(&grafica_->presentar));
    ImGui_ImplDX9_CreateDeviceObjects();
    return reiniciado;
  }
  return true;
}

std::size_t huella_del_dibujo(const ImDrawData& dibujo) {
  std::size_t huella = 0;
  for (const ImDrawList* lista : dibujo.CmdLists) {
    for (const std::string_view bytes : {como_bytes(lista->VtxBuffer), como_bytes(lista->IdxBuffer),
                                         como_bytes(lista->CmdBuffer)}) {
      huella = huella * 31 + std::hash<std::string_view>()(bytes);
    }
  }
  return huella;
}

bool pide_cuadros(UINT mensaje, WPARAM tecla, LPARAM datos, bool escribiendo, std::optional<LPARAM>& ultimo_mouse) {
  if (mensaje == WM_MOUSEMOVE) {
    const bool movido = ultimo_mouse != datos;
    ultimo_mouse = datos;
    return movido;
  }
  if (mensaje == WM_MOUSELEAVE) {
    ultimo_mouse.reset();
  }
  const bool repetida = (mensaje == WM_KEYDOWN || mensaje == WM_SYSKEYDOWN) && (datos & 0x40000000) != 0;
  const bool modificador =
      tecla == VK_SHIFT || tecla == VK_CONTROL || tecla == VK_MENU || tecla == VK_LWIN || tecla == VK_RWIN;
  return mensaje != WM_NCMOUSEMOVE && (!repetida || (escribiendo && !modificador));
}

DWORD espera_entre_cuadros(double ultimo_ms) {
  return std::max(kMilisegundosPorCuadro, static_cast<DWORD>(ultimo_ms * (kVecesElCuadroEntreCuadros + 1)));
}

bool Ventana::dibujar_cuadro(const std::function<void()>& dibujar) {
  if (perdido_ || ancho_nuevo_ != 0) {
    if (!recuperar_dispositivo()) {
      perdido_ = true;
      return false;
    }
    presentar_igual_ = true;
  }
  perdido_ = false;
  const auto empezo = Reloj::now();
  ImGui_ImplDX9_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  pantalla_completa(dibujar);
  ImGui::Render();
  const ImDrawData& datos = *ImGui::GetDrawData();
  const std::size_t huella = huella_del_dibujo(datos);
  uso_.imgui_ms += ms_desde(empezo);
  uso_.vertices_max = std::max(uso_.vertices_max, datos.TotalVtxCount);
  if (huella == huella_ && !presentar_igual_) {
    return false;
  }
  huella_ = huella;
  presentar_igual_ = false;
  const auto armado = Reloj::now();
  IDirect3DDevice9& dispositivo = *grafica_->dispositivo;
  dispositivo.Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0x16, 0x1B, 0x1E), 1.0f, 0);
  if (SUCCEEDED(dispositivo.BeginScene())) {
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    dispositivo.EndScene();
  }
  perdido_ = dispositivo.Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST;
  uso_.direct3d_ms += ms_desde(armado);
  ++uso_.presentados;
  if (!IsWindowVisible(ventana_)) {
    ShowWindow(ventana_, como_mostrar_);
  }
  return true;
}

int Ventana::correr(const std::function<void()>& atender, const std::function<void()>& dibujar) {
  int pendientes = kCuadrosTrasEvento;
  DWORD entre_cuadros = kMilisegundosPorCuadro;
  DWORD ultimo = GetTickCount() - entre_cuadros;
  DWORD ultimo_mensaje = GetTickCount();
  std::optional<LPARAM> ultimo_mouse;
  for (;;) {
    const auto empezo = Reloj::now();
    MSG mensaje = {};
    while (PeekMessageW(&mensaje, nullptr, 0, 0, PM_REMOVE)) {
      if (mensaje.message == WM_QUIT) {
        return static_cast<int>(mensaje.wParam);
      }
      ++uso_.mensajes[mensaje.message];
      presentar_igual_ = presentar_igual_ || mensaje.message == WM_PAINT;
      const bool pide =
          pide_cuadros(mensaje.message, mensaje.wParam, mensaje.lParam, ImGui::GetIO().WantTextInput, ultimo_mouse);
      TranslateMessage(&mensaje);
      DispatchMessageW(&mensaje);
      if (pide) {
        pendientes = kCuadrosTrasEvento;
        ultimo_mensaje = GetTickCount();
      }
    }
    atender();
    const bool minimizada = IsIconic(ventana_) != FALSE;
    const DWORD pasado = GetTickCount() - ultimo;
    DWORD espera = INFINITE;
    if (pendientes > 0 && !minimizada) {
      espera = pasado < entre_cuadros ? entre_cuadros - pasado : 0;
    } else if (!minimizada && (perdido_ || (ImGui::IsAnyItemHovered() &&
                                            GetTickCount() - ultimo_mensaje < kTiempoParaCarteles))) {
      espera = kEsperaConMouseEncima;
    }
    if (espera == 0) {
      ultimo = GetTickCount();
      const auto cuadro = Reloj::now();
      pendientes = dibujar_cuadro(dibujar) ? pendientes - 1 : 0;
      entre_cuadros = espera_entre_cuadros(ms_desde(cuadro));
    }
    const double pasada = ms_desde(empezo);
    uso_.ocupado_ms += pasada;
    uso_.pasada_maxima_ms = std::max(uso_.pasada_maxima_ms, pasada);
    if (espera != 0 && MsgWaitForMultipleObjects(0, nullptr, FALSE, espera, QS_ALLINPUT) == WAIT_TIMEOUT &&
        pendientes == 0) {
      pendientes = 1;
    }
  }
}

LRESULT CALLBACK Ventana::procesar(HWND ventana, UINT mensaje, WPARAM wparam, LPARAM lparam) {
  if (ImGui_ImplWin32_WndProcHandler(ventana, mensaje, wparam, lparam)) {
    return 1;
  }
  auto* yo = reinterpret_cast<Ventana*>(GetWindowLongPtrW(ventana, GWLP_USERDATA));
  switch (mensaje) {
    case WM_NCCREATE:
      SetWindowLongPtrW(ventana, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams));
      break;
    case WM_SIZE:
      if (yo != nullptr && wparam != SIZE_MINIMIZED && LOWORD(lparam) > 0 && HIWORD(lparam) > 0) {
        yo->ancho_nuevo_ = LOWORD(lparam);
        yo->alto_nuevo_ = HIWORD(lparam);
      }
      return 0;
    case WM_SYSCOMMAND:
      if ((wparam & 0xFFF0) == SC_KEYMENU && lparam == 0) {
        return 0;
      }
      break;
    case WM_GETMINMAXINFO:
      if (yo != nullptr) {
        RECT minimo = {0, 0, static_cast<LONG>(kAnchoMinimo * yo->escala_), static_cast<LONG>(kAltoMinimo * yo->escala_)};
        AdjustWindowRect(&minimo, WS_OVERLAPPEDWINDOW, FALSE);
        MONITORINFO monitor = {};
        monitor.cbSize = sizeof(monitor);
        GetMonitorInfoW(MonitorFromWindow(ventana, MONITOR_DEFAULTTONEAREST), &monitor);
        reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize = {
            std::min(minimo.right - minimo.left, monitor.rcWork.right - monitor.rcWork.left),
            std::min(minimo.bottom - minimo.top, monitor.rcWork.bottom - monitor.rcWork.top)};
      }
      return 0;
    case WM_ENDSESSION:
      if (yo != nullptr && wparam != FALSE && yo->al_terminar_) {
        yo->al_terminar_();
      }
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(ventana, mensaje, wparam, lparam);
}

}
