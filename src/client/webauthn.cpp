#include "client/webauthn.h"

#if defined(VENTRIK_MODERNA)
#include <array>
#include <utility>

#include <webauthn.h>

#include "common/cifrado/inicio.h"
#include "common/sistema.h"
#endif

namespace ventrik::cliente {
#if defined(VENTRIK_MODERNA)
namespace {

constexpr wchar_t kAplicacion[] = L"ventrik";
constexpr DWORD kTiempoLimite = 120000;

struct Funciones {
  decltype(&WebAuthNGetApiVersionNumber) version = nullptr;
  decltype(&WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable) hay_hello = nullptr;
  decltype(&WebAuthNAuthenticatorMakeCredential) crear = nullptr;
  decltype(&WebAuthNAuthenticatorGetAssertion) firmar = nullptr;
  decltype(&WebAuthNFreeCredentialAttestation) soltar_creada = nullptr;
  decltype(&WebAuthNFreeAssertion) soltar_firma = nullptr;
  bool lista = false;
};

template <typename Funcion>
bool buscar(HMODULE dll, const char* nombre, Funcion& funcion) {
  funcion = reinterpret_cast<Funcion>(GetProcAddress(dll, nombre));
  return funcion != nullptr;
}

const Funciones& funciones() {
  static const Funciones cargadas = [] {
    Funciones f;
    const HMODULE dll = cargar_de_sistema(L"webauthn.dll");
    f.lista = dll != nullptr && buscar(dll, "WebAuthNGetApiVersionNumber", f.version) &&
              buscar(dll, "WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable", f.hay_hello) &&
              buscar(dll, "WebAuthNAuthenticatorMakeCredential", f.crear) &&
              buscar(dll, "WebAuthNAuthenticatorGetAssertion", f.firmar) &&
              buscar(dll, "WebAuthNFreeCredentialAttestation", f.soltar_creada) &&
              buscar(dll, "WebAuthNFreeAssertion", f.soltar_firma) && f.version() >= WEBAUTHN_API_VERSION_1;
    return f;
  }();
  return cargadas;
}

std::vector<std::uint8_t> copia(const BYTE* datos, DWORD largo) {
  return datos != nullptr ? std::vector<std::uint8_t>(datos, datos + largo) : std::vector<std::uint8_t>();
}

std::vector<WEBAUTHN_CREDENTIAL> credenciales(const std::vector<IdDeLlave>& ids) {
  std::vector<WEBAUTHN_CREDENTIAL> lista;
  for (const auto& id : ids) {
    lista.push_back({WEBAUTHN_CREDENTIAL_CURRENT_VERSION, static_cast<DWORD>(id.size()), const_cast<PBYTE>(id.data()),
                     WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY});
  }
  return lista;
}

WEBAUTHN_CLIENT_DATA datos_de_cliente(std::string& texto) {
  return {WEBAUTHN_CLIENT_DATA_CURRENT_VERSION, static_cast<DWORD>(texto.size()), reinterpret_cast<PBYTE>(texto.data()),
          WEBAUTHN_HASH_ALGORITHM_SHA_256};
}

}

Autenticadores autenticadores_disponibles() {
  const auto& f = funciones();
  BOOL hello = FALSE;
  if (!f.lista) {
    return Autenticadores::ninguno;
  }
  return SUCCEEDED(f.hay_hello(&hello)) && hello ? Autenticadores::llaves_y_windows_hello : Autenticadores::llaves;
}

RespuestaDeWebAuthn<cifrado::LlaveCreada> crear_llave(HWND ventana, const std::wstring& nombre,
                                                      const std::vector<IdDeLlave>& excluir) {
  const auto& f = funciones();
  std::array<std::uint8_t, 32> usuario{};
  std::array<std::uint8_t, 32> desafio{};
  if (!f.lista) {
    return {};
  }
  if (nombre.empty() || cifrado::aleatorio(nullptr, usuario.data(), usuario.size()) != 0 ||
      cifrado::aleatorio(nullptr, desafio.data(), desafio.size()) != 0) {
    return {std::nullopt, E_INVALIDARG};
  }
  const WEBAUTHN_RP_ENTITY_INFORMATION aplicacion{WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION, kAplicacion,
                                                  kAplicacion, nullptr};
  const WEBAUTHN_USER_ENTITY_INFORMATION persona{WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION,
                                                 static_cast<DWORD>(usuario.size()), usuario.data(), nombre.c_str(),
                                                 nullptr, nombre.c_str()};
  WEBAUTHN_COSE_CREDENTIAL_PARAMETER es256{WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION,
                                           WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY,
                                           WEBAUTHN_COSE_ALGORITHM_ECDSA_P256_WITH_SHA256};
  const WEBAUTHN_COSE_CREDENTIAL_PARAMETERS algoritmos{1, &es256};
  std::string texto = cifrado::texto_de_cliente(cifrado::Ceremonia::crear, desafio);
  const WEBAUTHN_CLIENT_DATA cliente = datos_de_cliente(texto);
  auto excluidas = credenciales(excluir);
  WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS opciones{};
  opciones.dwVersion = WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_1;
  opciones.dwTimeoutMilliseconds = kTiempoLimite;
  opciones.CredentialList = {static_cast<DWORD>(excluidas.size()), excluidas.data()};
  opciones.dwAuthenticatorAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;
  opciones.dwUserVerificationRequirement = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
  opciones.dwAttestationConveyancePreference = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE;
  WEBAUTHN_CREDENTIAL_ATTESTATION* creada = nullptr;
  const HRESULT resultado = f.crear(ventana, &aplicacion, &persona, &algoritmos, &cliente, &opciones, &creada);
  const auto soltar = gsl::finally([&] { f.soltar_creada(creada); });
  if (FAILED(resultado)) {
    return {std::nullopt, resultado};
  }
  auto llave = creada != nullptr
                   ? cifrado::leer_llave_creada(copia(creada->pbAuthenticatorData, creada->cbAuthenticatorData))
                   : std::nullopt;
  const HRESULT error = llave ? S_OK : NTE_BAD_DATA;
  return {std::move(llave), error};
}

RespuestaDeWebAuthn<FirmaDeLlave> firmar_con_llave(HWND ventana, const std::vector<IdDeLlave>& llaves,
                                                   gsl::span<const std::uint8_t> desafio) {
  const auto& f = funciones();
  if (!f.lista) {
    return {};
  }
  if (llaves.empty()) {
    return {std::nullopt, E_INVALIDARG};
  }
  std::string texto = cifrado::texto_de_cliente(cifrado::Ceremonia::firmar, desafio);
  const WEBAUTHN_CLIENT_DATA cliente = datos_de_cliente(texto);
  auto permitidas = credenciales(llaves);
  WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS opciones{};
  opciones.dwVersion = WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_1;
  opciones.dwTimeoutMilliseconds = kTiempoLimite;
  opciones.CredentialList = {static_cast<DWORD>(permitidas.size()), permitidas.data()};
  opciones.dwAuthenticatorAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;
  opciones.dwUserVerificationRequirement = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
  WEBAUTHN_ASSERTION* firma = nullptr;
  const HRESULT resultado = f.firmar(ventana, kAplicacion, &cliente, &opciones, &firma);
  const auto soltar = gsl::finally([&] {
    if (firma != nullptr) {
      f.soltar_firma(firma);
    }
  });
  if (FAILED(resultado) || firma == nullptr) {
    return {std::nullopt, FAILED(resultado) ? resultado : NTE_BAD_DATA};
  }
  return {FirmaDeLlave{copia(firma->Credential.pbId, firma->Credential.cbId),
                       copia(firma->pbAuthenticatorData, firma->cbAuthenticatorData), std::move(texto),
                       copia(firma->pbSignature, firma->cbSignature)},
          S_OK};
}
#else
Autenticadores autenticadores_disponibles() { return Autenticadores::ninguno; }
RespuestaDeWebAuthn<cifrado::LlaveCreada> crear_llave(HWND, const std::wstring&, const std::vector<IdDeLlave>&) {
  return {};
}
RespuestaDeWebAuthn<FirmaDeLlave> firmar_con_llave(HWND, const std::vector<IdDeLlave>&, gsl::span<const std::uint8_t>) {
  return {};
}
#endif
}
