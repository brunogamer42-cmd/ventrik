#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ventrik::red {

constexpr std::uint16_t kPuertoPredeterminado = 7700;

struct Direccion {
  std::uint32_t ipv4 = 0;  // a.b.c.d es (a << 24) | (b << 16) | (c << 8) | d
  std::uint16_t puerto = kPuertoPredeterminado;
};

bool operator==(const Direccion& a, const Direccion& b);

// "a.b.c.d" o "a.b.c.d:puerto", sin ceros adelante. Por ahora solo IPv4.
std::optional<Direccion> leer_direccion(std::string_view texto);
std::string a_texto(const Direccion& direccion);

class Inicio {
 public:
  Inicio();
  ~Inicio();
  Inicio(const Inicio&) = delete;
  Inicio& operator=(const Inicio&) = delete;

  bool ok() const { return ok_; }

 private:
  bool ok_ = false;
};

// Un SOCKET de Winsock, guardado así para no meter winsock2.h en los encabezados.
class Socket {
 public:
  Socket() = default;
  explicit Socket(std::uintptr_t valor) : socket_(valor) {}
  ~Socket();
  Socket(Socket&& otro) noexcept : socket_(std::exchange(otro.socket_, kInvalido)) {}
  Socket& operator=(Socket&& otro) noexcept;
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  std::uintptr_t get() const { return socket_; }
  bool valido() const { return socket_ != kInvalido; }

  static constexpr std::uintptr_t kInvalido = ~std::uintptr_t{0};

 private:
  std::uintptr_t socket_ = kInvalido;
};

// Todos los sockets quedan sin trabarse al esperar datos, y sin heredarse a otros procesos.
Socket escuchar(const Direccion& donde);
std::optional<Direccion> direccion_local(const Socket& cual);
// `fallo` queda en true si el puerto mismo falló (no si solo no había nadie esperando).
std::optional<std::pair<Socket, Direccion>> aceptar(const Socket& escucha, bool& fallo);
Socket conectar(const Direccion& destino, std::chrono::milliseconds tiempo);

enum class Espera { lista, tiempo_vencido, error };
Espera esperar(const Socket& cual, bool para_leer, bool para_escribir,
               std::chrono::milliseconds tiempo);

}  // namespace ventrik::red
