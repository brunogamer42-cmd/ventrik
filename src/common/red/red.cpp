#include "common/red/red.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "common/pendiente.h"

namespace ventrik::red {

namespace {

constexpr int kColaDeEspera = 64;

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

bool preparar(SOCKET s) {
  u_long sin_trabarse = 1;
  BOOL sin_demora = TRUE;
  // Sin heredarse a ClamAV. Puede fallar en XP con firewalls viejos (LSP): no es motivo para no andar.
  SetHandleInformation(reinterpret_cast<HANDLE>(s), HANDLE_FLAG_INHERIT, 0);
  return ioctlsocket(s, FIONBIO, &sin_trabarse) == 0 &&
         setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&sin_demora),
                    sizeof(sin_demora)) == 0;
}

Espera esperar_socket(SOCKET s, bool para_leer, bool para_escribir, bool ver_fallas,
                           std::chrono::milliseconds tiempo) {
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
  const auto milisegundos = tiempo.count() < 0 ? 0 : tiempo.count();
  timeval espera = {};
  espera.tv_sec = static_cast<long>(milisegundos / 1000);
  espera.tv_usec = static_cast<long>((milisegundos % 1000) * 1000);
  const int listos = select(0, &lectura, &escritura, &falla, &espera);
  if (listos < 0 || FD_ISSET(s, &falla)) {
    return Espera::error;
  }
  return listos == 0 ? Espera::tiempo_vencido : Espera::lista;
}

Socket nuevo_socket() {
  VENTRIK_PENDIENTE("solo IPv4: IPv6 en la etapa 6 (XP no atiende las dos con un socket)");
  Socket nuevo(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
  if (!nuevo.valido() || !preparar(nuevo.get())) {
    return Socket();
  }
  return nuevo;
}

}  // namespace

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
  if (!escucha.valido()) {
    return Socket();
  }
  // Que otro programa no robe el puerto; si XP no lo deja (sin ser administrador), igual anda.
  BOOL exclusivo = TRUE;
  setsockopt(escucha.get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
             reinterpret_cast<const char*>(&exclusivo), sizeof(exclusivo));
  const sockaddr_in direccion = a_sockaddr(donde);
  if (bind(escucha.get(), reinterpret_cast<const sockaddr*>(&direccion), sizeof(direccion)) != 0 ||
      listen(escucha.get(), kColaDeEspera) != 0) {
    return Socket();
  }
  return escucha;
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
  // Cualquiera puede conectarse y cortar antes de ser aceptado: eso no es una falla del puerto.
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

Socket conectar(const Direccion& destino, std::chrono::milliseconds tiempo) {
  Socket nuevo = nuevo_socket();
  if (!nuevo.valido()) {
    return Socket();
  }
  const sockaddr_in direccion = a_sockaddr(destino);
  if (connect(nuevo.get(), reinterpret_cast<const sockaddr*>(&direccion), sizeof(direccion)) != 0 &&
      WSAGetLastError() != WSAEWOULDBLOCK) {
    return Socket();
  }
  // En Windows, un connect que falla avisa como falla y no como escritura.
  if (esperar_socket(static_cast<SOCKET>(nuevo.get()), false, true, true, tiempo) != Espera::lista) {
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

Espera esperar(const Socket& cual, bool para_leer, bool para_escribir,
               std::chrono::milliseconds tiempo) {
  // Sin mirar fallas: un byte urgente (fuera de banda) las marcaría aunque la conexión ande.
  return esperar_socket(static_cast<SOCKET>(cual.get()), para_leer, para_escribir, false, tiempo);
}

}  // namespace ventrik::red
