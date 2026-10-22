#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace ventrik::servidor {

// Valores iniciales, para ajustar después de medir en la HP 2133 (diseño: "Contra saturación").
struct OpcionesProteccion {
  double rafaga_por_ip = 16;
  double conexiones_por_segundo_por_ip = 1;
  // Cada saludo cuesta una firma ECDSA: con muchas IPs a la vez, esto cuida el procesador.
  double rafaga_total = 40;
  double saludos_por_segundo_total = 20;
  int fallas_para_bloquear = 5;
  std::chrono::seconds ventana_de_fallas{60};
  std::chrono::seconds tiempo_de_bloqueo{300};
  // Reserva para quien ya saludó bien, con techo: cualquiera puede saludar bien una vez.
  std::chrono::hours conocida_por{24};
  double rafaga_reserva = 20;
  double reserva_por_segundo = 10;
  // Durante un ataque, cada IP se avisa una vez cada tanto y no en cada intento.
  std::chrono::seconds silencio_entre_avisos{30};
  std::size_t ips_recordadas = 4096;
};

enum class Decision { aceptar, demasiado_rapido, bloqueada, ocupado };

struct Veredicto {
  Decision decision = Decision::aceptar;
  bool avisar = false;
};

class Proteccion {
 public:
  using Instante = std::chrono::steady_clock::time_point;

  explicit Proteccion(OpcionesProteccion opciones = {});

  // Al aceptar: bloqueo y ritmo de la IP.
  Veredicto nueva_conexion(std::uint32_t ip, Instante ahora);
  // Cuando el otro empieza a hablar, antes de la parte cara del cifrado: el turno global.
  Veredicto empezar_saludo(std::uint32_t ip, Instante ahora);
  // Devuelve true si con esta falla la IP quedó bloqueada.
  bool fallo(std::uint32_t ip, Instante ahora);
  // No borra las fallas: si no, un saludo bueno cada cuatro malos esquivaría el bloqueo.
  void saludo_completo(std::uint32_t ip, Instante ahora);

  std::size_t ips_recordadas() const { return ips_.size(); }

 private:
  struct Cubeta {
    double fichas = 0;
    Instante ultima;
  };
  struct Ip {
    Cubeta conexiones;
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

}  // namespace ventrik::servidor
