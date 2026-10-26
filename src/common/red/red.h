#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <gsl/gsl>

namespace ventrik::red {
constexpr std::uint16_t kPuertoPredeterminado = 7700;

struct Direccion {
  std::uint32_t ipv4 = 0;
  std::uint16_t puerto = kPuertoPredeterminado;
};

bool operator==(const Direccion& a, const Direccion& b);

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

Socket escuchar(const Direccion& donde);
std::optional<Direccion> direccion_local(const Socket& cual);
std::optional<std::pair<Socket, Direccion>> aceptar(const Socket& escucha, bool& fallo);
Socket conectar(const Direccion& destino, std::chrono::milliseconds tiempo, const Socket* cancelar = nullptr,
                const Direccion* origen = nullptr);

struct Recibido {
  std::size_t largo = 0;
  Direccion origen;
};

Socket abrir_udp(const Direccion& donde);
Socket conectar_udp(const Direccion& destino, const Direccion* origen = nullptr);
bool enviar_a(const Socket& cual, const Direccion& destino, gsl::span<const std::uint8_t> datos);
std::optional<Recibido> recibir_de(const Socket& cual, gsl::span<std::uint8_t> destino);

class PrioridadDeVoz {
 public:
  PrioridadDeVoz() = default;
  ~PrioridadDeVoz() { soltar(); }
  PrioridadDeVoz(const PrioridadDeVoz&) = delete;
  PrioridadDeVoz& operator=(const PrioridadDeVoz&) = delete;

  bool marcar(const Socket& cual, const Direccion& destino);
  void soltar();

 private:
  void* manija_ = nullptr;
};

enum class Espera { lista, tiempo_vencido, error };
Espera esperar(const Socket& cual, bool para_leer, bool para_escribir,
               std::chrono::milliseconds tiempo, const Socket* cancelar = nullptr);

class Despertador {
 public:
  Despertador();
  bool ok() const { return socket_.valido(); }
  const Socket& socket() const { return socket_; }
  void despertar() const;
  void vaciar() const;

 private:
  Socket socket_;
};

}
