#include "server/proteccion.h"

#include <algorithm>
#include <utility>

namespace ventrik::servidor {

bool Cubeta::tomar(Instante ahora) {
  const std::chrono::duration<double> pasado = ahora - ultima_;
  fichas_ = std::min(rafaga_, fichas_ + std::max(0.0, pasado.count()) * por_segundo_);
  ultima_ = ahora;
  if (fichas_ < 1) {
    return false;
  }
  fichas_ -= 1;
  return true;
}

Proteccion::Proteccion(OpcionesProteccion opciones)
    : opciones_(std::move(opciones)),
      total_(opciones_.rafaga_total, opciones_.saludos_por_segundo_total),
      reserva_(opciones_.rafaga_reserva, opciones_.reserva_por_segundo) {}

Proteccion::Ip& Proteccion::registro_de(std::uint32_t ip, Instante ahora) {
  auto encontrada = ips_.find(ip);
  if (encontrada == ips_.end()) {
    if (ips_.size() >= opciones_.ips_recordadas) {
      olvidar(ahora);
    }
    Ip nueva;
    nueva.conexiones = Cubeta(opciones_.rafaga_por_ip, opciones_.conexiones_por_segundo_por_ip, ahora);
    nueva.chat = Cubeta(opciones_.rafaga_chat_por_ip, opciones_.chat_por_segundo_por_ip, ahora);
    encontrada = ips_.emplace(ip, nueva).first;
  }
  encontrada->second.ultimo_uso = ahora;
  return encontrada->second;
}

void Proteccion::olvidar(Instante ahora) {
  for (auto ip = ips_.begin(); ip != ips_.end();) {
    const bool quieta = ahora - ip->second.ultimo_uso > opciones_.ventana_de_fallas;
    if (quieta && ahora >= ip->second.bloqueada_hasta && ahora >= ip->second.conocida_hasta) {
      ip = ips_.erase(ip);
    } else {
      ++ip;
    }
  }
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
  } else if (!registro.conexiones.tomar(ahora)) {
    veredicto.decision = Decision::demasiado_rapido;
  } else {
    return veredicto;
  }
  veredicto.avisar = avisar_una_vez(registro.avisada_hasta, ahora);
  return veredicto;
}

Veredicto Proteccion::empezar_saludo(std::uint32_t ip, Instante ahora) {
  Veredicto veredicto;
  if (total_.tomar(ahora)) {
    return veredicto;
  }
  const auto encontrada = ips_.find(ip);
  const bool conocida = encontrada != ips_.end() && ahora < encontrada->second.conocida_hasta;
  if (conocida && reserva_.tomar(ahora)) {
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

OpcionesProteccion proteccion_para(unsigned nucleos) {
  OpcionesProteccion opciones;
  if (nucleos <= 1) {
    opciones.saludos_por_segundo_total /= 2;
    opciones.rafaga_total /= 2;
    opciones.reserva_por_segundo /= 2;
    opciones.rafaga_reserva /= 2;
  }
  return opciones;
}

bool Proteccion::puede_chatear(std::uint32_t ip, Instante ahora) {
  return registro_de(ip, ahora).chat.tomar(ahora);
}

}
