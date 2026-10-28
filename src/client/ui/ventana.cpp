#include "client/ui/ventana.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

struct IUnknown;
#include <d3d9.h>
#include <shellapi.h>

#include <imgui.h>
#include <imgui_impl_win32.h>

#include "client/ui/dibujante.h"
#include "client/ui/piezas.h"
#include "client/ui/theme.h"
#include "common/sistema.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND ventana, UINT mensaje, WPARAM wparam,
                                                             LPARAM lparam);

namespace ventrik::ui {
namespace {
constexpr wchar_t kClase[] = L"VentrikVentanaPrincipal";
constexpr wchar_t kUnico[] = L"Global\\VentrikUnico";
constexpr int kAnchoReferencia = 1376;
constexpr int kAltoReferencia = 768;
constexpr int kAnchoMinimo = 1100;
constexpr int kAltoMinimo = 640;
constexpr int kCuadrosTrasEvento = 3;
constexpr DWORD kMilisegundosPorCuadro = 15;
constexpr DWORD kMilisegundosConMouse = 66;
constexpr DWORD kEsperaConMouseEncima = 100;
constexpr DWORD kTiempoParaCarteles = 1500;
constexpr DWORD kVecesElCuadroEntreCuadros = 4;
constexpr UINT_PTR kTic = 1;
constexpr UINT kMilisegundosPorTic = 1000;
constexpr ULONG kToleranciaDelTic = 50;
constexpr DWORD kEsperaParaRecrear = 2000;
constexpr UINT kCambioDeEnergia = 0x8013;
constexpr int kJuegoDeDirect3D = 3;
constexpr GUID kAhorroDeBateria = {0xE00958C0, 0xC213, 0x4ACE, {0xAC, 0x77, 0xFE, 0xCC, 0xED, 0x2E, 0xEE, 0xA5}};
const std::vector<RECT> kEntera;

using Reloj = std::chrono::steady_clock;
using Registrar = BOOL(WINAPI*)(HWND, DWORD);
using Desregistrar = BOOL(WINAPI*)(HWND);
using AvisarEnergia = HANDLE(WINAPI*)(HANDLE, const GUID*, DWORD);
using CallarEnergia = BOOL(WINAPI*)(HANDLE);
using PonerTic = UINT_PTR(WINAPI*)(HWND, UINT_PTR, UINT, TIMERPROC, ULONG);
using ConsultarAvisos = HRESULT(WINAPI*)(int*);

struct Region {
  RGNDATAHEADER cabecera;
  std::array<RECT, kZonasMaximas> rectangulos;
};

template <typename Funcion>
Funcion buscar(HMODULE modulo, const char* nombre) {
  return modulo != nullptr ? reinterpret_cast<Funcion>(GetProcAddress(modulo, nombre)) : nullptr;
}

template <typename Funcion>
Funcion buscar(const wchar_t* modulo, const char* nombre) {
  return buscar<Funcion>(GetModuleHandleW(modulo), nombre);
}

std::uint32_t ms_desde_que_arranco() {
  FILETIME creado = {};
  FILETIME terminado = {};
  FILETIME sistema = {};
  FILETIME usuario = {};
  FILETIME ahora = {};
  GetSystemTimeAsFileTime(&ahora);
  if (!GetProcessTimes(GetCurrentProcess(), &creado, &terminado, &sistema, &usuario)) {
    return 0;
  }
  const auto entero = [](const FILETIME& tiempo) {
    return (static_cast<std::uint64_t>(tiempo.dwHighDateTime) << 32) | tiempo.dwLowDateTime;
  };
  return static_cast<std::uint32_t>((entero(ahora) - std::min(entero(ahora), entero(creado))) / 10000);
}

bool salio_bien(HRESULT resultado, std::uint32_t& error) {
  if (FAILED(resultado)) {
    error = static_cast<std::uint32_t>(resultado);
  }
  return SUCCEEDED(resultado);
}

double ms_desde(Reloj::time_point desde) {
  return std::chrono::duration<double, std::milli>(Reloj::now() - desde).count();
}

float escala_de_pantalla() {
  if (const auto avisar = buscar<BOOL(WINAPI*)()>(L"user32.dll", "SetProcessDPIAware")) {
    avisar();
  }
  const HDC pantalla = GetDC(nullptr);
  const int puntos = pantalla != nullptr ? GetDeviceCaps(pantalla, LOGPIXELSX) : 0;
  ReleaseDC(nullptr, pantalla);
  return puntos > 0 ? static_cast<float>(puntos) / 96.0f : 1.0f;
}

bool hay_un_juego_adelante(HWND propia) {
  const HWND adelante = GetForegroundWindow();
  const HMONITOR monitor = MonitorFromWindow(propia, MONITOR_DEFAULTTONEAREST);
  MONITORINFO datos = {};
  datos.cbSize = sizeof(datos);
  RECT lugar = {};
  std::array<wchar_t, 16> clase = {};
  if (adelante == nullptr || adelante == propia || adelante == GetShellWindow() ||
      MonitorFromWindow(adelante, MONITOR_DEFAULTTONULL) != monitor || !GetMonitorInfoW(monitor, &datos) ||
      !GetWindowRect(adelante, &lugar) || GetClassNameW(adelante, clase.data(), static_cast<int>(clase.size())) == 0 ||
      std::wstring_view(clase.data()) == L"WorkerW" || std::wstring_view(clase.data()) == L"Progman") {
    return false;
  }
  const auto consultar = buscar<ConsultarAvisos>(L"shell32.dll", "SHQueryUserNotificationState");
  int estado = 0;
  return (consultar != nullptr && SUCCEEDED(consultar(&estado)) && estado == kJuegoDeDirect3D) ||
         tapa_el_monitor(lugar, datos.rcMonitor, GetWindowLongW(adelante, GWL_STYLE),
                         GetWindowLongW(adelante, GWL_EXSTYLE));
}

}

struct Ventana::Grafica {
  std::unique_ptr<IDirect3D9, Soltar> d3d;
  std::unique_ptr<IDirect3DDevice9, Soltar> dispositivo;
  IDirect3DDevice9Ex* ex = nullptr;
  std::unique_ptr<IDirect3DTexture9, Soltar> logo;
  std::unique_ptr<Dibujante> dibujante;
  Zonas zonas;
  D3DPRESENT_PARAMETERS presentar{};
  HANDLE aviso_de_energia = nullptr;
  Desregistrar desregistrar_sesion = nullptr;
  int textura_maxima = 2048;
  bool alfa = false;
  bool flip = false;
  bool pura = false;
  bool win32 = false;
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
  if (ventana_ == nullptr || !preparar_dispositivo() ||
      !preparar_interfaz(escala, grafica_->alfa ? ImTextureFormat_Alpha8 : ImTextureFormat_RGBA32)) {
    return;
  }
  ImGui::GetIO().ConfigDebugHighlightIdConflicts = false;
  SYSTEM_INFO sistema = {};
  GetSystemInfo(&sistema);
  if (sistema.dwNumberOfProcessors == 1 || grafica_->por_procesador) {
    ImGuiStyle& estilo = ImGui::GetStyle();
    estilo.AntiAliasedLines = estilo.AntiAliasedFill = false;
    estilo.CurveTessellationTol *= 2;
    estilo.CircleTessellationMaxError *= 2;
  }
  cargar_logo(escala);
  grafica_->dibujante = std::make_unique<Dibujante>(*grafica_->dispositivo, grafica_->textura_maxima);
  grafica_->win32 = ImGui_ImplWin32_Init(ventana_);
  lista_ = grafica_->win32;
  escuchar_a_windows();
}

Ventana::~Ventana() {
  if (grafica_->desregistrar_sesion != nullptr) {
    grafica_->desregistrar_sesion(ventana_);
  }
  if (const auto callar = buscar<CallarEnergia>(L"user32.dll", "UnregisterPowerSettingNotification");
      callar != nullptr && grafica_->aviso_de_energia != nullptr) {
    callar(grafica_->aviso_de_energia);
  }
  grafica_->dibujante.reset();
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
  IDirect3D9Ex* d3d_ex = nullptr;
  const auto crear_ex = buscar<HRESULT(WINAPI*)(UINT, IDirect3D9Ex**)>(L"d3d9.dll", "Direct3DCreate9Ex");
  if (crear_ex == nullptr || GetEnvironmentVariableW(L"VENTRIK_SIN_9EX", nullptr, 0) != 0 ||
      FAILED(crear_ex(D3D_SDK_VERSION, &d3d_ex))) {
    d3d_ex = nullptr;
  }
  grafica_->d3d.reset(d3d_ex != nullptr ? d3d_ex : Direct3DCreate9(D3D_SDK_VERSION));
  D3DCAPS9 capacidades = {};
  D3DDISPLAYMODE modo = {};
  if (!grafica_->d3d ||
      FAILED(grafica_->d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &capacidades)) ||
      (capacidades.Caps2 & D3DCAPS2_DYNAMICTEXTURES) == 0 ||
      FAILED(grafica_->d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &modo))) {
    return false;
  }
  grafica_->alfa = grafica_->d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, modo.Format,
                                                    D3DUSAGE_DYNAMIC | D3DUSAGE_QUERY_FILTER |
                                                        D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                                    D3DRTYPE_TEXTURE, D3DFMT_A8) == D3D_OK;
  grafica_->textura_maxima =
      static_cast<int>(std::min<DWORD>({4096, capacidades.MaxTextureWidth, capacidades.MaxTextureHeight}));
  D3DPRESENT_PARAMETERS& presentar = grafica_->presentar;
  presentar.Windowed = TRUE;
  presentar.BackBufferFormat = D3DFMT_UNKNOWN;
  presentar.BackBufferCount = 1;
  presentar.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
  for (const D3DSWAPEFFECT efecto : {D3DSWAPEFFECT_FLIPEX, D3DSWAPEFFECT_COPY}) {
    if (efecto == D3DSWAPEFFECT_FLIPEX && d3d_ex == nullptr) {
      continue;
    }
    for (const DWORD vertices : {DWORD{D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE},
                                 DWORD{D3DCREATE_HARDWARE_VERTEXPROCESSING}, DWORD{D3DCREATE_SOFTWARE_VERTEXPROCESSING}}) {
      presentar.SwapEffect = efecto;
      presentar.BackBufferCount = 1;
      const bool por_procesador = vertices == D3DCREATE_SOFTWARE_VERTEXPROCESSING;
      const DWORD forma =
          vertices | D3DCREATE_FPU_PRESERVE | (d3d_ex != nullptr && por_procesador ? D3DCREATE_DISABLE_PSGP_THREADING : 0);
      IDirect3DDevice9* dispositivo = nullptr;
      IDirect3DDevice9Ex* dispositivo_ex = nullptr;
      const HRESULT creado =
          d3d_ex != nullptr
              ? d3d_ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, ventana_, forma, &presentar, nullptr,
                                       &dispositivo_ex)
              : grafica_->d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, ventana_, forma, &presentar,
                                            &dispositivo);
      if (SUCCEEDED(creado)) {
        grafica_->ex = dispositivo_ex;
        grafica_->dispositivo.reset(dispositivo_ex != nullptr ? dispositivo_ex : dispositivo);
        grafica_->flip = efecto == D3DSWAPEFFECT_FLIPEX;
        grafica_->pura = (vertices & D3DCREATE_PUREDEVICE) != 0;
        grafica_->por_procesador = por_procesador;
        return true;
      }
    }
  }
  return false;
}

void Ventana::escuchar_a_windows() {
  const auto poner = buscar<PonerTic>(L"user32.dll", "SetCoalescableTimer");
  if (poner == nullptr || poner(ventana_, kTic, kMilisegundosPorTic, nullptr, kToleranciaDelTic) == 0) {
    SetTimer(ventana_, kTic, kMilisegundosPorTic, nullptr);
  }
  const HMODULE sesiones = cargar_de_sistema(L"wtsapi32.dll");
  const auto registrar = buscar<Registrar>(sesiones, "WTSRegisterSessionNotification");
  if (registrar != nullptr && registrar(ventana_, 0)) {
    grafica_->desregistrar_sesion = buscar<Desregistrar>(sesiones, "WTSUnRegisterSessionNotification");
  }
  if (const auto avisar = buscar<AvisarEnergia>(L"user32.dll", "RegisterPowerSettingNotification")) {
    grafica_->aviso_de_energia = avisar(ventana_, &kAhorroDeBateria, DEVICE_NOTIFY_WINDOW_HANDLE);
  }
}

ImTextureID Ventana::logo() const {
  return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(grafica_->logo.get()));
}

std::string Ventana::placa() const {
  D3DADAPTER_IDENTIFIER9 placa = {};
  grafica_->d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &placa);
  return placa.Description;
}

std::string Ventana::grafica() const {
  return std::string(grafica_->ex != nullptr ? "9ex" : "9") + (grafica_->flip ? "-flip" : "-copia") +
         (grafica_->pura ? "-pura" : "") + (grafica_->alfa ? "-a8" : "-argb");
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
  const bool ex = grafica_->ex != nullptr;
  if (!leido || FAILED(grafica_->dispositivo->CreateTexture(lado, lado, 1, ex ? D3DUSAGE_DYNAMIC : 0, D3DFMT_A8R8G8B8,
                                                            ex ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED, &textura,
                                                            nullptr))) {
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

Arreglo como_recuperar(HRESULT estado, bool otro_tamano, DWORD perdido_ms) {
  if (estado == D3DERR_DEVICEREMOVED || perdido_ms >= kEsperaParaRecrear) {
    return Arreglo::recrear;
  }
  if (estado == D3DERR_DEVICELOST) {
    return Arreglo::esperar;
  }
  return estado == D3DERR_DEVICENOTRESET || estado == D3DERR_DEVICEHUNG || otro_tamano ? Arreglo::reiniciar
                                                                                        : Arreglo::ninguno;
}

bool Ventana::recrear_dispositivo() {
  grafica_->dibujante.reset();
  grafica_->logo.reset();
  grafica_->ex = nullptr;
  grafica_->dispositivo.reset();
  if (!preparar_dispositivo()) {
    return false;
  }
  grafica_->dibujante = std::make_unique<Dibujante>(*grafica_->dispositivo, grafica_->textura_maxima);
  cargar_logo(escala_);
  return true;
}

bool Ventana::recuperar_dispositivo() {
  const HRESULT estado = !grafica_->dispositivo    ? D3DERR_DEVICELOST
                         : grafica_->ex != nullptr ? grafica_->ex->CheckDeviceState(ventana_)
                                                   : grafica_->dispositivo->TestCooperativeLevel();
  salio_bien(estado, uso_.error_direct3d);
  const DWORD ahora = GetTickCount();
  const Arreglo arreglo = como_recuperar(estado, ancho_nuevo_ != 0, ahora - perdido_desde_.value_or(ahora));
  if (ancho_nuevo_ != 0 && arreglo != Arreglo::esperar) {
    grafica_->presentar.BackBufferWidth = ancho_nuevo_;
    grafica_->presentar.BackBufferHeight = alto_nuevo_;
    ancho_nuevo_ = alto_nuevo_ = 0;
  }
  bool listo = arreglo == Arreglo::ninguno;
  if (arreglo == Arreglo::reiniciar) {
    grafica_->presentar.BackBufferFormat = D3DFMT_UNKNOWN;
    if (grafica_->ex == nullptr || estado == D3DERR_DEVICEHUNG) {
      grafica_->dibujante->soltar();
    }
    listo = salio_bien(grafica_->dispositivo->Reset(&grafica_->presentar), uso_.error_direct3d);
    uso_.reinicios_fallidos += listo ? 0 : 1;
    grafica_->dibujante->preparar();
  } else if (arreglo == Arreglo::recrear) {
    listo = recrear_dispositivo();
  }
  if (listo) {
    perdido_desde_.reset();
  } else if (!perdido_desde_ || arreglo == Arreglo::recrear) {
    perdido_desde_ = ahora;
  }
  return listo;
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
  const bool caracter = mensaje == WM_CHAR || mensaje == WM_DEADCHAR || mensaje == WM_SYSCHAR;
  const bool repetida = (mensaje == WM_KEYDOWN || mensaje == WM_SYSKEYDOWN) && (datos & 0x40000000) != 0;
  const bool de_edicion = tecla == VK_BACK || tecla == VK_DELETE || (tecla >= VK_PRIOR && tecla <= VK_DOWN);
  return mensaje != WM_NCMOUSEMOVE && mensaje != WM_TIMER && mensaje != kMensajeNovedades &&
         (caracter ? escribiendo : !repetida || (escribiendo && de_edicion));
}

DWORD espera_entre_cuadros(double ultimo_ms) {
  return std::max(kMilisegundosPorCuadro, static_cast<DWORD>(ultimo_ms * (kVecesElCuadroEntreCuadros + 1)));
}

bool tapa_el_monitor(const RECT& ventana, const RECT& monitor, LONG estilo, LONG extendido) {
  RECT cruce = {};
  return IntersectRect(&cruce, &ventana, &monitor) && EqualRect(&cruce, &monitor) &&
         (estilo & (WS_DLGFRAME | WS_THICKFRAME)) == 0 && (extendido & (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW)) == 0;
}

void Ventana::mirar_situacion() {
  BOOL protector = FALSE;
  SystemParametersInfoW(SPI_GETSCREENSAVERRUNNING, 0, &protector, 0);
  protector_ = protector != FALSE;
  situacion_.bloqueada = bloqueo_ || protector_;
  DWORD de = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &de);
  situacion_.con_foco = de == GetCurrentProcessId();
  const bool juego = hay_un_juego_adelante(ventana_);
  if (juego != situacion_.juego) {
    SetThreadPriority(GetCurrentThread(), juego ? THREAD_PRIORITY_BELOW_NORMAL : THREAD_PRIORITY_NORMAL);
    situacion_.juego = juego;
  }
  if (situacion_.tapada && grafica_->ex != nullptr && grafica_->ex->CheckDeviceState(ventana_) == S_OK) {
    situacion_.tapada = false;
  }
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
  const auto armar = [&dibujar] {
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    pantalla_completa(dibujar);
    ImGui::Render();
  };
  armar();
  for (int vez = 0; vez < 2 && por_aparecer(); ++vez) {
    armar();
  }
  const ImDrawData& datos = *ImGui::GetDrawData();
  const std::vector<RECT>& zonas = grafica_->zonas.cambiadas(datos);
  uso_.imgui_ms += ms_desde(empezo);
  uso_.vertices_max = std::max(uso_.vertices_max, datos.TotalVtxCount);
  if (zonas.empty() && !presentar_igual_) {
    return false;
  }
  const bool forzada = std::exchange(presentar_igual_, false);
  const bool entera = forzada || grafica_->flip;
  const auto armado = Reloj::now();
  IDirect3DDevice9& dispositivo = *grafica_->dispositivo;
  if (forzada) {
    dispositivo.Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0x16, 0x1B, 0x1E), 1.0f, 0);
  }
  if (SUCCEEDED(dispositivo.BeginScene())) {
    grafica_->dibujante->dibujar(datos, entera ? kEntera : zonas);
    dispositivo.EndScene();
  } else {
    presentar_igual_ = true;
  }
  Region region = {};
  const RECT pantalla = {0, 0, static_cast<LONG>(grafica_->presentar.BackBufferWidth),
                         static_cast<LONG>(grafica_->presentar.BackBufferHeight)};
  region.cabecera = {sizeof(RGNDATAHEADER), RDH_RECTANGLES, static_cast<DWORD>(entera ? 1 : zonas.size()), 0, {}};
  std::copy(zonas.begin(), zonas.end(), region.rectangulos.begin());
  if (entera) {
    region.rectangulos[0] = pantalla;
  }
  for (std::size_t i = 0; i < region.cabecera.nCount; ++i) {
    UnionRect(&region.cabecera.rcBound, &region.cabecera.rcBound, &region.rectangulos[i]);
    uso_.pixeles += static_cast<std::uint64_t>(region.rectangulos[i].right - region.rectangulos[i].left) *
                    static_cast<std::uint64_t>(region.rectangulos[i].bottom - region.rectangulos[i].top);
  }
  region.cabecera.nRgnSize = region.cabecera.nCount * sizeof(RECT);
  const auto* sucia = entera ? nullptr : reinterpret_cast<const RGNDATA*>(&region);
  const HRESULT resultado = grafica_->ex != nullptr ? grafica_->ex->PresentEx(nullptr, nullptr, nullptr, sucia, 0)
                                                    : dispositivo.Present(nullptr, nullptr, nullptr, sucia);
  salio_bien(resultado, uso_.error_direct3d);
  perdido_ = resultado == D3DERR_DEVICELOST || resultado == D3DERR_DEVICEHUNG || resultado == D3DERR_DEVICEREMOVED;
  situacion_.tapada = resultado == S_PRESENT_OCCLUDED;
  uso_.direct3d_ms += ms_desde(armado);
  ++uso_.presentados;
  if (arranque_ms_ == 0) {
    arranque_ms_ = ms_desde_que_arranco();
  }
  if (!IsWindowVisible(ventana_) && !situacion_.oculta) {
    ShowWindow(ventana_, como_mostrar_);
  }
  return true;
}

void Ventana::traer_al_frente() {
  if (situacion_.oculta) {
    situacion_.oculta = false;
    poner_en_bandeja(false);
    ShowWindow(ventana_, SW_SHOW);
  }
  if (IsIconic(ventana_)) {
    ShowWindow(ventana_, SW_RESTORE);
  }
  SetForegroundWindow(ventana_);
}

void Ventana::bandeja(bool al_cerrar, bool al_minimizar, std::wstring abrir, std::wstring salir) {
  cerrar_a_bandeja_ = al_cerrar;
  minimizar_a_bandeja_ = al_minimizar;
  abrir_ = std::move(abrir);
  salir_ = std::move(salir);
}

std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)> ser_el_unico() {
  decltype(ser_el_unico()) unico(CreateMutexW(nullptr, FALSE, kUnico), &CloseHandle);
  if (unico && (GetLastError() == ERROR_ALREADY_EXISTS || FindWindowW(kClase, nullptr) != nullptr)) {
    unico.reset();
  }
  return unico;
}

void mostrar_el_otro() {
  const HWND otro = FindWindowW(kClase, nullptr);
  DWORD proceso = 0;
  if (otro != nullptr && GetWindowThreadProcessId(otro, &proceso) != 0) {
    AllowSetForegroundWindow(proceso);
    PostMessageW(otro, WM_COMMAND, kOrdenAbrir, 0);
  }
}

void Ventana::ocultar() {
  situacion_.oculta = true;
  ShowWindow(ventana_, SW_HIDE);
  poner_en_bandeja(true);
}

void Ventana::poner_en_bandeja(bool poner) {
  NOTIFYICONDATAW icono = {};
  icono.cbSize = NOTIFYICONDATAW_V2_SIZE;
  icono.hWnd = ventana_;
  icono.uID = 1;
  icono.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  icono.uCallbackMessage = kMensajeBandeja;
  icono.hIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), L"VENTRIK", IMAGE_ICON,
                                              GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                              LR_DEFAULTCOLOR | LR_SHARED));
  GetWindowTextW(ventana_, icono.szTip, ARRAYSIZE(icono.szTip));
  Shell_NotifyIconW(poner ? NIM_ADD : NIM_DELETE, &icono);
}

void Ventana::menu_de_bandeja() {
  const HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, kOrdenAbrir, abrir_.c_str());
  AppendMenuW(menu, MF_STRING, kOrdenSalir, salir_.c_str());
  POINT donde = {};
  GetCursorPos(&donde);
  SetForegroundWindow(ventana_);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON, donde.x, donde.y, 0, ventana_, nullptr);
  PostMessageW(ventana_, WM_NULL, 0, 0);
  DestroyMenu(menu);
}

int Ventana::correr(const std::function<bool()>& atender, const std::function<void()>& dibujar) {
  int pendientes = kCuadrosTrasEvento;
  DWORD entre_cuadros = kMilisegundosPorCuadro;
  DWORD ultimo = GetTickCount() - entre_cuadros;
  DWORD ultimo_mensaje = GetTickCount();
  std::optional<LPARAM> ultimo_mouse;
  bool solo_mouse = false;
  bool dibujaba = true;
  for (;;) {
    const auto empezo = Reloj::now();
    ++uso_.despertares;
    MSG mensaje = {};
    while (PeekMessageW(&mensaje, nullptr, 0, 0, PM_REMOVE)) {
      if (mensaje.message == WM_QUIT) {
        return static_cast<int>(mensaje.wParam);
      }
      ++uso_.mensajes[mensaje.message];
      const bool propio = mensaje.hwnd == ventana_;
      presentar_igual_ = presentar_igual_ || (propio && mensaje.message == WM_PAINT);
      if (propio && mensaje.message == WM_TIMER && mensaje.wParam == kTic) {
        mirar_situacion();
      }
      const bool pide = propio && pide_cuadros(mensaje.message, mensaje.wParam, mensaje.lParam,
                                               ImGui::GetIO().WantTextInput, ultimo_mouse);
      TranslateMessage(&mensaje);
      DispatchMessageW(&mensaje);
      if (pide) {
        solo_mouse = (pendientes == 0 || solo_mouse) && mensaje.message == WM_MOUSEMOVE;
        pendientes = kCuadrosTrasEvento;
        ultimo_mensaje = GetTickCount();
      }
    }
    if (atender()) {
      pendientes = std::max(pendientes, 1);
    }
    situacion_.minimizada = IsIconic(ventana_) != FALSE;
    const bool dibuja = situacion_.dibuja();
    if (dibuja != dibujaba) {
      dibujaba = dibuja;
      grafica_->zonas.olvidar();
      presentar_igual_ = true;
      if (dibuja) {
        pendientes = kCuadrosTrasEvento;
      } else if (grafica_->dibujante) {
        grafica_->dibujante->soltar();
      }
    }
    const DWORD pasado = GetTickCount() - ultimo;
    const DWORD minimo = std::max(entre_cuadros, solo_mouse ? kMilisegundosConMouse : 0);
    DWORD espera = INFINITE;
    if (pendientes > 0 && dibuja) {
      espera = pasado < minimo ? minimo - pasado : 0;
    } else if (dibuja && (perdido_ || (situacion_.con_foco && ImGui::IsAnyItemHovered() &&
                                       GetTickCount() - ultimo_mensaje < kTiempoParaCarteles))) {
      espera = kEsperaConMouseEncima;
    }
    if (espera == 0) {
      ultimo = GetTickCount();
      const auto cuadro = Reloj::now();
      pendientes = dibujar_cuadro(dibujar) ? pendientes - 1 : 0;
      entre_cuadros = espera_entre_cuadros(ms_desde(cuadro));
      solo_mouse = solo_mouse && pendientes > 0;
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
  if (yo != nullptr && mensaje == yo->barra_creada_ && yo->situacion_.oculta) {
    yo->poner_en_bandeja(true);
    return 0;
  }
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
    case WM_ACTIVATEAPP:
      if (yo != nullptr) {
        yo->situacion_.con_foco = wparam != FALSE;
      }
      break;
    case WM_WTSSESSION_CHANGE:
      if (yo != nullptr) {
        yo->uso_.aviso_de_sesion = static_cast<std::uint32_t>(wparam);
        if (wparam == WTS_SESSION_LOCK || wparam == WTS_CONSOLE_DISCONNECT || wparam == WTS_REMOTE_DISCONNECT) {
          yo->bloqueo_ = true;
        } else if (wparam == WTS_SESSION_UNLOCK || wparam == WTS_CONSOLE_CONNECT || wparam == WTS_REMOTE_CONNECT) {
          yo->bloqueo_ = false;
        }
        yo->situacion_.bloqueada = yo->bloqueo_ || yo->protector_;
      }
      return 0;
    case WM_POWERBROADCAST:
      if (SYSTEM_POWER_STATUS energia = {}; yo != nullptr && wparam == kCambioDeEnergia && GetSystemPowerStatus(&energia)) {
        [[maybe_unused]] const auto& [enchufada, bateria, carga, ahorro, queda, dura] = energia;
        yo->situacion_.ahorro = ahorro != 0;
      }
      return TRUE;
    case WM_SYSCOMMAND:
      if ((wparam & 0xFFF0) == SC_KEYMENU && lparam == 0) {
        return 0;
      }
      if ((wparam & 0xFFF0) == SC_MINIMIZE && yo != nullptr && yo->minimizar_a_bandeja_) {
        yo->ocultar();
        return 0;
      }
      break;
    case WM_CLOSE:
      if (yo != nullptr && yo->cerrar_a_bandeja_) {
        yo->ocultar();
        return 0;
      }
      break;
    case kMensajeBandeja:
      if (yo != nullptr && LOWORD(lparam) == WM_LBUTTONUP) {
        yo->traer_al_frente();
      } else if (yo != nullptr && LOWORD(lparam) == WM_RBUTTONUP) {
        yo->menu_de_bandeja();
      }
      return 0;
    case WM_COMMAND:
      if (yo != nullptr && wparam == kOrdenAbrir) {
        yo->traer_al_frente();
        return 0;
      }
      if (wparam == kOrdenSalir) {
        DestroyWindow(ventana);
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
      if (yo != nullptr && yo->situacion_.oculta) {
        yo->poner_en_bandeja(false);
      }
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(ventana, mensaje, wparam, lparam);
}

}
