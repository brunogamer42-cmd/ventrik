#include "client/ui/aviso_ventana.h"

#include <algorithm>
#include <string>

#include <windowsx.h>

#include "client/ui/theme.h"
#include "common/utf.h"

namespace ventrik::ui {
namespace {
constexpr wchar_t kClase[] = L"VentrikAviso";
constexpr UINT_PTR kTemporizador = 2;
constexpr float kAncho = 360;
constexpr float kMargen = 12;
constexpr float kEmblema = 32;
constexpr float kSeparacion = 10;
constexpr float kLugarCruz = 24;
constexpr float kCruz = 8;
constexpr float kEntreLineas = 4;
constexpr float kEntreAvisos = 8;
constexpr int kLineasDeCuerpo = 2;

COLORREF gdi(ImU32 color) {
  return RGB(color >> IM_COL32_R_SHIFT & 0xFF, color >> IM_COL32_G_SHIFT & 0xFF, color >> IM_COL32_B_SHIFT & 0xFF);
}

int alto_de(HDC dc, HFONT letra) {
  const HGDIOBJ antes = SelectObject(dc, letra);
  TEXTMETRICW medidas = {};
  GetTextMetricsW(dc, &medidas);
  SelectObject(dc, antes);
  return medidas.tmHeight;
}

RECT texto_de(const RECT& area, const PiezasAviso& piezas) {
  return {area.left + piezas.px(kMargen + kEmblema + kSeparacion), area.top + piezas.px(kMargen),
          area.right - piezas.px(kLugarCruz), area.bottom - piezas.px(kMargen)};
}

void triangulo(HDC dc, POINT a, POINT b, POINT c) {
  const POINT puntos[] = {a, b, c};
  Polygon(dc, puntos, 3);
}

ATOM registrar_clase(WNDPROC procesar) {
  WNDCLASSEXW clase = {};
  clase.cbSize = sizeof(clase);
  clase.lpfnWndProc = procesar;
  clase.hInstance = GetModuleHandleW(nullptr);
  clase.hCursor = LoadCursorW(nullptr, IDC_HAND);
  clase.lpszClassName = kClase;
  return RegisterClassExW(&clase);
}
}

PiezasAviso::PiezasAviso(float escala) : escala_(escala) {
  for (const wchar_t* nombre : {L"BARLOW_NORMAL", L"BARLOW_FUERTE"}) {
    const HRSRC recurso = FindResourceW(nullptr, nombre, RT_RCDATA);
    const HGLOBAL cargado = recurso != nullptr ? LoadResource(nullptr, recurso) : nullptr;
    void* datos = cargado != nullptr ? LockResource(cargado) : nullptr;
    DWORD cuantas = 0;
    if (const HANDLE fuente = datos != nullptr ? AddFontMemResourceEx(datos, SizeofResource(nullptr, recurso),
                                                                       nullptr, &cuantas)
                                               : nullptr) {
      cargadas_.push_back(fuente);
    }
  }
  const int alto = -px(medida::letra * medida::em);
  const auto letra = [alto](const wchar_t* nombre) {
    return CreateFontW(alto, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, nombre);
  };
  titulo_ = letra(L"Barlow SemiBold");
  cuerpo_ = letra(L"Barlow");
  emblema_ = static_cast<HICON>(
      LoadImageW(GetModuleHandleW(nullptr), L"VENTRIK", IMAGE_ICON, px(kEmblema), px(kEmblema), LR_DEFAULTCOLOR));
}

PiezasAviso::~PiezasAviso() {
  DeleteObject(titulo_);
  DeleteObject(cuerpo_);
  if (emblema_ != nullptr) {
    DestroyIcon(emblema_);
  }
  for (const HANDLE fuente : cargadas_) {
    RemoveFontMemResourceEx(fuente);
  }
}

SIZE medir_aviso(HDC dc, const cliente::Notificacion& aviso, const PiezasAviso& piezas) {
  const int ancho = piezas.px(kAncho);
  RECT cuerpo = texto_de({0, 0, ancho, 0}, piezas);
  cuerpo.bottom = cuerpo.top;
  const HGDIOBJ antes = SelectObject(dc, piezas.cuerpo());
  const std::wstring texto = a_utf16(aviso.cuerpo);
  DrawTextW(dc, texto.c_str(), static_cast<int>(texto.size()), &cuerpo,
            DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
  SelectObject(dc, antes);
  const int alto_cuerpo = std::min<int>(cuerpo.bottom - cuerpo.top, kLineasDeCuerpo * alto_de(dc, piezas.cuerpo()));
  const int alto = piezas.px(kMargen) * 2 + alto_de(dc, piezas.titulo()) + piezas.px(kEntreLineas) + alto_cuerpo;
  return {ancho, std::max(alto, piezas.px(kMargen * 2 + kEmblema))};
}

void pintar_aviso(HDC dc, const RECT& area, const cliente::Notificacion& aviso, const PiezasAviso& piezas, bool cruz) {
  const HBRUSH fondo = CreateSolidBrush(gdi(color::panel));
  const HBRUSH borde = CreateSolidBrush(gdi(color::linea));
  const HBRUSH relleno = CreateSolidBrush(gdi(color::triangulo));
  FillRect(dc, &area, fondo);
  FrameRect(dc, &area, borde);
  const int lado = piezas.px(medida::triangulo);
  const HGDIOBJ pincel_antes = SelectObject(dc, relleno);
  const HGDIOBJ pluma_antes = SelectObject(dc, GetStockObject(NULL_PEN));
  triangulo(dc, {area.left, area.top}, {area.left + lado, area.top}, {area.left, area.top + lado});
  triangulo(dc, {area.right, area.bottom}, {area.right - lado, area.bottom}, {area.right, area.bottom - lado});
  if (piezas.emblema() != nullptr) {
    DrawIconEx(dc, area.left + piezas.px(kMargen), area.top + piezas.px(kMargen), piezas.emblema(),
               piezas.px(kEmblema), piezas.px(kEmblema), 0, nullptr, DI_NORMAL);
  }
  SetBkMode(dc, TRANSPARENT);
  RECT texto = texto_de(area, piezas);
  const int alto_titulo = alto_de(dc, piezas.titulo());
  RECT arriba = {texto.left, texto.top, texto.right, texto.top + alto_titulo};
  const HGDIOBJ letra_antes = SelectObject(dc, piezas.titulo());
  SetTextColor(dc, gdi(color::texto));
  const std::wstring titulo = a_utf16(aviso.titulo);
  DrawTextW(dc, titulo.c_str(), static_cast<int>(titulo.size()), &arriba,
            DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
  texto.top = arriba.bottom + piezas.px(kEntreLineas);
  SelectObject(dc, piezas.cuerpo());
  SetTextColor(dc, gdi(color::texto2));
  const std::wstring cuerpo = a_utf16(aviso.cuerpo);
  DrawTextW(dc, cuerpo.c_str(), static_cast<int>(cuerpo.size()), &texto,
            DT_WORDBREAK | DT_EDITCONTROL | DT_END_ELLIPSIS | DT_NOPREFIX);
  SelectObject(dc, letra_antes);
  const HPEN pluma = CreatePen(PS_SOLID, std::max(1, piezas.px(1.5f)), gdi(cruz ? color::texto : color::apagado));
  SelectObject(dc, pluma);
  const int x = area.right - piezas.px(kSeparacion + kCruz);
  const int y = area.top + piezas.px(kSeparacion);
  const int c = piezas.px(kCruz);
  MoveToEx(dc, x, y, nullptr);
  LineTo(dc, x + c, y + c);
  MoveToEx(dc, x + c, y, nullptr);
  LineTo(dc, x, y + c);
  SelectObject(dc, pluma_antes);
  SelectObject(dc, pincel_antes);
  DeleteObject(pluma);
  DeleteObject(relleno);
  DeleteObject(borde);
  DeleteObject(fondo);
}

AvisosEnPantalla::AvisosEnPantalla(HWND principal, float escala) : principal_(principal), piezas_(escala) {
  static const ATOM clase = registrar_clase(&AvisosEnPantalla::procesar);
  static_cast<void>(clase);
}

AvisosEnPantalla::~AvisosEnPantalla() { ocultar(); }

void AvisosEnPantalla::ocultar() {
  while (!abiertas_.empty()) {
    const HWND ventana = abiertas_.back().ventana;
    abiertas_.pop_back();
    DestroyWindow(ventana);
  }
}

void AvisosEnPantalla::mostrar(const std::vector<cliente::Notificacion>& avisos) {
  const auto esta = [&avisos](std::uint32_t canal) {
    return std::any_of(avisos.begin(), avisos.end(), [canal](const cliente::Notificacion& a) { return a.canal == canal; });
  };
  for (auto vista = vistas_.begin(); vista != vistas_.end();) {
    vista = esta(vista->first) ? std::next(vista) : vistas_.erase(vista);
  }
  std::vector<HWND> sobran;
  for (const Abierta& abierta : abiertas_) {
    if (!esta(abierta.aviso.canal)) {
      sobran.push_back(abierta.ventana);
    }
  }
  for (const HWND ventana : sobran) {
    cerrar(ventana);
  }
  for (const cliente::Notificacion& aviso : avisos) {
    std::uint64_t& vista = vistas_[aviso.canal];
    if (aviso.version <= vista) {
      continue;
    }
    vista = aviso.version;
    const auto abierta = std::find_if(abiertas_.begin(), abiertas_.end(),
                                      [&aviso](const Abierta& a) { return a.aviso.canal == aviso.canal; });
    if (abierta == abiertas_.end()) {
      abrir(aviso);
      continue;
    }
    abierta->aviso = aviso;
    abierta->tamano = medir(aviso);
    if (!abierta->encima) {
      SetTimer(abierta->ventana, kTemporizador, kSegundosDeAviso * 1000, nullptr);
    }
    InvalidateRect(abierta->ventana, nullptr, FALSE);
  }
  acomodar();
}

SIZE AvisosEnPantalla::medir(const cliente::Notificacion& aviso) const {
  const HDC dc = GetDC(nullptr);
  const SIZE tamano = medir_aviso(dc, aviso, piezas_);
  ReleaseDC(nullptr, dc);
  return tamano;
}

void AvisosEnPantalla::abrir(const cliente::Notificacion& aviso) {
  if (abiertas_.size() >= kAvisosALaVez) {
    cerrar(abiertas_.front().ventana);
  }
  const SIZE tamano = medir(aviso);
  const HWND ventana = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kClase, L"", WS_POPUP, 0,
                                       0, tamano.cx, tamano.cy, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (ventana == nullptr) {
    return;
  }
  SetWindowLongPtrW(ventana, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  abiertas_.push_back({ventana, aviso, tamano});
  ++mostrados_;
  SetTimer(ventana, kTemporizador, kSegundosDeAviso * 1000, nullptr);
  acomodar();
  ShowWindow(ventana, SW_SHOWNOACTIVATE);
}

void AvisosEnPantalla::cerrar(HWND ventana) {
  const auto abierta = std::find_if(abiertas_.begin(), abiertas_.end(),
                                    [ventana](const Abierta& a) { return a.ventana == ventana; });
  if (abierta == abiertas_.end()) {
    return;
  }
  abiertas_.erase(abierta);
  DestroyWindow(ventana);
  acomodar();
}

void AvisosEnPantalla::acomodar() {
  MONITORINFO monitor = {};
  monitor.cbSize = sizeof(monitor);
  GetMonitorInfoW(MonitorFromWindow(principal_, MONITOR_DEFAULTTOPRIMARY), &monitor);
  LONG abajo = monitor.rcWork.bottom - piezas_.px(kMargen);
  for (auto abierta = abiertas_.rbegin(); abierta != abiertas_.rend(); ++abierta) {
    const SIZE& tamano = abierta->tamano;
    SetWindowPos(abierta->ventana, HWND_TOPMOST, monitor.rcWork.right - piezas_.px(kMargen) - tamano.cx,
                 abajo - tamano.cy, tamano.cx, tamano.cy, SWP_NOACTIVATE);
    abajo -= tamano.cy + piezas_.px(kEntreAvisos);
  }
}

RECT AvisosEnPantalla::cruz(HWND ventana) const {
  RECT area = {};
  GetClientRect(ventana, &area);
  return {area.right - piezas_.px(kLugarCruz), area.top, area.right, area.top + piezas_.px(kLugarCruz)};
}

LRESULT CALLBACK AvisosEnPantalla::procesar(HWND ventana, UINT mensaje, WPARAM wparam, LPARAM lparam) {
  auto* yo = reinterpret_cast<AvisosEnPantalla*>(GetWindowLongPtrW(ventana, GWLP_USERDATA));
  return yo != nullptr ? yo->atender(ventana, mensaje, wparam, lparam)
                       : DefWindowProcW(ventana, mensaje, wparam, lparam);
}

LRESULT AvisosEnPantalla::atender(HWND ventana, UINT mensaje, WPARAM wparam, LPARAM lparam) {
  const auto abierta = std::find_if(abiertas_.begin(), abiertas_.end(),
                                    [ventana](const Abierta& a) { return a.ventana == ventana; });
  if (abierta == abiertas_.end()) {
    return DefWindowProcW(ventana, mensaje, wparam, lparam);
  }
  const POINT punto = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
  switch (mensaje) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT pintura = {};
      const HDC dc = BeginPaint(ventana, &pintura);
      RECT area = {};
      GetClientRect(ventana, &area);
      const HDC memoria = CreateCompatibleDC(dc);
      const HBITMAP lienzo = CreateCompatibleBitmap(dc, area.right, area.bottom);
      const HGDIOBJ antes = SelectObject(memoria, lienzo);
      pintar_aviso(memoria, area, abierta->aviso, piezas_, abierta->cruz);
      BitBlt(dc, 0, 0, area.right, area.bottom, memoria, 0, 0, SRCCOPY);
      SelectObject(memoria, antes);
      DeleteObject(lienzo);
      DeleteDC(memoria);
      EndPaint(ventana, &pintura);
      return 0;
    }
    case WM_MOUSEMOVE: {
      if (!abierta->encima) {
        abierta->encima = true;
        KillTimer(ventana, kTemporizador);
        TRACKMOUSEEVENT seguir = {sizeof(seguir), TME_LEAVE, ventana, 0};
        TrackMouseEvent(&seguir);
      }
      const RECT x = cruz(ventana);
      if (const bool sobre = PtInRect(&x, punto) != FALSE; sobre != abierta->cruz) {
        abierta->cruz = sobre;
        InvalidateRect(ventana, nullptr, FALSE);
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      abierta->encima = false;
      abierta->cruz = false;
      InvalidateRect(ventana, nullptr, FALSE);
      SetTimer(ventana, kTemporizador, kSegundosDeAviso * 1000, nullptr);
      return 0;
    case WM_LBUTTONDOWN:
      abierta->apretada = true;
      SetCapture(ventana);
      return 0;
    case WM_CAPTURECHANGED:
      abierta->apretada = false;
      return 0;
    case WM_LBUTTONUP: {
      if (!std::exchange(abierta->apretada, false)) {
        return 0;
      }
      ReleaseCapture();
      RECT area = {};
      GetClientRect(ventana, &area);
      const RECT x = cruz(ventana);
      if (PtInRect(&area, punto) == FALSE) {
        return 0;
      }
      if (PtInRect(&x, punto) == FALSE) {
        tocado_ = abierta->aviso;
      }
      cerrar(ventana);
      return 0;
    }
    case WM_TIMER:
      if (wparam == kTemporizador) {
        cerrar(ventana);
      }
      return 0;
    default:
      return DefWindowProcW(ventana, mensaje, wparam, lparam);
  }
}

}
