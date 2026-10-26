#include "client/ui/notificador.h"

#if defined(VENTRIK_MODERNA)
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cwchar>
#include <deque>
#include <map>
#include <mutex>

#include <notificationactivationcallback.h>
#include <roapi.h>
#include <windows.data.xml.dom.h>
#include <windows.ui.notifications.h>
#include <winstring.h>
#include <wrl/client.h>

#include "client/ui/ventana.h"
#include "common/archivos.h"
#include "common/protocol/bytes.h"
#include "common/sistema.h"
#include "common/utf.h"
#endif

namespace ventrik::ui {
#if defined(VENTRIK_MODERNA)
namespace {
using Microsoft::WRL::ComPtr;
using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::UI::Notifications;
using Reloj = std::chrono::steady_clock;

constexpr CLSID kActivador = {0x7222DDF2, 0xA9DA, 0x466B, {0xAD, 0x71, 0xBF, 0x2D, 0x15, 0x44, 0x80, 0x90}};
constexpr CLSID kActivadorDePrueba = {0x9C5D041F, 0xAA0D, 0x40FF, {0x8C, 0x6B, 0x33, 0x18, 0x25, 0x3E, 0x0E, 0x6C}};
constexpr wchar_t kAplicacion[] = L"Ventrik";
constexpr wchar_t kAplicacionDePrueba[] = L"Ventrik.Prueba";
constexpr wchar_t kPlantilla[] =
    L"<toast><visual><binding template=\"ToastGeneric\"><text/><text/></binding></visual></toast>";
constexpr wchar_t kPlantillaMuda[] =
    L"<toast><visual><binding template=\"ToastGeneric\"><text/><text/></binding></visual>"
    L"<audio silent=\"true\"/></toast>";
constexpr auto kSinRepetirAviso = std::chrono::seconds(10);
constexpr std::size_t kClicsGuardados = 8;
constexpr std::uint32_t kMayorIcono = 128;

struct Combase {
  decltype(&RoGetActivationFactory) fabrica = nullptr;
  decltype(&WindowsCreateString) crear = nullptr;
  decltype(&WindowsDeleteString) borrar = nullptr;
  decltype(&WindowsGetStringRawBuffer) leer = nullptr;
  bool ok() const { return fabrica != nullptr && crear != nullptr && borrar != nullptr && leer != nullptr; }
};

template <typename Funcion>
Funcion buscar(HMODULE dll, const char* nombre) {
  return reinterpret_cast<Funcion>(GetProcAddress(dll, nombre));
}

const Combase& combase() {
  static const Combase cargado = [] {
    Combase funciones;
    if (const HMODULE dll = cargar_de_sistema(L"combase.dll")) {
      funciones.fabrica = buscar<decltype(&RoGetActivationFactory)>(dll, "RoGetActivationFactory");
      funciones.crear = buscar<decltype(&WindowsCreateString)>(dll, "WindowsCreateString");
      funciones.borrar = buscar<decltype(&WindowsDeleteString)>(dll, "WindowsDeleteString");
      funciones.leer = buscar<decltype(&WindowsGetStringRawBuffer)>(dll, "WindowsGetStringRawBuffer");
    }
    return funciones;
  }();
  return cargado;
}

class Cadena {
 public:
  explicit Cadena(std::wstring_view texto) {
    combase().crear(texto.data(), static_cast<UINT32>(texto.size()), &valor_);
  }
  ~Cadena() {
    if (valor_ != nullptr) {
      combase().borrar(valor_);
    }
  }
  Cadena(const Cadena&) = delete;
  Cadena& operator=(const Cadena&) = delete;
  HSTRING get() const { return valor_; }

 private:
  HSTRING valor_ = nullptr;
};

template <typename Interfaz>
ComPtr<Interfaz> fabrica(const wchar_t* clase) {
  ComPtr<Interfaz> resultado;
  if (!combase().ok() || FAILED(combase().fabrica(Cadena(clase).get(), __uuidof(Interfaz),
                               reinterpret_cast<void**>(resultado.GetAddressOf())))) {
    return nullptr;
  }
  return resultado;
}

std::atomic<HWND> principal_global{nullptr};
std::mutex candado_de_clics;
std::deque<std::wstring> clics;

class Activador final : public INotificationActivationCallback {
 public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** salida) override {
    if (salida == nullptr) {
      return E_POINTER;
    }
    *salida = id == __uuidof(IUnknown) || id == __uuidof(INotificationActivationCallback)
                  ? static_cast<INotificationActivationCallback*>(this)
                  : nullptr;
    return *salida != nullptr ? S_OK : E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
  ULONG STDMETHODCALLTYPE Release() override { return 1; }
  HRESULT STDMETHODCALLTYPE Activate(LPCWSTR, LPCWSTR argumentos, const NOTIFICATION_USER_INPUT_DATA*,
                                     ULONG) override {
    if (argumentos != nullptr) {
      const std::lock_guard<std::mutex> guarda(candado_de_clics);
      if (clics.size() < kClicsGuardados) {
        clics.emplace_back(argumentos, wcsnlen(argumentos, cliente::kTopeArgumentosDeAviso + 1));
      }
    }
    if (const HWND principal = principal_global.load()) {
      PostMessageW(principal, kMensajeNovedades, 0, 0);
    }
    return S_OK;
  }
};

class FabricaDeActivador final : public IClassFactory {
 public:
  explicit FabricaDeActivador(Activador& activador) : activador_(activador) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** salida) override {
    if (salida == nullptr) {
      return E_POINTER;
    }
    *salida = id == __uuidof(IUnknown) || id == __uuidof(IClassFactory) ? static_cast<IClassFactory*>(this) : nullptr;
    return *salida != nullptr ? S_OK : E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
  ULONG STDMETHODCALLTYPE Release() override { return 1; }
  HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* externo, REFIID id, void** salida) override {
    if (salida != nullptr && externo != nullptr) {
      *salida = nullptr;
    }
    return externo != nullptr ? CLASS_E_NOAGGREGATION : activador_.QueryInterface(id, salida);
  }
  HRESULT STDMETHODCALLTYPE LockServer(BOOL) override { return S_OK; }

 private:
  Activador& activador_;
};

Activador activador;
FabricaDeActivador fabrica_de_activador(activador);

bool iniciar_com() {
  const HRESULT resultado = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  return SUCCEEDED(resultado) || resultado == RPC_E_CHANGED_MODE;
}

std::atomic<long> registro_del_activador{S_FALSE};
std::atomic<DWORD> galleta_del_activador{0};

void registrar_activador(const CLSID& clase) {
  static std::atomic<bool> registrado{false};
  DWORD galleta = 0;
  if (!registrado.exchange(true)) {
    registro_del_activador = iniciar_com() ? CoRegisterClassObject(clase, &fabrica_de_activador, CLSCTX_LOCAL_SERVER,
                                                                   REGCLS_MULTIPLEUSE, &galleta)
                                           : E_FAIL;
    galleta_del_activador = galleta;
  }
}

std::wstring texto_de_guid(const CLSID& clase) {
  std::array<wchar_t, 40> texto{};
  StringFromGUID2(clase, texto.data(), static_cast<int>(texto.size()));
  return texto.data();
}

std::wstring clave_de_aplicacion(bool prueba) {
  return std::wstring(L"Software\\Classes\\AppUserModelId\\") + (prueba ? kAplicacionDePrueba : kAplicacion);
}

std::wstring clave_de_activador(bool prueba) {
  return L"Software\\Classes\\CLSID\\" + texto_de_guid(prueba ? kActivadorDePrueba : kActivador);
}

std::wstring leer_valor(const std::wstring& clave, const wchar_t* nombre) {
  std::array<wchar_t, 1024> valor{};
  DWORD bytes = static_cast<DWORD>(valor.size() * sizeof(wchar_t));
  if (RegGetValueW(HKEY_CURRENT_USER, clave.c_str(), nombre, RRF_RT_REG_SZ | RRF_NOEXPAND, nullptr, valor.data(),
                   &bytes) != ERROR_SUCCESS) {
    return {};
  }
  valor.back() = L'\0';
  return valor.data();
}

bool escribir_valor(const std::wstring& clave, const wchar_t* nombre, const std::wstring& valor) {
  if (leer_valor(clave, nombre) == valor) {
    return true;
  }
  HKEY abierta = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, clave.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &abierta, nullptr) !=
      ERROR_SUCCESS) {
    return false;
  }
  const LONG resultado = RegSetValueExW(abierta, nombre, 0, REG_SZ, reinterpret_cast<const BYTE*>(valor.c_str()),
                                        static_cast<DWORD>((valor.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(abierta);
  return resultado == ERROR_SUCCESS;
}

gsl::span<const std::uint8_t> recurso(LPCWSTR nombre, LPCWSTR tipo) {
  const HRSRC encontrado = FindResourceW(nullptr, nombre, tipo);
  const HGLOBAL cargado = encontrado != nullptr ? LoadResource(nullptr, encontrado) : nullptr;
  const void* datos = cargado != nullptr ? LockResource(cargado) : nullptr;
  return datos != nullptr ? gsl::make_span(static_cast<const std::uint8_t*>(datos), SizeofResource(nullptr, encontrado))
                          : gsl::span<const std::uint8_t>();
}

bool escribir_icono(const std::wstring& ruta) {
  protocolo::Lector grupo(recurso(L"VENTRIK", RT_GROUP_ICON));
  grupo.leer_u16();
  grupo.leer_u16();
  const std::uint16_t cuantas = grupo.leer_u16();
  std::array<std::uint8_t, 4> medidas{};
  std::uint16_t planos = 0;
  std::uint16_t bits = 0;
  std::uint16_t elegida = 0;
  std::uint32_t mejor = 0;
  for (std::uint16_t i = 0; i < cuantas && grupo.ok(); ++i) {
    const std::array<std::uint8_t, 4> estas = {grupo.leer_u8(), grupo.leer_u8(), grupo.leer_u8(), grupo.leer_u8()};
    const std::uint16_t sus_planos = grupo.leer_u16();
    const std::uint16_t sus_bits = grupo.leer_u16();
    grupo.leer_u32();
    const std::uint16_t id = grupo.leer_u16();
    const std::uint32_t lado = estas[0] == 0 ? 256 : estas[0];
    if (grupo.ok() && lado <= kMayorIcono && lado > mejor) {
      mejor = lado;
      medidas = estas;
      planos = sus_planos;
      bits = sus_bits;
      elegida = id;
    }
  }
  const auto imagen = elegida != 0 ? recurso(MAKEINTRESOURCEW(elegida), RT_ICON) : gsl::span<const std::uint8_t>();
  if (imagen.empty()) {
    return false;
  }
  protocolo::Escritor icono;
  icono.escribir_u16(0);
  icono.escribir_u16(1);
  icono.escribir_u16(1);
  for (const std::uint8_t medida : medidas) {
    icono.escribir_u8(medida);
  }
  icono.escribir_u16(planos);
  icono.escribir_u16(bits);
  icono.escribir_u32(static_cast<std::uint32_t>(imagen.size()));
  icono.escribir_u32(22);
  icono.escribir_bytes(imagen);
  return icono.ok() && escribir_archivo_completo(ruta, icono.datos());
}

bool elevado() {
  HANDLE ficha = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &ficha)) {
    return false;
  }
  TOKEN_ELEVATION elevacion = {};
  DWORD largo = 0;
  const bool si = GetTokenInformation(ficha, TokenElevation, &elevacion, sizeof(elevacion), &largo) &&
                  elevacion.TokenIsElevated != 0;
  CloseHandle(ficha);
  return si;
}

bool registrar(const std::wstring& carpeta, bool prueba) {
  std::array<wchar_t, 32768> exe{};
  const DWORD largo = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
  const std::wstring icono = carpeta + L"\\ventrik.ico";
  if (largo == 0 || largo >= exe.size() || !crear_carpetas(carpeta) || !escribir_icono(icono)) {
    return false;
  }
  const std::wstring aplicacion = clave_de_aplicacion(prueba);
  return escribir_valor(aplicacion, L"DisplayName", prueba ? kAplicacionDePrueba : kAplicacion) &&
         escribir_valor(aplicacion, L"IconUri", icono) &&
         escribir_valor(aplicacion, L"CustomActivator", texto_de_guid(prueba ? kActivadorDePrueba : kActivador)) &&
         escribir_valor(clave_de_activador(prueba) + L"\\LocalServer32", nullptr,
                        L"\"" + (prueba ? carpeta + L"\\no_existe.exe" : std::wstring(exe.data(), largo)) +
                            L"\" -ToastActivated");
}

std::wstring grupo_de(const std::string& servidor) {
  std::uint32_t suma = 2166136261u;
  for (const char letra : servidor) {
    suma = (suma ^ static_cast<std::uint8_t>(letra)) * 16777619u;
  }
  return std::to_wstring(suma);
}

ComPtr<IXmlDocument> documento_de(const cliente::Notificacion& aviso, bool mudo) {
  const auto fabrica_de_xml = fabrica<IActivationFactory>(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument);
  ComPtr<IInspectable> instancia;
  ComPtr<IXmlDocument> documento;
  ComPtr<IXmlDocumentIO> lector;
  ComPtr<IXmlNodeList> textos;
  if (!fabrica_de_xml || FAILED(fabrica_de_xml->ActivateInstance(&instancia)) || FAILED(instancia.As(&documento)) ||
      FAILED(documento.As(&lector)) || FAILED(lector->LoadXml(Cadena(mudo ? kPlantillaMuda : kPlantilla).get())) ||
      FAILED(documento->GetElementsByTagName(Cadena(L"text").get(), &textos))) {
    return nullptr;
  }
  std::array<std::wstring, 2> contenidos = {a_utf16(aviso.titulo), a_utf16(aviso.cuerpo)};
  for (std::wstring& contenido : contenidos) {
    std::replace_if(
        contenido.begin(), contenido.end(),
        [](wchar_t letra) { return letra >= 0xFFFE || (letra < 0x20 && letra != L'\t' && letra != L'\n' && letra != L'\r'); },
        wchar_t{0xFFFD});
  }
  for (UINT32 i = 0; i < contenidos.size(); ++i) {
    ComPtr<IXmlNode> nodo;
    ComPtr<IXmlText> texto;
    ComPtr<IXmlNode> hijo;
    ComPtr<IXmlNode> agregado;
    if (FAILED(textos->Item(i, &nodo)) || !nodo ||
        FAILED(documento->CreateTextNode(Cadena(contenidos[i]).get(), &texto)) || FAILED(texto.As(&hijo)) ||
        FAILED(nodo->AppendChild(hijo.Get(), &agregado))) {
      return nullptr;
    }
  }
  ComPtr<IXmlElement> raiz;
  if (FAILED(documento->get_DocumentElement(&raiz)) ||
      FAILED(raiz->SetAttribute(Cadena(L"launch").get(), Cadena(a_utf16(cliente::argumentos_de_aviso(aviso))).get()))) {
    return nullptr;
  }
  return documento;
}

ComPtr<IToastNotificationHistory> historia() {
  const auto estaticos = fabrica<IToastNotificationManagerStatics>(
      RuntimeClass_Windows_UI_Notifications_ToastNotificationManager);
  ComPtr<IToastNotificationManagerStatics2> estaticos2;
  ComPtr<IToastNotificationHistory> resultado;
  if (!estaticos || FAILED(estaticos.As(&estaticos2)) || FAILED(estaticos2->get_History(&resultado))) {
    return nullptr;
  }
  return resultado;
}

class NotificacionesDeWindows final : public Notificador {
 public:
  NotificacionesDeWindows(ComPtr<IToastNotifier> notificador, ComPtr<IToastNotificationHistory> historia,
                          bool prueba)
      : notificador_(std::move(notificador)), historia_(std::move(historia)), prueba_(prueba) {
    limpiar();
  }

  bool mostrar(const std::vector<cliente::Notificacion>& avisos) override {
    if (avisos.empty()) {
      if (!vistas_.empty()) {
        limpiar();
      }
      return true;
    }
    for (auto vista = vistas_.begin(); vista != vistas_.end();) {
      if (std::none_of(avisos.begin(), avisos.end(), [&vista](const cliente::Notificacion& aviso) {
            return Clave{aviso.servidor, aviso.canal} == vista->first;
          })) {
        historia_->RemoveGroupedTagWithId(Cadena(std::to_wstring(vista->first.second)).get(),
                                          Cadena(grupo_de(vista->first.first)).get(), Cadena(aplicacion()).get());
        vista = vistas_.erase(vista);
      } else {
        ++vista;
      }
    }
    const auto ahora = Reloj::now();
    for (const cliente::Notificacion& aviso : avisos) {
      Vista& vista = vistas_[{aviso.servidor, aviso.canal}];
      if (aviso.version <= vista.version) {
        continue;
      }
      const bool nueva = vista.version == 0;
      const bool callada = prueba_ || (!nueva && ahora - vista.sono < kSinRepetirAviso);
      if (!enviar(aviso, callada)) {
        limpiar();
        return false;
      }
      mostrados_ += nueva ? 1u : 0u;
      vista.version = aviso.version;
      vista.sono = callada ? vista.sono : ahora;
    }
    return true;
  }
  void ocultar() override {}
  std::optional<cliente::Notificacion> tocado() override { return std::nullopt; }
  std::uint32_t mostrados() const override { return mostrados_; }

 private:
  using Clave = std::pair<std::string, std::uint32_t>;
  struct Vista {
    std::uint64_t version = 0;
    Reloj::time_point sono{};
  };

  const wchar_t* aplicacion() const { return prueba_ ? kAplicacionDePrueba : kAplicacion; }

  void limpiar() {
    historia_->ClearWithId(Cadena(aplicacion()).get());
    vistas_.clear();
  }

  bool enviar(const cliente::Notificacion& aviso, bool callada) {
    const auto documento = documento_de(aviso, prueba_);
    const auto fabrica_de_avisos =
        fabrica<IToastNotificationFactory>(RuntimeClass_Windows_UI_Notifications_ToastNotification);
    ComPtr<IToastNotification> toast;
    ComPtr<IToastNotification2> toast2;
    return documento && fabrica_de_avisos &&
           SUCCEEDED(fabrica_de_avisos->CreateToastNotification(documento.Get(), &toast)) &&
           SUCCEEDED(toast.As(&toast2)) && SUCCEEDED(toast2->put_Tag(Cadena(std::to_wstring(aviso.canal)).get())) &&
           SUCCEEDED(toast2->put_Group(Cadena(grupo_de(aviso.servidor)).get())) &&
           SUCCEEDED(toast2->put_SuppressPopup(callada)) && SUCCEEDED(notificador_->Show(toast.Get()));
  }

  ComPtr<IToastNotifier> notificador_;
  ComPtr<IToastNotificationHistory> historia_;
  bool prueba_;
  std::map<Clave, Vista> vistas_;
  std::uint32_t mostrados_ = 0;
};

}

std::unique_ptr<Notificador> notificaciones_de_windows(HWND principal, const std::wstring& carpeta, bool prueba) {
  if (!combase().ok() || elevado() || !iniciar_com() || !registrar(carpeta, prueba)) {
    return nullptr;
  }
  principal_global = principal;
  registrar_activador(prueba ? kActivadorDePrueba : kActivador);
  const auto estaticos = fabrica<IToastNotificationManagerStatics>(
      RuntimeClass_Windows_UI_Notifications_ToastNotificationManager);
  ComPtr<IToastNotifier> notificador;
  NotificationSetting ajuste = NotificationSetting_Enabled;
  auto lista = historia();
  if (!estaticos || !lista ||
      FAILED(estaticos->CreateToastNotifierWithId(Cadena(prueba ? kAplicacionDePrueba : kAplicacion).get(),
                                                  &notificador)) ||
      (SUCCEEDED(notificador->get_Setting(&ajuste)) && ajuste == NotificationSetting_DisabledByManifest)) {
    return nullptr;
  }
  return std::make_unique<NotificacionesDeWindows>(std::move(notificador), std::move(lista), prueba);
}

void escuchar_clics_de_windows(HWND principal) {
  if (!elevado() && leer_valor(clave_de_aplicacion(false), L"CustomActivator") == texto_de_guid(kActivador)) {
    principal_global = principal;
    registrar_activador(kActivador);
  }
}

void soltar_clics_de_windows() {
  principal_global = nullptr;
  if (const DWORD galleta = galleta_del_activador.exchange(0)) {
    CoRevokeClassObject(galleta);
  }
}

long registro_de_clics_de_windows() { return registro_del_activador; }

std::optional<cliente::Notificacion> tomar_clic_de_windows() {
  std::wstring argumentos;
  {
    const std::lock_guard<std::mutex> guarda(candado_de_clics);
    if (clics.empty()) {
      return std::nullopt;
    }
    argumentos = std::move(clics.front());
    clics.pop_front();
  }
  return cliente::leer_argumentos_de_aviso(a_utf8(argumentos));
}

std::size_t notificaciones_en_windows(bool prueba) {
  const auto lista = historia();
  ComPtr<IToastNotificationHistory2> lista2;
  ComPtr<ABI::Windows::Foundation::Collections::IVectorView<ToastNotification*>> avisos;
  UINT32 cuantos = 0;
  if (!lista || FAILED(lista.As(&lista2)) ||
      FAILED(lista2->GetHistoryWithId(Cadena(prueba ? kAplicacionDePrueba : kAplicacion).get(), &avisos)) ||
      FAILED(avisos->get_Size(&cuantos))) {
    return 0;
  }
  return cuantos;
}

void olvidar_notificaciones_de_windows(bool prueba) {
  if (const auto lista = historia()) {
    lista->ClearWithId(Cadena(prueba ? kAplicacionDePrueba : kAplicacion).get());
  }
  RegDeleteTreeW(HKEY_CURRENT_USER, clave_de_aplicacion(prueba).c_str());
  RegDeleteTreeW(HKEY_CURRENT_USER, clave_de_activador(prueba).c_str());
}

std::wstring xml_de_aviso(const cliente::Notificacion& aviso) {
  const auto documento = combase().ok() && iniciar_com() ? documento_de(aviso, false) : nullptr;
  ComPtr<IXmlNodeSerializer> serializador;
  HSTRING xml = nullptr;
  if (!documento || FAILED(documento.As(&serializador)) || FAILED(serializador->GetXml(&xml))) {
    return {};
  }
  UINT32 largo = 0;
  const wchar_t* texto = combase().leer(xml, &largo);
  std::wstring resultado(texto, largo);
  combase().borrar(xml);
  return resultado;
}
#else
std::unique_ptr<Notificador> notificaciones_de_windows(HWND, const std::wstring&, bool) { return nullptr; }
void escuchar_clics_de_windows(HWND) {}
void soltar_clics_de_windows() {}
long registro_de_clics_de_windows() { return 1; }
std::optional<cliente::Notificacion> tomar_clic_de_windows() { return std::nullopt; }
std::size_t notificaciones_en_windows(bool) { return 0; }
void olvidar_notificaciones_de_windows(bool) {}
std::wstring xml_de_aviso(const cliente::Notificacion&) { return {}; }
#endif
}
