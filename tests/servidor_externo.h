#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "common/archivos.h"
#include "common/cifrado/codigo_dueno.h"
#include "common/cifrado/identidad.h"
#include "common/red/red.h"
#include "common/textos.h"
#include "proceso_de_prueba.h"
#include <doctest/doctest.h>

namespace prueba {

inline std::uint16_t puerto_libre() {
  for (int intento = 0;; ++intento) {
    REQUIRE(intento < 20);
    const auto escucha = ventrik::red::escuchar({kLocal, 0});
    const auto donde = ventrik::red::direccion_local(escucha);
    REQUIRE(donde);
    if (ventrik::red::abrir_udp(*donde).valido()) {
      return donde->puerto;
    }
  }
}

class ServidorExterno {
 public:
  ServidorExterno(const std::wstring& exe, const std::wstring& carpeta)
      : carpeta_(carpeta),
        puerto_(puerto_libre()),
        proceso_(exe, {L"--carpeta", carpeta, L"--puerto", std::to_wstring(puerto_), L"--local"},
                 carpeta + L"\\salida.txt") {
    const std::string listo = std::string(ventrik::texto(ventrik::idioma_de_windows(), ventrik::Clave::servidor_puerto)) +
                              " " + std::to_string(puerto_) + "\r\n";
    REQUIRE(esperar_que([&] { return salida().find(listo) != std::string::npos || !proceso_.vivo(); }, 20s));
    REQUIRE(proceso_.vivo());
    auto cargada = ventrik::cifrado::cargar_o_crear_identidad(carpeta + L"\\clave_servidor.der");
    REQUIRE(cargada.resultado == ventrik::cifrado::ResultadoIdentidad::cargada);
    huella_ = cargada.identidad->huella();
    ventrik::cifrado::Secreto leido;
    REQUIRE(ventrik::leer_archivo(carpeta + L"\\codigo_dueno.txt", 256, leido) == ventrik::LecturaArchivo::leido);
    const std::string_view texto(reinterpret_cast<const char*>(leido.bytes().data()), leido.size());
    codigo_ = ventrik::cifrado::leer_codigo_dueno(texto.substr(0, texto.find_last_not_of(" \r\n") + 1));
    REQUIRE(codigo_);
  }

  ventrik::red::Direccion direccion() const { return {kLocal, puerto_}; }
  const ventrik::cifrado::Huella& huella() const { return *huella_; }
  const ventrik::cifrado::CodigoDueno& codigo() const { return *codigo_; }
  std::string salida() const { return leer_todo(carpeta_ + L"\\salida.txt"); }
  const Proceso& proceso() const { return proceso_; }

  std::optional<DWORD> cerrar() const {
    return proceso_.cortar() ? proceso_.esperar_salida(10s) : std::nullopt;
  }

 private:
  std::wstring carpeta_;
  std::uint16_t puerto_;
  Proceso proceso_;
  std::optional<ventrik::cifrado::Huella> huella_;
  std::optional<ventrik::cifrado::CodigoDueno> codigo_;
};

}
