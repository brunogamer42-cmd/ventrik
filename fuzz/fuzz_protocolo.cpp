#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <gsl/gsl>

#include "common/cifrado/clave.h"
#include "common/cifrado/inicio.h"
#include "common/protocol/bytes.h"
#include "common/protocol/marcos.h"
#include "common/protocol/mensajes.h"
#include "common/protocol/negociacion.h"
#include "common/protocol/pedidos.h"
#include "common/utf.h"
#include "common/version.h"

using namespace ventrik::protocolo;
using ventrik::VersionPrograma;

namespace {

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

struct Separado {
  std::vector<Marco> marcos;
  bool error = false;
};

struct Topes {
  std::size_t primero = kMaximoCuerpo;
  std::size_t despues = kMaximoCuerpo;
};

Separado separar_a_mano(gsl::span<const std::uint8_t> flujo, const Topes& topes) {
  Separado separado;
  std::size_t i = 0;
  while (flujo.size() - i >= kCabeceraMarco) {
    const std::size_t maximo = separado.marcos.empty() ? topes.primero : topes.despues;
    const auto cabecera = flujo.subspan(i, kCabeceraMarco);
    const std::uint32_t largo = std::uint32_t{cabecera[0]} | (std::uint32_t{cabecera[1]} << 8) |
                                (std::uint32_t{cabecera[2]} << 16) | (std::uint32_t{cabecera[3]} << 24);
    const auto tipo = static_cast<std::uint16_t>(cabecera[4] | (cabecera[5] << 8));
    if (largo > maximo) {
      separado.error = true;
      break;
    }
    if (flujo.size() - i - kCabeceraMarco < largo) {
      break;
    }
    const auto cuerpo = flujo.subspan(i + kCabeceraMarco, largo);
    separado.marcos.push_back(Marco{tipo, std::vector<std::uint8_t>(cuerpo.begin(), cuerpo.end())});
    i += kCabeceraMarco + largo;
  }
  return separado;
}

Separado ensamblar(gsl::span<const std::uint8_t> flujo, const Topes& topes, std::uint32_t semilla) {
  Separado separado;
  Ensamblador ensamblador(topes.primero);
  while (!flujo.empty()) {
    semilla = semilla * 1103515245u + 12345u;
    const std::size_t paso = 1 + (semilla >> 16) % 300;
    auto pedazo = flujo.first(std::min(paso, flujo.size()));
    flujo = flujo.subspan(pedazo.size());
    while (auto marco = ensamblador.siguiente(pedazo)) {
      exigir(marco->cuerpo.size() <= (separado.marcos.empty() ? topes.primero : topes.despues));
      separado.marcos.push_back(std::move(*marco));
      ensamblador.cambiar_maximo(topes.despues);
    }
    exigir(pedazo.empty());
  }
  separado.error = ensamblador.error();
  return separado;
}

Separado ensamblar_entero(gsl::span<const std::uint8_t> flujo, const Topes& topes) {
  Separado separado;
  Ensamblador ensamblador(topes.primero);
  while (auto marco = ensamblador.siguiente(flujo)) {
    separado.marcos.push_back(std::move(*marco));
    ensamblador.cambiar_maximo(topes.despues);
  }
  exigir(flujo.empty());
  separado.error = ensamblador.error();
  return separado;
}

bool mismos_marcos(const Separado& a, const Separado& b) {
  if (a.error != b.error || a.marcos.size() != b.marcos.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.marcos.size(); ++i) {
    if (a.marcos[i].tipo != b.marcos[i].tipo || a.marcos[i].cuerpo != b.marcos[i].cuerpo) {
      return false;
    }
  }
  return true;
}

template <typename Mensaje>
void exigir_ida_y_vuelta(const Mensaje& mensaje, gsl::span<const std::uint8_t> cuerpo) {
  Escritor escritor;
  escribir(escritor, mensaje);
  exigir(escritor.ok());
  exigir(escritor.datos() == std::vector<std::uint8_t>(cuerpo.begin(), cuerpo.end()));
}

template <typename... Mensajes>
void revisar_pedidos(gsl::span<const std::uint8_t> cuerpo) {
  const auto revisar = [cuerpo](auto mensaje) {
    if (auto leido = leer_mensaje<decltype(mensaje)>(cuerpo)) {
      exigir_ida_y_vuelta(*leido, cuerpo);
    }
  };
  (revisar(Mensajes{}), ...);
}

void revisar_negociacion(const Hola& hola, const ConfigServidor& servidor) {
  const auto respuesta = negociar(hola, servidor);
  const bool rango_valido = hola.protocolo_minimo > 0 && hola.protocolo_minimo <= hola.protocolo_maximo;
  const std::size_t conocidos = std::size(ventrik::kPrimeraVersionDelProtocolo);
  const VersionPrograma pedida = std::max(
      servidor.programa_minimo,
      ventrik::kPrimeraVersionDelProtocolo[std::min<std::size_t>(servidor.protocolo_minimo, conocidos) - 1]);
  const bool programa_viejo = hola.protocolo_maximo < servidor.protocolo_minimo || hola.programa < pedida;
  const bool servidor_viejo = hola.protocolo_minimo > servidor.protocolo_maximo;

  if (const auto* bienvenida = std::get_if<Bienvenida>(&respuesta)) {
    exigir(rango_valido && !programa_viejo && !servidor_viejo);
    exigir(bienvenida->protocolo == std::min(hola.protocolo_maximo, servidor.protocolo_maximo));
    exigir(bienvenida->protocolo >= std::max(hola.protocolo_minimo, servidor.protocolo_minimo));
    exigir(bienvenida->funciones == (hola.funciones & servidor.funciones));
    exigir(aceptar_bienvenida(hola, *bienvenida));
    Escritor escritor;
    escribir(escritor, *bienvenida);
    exigir(leer_bienvenida(escritor.datos()) == *bienvenida);
    return;
  }

  const Rechazo& rechazo = std::get<Rechazo>(respuesta);
  if (!rango_valido) {
    exigir(rechazo == Rechazo{MotivoRechazo::saludo_invalido, {}});
  } else if (programa_viejo) {
    exigir(rechazo == Rechazo{MotivoRechazo::programa_viejo, pedida});
    exigir(rechazo.version_minima != VersionPrograma{});
  } else {
    exigir(servidor_viejo);
    exigir(rechazo == Rechazo{MotivoRechazo::servidor_viejo, {}});
  }
  Escritor escritor;
  escribir(escritor, rechazo);
  exigir(leer_rechazo(escritor.datos()) == rechazo);
}

std::optional<ventrik::cifrado::Clave> clave_fija;

void revisar_marco(const Marco& marco, std::optional<Hola>& ultima_hola) {
  std::vector<std::uint8_t> armado;
  exigir(armar_marco(marco.tipo, marco.cuerpo, armado));
  exigir(armado.size() == kCabeceraMarco + marco.cuerpo.size());

  revisar_pedidos<Presentarse, Presentado, PresentacionRechazada, UsuarioEntro, UsuarioSalio,
                  UsuarioCambioCanal, UsuarioSilencio, CanalCreado, CanalCambiado, CanalBorrado, EntrarACanal,
                  Silenciar, SilenciarPersonas, DestinosDeSusurro,
                  EnviarChat, MensajeChat, PedirHistorial, Historial, CrearCanal, CambiarCanal,
                  MirarCanal, MensajesNuevos, BorrarCanal, Negado, Desplazado, NombrarAdmin, UsuarioRol, LiberarNombre,
                  AvisoDeSeguridad, CadenaDeClaves, Ping, Pong, ClaveVoz, MedioSubida, MedioBajada>(marco.cuerpo);
  if (marco.tipo == static_cast<std::uint16_t>(TipoMensaje::cadena_de_claves)) {
    if (const auto cadena = leer_mensaje<CadenaDeClaves>(marco.cuerpo)) {
      exigir(marco.cuerpo.size() <= kMaximoCuerpoCadena && !cadena->cadena.empty());
    }
  }
  if (marco.tipo == static_cast<std::uint16_t>(TipoMensaje::presentarse)) {
    if (const auto presentarse = leer_mensaje<Presentarse>(marco.cuerpo)) {
      const std::array<std::uint8_t, kBytesValorDeConexion> nada{};
      exigir(!ventrik::cifrado::verificar_firma(presentarse->clave,
                                                lo_que_se_firma(*presentarse, ventrik::kProtocoloMaximo, nada, nada),
                                                presentarse->firma));
      Presentarse firmado = *presentarse;
      firmado.clave = clave_fija->publica();
      const auto mensaje = lo_que_se_firma(firmado, ventrik::kProtocoloMaximo, nada, nada);
      const auto firma = clave_fija->firmar(mensaje);
      exigir(!mensaje.empty() && firma && ventrik::cifrado::verificar_firma(firmado.clave, mensaje, *firma));
      firmado.firma = *firma;
      firmado.equipo[0] ^= 1;
      exigir(!ventrik::cifrado::verificar_firma(
          firmado.clave, lo_que_se_firma(firmado, ventrik::kProtocoloMaximo, nada, nada), firmado.firma));
      std::vector<std::uint8_t> con_firma;
      exigir(armar_mensaje(firmado, con_firma));
    }
  }
  switch (static_cast<TipoMensaje>(marco.tipo)) {
    case TipoMensaje::hola:
      if (const auto hola = leer_hola(marco.cuerpo)) {
        exigir_ida_y_vuelta(*hola, marco.cuerpo);
        ConfigServidor servidor;
        revisar_negociacion(*hola, servidor);
        servidor.protocolo_minimo = 2;
        servidor.protocolo_maximo = 5;
        servidor.programa_minimo = {1, 0, 0};
        servidor.funciones = 0x00FF00FF00FF00FFu;
        revisar_negociacion(*hola, servidor);
        if (hola->protocolo_minimo > 0 && hola->protocolo_minimo <= hola->protocolo_maximo) {
          servidor.protocolo_minimo = hola->protocolo_minimo;
          servidor.protocolo_maximo = hola->protocolo_maximo;
          servidor.programa_minimo = hola->programa;
          servidor.funciones = ~hola->funciones;
          revisar_negociacion(*hola, servidor);
        }
        ultima_hola = hola;
      }
      break;
    case TipoMensaje::bienvenida:
      if (const auto bienvenida = leer_bienvenida(marco.cuerpo)) {
        exigir_ida_y_vuelta(*bienvenida, marco.cuerpo);
        if (ultima_hola) {
          const bool esperado = bienvenida->protocolo >= ultima_hola->protocolo_minimo &&
                                bienvenida->protocolo <= ultima_hola->protocolo_maximo &&
                                (bienvenida->funciones | ultima_hola->funciones) == ultima_hola->funciones;
          exigir(aceptar_bienvenida(*ultima_hola, *bienvenida) == esperado);
        }
      }
      break;
    case TipoMensaje::rechazo:
      if (const auto rechazo = leer_rechazo(marco.cuerpo)) {
        exigir_ida_y_vuelta(*rechazo, marco.cuerpo);
        exigir((rechazo->motivo == MotivoRechazo::programa_viejo) ==
               (rechazo->version_minima != VersionPrograma{}));
        if (ultima_hola) {
          const bool pide_algo_nuevo = !(rechazo->version_minima < ultima_hola->programa) &&
                                       rechazo->version_minima != ultima_hola->programa;
          exigir(aceptar_rechazo(*ultima_hola, *rechazo) ==
                 (rechazo->motivo != MotivoRechazo::programa_viejo || pide_algo_nuevo));
        }
      }
      break;
  }
}

void probar_marcos(gsl::span<const std::uint8_t> entrada) {
  const bool tope_chico = (entrada[0] & 0x80) != 0;
  const bool sube_tras_el_saludo = (entrada[0] & 0x40) != 0;
  Topes topes;
  topes.primero = tope_chico ? kMaximoCuerpoSaludo : kMaximoCuerpo;
  topes.despues = tope_chico && !sube_tras_el_saludo ? kMaximoCuerpoSaludo : kMaximoCuerpo;
  const auto flujo = entrada.subspan(1);
  const Separado esperado = separar_a_mano(flujo, topes);
  const Separado de_a_pedazos = ensamblar(flujo, topes, entrada[0]);
  exigir(mismos_marcos(de_a_pedazos, esperado));
  exigir(mismos_marcos(ensamblar_entero(flujo, topes), esperado));

  std::optional<Hola> ultima_hola;
  for (const Marco& marco : de_a_pedazos.marcos) {
    revisar_marco(marco, ultima_hola);
  }
}

void probar_lector(gsl::span<const std::uint8_t> entrada) {
  const std::size_t mitad = entrada.size() / 2;
  Lector ordenes(entrada.first(std::min<std::size_t>(mitad, 256)));
  Lector lector(entrada.subspan(mitad));
  bool fallo = false;
  while (ordenes.restante() > 0) {
    const std::size_t antes = lector.restante();
    const std::uint8_t orden = ordenes.leer_u8();
    switch (orden % 8) {
      case 0: lector.leer_u8(); break;
      case 1: lector.leer_u16(); break;
      case 2: lector.leer_u32(); break;
      case 3: lector.leer_u64(); break;
      case 4: lector.leer_booleano(); break;
      case 5: lector.leer_bytes(orden / 8); break;
      case 6: {
        const std::size_t maximo = ordenes.leer_u16();
        const std::string texto = lector.leer_texto(maximo);
        exigir(texto.size() <= maximo);
        exigir(texto.empty() || es_texto_valido(texto, maximo));
        break;
      }
      default: lector.leer_bytes(static_cast<std::size_t>(-1) - std::size_t{orden}); break;
    }
    exigir(lector.restante() <= antes);
    exigir(!fallo || !lector.ok());
    fallo = !lector.ok();
  }
}

void probar_escritor(gsl::span<const std::uint8_t> entrada) {
  const std::string texto(entrada.begin(), entrada.end());
  const std::size_t maximo = entrada.size() > 1 ? std::size_t{entrada[1]} * 257 : 0;
  Escritor escritor;
  escritor.escribir_u32(0x01020304u);
  escritor.escribir_texto(texto, maximo);
  exigir(escritor.ok() == es_texto_valido(texto, maximo));
  if (!escritor.ok()) {
    exigir(escritor.datos().empty());
    return;
  }
  Lector lector(escritor.datos());
  exigir(lector.leer_u32() == 0x01020304u);
  exigir(lector.leer_texto(maximo) == texto);
  exigir(lector.terminado());
}

std::unique_ptr<ventrik::cifrado::Inicio> cifrado_listo;

}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  cifrado_listo = std::make_unique<ventrik::cifrado::Inicio>();
  exigir(cifrado_listo->ok());
  clave_fija = ventrik::cifrado::Clave::crear_nueva();
  exigir(clave_fija.has_value());
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  if (largo == 0) {
    return 0;
  }
  const gsl::span<const std::uint8_t> entrada(datos, largo);
  probar_marcos(entrada);
  if (const auto datagrama = separar_datagrama(entrada)) {
    exigir(datagrama->cuerpo.size() + 2 == entrada.size());
    revisar_pedidos<Ping, Pong, MedioSubida, MedioBajada>(datagrama->cuerpo);
  }
  probar_lector(entrada);
  probar_escritor(entrada);
  return 0;
}
