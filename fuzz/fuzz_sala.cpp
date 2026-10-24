#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include <gsl/gsl>

#include "client/sala.h"
#include "common/protocol/bytes.h"

using namespace ventrik::protocolo;
using ventrik::cliente::Sala;

namespace {
void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

void revisar(const Sala& sala) {
  const auto& chat = sala.chat();
  std::size_t bytes = 0;
  for (std::size_t i = 0; i < chat.mensajes.size(); ++i) {
    exigir(i == 0 || chat.mensajes[i - 1].id < chat.mensajes[i].id);
    bytes += chat.mensajes[i].autor.size() + chat.mensajes[i].texto.size();
  }
  exigir(bytes == chat.bytes);
  exigir(chat.mensajes.size() <= ventrik::cliente::kTopeMensajesPorChat);
  exigir(chat.bytes <= ventrik::cliente::kTopeBytesPorChat);
  exigir(sala.usuarios().size() <= kMaximoUsuariosEnLista);
  exigir(sala.canales().size() <= kMaximoCanalesEnLista);
  for (const auto& canal : sala.canales()) {
    exigir(sala.nuevos(canal.id) <= canal.contador);
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  Lector lector(gsl::make_span(datos, largo));
  Presentado presentado;
  presentado.tu_id = 1;
  presentado.dueno = (lector.leer_u8() & 1) != 0;
  presentado.canales = {{1, "General"}, {2, "charla", TipoCanal::texto, false, 3},
                        {3, "anuncios", TipoCanal::texto, true, 0}, {4, "Juegos"}};
  presentado.usuarios = {{1, "Ana", 1}, {2, "Beto", 4}};
  Sala sala(presentado);
  sala.empezar();
  while (lector.ok() && lector.restante() >= 5) {
    const std::uint8_t que = lector.leer_u8();
    const std::uint32_t a = lector.leer_u8();
    const std::uint32_t b = lector.leer_u8();
    const std::uint16_t c = lector.leer_u16();
    const TipoCanal tipo = (c & 1) != 0 ? TipoCanal::texto : TipoCanal::voz;
    const bool solo_admins = tipo == TipoCanal::texto && (c & 2) != 0;
    switch (que % 16) {
      case 0: sala.aplicar(UsuarioEntro{{a, "Alguien", b}}); break;
      case 1: sala.aplicar(UsuarioSalio{a}); break;
      case 2: sala.aplicar(UsuarioCambioCanal{a, b}); break;
      case 3: sala.aplicar(CanalCreado{{a, "creado", tipo, solo_admins, c}}); break;
      case 4: sala.aplicar(CanalCambiado{{a, "cambiado", tipo, solo_admins, c}}); break;
      case 5: sala.aplicar(CanalBorrado{a, b}); break;
      case 6: sala.aplicar(MensajeChat{c, a, b, 2, "Beto", c * 1000ull, std::string(1 + c % 64, 'x')}); break;
      case 7: sala.aplicar(MensajesNuevos{{{a, c}}}); break;
      case 8: {
        Historial historial{a, (c & 1) != 0, {}};
        for (std::uint32_t k = 0; k < c % 8u; ++k) {
          historial.mensajes.push_back({c * 8ull - k * (b + 1), k, "Ana", k, std::string(1 + k, 'h')});
        }
        sala.aplicar(historial);
        break;
      }
      case 9: sala.aplicar(Negado{static_cast<TipoMensaje>(10 + c % 9), static_cast<MotivoNegado>(1 + b % 7)}); break;
      case 10: sala.aplicar(ventrik::cliente::PingMedido{c}); break;
      case 11: sala.elegir_voz(a); break;
      case 12: sala.elegir_texto(a); break;
      case 13: sala.enviar(std::string(c % 10, 'y')); break;
      case 14: sala.pedir_mas_viejos(); break;
      default: break;
    }
    revisar(sala);
  }
  return 0;
}
