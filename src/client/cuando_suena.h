#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "client/sala.h"
#include "client/sesion.h"
#include "client/silencios.h"
#include "client/sonidos.h"

namespace ventrik::cliente {
std::optional<Sonido> sonido_de_gente(const Sala& sala, const Novedad& novedad);
std::optional<Sonido> sonido_de_mensaje(const protocolo::MensajeChat& mensaje, std::uint32_t tu_id,
                                        std::string_view tu_nombre, bool a_la_vista, const AvisosDelServidor& avisos,
                                        std::uint64_t ahora);
std::optional<Sonido> sonido_de_nuevos(const Sala& sala, const protocolo::MensajesNuevos& nuevos,
                                       const AvisosDelServidor& avisos, std::uint64_t ahora);
std::optional<Sonido> sonido_de_silencio(std::uint8_t antes, std::uint8_t ahora);
std::optional<Sonido> sonido_de_negado(const protocolo::Negado& negado);
bool es_sonido_de_voz(Sonido sonido);
bool suena_con(std::uint8_t no_molestar, bool jugando);

}
