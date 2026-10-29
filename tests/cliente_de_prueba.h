#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "client/sesion.h"
#include "common/cifrado/codigo_dueno.h"
#include "common/cifrado/totp.h"
#include "identidad_de_prueba.h"
#include "servidor_de_prueba.h"
#include <doctest/doctest.h>

namespace prueba {

class Cliente {
 public:
  Cliente(const ventrik::red::Direccion& direccion, const ventrik::cifrado::Huella& huella, std::string nombre,
          std::optional<ventrik::cifrado::CodigoDueno> codigo = {}, ventrik::cliente::OpcionesSesion opciones = {})
      : sesion_(direccion, huella, presentacion(std::move(nombre), codigo), std::move(opciones)) {}
  Cliente(const ServidorDePrueba& servidor, ventrik::cliente::Presentacion quien,
          ventrik::cliente::OpcionesSesion opciones = {})
      : sesion_(servidor.direccion(), servidor.huella(), std::move(quien), std::move(opciones)) {}
  Cliente(const ServidorDePrueba& servidor, std::string nombre,
          std::optional<ventrik::cifrado::CodigoDueno> codigo = {}, ventrik::cliente::OpcionesSesion opciones = {})
      : Cliente(servidor.direccion(), servidor.huella(), std::move(nombre), codigo, std::move(opciones)) {}

  template <typename T>
  std::optional<T> esperar(const std::function<bool(const T&)>& condicion = {},
                           std::chrono::milliseconds limite = 5s) {
    const auto hasta = std::chrono::steady_clock::now() + limite;
    for (;;) {
      for (auto i = recibidas_.begin(); i != recibidas_.end(); ++i) {
        if (const T* cual = std::get_if<T>(&*i); cual && (!condicion || condicion(*cual))) {
          T encontrado = *cual;
          recibidas_.erase(i);
          return encontrado;
        }
      }
      if (std::chrono::steady_clock::now() >= hasta) {
        return std::nullopt;
      }
      for (auto& novedad : sesion_.esperar_novedades(50ms)) {
        recibidas_.push_back(std::move(novedad));
      }
    }
  }

  template <typename T>
  bool no_llega(std::chrono::milliseconds tiempo = 300ms) {
    return !esperar<T>({}, tiempo);
  }

  ventrik::protocolo::Presentado entrar() {
    const auto intento = esperar<ventrik::cliente::IntentoDeConexion>();
    REQUIRE(intento);
    REQUIRE(intento->estado == ventrik::cliente::EstadoConexion::conectado);
    REQUIRE(intento->presentado);
    return *intento->presentado;
  }

  ventrik::protocolo::Presentado entrar_con_voz() {
    const ventrik::protocolo::Presentado presentado = entrar();
    ordenar(ventrik::protocolo::EntrarACanal{ventrik::servidor::kCanalGeneral});
    REQUIRE(esperar<ventrik::protocolo::UsuarioCambioCanal>(
        [&](const ventrik::protocolo::UsuarioCambioCanal& c) { return c.id == presentado.tu_id; }));
    return presentado;
  }

  void ordenar(ventrik::cliente::Orden orden) { sesion_.ordenar(std::move(orden)); }
  void voz(std::shared_ptr<ventrik::cliente::PuenteVoz> puente) { sesion_.voz(std::move(puente)); }
  void ordenar_juntas(std::vector<ventrik::cliente::Orden> ordenes) { sesion_.ordenar(std::move(ordenes)); }
  void ahorrar(bool si) { sesion_.ahorrar(si); }

 private:
  ventrik::cliente::Sesion sesion_;
  std::vector<ventrik::cliente::Novedad> recibidas_;
};

inline std::uint64_t paso_totp_de_ahora() {
  return static_cast<std::uint64_t>(
             std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()) /
         ventrik::cifrado::kSegundosPasoTotp;
}

inline std::uint32_t codigo_totp_de(const ventrik::protocolo::ConfigurarSegundoPaso& oferta, std::uint64_t paso) {
  const auto codigo = ventrik::cifrado::codigo_totp(oferta.secreto, paso);
  REQUIRE(codigo);
  return *codigo;
}

inline ventrik::protocolo::ConfigurarSegundoPaso activar_segundo_paso(Cliente& cliente) {
  const auto oferta = cliente.esperar<ventrik::protocolo::ConfigurarSegundoPaso>();
  REQUIRE(oferta);
  cliente.ordenar(ventrik::protocolo::SegundoPaso{codigo_totp_de(*oferta, paso_totp_de_ahora())});
  REQUIRE(cliente.esperar<ventrik::protocolo::AvisoDeSeguridad>([](const ventrik::protocolo::AvisoDeSeguridad& aviso) {
    return aviso.tipo == ventrik::protocolo::Seguridad::segundo_paso_configurado;
  }));
  return *oferta;
}

inline std::uint16_t hablar(ventrik::cliente::PuenteVoz& puente, std::uint8_t flujo = 0) {
  static std::uint16_t numero = 7;
  ventrik::cliente::PaqueteVoz paquete;
  paquete.flujo = flujo;
  paquete.numero = numero++;
  paquete.tiempo = static_cast<std::uint16_t>(paquete.numero * 20);
  paquete.nivel = 90;
  paquete.largo = 3;
  paquete.opus[0] = 1;
  paquete.opus[1] = 2;
  paquete.opus[2] = 3;
  REQUIRE(puente.subida.poner(paquete));
  puente.despertador.despertar();
  return paquete.numero;
}

inline std::optional<ventrik::cliente::PaqueteVoz> escuchar(ventrik::cliente::PuenteVoz& puente,
                                                            std::chrono::milliseconds limite) {
  ventrik::cliente::PaqueteVoz paquete;
  return esperar_que([&] { return puente.bajada.sacar(paquete); }, limite)
             ? std::optional<ventrik::cliente::PaqueteVoz>(paquete)
             : std::nullopt;
}

inline bool llega_alguno(ventrik::cliente::PuenteVoz& habla, ventrik::cliente::PuenteVoz& escucha) {
  for (int intento = 0; intento < 50; ++intento) {
    hablar(habla);
    if (escuchar(escucha, 100ms)) {
      return true;
    }
  }
  return false;
}

}
