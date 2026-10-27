#include "client/cuando_suena.h"

#include <algorithm>
#include <variant>

namespace ventrik::cliente {
namespace {
std::uint32_t canal_de(const Sala& sala, std::uint32_t id) {
  const auto usuario = sala.usuarios().find(id);
  return usuario == sala.usuarios().end() ? 0 : usuario->second.canal;
}

}

std::optional<Sonido> sonido_de_gente(const Sala& sala, const Novedad& novedad) {
  const std::uint32_t mio = sala.tu_canal();
  if (const auto* cambio = std::get_if<protocolo::UsuarioCambioCanal>(&novedad);
      cambio != nullptr && mio != 0 && cambio->id != sala.tu_id()) {
    const std::uint32_t antes = canal_de(sala, cambio->id);
    return cambio->canal == mio && antes != mio   ? std::optional<Sonido>(Sonido::alguien_entra)
           : antes == mio && cambio->canal != mio ? std::optional<Sonido>(Sonido::alguien_sale)
                                                  : std::nullopt;
  }
  if (const auto* entro = std::get_if<protocolo::UsuarioEntro>(&novedad);
      entro != nullptr && mio != 0 && entro->usuario.canal == mio) {
    return Sonido::alguien_entra;
  }
  if (const auto* salio = std::get_if<protocolo::UsuarioSalio>(&novedad);
      salio != nullptr && mio != 0 && salio->id != sala.tu_id() && canal_de(sala, salio->id) == mio) {
    return Sonido::alguien_sale;
  }
  return std::nullopt;
}

std::optional<Sonido> sonido_de_mensaje(const protocolo::MensajeChat& mensaje, std::uint32_t tu_id,
                                        std::string_view tu_nombre, bool a_la_vista, const AvisosDelServidor& avisos,
                                        std::uint64_t ahora) {
  const Mencion mencion = mencion_en(mensaje.texto, tu_nombre);
  if (mensaje.autor_id == tu_id || !avisa(avisos, mensaje.canal, mencion, ahora)) {
    return std::nullopt;
  }
  return mencion != Mencion::ninguna ? std::optional<Sonido>(Sonido::mencion)
         : a_la_vista                ? std::nullopt
                                     : std::optional<Sonido>(Sonido::mensaje);
}

std::optional<Sonido> sonido_de_nuevos(const Sala& sala, const protocolo::MensajesNuevos& nuevos,
                                       const AvisosDelServidor& avisos, std::uint64_t ahora) {
  const bool alguno = std::any_of(nuevos.canales.begin(), nuevos.canales.end(), [&](const protocolo::Contador& contador) {
    const auto* canal = sala.canal(contador.canal);
    return canal != nullptr && canal->tipo == protocolo::TipoCanal::texto && canal->id != sala.mirando() &&
           contador.contador > canal->contador && avisa(avisos, canal->id, Mencion::ninguna, ahora);
  });
  return alguno ? std::optional<Sonido>(Sonido::mensaje) : std::nullopt;
}

std::optional<Sonido> sonido_de_silencio(std::uint8_t antes, std::uint8_t ahora) {
  const auto cambio = [antes, ahora](std::uint8_t bit) { return (antes & bit) != (ahora & bit); };
  if (cambio(protocolo::kSilencioAudio)) {
    return (ahora & protocolo::kSilencioAudio) != 0 ? Sonido::audio_silenciado : Sonido::audio_activado;
  }
  if (cambio(protocolo::kSilencioMicrofono)) {
    return (ahora & protocolo::kSilencioMicrofono) != 0 ? Sonido::microfono_silenciado : Sonido::microfono_activado;
  }
  return std::nullopt;
}

std::optional<Sonido> sonido_de_negado(const protocolo::Negado& negado) {
  switch (negado.pedido) {
    case protocolo::TipoMensaje::entrar_a_canal:
    case protocolo::TipoMensaje::enviar_chat:
    case protocolo::TipoMensaje::crear_canal:
    case protocolo::TipoMensaje::cambiar_canal:
    case protocolo::TipoMensaje::borrar_canal:
      return Sonido::error;
    default:
      return std::nullopt;
  }
}

bool es_sonido_de_voz(Sonido sonido) { return sonido <= Sonido::transmitir_termina; }

bool suena_con(std::uint8_t no_molestar, bool jugando) {
  return no_molestar != kNoMolestarSiempre && !(no_molestar == kNoMolestarAlJugar && jugando);
}

}
