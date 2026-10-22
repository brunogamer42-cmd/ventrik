#include "server/proteccion.h"

#include <algorithm>
#include <utility>

namespace ventrik::servidor {

namespace {

bool tomar_ficha(double& fichas, Proteccion::Instante& ultima, double rafaga, double por_segundo,
                 Proteccion::Instante ahora) {
  const std::chrono::duration<double> pasado = ahora - ultima;
  fichas = std::min(rafaga, fichas + std::max(0.0, pasado.count()) * por_segundo);
  ultima = ahora;
  if (fichas < 1) {
    return false;
  }
  fichas -= 1;
  return true;
}

}  // namespace

Proteccion::Proteccion(OpcionesProteccion opciones) : opciones_(std::move(opciones)) {
  total_.fichas = opciones_.rafaga_total;
  reserva_.fichas = opciones_.rafaga_reserva;
}

Proteccion::Ip& Proteccion::registro_de(std::uint32_t ip, Instante ahora) {
  auto encontrada = ips_.find(ip);
  if (encontrada == ips_.end()) {
    if (ips_.size() >= opciones_.ips_recordadas) {
      olvidar(ahora);
    }
    Ip nueva;
    nueva.conexiones.fichas = opciones_.rafaga_por_ip;
    nueva.conexiones.ultima = ahora;
    encontrada = ips_.emplace(ip, nueva).first;
  }
  encontrada->second.ultimo_uso = ahora;
  return encontrada->second;
}

void Proteccion::olvidar(Instante ahora) {
  // Primero las que no guardan nada importante: sin bloqueo, sin saludo bueno y quietas hace rato.
  for (auto ip = ips_.begin(); ip != ips_.end();) {
    const bool quieta = ahora - ip->second.ultimo_uso > opciones_.ventana_de_fallas;
    if (quieta && ahora >= ip->second.bloqueada_hasta && ahora >= ip->second.conocida_hasta) {
      ip = ips_.erase(ip);
    } else {
      ++ip;
    }
  }
  // Con miles de IPs atacando a la vez, la memoria igual tiene techo: se van primero las desconocidas.
  const auto importa = [ahora](const Ip& ip) {
    return ahora < ip.bloqueada_hasta || ahora < ip.conocida_hasta;
  };
  while (!ips_.empty() && ips_.size() >= opciones_.ips_recordadas) {
    const auto menos_importante = std::min_element(
        ips_.begin(), ips_.end(), [&importa](const auto& a, const auto& b) {
          const bool importa_a = importa(a.second);
          const bool importa_b = importa(b.second);
          return importa_a != importa_b ? !importa_a : a.second.ultimo_uso < b.second.ultimo_uso;
        });
    ips_.erase(menos_importante);
  }
}

bool Proteccion::avisar_una_vez(Instante& avisada_hasta, Instante ahora) const {
  if (ahora < avisada_hasta) {
    return false;
  }
  avisada_hasta = ahora + opciones_.silencio_entre_avisos;
  return true;
}

Veredicto Proteccion::nueva_conexion(std::uint32_t ip, Instante ahora) {
  Ip& registro = registro_de(ip, ahora);
  Veredicto veredicto;
  if (ahora < registro.bloqueada_hasta) {
    veredicto.decision = Decision::bloqueada;
  } else if (!tomar_ficha(registro.conexiones.fichas, registro.conexiones.ultima,
                          opciones_.rafaga_por_ip, opciones_.conexiones_por_segundo_por_ip,
                          ahora)) {
    veredicto.decision = Decision::demasiado_rapido;
  } else {
    return veredicto;
  }
  veredicto.avisar = avisar_una_vez(registro.avisada_hasta, ahora);
  return veredicto;
}

Veredicto Proteccion::empezar_saludo(std::uint32_t ip, Instante ahora) {
  Veredicto veredicto;
  if (tomar_ficha(total_.fichas, total_.ultima, opciones_.rafaga_total,
                  opciones_.saludos_por_segundo_total, ahora)) {
    return veredicto;
  }
  const auto encontrada = ips_.find(ip);
  const bool conocida = encontrada != ips_.end() && ahora < encontrada->second.conocida_hasta;
  if (conocida && tomar_ficha(reserva_.fichas, reserva_.ultima, opciones_.rafaga_reserva,
                              opciones_.reserva_por_segundo, ahora)) {
    return veredicto;
  }
  veredicto.decision = Decision::ocupado;
  veredicto.avisar = avisar_una_vez(total_avisado_hasta_, ahora);
  return veredicto;
}

bool Proteccion::fallo(std::uint32_t ip, Instante ahora) {
  Ip& registro = registro_de(ip, ahora);
  registro.conocida_hasta = Instante{};
  if (registro.fallas == 0 || ahora - registro.primera_falla > opciones_.ventana_de_fallas) {
    registro.fallas = 0;
    registro.primera_falla = ahora;
  }
  if (++registro.fallas < opciones_.fallas_para_bloquear) {
    return false;
  }
  registro.fallas = 0;
  registro.bloqueada_hasta = ahora + opciones_.tiempo_de_bloqueo;
  return true;
}

void Proteccion::saludo_completo(std::uint32_t ip, Instante ahora) {
  registro_de(ip, ahora).conocida_hasta = ahora + opciones_.conocida_por;
}

}  // namespace ventrik::servidor
