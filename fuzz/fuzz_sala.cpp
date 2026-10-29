#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <variant>

#include <gsl/gsl>

#include "client/avisos.h"
#include "client/guardados.h"
#include "client/sala.h"
#include "common/protocol/bytes.h"
#include "common/utf.h"

using namespace ventrik::protocolo;
using ventrik::cliente::Avisos;
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

void revisar(const Avisos& avisos, std::uint64_t& version) {
  exigir(avisos.version() >= version);
  version = avisos.version();
  exigir(avisos.pendientes() <= kMaximoCanales);
  const auto lista = avisos.lista();
  exigir(lista.size() <= kMaximoCanales + 2);
  for (std::size_t i = 0; i < lista.size(); ++i) {
    exigir(i == 0 || lista[i - 1].version < lista[i].version);
    exigir(ventrik::es_utf8_valido(lista[i].titulo) && ventrik::es_utf8_valido(lista[i].cuerpo));
    exigir(lista[i].cuerpo.size() <= 512);
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  Lector lector(gsl::make_span(datos, largo));
  Presentado presentado;
  presentado.tu_id = 1;
  presentado.servidor = "Servidor";
  const std::uint8_t inicio = lector.leer_u8();
  presentado.rol = (inicio & 1) != 0 ? Rol::dueno : Rol::ninguno;
  presentado.canales = {{1, "General"}, {2, "charla", TipoCanal::texto, false, 3},
                        {3, "anuncios", TipoCanal::texto, true, 0}, {4, "Juegos"}};
  presentado.usuarios = {{1, "Ana", (inicio & 2) != 0 ? 0u : 1u}, {2, "Beto", 4, 3}};
  std::vector<ventrik::cliente::Leido> leidos;
  if (presentado.rol == Rol::dueno) {
    leidos = {{1, lector.leer_u8()}, {2, lector.leer_u16()}, {7, lector.leer_u8()}};
  }
  Sala sala(presentado, leidos, (inicio >> 2) % 6);
  ventrik::cliente::OpcionesAvisos opciones;
  opciones.privado = (inicio & 64) != 0;
  Avisos avisos(ventrik::elegir_idioma("es"), opciones);
  avisos.conectado("srv", 1, "Ana");
  avisos.fuera((inicio & 128) == 0);
  std::uint64_t version = 0;
  Avisos::Reloj::time_point reloj{};
  std::optional<PedirHistorial> del_aviso;
  std::optional<PedirHistorial> pedido;
  const auto anotar = [&pedido](const std::optional<ventrik::cliente::Orden>& orden) {
    if (orden) {
      pedido = std::get<PedirHistorial>(*orden);
    }
  };
  const auto anotar_todas = [&anotar](const std::vector<ventrik::cliente::Orden>& ordenes) {
    for (const auto& orden : ordenes) {
      if (std::holds_alternative<PedirHistorial>(orden)) {
        anotar(orden);
      }
    }
  };
  anotar_todas(sala.empezar());
  const char* const nombres[] = {"Ana", "Beto", "Carla", "Dani", "Persona con un nombre bastante largo"};
  while (lector.ok() && lector.restante() >= 5) {
    const std::uint8_t que = lector.leer_u8();
    const std::uint32_t a = lector.leer_u8();
    const std::uint32_t b = lector.leer_u8();
    const std::uint16_t c = lector.leer_u16();
    const TipoCanal tipo = (c & 1) != 0 ? TipoCanal::texto : TipoCanal::voz;
    const bool solo_admins = tipo == TipoCanal::texto && (c & 2) != 0;
    switch (que % 29) {
      case 0: sala.aplicar(UsuarioEntro{{a, "Alguien", b}}); break;
      case 1: sala.aplicar(UsuarioSalio{a}); break;
      case 2: sala.aplicar(UsuarioCambioCanal{a, b}); break;
      case 3: sala.aplicar(CanalCreado{{a, "creado", tipo, solo_admins, c}}); break;
      case 4: sala.aplicar(CanalCambiado{{a, "cambiado", tipo, solo_admins, c}}); break;
      case 5: sala.aplicar(CanalBorrado{a, b}); break;
      case 6: {
        const MensajeChat mensaje{c, a, b, static_cast<std::uint32_t>(1 + (c & 4) / 4), nombres[b % 5], c * 1000ull,
                                  ((c & 32) != 0 ? "@Ana " : (c & 64) != 0 ? "@todos " : "") +
                                      std::string(1 + c % ((c & 8) != 0 ? 3900 : 64), 'x')};
        sala.aplicar(mensaje);
        avisos.mensaje(mensaje, nombres[a % 5], (c & 16) != 0);
        break;
      }
      case 7:
        sala.aplicar(MensajesNuevos{{{a, c}}});
        avisos.nuevos(a, nombres[a % 5], sala.nuevos(a));
        break;
      case 8: {
        const bool al_aviso = del_aviso && (b & 2) != 0;
        const bool responde = !al_aviso && pedido && (b & 1) != 0;
        const Sentido sentido = al_aviso   ? Sentido::viejos
                                : responde ? pedido->sentido
                                           : (c & 2) != 0 ? Sentido::nuevos : Sentido::viejos;
        const std::uint64_t desde = al_aviso ? ventrik::cliente::kDesdeAviso : responde ? pedido->desde : c;
        Historial historial{al_aviso ? del_aviso->canal : responde ? pedido->canal : a, desde, sentido, (c & 1) != 0,
                            {}};
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
        if (desde == ventrik::cliente::kDesdeAviso && avisos.espera(historial.canal)) {
          avisos.historial(historial, sala.leido(historial.canal));
          del_aviso.reset();
        } else {
          sala.aplicar(historial);
        }
        break;
      }
      case 9: {
        const Negado negado{static_cast<TipoMensaje>(10 + c % 18), static_cast<MotivoNegado>(1 + b % 9)};
        if (negado.pedido == TipoMensaje::pedir_historial && avisos.pidiendo() && !sala.esperando()) {
          avisos.negado();
        } else {
          sala.aplicar(negado);
          if (negado.pedido == TipoMensaje::pedir_historial) {
            avisos.negado();
          }
        }
        break;
      }
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
      case 19: exigir(sala.salir_de_la_voz().empty() == (sala.tu_canal() == 0)); break;
      case 20: sala.aplicar(UsuarioSilencio{a, static_cast<std::uint8_t>(b % 3 == 2 ? 3 : b % 3)}); break;
      case 21: avisos.fuera((b & 1) != 0); break;
      case 27: {
        ConfigurarSegundoPaso oferta;
        oferta.secreto.fill(static_cast<std::uint8_t>(a));
        exigir(sala.aplicar(oferta).empty());
        ventrik::Seguridad seguridad;
        ventrik::elegir_nivel(seguridad, static_cast<ventrik::Nivel>(b % ventrik::kNiveles));
        exigir(sala.aplicar(ConfigSeguridad{seguridad, (c & 1) != 0}).empty());
        break;
      }
      case 24: {
        ventrik::cliente::AvisosDelServidor ajustes;
        ajustes.servidor = {(b & 1) != 0 ? c * 1000ull : 0, (b & 2) != 0,
                            static_cast<ventrik::cliente::Avisar>(1 + (b >> 2) % 3)};
        ajustes.contar_todos = (b & 32) != 0;
        ventrik::cliente::cambiar_canal(ajustes, a, {(b & 64) != 0 ? ventrik::cliente::kHastaQueLoActive : 0,
                                                     (b & 128) != 0, static_cast<ventrik::cliente::Avisar>(c % 4)});
        avisos.ajustes(ajustes);
        avisos.reloj(c * 500ull);
        break;
      }
      case 22: avisos.visto(a); break;
      case 23: (b & 1) != 0 ? avisos.corte(std::string(c % 300, 'z')) : avisos.desconectado(); break;
      case 25:
        sala.aplicar(UsuarioRol{a, static_cast<Rol>(b % 3)});
        exigir(a != sala.tu_id() || sala.admin() == (b % 3 != 0));
        break;
      case 28:
        sala.aplicar(EstadoSegundoPaso{static_cast<FaltaSegundoPaso>(b % 4), (c & 1) != 0});
        exigir(sala.leidos().size() <= ventrik::cliente::kMaximoLeidosPorServidor);
        break;
      case 26:
        avisos.seguridad(ventrik::cliente::explicar(
            ventrik::elegir_idioma((b & 1) != 0 ? "es" : "en"),
            AvisoDeSeguridad{static_cast<Seguridad>(1 + b % static_cast<int>(kUltimaSeguridad)), c * 1000ull,
                             nombres[a % 5], nombres[b % 5]}));
        break;
      default: break;
    }
    anotar_todas(sala.revisar());
    reloj += std::chrono::milliseconds(c % 3000);
    if (const auto orden = avisos.pedir(reloj, sala.esperando(), (que & 32) != 0)) {
      del_aviso = std::get<PedirHistorial>(*orden);
      exigir(del_aviso->desde == ventrik::cliente::kDesdeAviso && avisos.espera(del_aviso->canal));
    }
    revisar(sala);
    revisar(avisos, version);
  }
  return 0;
}
