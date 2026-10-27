#include <array>
#include <string>

#include <windows.h>

#if defined(VENTRIK_MODERNA)
#include <notificationactivationcallback.h>
#include <tlhelp32.h>

#include "client/diagnostico.h"
#include "common/archivos.h"
#include "common/protocol/bytes.h"
#endif

#include "client/ui/notificador.h"
#include "servidor_de_prueba.h"
#include <doctest/doctest.h>

#if defined(VENTRIK_MODERNA)
namespace {
ventrik::cliente::Notificacion aviso_de_prueba(std::uint32_t canal, std::uint64_t version) {
  ventrik::cliente::Notificacion aviso;
  aviso.canal = canal;
  aviso.titulo = "srv \xC2\xB7 #<general> & \"amigos\"";
  aviso.cuerpo = "Tano: <b>hola</b> & chau \xF0\x9F\x98\x80 \xEF\xBF\xBF\x01";
  aviso.version = version;
  aviso.servidor = "127.0.0.1:7700";
  return aviso;
}

std::size_t hilos_propios() {
  const HANDLE foto = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  THREADENTRY32 hilo = {};
  hilo.dwSize = sizeof(hilo);
  std::size_t cuantos = 0;
  for (BOOL hay = Thread32First(foto, &hilo); hay; hay = Thread32Next(foto, &hilo)) {
    cuantos += hilo.th32OwnerProcessID == GetCurrentProcessId() ? 1 : 0;
  }
  CloseHandle(foto);
  return cuantos;
}

struct OlvidarAlSalir {
  ~OlvidarAlSalir() { ventrik::ui::olvidar_notificaciones_de_windows(true); }
};
}

TEST_CASE("El texto de una notificación de Windows va como texto, nunca como XML") {
  const std::wstring xml = ventrik::ui::xml_de_aviso(aviso_de_prueba(7, 1));
  if (xml.empty()) {
    MESSAGE("las notificaciones de Windows no estan disponibles en esta PC");
    return;
  }
  CHECK(xml.find(L"&lt;b&gt;hola&lt;/b&gt; &amp; chau") != std::wstring::npos);
  CHECK(xml.find(L"<b>") == std::wstring::npos);
  CHECK(xml.find(L"#&lt;general&gt; &amp;") != std::wstring::npos);
  CHECK(xml.find(L"launch=\"servidor=127.0.0.1:7700;canal=7;voz=0\"") != std::wstring::npos);
  CHECK(xml.find(L"template=\"ToastGeneric\"") != std::wstring::npos);
  CHECK(xml.find(wchar_t{0xFFFF}) == std::wstring::npos);
  CHECK(xml.find(wchar_t{0x01}) == std::wstring::npos);
  CHECK(xml.find(std::wstring(2, wchar_t{0xFFFD})) != std::wstring::npos);
}

TEST_CASE("Windows muestra, reemplaza y quita las notificaciones de un canal") {
  std::array<wchar_t, MAX_PATH> temporal{};
  GetTempPathW(static_cast<DWORD>(temporal.size()), temporal.data());
  const std::wstring carpeta = std::wstring(temporal.data()) + L"ventrik_notificaciones_de_prueba";
  DeleteFileW((carpeta + L"\\ventrik.ico").c_str());
  const OlvidarAlSalir olvidar;
  const std::size_t hilos_antes = hilos_propios();
  const std::uint64_t memoria_antes = ventrik::cliente::Diagnostico().medir().memoria_propia;
  auto windows = ventrik::ui::notificaciones_de_windows(nullptr, carpeta, true);
  if (!windows) {
    CHECK((ventrik::ui::proceso_elevado() || ventrik::ui::xml_de_aviso(aviso_de_prueba(7, 1)).empty()));
    MESSAGE("las notificaciones de Windows no estan disponibles en esta PC");
    return;
  }
  MESSAGE("notificaciones de Windows: "
          << static_cast<long long>(hilos_propios()) - static_cast<long long>(hilos_antes) << " hilos y "
          << (static_cast<long long>(ventrik::cliente::Diagnostico().medir().memoria_propia) -
              static_cast<long long>(memoria_antes)) / 1024
          << " KB mas");

  ventrik::cifrado::Secreto icono;
  REQUIRE(ventrik::leer_archivo(carpeta + L"\\ventrik.ico", 1 << 20, icono) == ventrik::LecturaArchivo::leido);
  ventrik::protocolo::Lector cabecera(icono.bytes());
  CHECK(cabecera.leer_u16() == 0);
  CHECK(cabecera.leer_u16() == 1);
  CHECK(cabecera.leer_u16() == 1);
  cabecera.leer_u32();
  cabecera.leer_u32();
  CHECK(cabecera.leer_u32() + 22 == icono.size());
  CHECK(cabecera.leer_u32() == 22);
  REQUIRE(cabecera.ok());

  REQUIRE(windows->mostrar({aviso_de_prueba(7, 1)}));
  CHECK(prueba::esperar_que([] { return ventrik::ui::notificaciones_en_windows(true) == 1; }));
  REQUIRE(windows->mostrar({aviso_de_prueba(7, 2), aviso_de_prueba(8, 3)}));
  CHECK(prueba::esperar_que([] { return ventrik::ui::notificaciones_en_windows(true) == 2; }));
  CHECK(windows->mostrados() == 2);
  REQUIRE(windows->mostrar({aviso_de_prueba(8, 3)}));
  CHECK(prueba::esperar_que([] { return ventrik::ui::notificaciones_en_windows(true) == 1; }));
  REQUIRE(windows->mostrar({}));
  CHECK(prueba::esperar_que([] { return ventrik::ui::notificaciones_en_windows(true) == 0; }));

  std::array<wchar_t, 64> guid{};
  DWORD bytes = static_cast<DWORD>(guid.size() * sizeof(wchar_t));
  REQUIRE(RegGetValueW(HKEY_CURRENT_USER, L"Software\\Classes\\AppUserModelId\\Ventrik.Prueba", L"CustomActivator",
                       RRF_RT_REG_SZ, nullptr, guid.data(), &bytes) == ERROR_SUCCESS);
  CLSID clase = {};
  REQUIRE(SUCCEEDED(CLSIDFromString(guid.data(), &clase)));
  const long registro = ventrik::ui::registro_de_clics_de_windows();
  INFO("registro del activador: " << static_cast<unsigned long>(registro));
  REQUIRE(registro == S_OK);
  INotificationActivationCallback* activador = nullptr;
  const HRESULT creado = CoCreateInstance(clase, nullptr, CLSCTX_LOCAL_SERVER, __uuidof(INotificationActivationCallback),
                                          reinterpret_cast<void**>(&activador));
  INFO("CoCreateInstance: " << static_cast<unsigned long>(creado));
  REQUIRE(SUCCEEDED(creado));
  const auto tocar = [activador](const wchar_t* argumentos) {
    CHECK(SUCCEEDED(activador->Activate(L"Ventrik.Prueba", argumentos, nullptr, 0)));
  };
  const auto canal_tocado = [] {
    const auto clic = ventrik::ui::tomar_clic_de_windows();
    return clic && clic->servidor == "127.0.0.1:7700" ? clic->canal : 0u;
  };
  tocar(L"servidor=127.0.0.1:7700;canal=7;voz=0");
  tocar(L"servidor=127.0.0.1:7700;canal=<script>;voz=0");
  tocar((L"servidor=127.0.0.1:7700;canal=8;voz=0" + std::wstring(200, L'x')).c_str());
  tocar(nullptr);
  tocar(L"servidor=127.0.0.1:7700;canal=9;voz=0");
  CHECK(canal_tocado() == 7);
  CHECK(canal_tocado() == 0);
  CHECK(canal_tocado() == 0);
  CHECK(canal_tocado() == 9);
  CHECK_FALSE(ventrik::ui::tomar_clic_de_windows());
  for (int i = 0; i < 9; ++i) {
    tocar(L"servidor=127.0.0.1:7700;canal=5;voz=0");
  }
  for (int i = 0; i < 8; ++i) {
    CHECK(canal_tocado() == 5);
  }
  CHECK_FALSE(ventrik::ui::tomar_clic_de_windows());
  activador->Release();

  ventrik::ui::olvidar_notificaciones_de_windows(true);
  HKEY clave = nullptr;
  CHECK(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\AppUserModelId\\Ventrik.Prueba", 0, KEY_READ,
                      &clave) != ERROR_SUCCESS);
}
#endif
