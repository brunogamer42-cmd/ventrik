#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace ventrik::servidor {
using Instante = std::chrono::steady_clock::time_point;

class Cubeta {
 public:
  Cubeta() = default;
  Cubeta(double rafaga, double por_segundo, Instante ahora = {})
      : rafaga_(rafaga), por_segundo_(por_segundo), fichas_(rafaga), ultima_(ahora) {}
  bool tomar(Instante ahora);

 private:
  double rafaga_ = 0;
  double por_segundo_ = 0;
  double fichas_ = 0;
  Instante ultima_;
};

struct OpcionesProteccion {
  double rafaga_por_ip = 16;
  double conexiones_por_segundo_por_ip = 1;
  double rafaga_total = 40;
  double saludos_por_segundo_total = 20;
  int fallas_para_bloquear = 5;
  std::chrono::seconds ventana_de_fallas{60};
  std::chrono::seconds tiempo_de_bloqueo{300};
  std::chrono::hours conocida_por{24};
  double rafaga_reserva = 20;
  double reserva_por_segundo = 10;
  double rafaga_chat_por_ip = 20;
  double chat_por_segundo_por_ip = 2;
  std::chrono::seconds silencio_entre_avisos{30};
  std::size_t ips_recordadas = 4096;
};

OpcionesProteccion proteccion_para(unsigned nucleos);

enum class Decision { aceptar, demasiado_rapido, bloqueada, ocupado };

struct Veredicto {
  Decision decision = Decision::aceptar;
  bool avisar = false;
};

class Proteccion {
 public:
  explicit Proteccion(OpcionesProteccion opciones = {});

  Veredicto nueva_conexion(std::uint32_t ip, Instante ahora);
  Veredicto empezar_saludo(std::uint32_t ip, Instante ahora);
  bool fallo(std::uint32_t ip, Instante ahora);
  void saludo_completo(std::uint32_t ip, Instante ahora);
  bool puede_chatear(std::uint32_t ip, Instante ahora);

  std::size_t ips_recordadas() const { return ips_.size(); }

 private:
  struct Ip {
    Cubeta conexiones;
    Cubeta chat;
    int fallas = 0;
    Instante primera_falla;
    Instante bloqueada_hasta;
    Instante avisada_hasta;
    Instante conocida_hasta;
    Instante ultimo_uso;
  };

  Ip& registro_de(std::uint32_t ip, Instante ahora);
  void olvidar(Instante ahora);
  bool avisar_una_vez(Instante& avisada_hasta, Instante ahora) const;

  OpcionesProteccion opciones_;
  std::unordered_map<std::uint32_t, Ip> ips_;
  Cubeta total_;
  Cubeta reserva_;
  Instante total_avisado_hasta_;
};

}
