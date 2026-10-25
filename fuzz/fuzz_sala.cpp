#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <variant>

#include <gsl/gsl>

#include "client/guardados.h"
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
    exigir(chat.mensajes[i].autor < chat.autores.size());
    bytes += chat.mensajes[i].texto.size();
  }
  for (std::size_t i = 0; i < chat.autores.size(); ++i) {
    exigir(std::find(chat.autores.begin(), chat.autores.begin() + static_cast<std::ptrdiff_t>(i), chat.autores[i]) ==
           chat.autores.begin() + static_cast<std::ptrdiff_t>(i));
  }
  exigir(bytes == chat.bytes);
  exigir(chat.mensajes.size() <= ventrik::cliente::kVentanaChat);
  exigir(chat.autores.size() <= 2 * ventrik::cliente::kVentanaChat);
  exigir(chat.bytes <= ventrik::cliente::kTopeBytesPorChat);
  exigir(!chat.pidiendo || chat.pedido.canal == chat.canal);
  exigir(sala.usuarios().size() <= kMaximoUsuariosEnLista);
  for (const auto& par : sala.usuarios()) {
    const auto& lista = sala.en_canal(par.second.canal);
    exigir(std::find(lista.begin(), lista.end(), &par.second) != lista.end());
  }
  exigir(sala.canales().size() <= kMaximoCanalesEnLista);
  for (const auto& canal : sala.canales()) {
    exigir(sala.nuevos(canal.id) <= canal.contador);
  }
  ventrik::cliente::ServidorGuardado guardado{{1, 1}, {}, "x", 0, {}, 0, sala.leidos()};
  ventrik::cliente::Guardados guardados;
  guardados.servidores = {guardado};
  exigir(!ventrik::cliente::armar_guardados(guardados).empty());
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
  std::vector<ventrik::cliente::Leido> leidos;
  if (presentado.dueno) {
    leidos = {{1, lector.leer_u8()}, {2, lector.leer_u16()}, {7, lector.leer_u8()}};
  }
  Sala sala(presentado, leidos);
  sala.empezar();
  std::optional<PedirHistorial> pedido;
  const auto anotar = [&pedido](const std::optional<ventrik::cliente::Orden>& orden) {
    if (orden) {
      pedido = std::get<PedirHistorial>(*orden);
    }
  };
  const char* const nombres[] = {"Ana", "Beto", "Carla", "Dani", "Persona con un nombre bastante largo"};
  while (lector.ok() && lector.restante() >= 5) {
    const std::uint8_t que = lector.leer_u8();
    const std::uint32_t a = lector.leer_u8();
    const std::uint32_t b = lector.leer_u8();
    const std::uint16_t c = lector.leer_u16();
    const TipoCanal tipo = (c & 1) != 0 ? TipoCanal::texto : TipoCanal::voz;
    const bool solo_admins = tipo == TipoCanal::texto && (c & 2) != 0;
    switch (que % 20) {
      case 0: sala.aplicar(UsuarioEntro{{a, "Alguien", b}}); break;
      case 1: sala.aplicar(UsuarioSalio{a}); break;
      case 2: sala.aplicar(UsuarioCambioCanal{a, b}); break;
      case 3: sala.aplicar(CanalCreado{{a, "creado", tipo, solo_admins, c}}); break;
      case 4: sala.aplicar(CanalCambiado{{a, "cambiado", tipo, solo_admins, c}}); break;
      case 5: sala.aplicar(CanalBorrado{a, b}); break;
      case 6:
        sala.aplicar(MensajeChat{c, a, b, 2, nombres[b % 5], c * 1000ull, std::string(1 + c % 64, 'x')});
        break;
      case 7: sala.aplicar(MensajesNuevos{{{a, c}}}); break;
      case 8: {
        const bool responde = pedido && (b & 1) != 0;
        const Sentido sentido = responde ? pedido->sentido : (c & 2) != 0 ? Sentido::nuevos : Sentido::viejos;
        const std::uint64_t desde = responde ? pedido->desde : c;
        Historial historial{responde ? pedido->canal : a, desde, sentido, (c & 1) != 0, {}};
        const std::size_t cuantos = (c >> 4) % ((c & 8) != 0 ? kMaximoMensajesEnHistorial + 1 : 9);
        for (std::uint32_t k = 1; k <= cuantos; ++k) {
          const std::uint64_t paso = k * (1 + b % 3);
          if (sentido == Sentido::viejos && desde != 0 && paso >= desde) {
            break;
          }
          const std::uint64_t id = sentido == Sentido::nuevos ? desde + paso : desde == 0 ? 100000 - paso : desde - paso;
          historial.mensajes.push_back(
              {id, k, nombres[(k + b) % 5], k, std::string(1 + ((k * c) % 7 == 0 ? (k * c) % 4000 : k % 16), 'h')});
        }
        sala.aplicar(historial);
        break;
      }
      case 9: sala.aplicar(Negado{static_cast<TipoMensaje>(10 + c % 9), static_cast<MotivoNegado>(1 + b % 7)}); break;
      case 10: sala.aplicar(ventrik::cliente::PingMedido{c}); break;
      case 11: sala.elegir_voz(a); break;
      case 12: sala.elegir_texto(a); break;
      case 13: anotar(sala.enviar(std::string(c % 10, 'y')) ? sala.ir_a_lo_ultimo() : std::nullopt); break;
      case 14: anotar(sala.pedir_mas_viejos()); break;
      case 15: anotar(sala.pedir_mas_nuevos()); break;
      case 16: anotar(sala.ir_a_lo_ultimo()); break;
      case 17: {
        const auto& mensajes = sala.chat().mensajes;
        sala.a_la_vista(mensajes.empty() || (b & 1) != 0 ? c : mensajes[c % mensajes.size()].id);
        break;
      }
      case 18: sala.soltar_lejanos(); break;
      default: break;
    }
    revisar(sala);
  }
  return 0;
}
