#include "common/red/red.h"

#include <array>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <qos2.h>

#include "common/pendiente.h"
#include "common/sistema.h"

namespace ventrik::red {
namespace {
constexpr int kColaDeEspera = 64;
constexpr int kBufferUdp = 256 * 1024;
constexpr DWORD kDscpDeVoz = 46;
constexpr DWORD kSinAvisoDePuertoCerrado = _WSAIOW(IOC_VENDOR, 12);
constexpr DWORD kSinAvisoDeRedCaida = _WSAIOW(IOC_VENDOR, 15);

std::optional<std::uint32_t> leer_numero(std::string_view& texto, std::size_t digitos,
                                         std::uint32_t maximo) {
  std::size_t usados = 0;
  std::uint32_t valor = 0;
  while (usados < texto.size() && texto[usados] >= '0' && texto[usados] <= '9') {
    if (usados == digitos || (usados == 1 && valor == 0)) {
      return std::nullopt;
    }
    valor = valor * 10 + static_cast<std::uint32_t>(texto[usados] - '0');
    ++usados;
  }
  if (usados == 0 || valor > maximo) {
    return std::nullopt;
  }
  texto.remove_prefix(usados);
  return valor;
}

sockaddr_in a_sockaddr(const Direccion& direccion) {
  sockaddr_in destino = {};
  destino.sin_family = AF_INET;
  destino.sin_port = htons(direccion.puerto);
  destino.sin_addr.s_addr = htonl(direccion.ipv4);
  return destino;
}

Direccion de_sockaddr(const sockaddr_in& origen) {
  return Direccion{ntohl(origen.sin_addr.s_addr), ntohs(origen.sin_port)};
}

bool preparar(SOCKET s, bool udp = false) {
  u_long sin_trabarse = 1;
  BOOL sin_demora = TRUE;
  BOOL avisar = FALSE;
  DWORD devueltos = 0;
  SetHandleInformation(reinterpret_cast<HANDLE>(s), HANDLE_FLAG_INHERIT, 0);
  if (ioctlsocket(s, FIONBIO, &sin_trabarse) != 0) {
    return false;
  }
  if (!udp) {
    return setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&sin_demora),
                      sizeof(sin_demora)) == 0;
  }
  for (const DWORD aviso : {kSinAvisoDePuertoCerrado, kSinAvisoDeRedCaida}) {
    WSAIoctl(s, aviso, &avisar, sizeof(avisar), nullptr, 0, &devueltos, nullptr, nullptr);
  }
  setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&kBufferUdp),
             sizeof(kBufferUdp));
  return true;
}

Espera esperar_socket(SOCKET s, bool para_leer, bool para_escribir, bool ver_fallas,
                           std::chrono::milliseconds tiempo, const Socket* cancelar) {
  fd_set lectura;
  fd_set escritura;
  fd_set falla;
  FD_ZERO(&lectura);
  FD_ZERO(&escritura);
  FD_ZERO(&falla);
  if (para_leer) {
    FD_SET(s, &lectura);
  }
  if (para_escribir) {
    FD_SET(s, &escritura);
  }
  if (ver_fallas) {
    FD_SET(s, &falla);
  }
  if (cancelar != nullptr) {
    FD_SET(static_cast<SOCKET>(cancelar->get()), &lectura);
  }
  const auto milisegundos = tiempo.count() < 0 ? 0 : tiempo.count();
  timeval espera = {};
  espera.tv_sec = static_cast<long>(milisegundos / 1000);
  espera.tv_usec = static_cast<long>((milisegundos % 1000) * 1000);
  const int listos = select(0, &lectura, &escritura, &falla, &espera);
  if (listos < 0 || FD_ISSET(s, &falla) ||
      (cancelar != nullptr && FD_ISSET(static_cast<SOCKET>(cancelar->get()), &lectura))) {
    return Espera::error;
  }
  return listos == 0 ? Espera::tiempo_vencido : Espera::lista;
}

Socket nuevo_socket(bool udp = false) {
  VENTRIK_PENDIENTE("solo IPv4: IPv6 en la etapa 6 (XP no atiende las dos con un socket)");
  Socket nuevo(socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, udp ? IPPROTO_UDP : IPPROTO_TCP));
  if (!nuevo.valido() || !preparar(nuevo.get(), udp)) {
    return Socket();
  }
  return nuevo;
}

bool atar(const Socket& cual, const Direccion& donde) {
  if (!cual.valido()) {
    return false;
  }
  BOOL exclusivo = TRUE;
  setsockopt(cual.get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusivo),
             sizeof(exclusivo));
  const sockaddr_in direccion = a_sockaddr(donde);
  return bind(cual.get(), reinterpret_cast<const sockaddr*>(&direccion), sizeof(direccion)) == 0;
}

}

bool operator==(const Direccion& a, const Direccion& b) {
  return a.ipv4 == b.ipv4 && a.puerto == b.puerto;
}

std::optional<Direccion> leer_direccion(std::string_view texto) {
  Direccion direccion;
  direccion.ipv4 = 0;
  for (int parte = 0; parte < 4; ++parte) {
    if (parte > 0) {
      if (texto.empty() || texto.front() != '.') {
        return std::nullopt;
      }
      texto.remove_prefix(1);
    }
    const auto valor = leer_numero(texto, 3, 255);
    if (!valor) {
      return std::nullopt;
    }
    direccion.ipv4 = (direccion.ipv4 << 8) | *valor;
  }
  if (!texto.empty()) {
    if (texto.front() != ':') {
      return std::nullopt;
    }
    texto.remove_prefix(1);
    const auto puerto = leer_numero(texto, 5, 65535);
    if (!puerto || *puerto == 0) {
      return std::nullopt;
    }
    direccion.puerto = static_cast<std::uint16_t>(*puerto);
  }
  if (!texto.empty()) {
    return std::nullopt;
  }
  return direccion;
}

std::string a_texto(const Direccion& direccion) {
  std::string texto;
  for (int parte = 3; parte >= 0; --parte) {
    texto += std::to_string((direccion.ipv4 >> (8 * parte)) & 0xFF);
    if (parte > 0) {
      texto.push_back('.');
    }
  }
  texto.push_back(':');
  texto += std::to_string(direccion.puerto);
  return texto;
}

Inicio::Inicio() {
  WSADATA datos = {};
  ok_ = WSAStartup(MAKEWORD(2, 2), &datos) == 0;
}

Inicio::~Inicio() {
  if (ok_) {
    WSACleanup();
  }
}

Socket::~Socket() {
  if (valido()) {
    closesocket(static_cast<SOCKET>(socket_));
  }
}

Socket& Socket::operator=(Socket&& otro) noexcept {
  if (this != &otro) {
    if (valido()) {
      closesocket(static_cast<SOCKET>(socket_));
    }
    socket_ = std::exchange(otro.socket_, kInvalido);
  }
  return *this;
}

Socket escuchar(const Direccion& donde) {
  Socket escucha = nuevo_socket();
  if (!atar(escucha, donde) || listen(escucha.get(), kColaDeEspera) != 0) {
    return Socket();
  }
  return escucha;
}

Socket abrir_udp(const Direccion& donde) {
  Socket nuevo = nuevo_socket(true);
  return atar(nuevo, donde) ? std::move(nuevo) : Socket();
}

Socket conectar_udp(const Direccion& destino) {
  Socket nuevo = nuevo_socket(true);
  const sockaddr_in direccion = a_sockaddr(destino);
  return nuevo.valido() && connect(nuevo.get(), reinterpret_cast<const sockaddr*>(&direccion),
                                   sizeof(direccion)) == 0
             ? std::move(nuevo)
             : Socket();
}

bool enviar_a(const Socket& cual, const Direccion& destino, gsl::span<const std::uint8_t> datos) {
  const sockaddr_in direccion = a_sockaddr(destino);
  return sendto(cual.get(), reinterpret_cast<const char*>(datos.data()),
                static_cast<int>(datos.size()), 0, reinterpret_cast<const sockaddr*>(&direccion),
                sizeof(direccion)) == static_cast<int>(datos.size());
}

std::optional<Recibido> recibir_de(const Socket& cual, gsl::span<std::uint8_t> destino) {
  sockaddr_in origen = {};
  int largo = sizeof(origen);
  const int leidos = recvfrom(cual.get(), reinterpret_cast<char*>(destino.data()),
                              static_cast<int>(destino.size()), 0,
                              reinterpret_cast<sockaddr*>(&origen), &largo);
  if (leidos >= 0) {
    return Recibido{static_cast<std::size_t>(leidos), de_sockaddr(origen)};
  }
  const int error = WSAGetLastError();
  if (error == WSAECONNRESET || error == WSAENETRESET || error == WSAEMSGSIZE) {
    return Recibido{0, de_sockaddr(origen)};
  }
  return std::nullopt;
}

std::optional<Direccion> direccion_local(const Socket& cual) {
  sockaddr_in local = {};
  int largo = sizeof(local);
  if (getsockname(cual.get(), reinterpret_cast<sockaddr*>(&local), &largo) != 0 ||
      local.sin_family != AF_INET) {
    return std::nullopt;
  }
  return de_sockaddr(local);
}

std::optional<std::pair<Socket, Direccion>> aceptar(const Socket& escucha, bool& fallo) {
  fallo = false;
  for (int intento = 0; intento < kColaDeEspera; ++intento) {
    sockaddr_in origen = {};
    int largo = sizeof(origen);
    Socket nuevo(accept(escucha.get(), reinterpret_cast<sockaddr*>(&origen), &largo));
    if (!nuevo.valido()) {
      const int error = WSAGetLastError();
      if (error == WSAECONNRESET || error == WSAECONNABORTED) {
        continue;
      }
      fallo = error != WSAEWOULDBLOCK;
      return std::nullopt;
    }
    if (origen.sin_family != AF_INET || !preparar(nuevo.get())) {
      return std::nullopt;
    }
    return std::make_pair(std::move(nuevo), de_sockaddr(origen));
  }
  return std::nullopt;
}

Socket conectar(const Direccion& destino, std::chrono::milliseconds tiempo, const Socket* cancelar) {
  Socket nuevo = nuevo_socket();
  if (!nuevo.valido()) {
    return Socket();
  }
  const sockaddr_in direccion = a_sockaddr(destino);
  if (connect(nuevo.get(), reinterpret_cast<const sockaddr*>(&direccion), sizeof(direccion)) != 0 &&
      WSAGetLastError() != WSAEWOULDBLOCK) {
    return Socket();
  }
  if (esperar_socket(static_cast<SOCKET>(nuevo.get()), false, true, true, tiempo, cancelar) != Espera::lista) {
    return Socket();
  }
  int error = 0;
  int largo = sizeof(error);
  if (getsockopt(nuevo.get(), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &largo) != 0 ||
      error != 0) {
    return Socket();
  }
  return nuevo;
}

Despertador::Despertador() {
  Socket nuevo = abrir_udp(Direccion{0x7F000001, 0});
  const auto propia = direccion_local(nuevo);
  const sockaddr_in destino = a_sockaddr(propia.value_or(Direccion{}));
  if (propia &&
      connect(nuevo.get(), reinterpret_cast<const sockaddr*>(&destino), sizeof(destino)) == 0) {
    socket_ = std::move(nuevo);
  }
}

void Despertador::despertar() const {
  const char uno = 1;
  if (socket_.valido()) {
    send(socket_.get(), &uno, 1, 0);
  }
}

void Despertador::vaciar() const {
  std::array<char, 64> basura{};
  for (int i = 0; i < 256 && socket_.valido() &&
                  recv(socket_.get(), basura.data(), static_cast<int>(basura.size()), 0) > 0;
       ++i) {
  }
}

bool PrioridadDeVoz::marcar(const Socket& cual, const Direccion& destino) {
  soltar();
  const HMODULE qwave = cargar_de_sistema(L"qwave.dll");
  if (qwave == nullptr) {
    return false;
  }
  const auto crear = reinterpret_cast<decltype(&QOSCreateHandle)>(GetProcAddress(qwave, "QOSCreateHandle"));
  const auto sumar = reinterpret_cast<decltype(&QOSAddSocketToFlow)>(GetProcAddress(qwave, "QOSAddSocketToFlow"));
  const auto ajustar = reinterpret_cast<decltype(&QOSSetFlow)>(GetProcAddress(qwave, "QOSSetFlow"));
  QOS_VERSION version = {1, 0};
  HANDLE manija = nullptr;
  if (crear == nullptr || sumar == nullptr || ajustar == nullptr || !crear(&version, &manija)) {
    return false;
  }
  manija_ = manija;
  sockaddr_in direccion = a_sockaddr(destino);
  QOS_FLOWID flujo = 0;
  if (!sumar(manija, cual.get(), reinterpret_cast<sockaddr*>(&direccion), QOSTrafficTypeVoice,
             QOS_NON_ADAPTIVE_FLOW, &flujo)) {
    soltar();
    return false;
  }
  DWORD dscp = kDscpDeVoz;
  ajustar(manija, flujo, QOSSetOutgoingDSCPValue, sizeof(dscp), &dscp, 0, nullptr);
  return true;
}

void PrioridadDeVoz::soltar() {
  if (manija_ != nullptr) {
    const auto cerrar = reinterpret_cast<decltype(&QOSCloseHandle)>(
        GetProcAddress(GetModuleHandleW(L"qwave.dll"), "QOSCloseHandle"));
    if (cerrar != nullptr) {
      cerrar(manija_);
    }
  }
  manija_ = nullptr;
}

Espera esperar(const Socket& cual, bool para_leer, bool para_escribir,
               std::chrono::milliseconds tiempo, const Socket* cancelar) {
  return esperar_socket(static_cast<SOCKET>(cual.get()), para_leer, para_escribir, false, tiempo, cancelar);
}

}
