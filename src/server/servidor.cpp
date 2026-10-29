#include "server/servidor.h"

#include <algorithm>
#include <array>
#include <future>
#include <optional>
#include <set>
#include <utility>
#include <variant>

#include <winsock2.h>

#include "common/cifrado/clave.h"
#include "common/cifrado/totp.h"
#include "common/texto.h"

namespace ventrik::servidor {
namespace {
using Reloj = std::chrono::steady_clock;
using protocolo::MotivoNegado;
using protocolo::TipoCanal;
using protocolo::TipoMensaje;

constexpr std::chrono::milliseconds kEsperaMaxima{500};
constexpr std::chrono::milliseconds kPausaSiFallaAceptar{100};
constexpr std::size_t kAceptadasPorVuelta = 64;
constexpr std::size_t kMaximoAntesDelSaludo = 16 * 1024;
constexpr int kLecturasPorVuelta = 16;
constexpr std::size_t kDatagramasPorVuelta = 256;
constexpr std::chrono::milliseconds kCadaCuantoRevisarVoz{100};
constexpr std::size_t kMaximoHistorial = protocolo::kMaximoCuerpo - 64;
constexpr std::chrono::seconds kEntreAvisos{1};
constexpr std::chrono::hours kVentanaDeIdentidadesNuevas{1};
constexpr std::size_t kMaximoIpsAnotadas = 4096;
static_assert(protocolo::kBytesSecretoSegundoPaso == cifrado::kBytesSecretoTotp &&
                  protocolo::kTopeCodigoSegundoPaso == cifrado::kTopeCodigoTotp,
              "El segundo paso del protocolo y el de TOTP tienen que coincidir");

cifrado::Secreto sacar_contrasena(std::string& texto) {
  cifrado::Secreto secreto(texto.size());
  std::transform(texto.begin(), texto.end(), secreto.bytes().begin(),
                 [](char letra) { return static_cast<std::uint8_t>(letra); });
  mbedtls_platform_zeroize(&texto[0], texto.size());
  texto.clear();
  return secreto;
}
constexpr std::chrono::hours kCadaCuantoVencer{1};

enum class Estado { cifrando, saludando, presentando, comprobando, conectada, cerrando };

Derivar derivar_de(cifrado::SesionTls& tls) {
  return [&tls](gsl::span<const std::uint8_t> identidad, gsl::span<std::uint8_t> clave) {
    return tls.exportar(identidad, clave);
  };
}

std::uint64_t ahora_ms() {
  const auto desde_1970 = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(desde_1970).count());
}

protocolo::CanalInfo info_de(const Canal& canal) {
  return {canal.id, canal.nombre, canal.tipo, canal.solo_admins, canal.contador};
}

std::string solo_letras(std::string_view texto) {
  std::string letras;
  for (const char letra : texto) {
    if (letra != '-' && letra != ' ') {
      letras.push_back(letra >= 'a' && letra <= 'z' ? static_cast<char>(letra - 'a' + 'A') : letra);
    }
  }
  return letras;
}

bool aplicar_en_la_base(Base& base, const Seguridad& seguridad) {
  const bool sin_historial = seguridad.opciones[Opcion::sin_historial] != 0;
  const Retencion antes = base.retencion();
  const Retencion retencion = sin_historial                     ? Retencion::no_guardar
                              : antes == Retencion::no_guardar ? Retencion::siempre
                                                               : antes;
  const bool retenido = retencion == antes || base.cambiar_retencion(retencion);
  if (retenido && retencion != antes && sin_historial) {
    base.borrar_viejos(ahora_ms());
  }
  return retenido && base.cifrar_historial(seguridad.opciones[Opcion::historial_cifrado] != 0);
}

Seguridad como_quedo(Base& base, Seguridad pedida) {
  pedida.opciones[Opcion::historial_cifrado] = base.historial_cifrado() ? 1 : 0;
  pedida.opciones[Opcion::sin_historial] = base.retencion() == Retencion::no_guardar ? 1 : 0;
  if (!es_valida(pedida)) {
    pedida.nivel = nivel_de(pedida.opciones);
  }
  return pedida;
}

bool pide_con(const Seguridad& seguridad, const UsuarioGuardado& usuario, bool reclama) {
  const auto pedido = static_cast<SegundoPaso>(seguridad.opciones[Opcion::segundo_paso]);
  return pedido == SegundoPaso::todos || (pedido == SegundoPaso::admins && (usuario.dueno || usuario.admin || reclama));
}

bool es_identidad_corta(const ClaveDeUsuario& clave, std::string_view texto) {
  constexpr std::size_t kLetrasCortas = 16;
  const auto huella = cifrado::huella_de_publica(clave);
  const std::string buscada = solo_letras(texto);
  return huella && buscada.size() == kLetrasCortas && solo_letras(huella->a_texto()).compare(0, kLetrasCortas, buscada) == 0;
}

}

struct Servidor::Conexion {
  bool pendiente() const {
    return estado == Estado::cifrando || estado == Estado::saludando ||
           estado == Estado::presentando || estado == Estado::comprobando;
  }
  bool tiene_limite() const { return pendiente() || estado == Estado::cerrando; }
  bool es_dueno() const { return dueno && !sin_segundo_paso; }
  Persona persona() const { return Persona{es_dueno(), admin && !sin_segundo_paso, canal}; }
  protocolo::Rol rol() const {
    return es_dueno() ? protocolo::Rol::dueno : persona().admin ? protocolo::Rol::admin : protocolo::Rol::ninguno;
  }
  protocolo::UsuarioInfo info() const { return {id, nombre, canal, silencio, rol()}; }
  std::size_t por_enviar() const { return salida.size() - enviado; }

  red::Socket socket;
  red::Direccion origen;
  std::unique_ptr<cifrado::SesionTls> tls;
  Estado estado = Estado::cifrando;
  Reloj::time_point limite;
  Reloj::time_point ultimo_dato;
  protocolo::Ensamblador ensamblador{protocolo::kMaximoCuerpoSaludo};
  std::vector<std::uint8_t> salida;
  std::size_t enviado = 0;
  bool turno_global = false;
  bool quiere_escribir = false;
  bool datos_guardados = false;
  bool terminar = false;
  std::uint16_t protocolo = 0;
  std::uint64_t marca = 0;
  std::optional<protocolo::Presentarse> esperando;
  ClaveDeUsuario clave{};
  std::uint32_t id = 0;
  std::string nombre;
  std::wstring comparable;
  bool dueno = false;
  bool admin = false;
  bool sin_segundo_paso = false;
  bool segundo_paso_hecho = false;
  bool puede_configurar = false;
  std::optional<cifrado::Secreto> ofrecido;
  std::uint32_t canal = 0;
  std::uint32_t mirando = 0;
  std::uint8_t silencio = 0;
  std::vector<std::uint32_t> silenciados;
  Susurros susurros{};
  std::vector<std::uint32_t> novedades;
  Reloj::time_point ultimo_aviso{};
  Cubeta chat;
  Cubeta pedidos;
  Cubeta historial;
  Cubeta entrar_a_la_voz;
};

Servidor::Servidor(cifrado::IdentidadServidor& identidad, Opciones opciones, Avisar avisar)
    : identidad_(identidad),
      opciones_(std::move(opciones)),
      avisar_(std::move(avisar)),
      proteccion_(opciones_.proteccion),
      comprobador_(despertador_, opciones_.comprobaciones_en_cola) {
  programa_minimo_fijo_ = opciones_.protocolo.programa_minimo;
  opciones_.conexiones_maximas =
      std::min(opciones_.conexiones_maximas, static_cast<std::size_t>(FD_SETSIZE - 3));
}

Servidor::~Servidor() = default;

bool Servidor::escuchar() {
  for (std::size_t cual = 0; cual < configs_.size(); ++cual) {
    configs_[cual] = cifrado::ConfigTls::para_servidor(identidad_, static_cast<Algoritmo>(cual));
  }
  if (std::any_of(configs_.begin(), configs_.end(), [](const auto& config) { return !config; }) || !despertador_.ok() ||
      (!opciones_.cadena.empty() &&
       !protocolo::armar_mensaje(protocolo::CadenaDeClaves{opciones_.cadena}, marco_cadena_)) ||
      !base_.arrancar(opciones_.archivo_base, opciones_.nombre_general, despertador_, std::move(clave_historial_))) {
    return false;
  }
  struct Cargado {
    std::vector<protocolo::CanalInfo> canales;
    Retencion retencion = Retencion::siempre;
    std::optional<std::vector<UsuarioGuardado>> usuarios;
    std::string nombre;
    std::string contrasena;
    bool cifra = false;
    Seguridad seguridad;
    std::optional<std::vector<SegundoPasoGuardado>> segundos;
    std::string quitado;
    std::string permitido;
    bool bien = false;
  };
  std::promise<Cargado> cargado;
  auto futuro = cargado.get_future();
  if (!base_.pedir([&cargado, this](Base& base) -> std::function<void()> {
        std::optional<Seguridad> guardada;
        bool bien = (!opciones_.quitar_dueno || base.quitar_dueno()) &&
                    (opciones_.liberar_nombre.empty() || base.liberar_nombre(opciones_.liberar_nombre)) &&
                    (!es_nombre_valido(opciones_.nombre_servidor) ||
                     base.guardar_texto("nombre", opciones_.nombre_servidor)) &&
                    (!opciones_.poner_contrasena || base.guardar_texto("contrasena", *opciones_.poner_contrasena)) &&
                    base.leer_seguridad(guardada);
        Seguridad seguridad = opciones_.seguridad ? *opciones_.seguridad : guardada.value_or(Seguridad{});
        if (!guardada && !opciones_.seguridad) {
          cambiar_opcion(seguridad, Opcion::sin_historial, base.retencion() == Retencion::no_guardar);
          cambiar_opcion(seguridad, Opcion::historial_cifrado, base.historial_cifrado());
        }
        if (opciones_.segundo_paso) {
          cambiar_opcion(seguridad, Opcion::segundo_paso, static_cast<std::uint8_t>(*opciones_.segundo_paso));
        }
        if (opciones_.cifrar_historial) {
          cambiar_opcion(seguridad, Opcion::historial_cifrado, *opciones_.cifrar_historial);
        }
        bien = bien && aplicar_en_la_base(base, seguridad) && base.guardar_seguridad(seguridad) &&
               (!guardada || guardada->opciones == seguridad.opciones || opciones_.seguridad ||
                base.anotar({protocolo::Seguridad::config_cambiada, ahora_ms(), {}, {}}));
        auto usuarios = base.usuarios(ahora_ms());
        std::string quitado;
        std::string permitido;
        for (const auto& usuario : usuarios.value_or(std::vector<UsuarioGuardado>())) {
          if (es_identidad_corta(usuario.clave, opciones_.quitar_segundo_paso) && base.quitar_segundo_paso(usuario.clave) &&
              base.anotar({protocolo::Seguridad::segundo_paso_quitado, ahora_ms(), {}, usuario.nombre})) {
            quitado = usuario.nombre;
          }
          if (es_identidad_corta(usuario.clave, opciones_.permitir_segundo_paso) && base.confiar_equipos(usuario.clave)) {
            permitido = usuario.nombre;
          }
        }
        cargado.set_value({base.canales(), base.retencion(), std::move(usuarios), base.texto_guardado("nombre"),
                           base.texto_guardado("contrasena"), base.historial_cifrado(), seguridad,
                           base.segundos_pasos(), quitado, permitido, bien});
        return {};
      }, true)) {
    return false;
  }
  auto [canales, retencion, usuarios, nombre, contrasena, cifra, seguridad, segundos, quitado, permitido, bien] =
      futuro.get();
  contrasena_ = contrasena.empty() ? std::nullopt : leer_contrasena(contrasena);
  if (!bien || !usuarios || !segundos || (!contrasena.empty() && !contrasena_)) {
    return false;
  }
  seguridad_ = seguridad;
  segundo_paso_quitado_ = quitado;
  segundo_paso_permitido_ = permitido;
  if (contrasena_) {
    comprobador_.arrancar();
  }
  retencion_ = retencion;
  historial_cifrado_ = cifra;
  nombre_ = es_nombre_valido(nombre) ? nombre : opciones_.nombre_de_fabrica;
  for (auto& usuario : *usuarios) {
    std::wstring comparable = nombre_para_comparar(usuario.nombre);
    registrados_.push_back(Registrado{std::move(usuario), std::nullopt, std::move(comparable)});
  }
  for (auto& guardado : *segundos) {
    const auto yo = registrado_de(guardado.clave);
    if (yo != registrados_.end()) {
      yo->segundo_paso = std::move(guardado);
    }
  }
  for (auto& canal : canales) {
    canales_.push_back(Canal{canal.id, canal.nombre, nombre_para_comparar(canal.nombre),
                             canal.tipo, canal.solo_admins, canal.contador});
  }
  if (puede_entrar(canales_, kCanalGeneral)) {
    return false;
  }
  for (int intento = 0; intento < 8 && !udp_.valido(); ++intento) {
    escucha_ = red::escuchar(opciones_.escuchar);
    const auto local = escucha_.valido() ? red::direccion_local(escucha_) : std::nullopt;
    if (!local) {
      break;
    }
    puerto_ = local->puerto;
    udp_ = red::abrir_udp({opciones_.escuchar.ipv4, puerto_});
    if (opciones_.escuchar.puerto != 0) {
      break;
    }
  }
  voz_ = std::make_unique<Voz>(opciones_.voz, [this](const red::Direccion& destino,
                                                     gsl::span<const std::uint8_t> datos) {
    red::enviar_a(udp_, destino, datos);
  });
  if (!udp_.valido() || !voz_->ok()) {
    escucha_ = red::Socket();
    return false;
  }
  aplicar_seguridad(seguridad_);
  return true;
}

void Servidor::avisar(Evento evento, const red::Direccion& origen) {
  if (avisar_) {
    avisar_(evento, origen, std::string());
  }
}

void Servidor::avisar(Evento evento, const Conexion& conexion) {
  if (avisar_) {
    avisar_(evento, conexion.origen, conexion.nombre);
  }
}

void Servidor::falla(const red::Direccion& origen) {
  if (proteccion_.fallo(origen.ipv4, Reloj::now())) {
    avisar(Evento::bloqueada, origen);
    anotar({protocolo::Seguridad::ip_bloqueada, ahora_ms(), {}, red::a_texto(origen)});
  }
}

void Servidor::correr(const std::atomic<bool>& seguir) {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
  while (seguir.load() && escucha_.valido()) {
    fd_set lectura;
    fd_set escritura;
    FD_ZERO(&lectura);
    FD_ZERO(&escritura);
    FD_SET(static_cast<SOCKET>(despertador_.socket().get()), &lectura);
    FD_SET(static_cast<SOCKET>(udp_.get()), &lectura);
    auto espera = kEsperaMaxima;
    const auto ahora = Reloj::now();
    if (ahora >= aceptar_desde_) {
      FD_SET(static_cast<SOCKET>(escucha_.get()), &lectura);
    } else {
      espera = std::min(espera, std::chrono::duration_cast<std::chrono::milliseconds>(
                                    aceptar_desde_ - ahora));
    }
    for (const auto& conexion : conexiones_) {
      const auto s = static_cast<SOCKET>(conexion->socket.get());
      if (conexion->quiere_escribir || conexion->por_enviar() > 0) {
        FD_SET(s, &escritura);
      }
      if (!conexion->quiere_escribir) {
        FD_SET(s, &lectura);
      }
      if (conexion->datos_guardados) {
        espera = std::chrono::milliseconds{0};
      }
      if (!conexion->novedades.empty()) {
        espera = std::min(espera, std::max(std::chrono::milliseconds{0},
                                           std::chrono::ceil<std::chrono::milliseconds>(
                                               conexion->ultimo_aviso + kEntreAvisos - ahora)));
      }
      const auto limite = conexion->tiene_limite() ? conexion->limite
                          : conexion->estado == Estado::conectada
                              ? conexion->ultimo_dato + opciones_.inactividad
                              : Reloj::time_point::max();
      if (limite != Reloj::time_point::max()) {
        const auto falta = std::chrono::duration_cast<std::chrono::milliseconds>(limite - ahora);
        espera = std::max(std::chrono::milliseconds{0}, std::min(espera, falta));
      }
    }
    timeval tiempo = {};
    tiempo.tv_sec = static_cast<long>(espera.count() / 1000);
    tiempo.tv_usec = static_cast<long>((espera.count() % 1000) * 1000);
    if (select(0, &lectura, &escritura, nullptr, &tiempo) < 0) {
      FD_ZERO(&lectura);
      FD_ZERO(&escritura);
      Sleep(50);
    }

    if (FD_ISSET(static_cast<SOCKET>(despertador_.socket().get()), &lectura)) {
      despertador_.vaciar();
    }
    if (FD_ISSET(static_cast<SOCKET>(udp_.get()), &lectura)) {
      atender_udp();
    }
    renovar_claves();
    vencer_identidades();
    terminar_comprobaciones();
    base_.atender_respuestas();
    for (std::size_t i = 0; i < conexiones_.size(); ++i) {
      auto& conexion = *conexiones_[i];
      const auto s = static_cast<SOCKET>(conexion.socket.get());
      if (!conexion.terminar && (FD_ISSET(s, &lectura) || FD_ISSET(s, &escritura) ||
                                 conexion.datos_guardados)) {
        avanzar(conexion);
      }
    }
    if (FD_ISSET(static_cast<SOCKET>(escucha_.get()), &lectura)) {
      aceptar_nuevas();
    }
    avisar_novedades();

    const auto despues = Reloj::now();
    for (auto& conexion : conexiones_) {
      if (conexion->terminar) {
        continue;
      }
      if (conexion->tiene_limite() && despues >= conexion->limite) {
        if (conexion->pendiente()) {
          avisar(Evento::tiempo_vencido, conexion->origen);
          if (conexion->estado != Estado::comprobando) {
            falla(conexion->origen);
          }
        }
        conexion->terminar = true;
      } else if (conexion->estado == Estado::conectada &&
                 despues - conexion->ultimo_dato > opciones_.inactividad) {
        avisar(Evento::sin_respuesta, *conexion);
        conexion->terminar = true;
      }
    }
    quitar_terminadas();
  }
  for (const auto& conexion : conexiones_) {
    if (conexion->estado == Estado::conectada) {
      anotar_salida(conexion->clave);
    }
  }
  conexiones_.clear();
}

void Servidor::quitar_terminadas() {
  std::vector<std::uint32_t> se_fueron;
  for (const auto& conexion : conexiones_) {
    if (conexion->terminar && conexion->estado == Estado::conectada) {
      se_fueron.push_back(conexion->id);
      avisar(Evento::salio, *conexion);
      anotar_salida(conexion->clave);
    }
  }
  conexiones_.erase(std::remove_if(conexiones_.begin(), conexiones_.end(),
                                   [](const auto& conexion) { return conexion->terminar; }),
                    conexiones_.end());
  for (const std::uint32_t id : se_fueron) {
    voz_->baja(id);
    difundir(protocolo::UsuarioSalio{id});
  }
}

void Servidor::aceptar_nuevas() {
  for (std::size_t vuelta = 0; vuelta < kAceptadasPorVuelta; ++vuelta) {
    bool fallo = false;
    auto aceptada = red::aceptar(escucha_, fallo);
    if (!aceptada) {
      if (fallo) {
        aceptar_desde_ = Reloj::now() + kPausaSiFallaAceptar;
      }
      return;
    }
    const red::Direccion origen = aceptada->second;
    const auto veredicto = proteccion_.nueva_conexion(origen.ipv4, Reloj::now());
    if (veredicto.decision != Decision::aceptar) {
      if (veredicto.avisar) {
        avisar(veredicto.decision == Decision::bloqueada  ? Evento::rechazo_bloqueada
               : veredicto.decision == Decision::ocupado ? Evento::ocupado
                                                          : Evento::demasiado_rapido,
               origen);
      }
      continue;
    }
    if (conexiones_.size() >= opciones_.conexiones_maximas) {
      avisar(Evento::lleno, origen);
      continue;
    }
    std::size_t pendientes = 0;
    std::size_t pendientes_de_esta_ip = 0;
    std::size_t de_esta_ip = 0;
    for (const auto& otra : conexiones_) {
      const bool misma_ip = otra->origen.ipv4 == origen.ipv4;
      pendientes += otra->pendiente() ? 1 : 0;
      pendientes_de_esta_ip += otra->pendiente() && misma_ip ? 1 : 0;
      de_esta_ip += misma_ip ? 1 : 0;
    }
    if (de_esta_ip >= opciones_.conexiones_por_ip) {
      avisar(Evento::demasiadas_por_ip, origen);
      continue;
    }
    if (pendientes >= opciones_.pendientes_en_total ||
        pendientes_de_esta_ip >= opciones_.pendientes_por_ip) {
      avisar(Evento::demasiadas_pendientes, origen);
      continue;
    }
    auto conexion = std::make_unique<Conexion>();
    conexion->socket = std::move(aceptada->first);
    conexion->origen = origen;
    conexion->tls = std::make_unique<cifrado::SesionTls>(*configs_[seguridad_.opciones[Opcion::algoritmo] % configs_.size()],
                                                         conexion->socket.get());
    conexion->limite = Reloj::now() + opciones_.tiempo_para_saludar;
    if (conexion->tls->ok()) {
      conexiones_.push_back(std::move(conexion));
    }
  }
}

void Servidor::atender_udp() {
  std::array<std::uint8_t, protocolo::kMaximoDatagrama> datagrama{};
  for (std::size_t vuelta = 0; vuelta < kDatagramasPorVuelta; ++vuelta) {
    const auto recibido = red::recibir_de(udp_, datagrama);
    if (!recibido) {
      return;
    }
    const auto atada = voz_->recibir(recibido->origen,
                                     gsl::span<const std::uint8_t>(datagrama).first(recibido->largo),
                                     Reloj::now());
    if (const Conexion* conexion = atada ? buscar_conectada(*atada) : nullptr) {
      avisar(Evento::voz_conectada, *conexion);
    }
  }
}

void Servidor::renovar_claves() {
  const auto ahora = Reloj::now();
  if (ahora < revisar_voz_en_) {
    return;
  }
  revisar_voz_en_ = ahora + kCadaCuantoRevisarVoz;
  renovar(voz_->revisar(ahora));
}

void Servidor::renovar(const std::vector<std::uint32_t>& ids) {
  const auto ahora = Reloj::now();
  for (const std::uint32_t id : ids) {
    Conexion* conexion = buscar_conectada(id);
    const auto clave = conexion != nullptr ? voz_->renovar(id, ahora, derivar_de(*conexion->tls)) : std::nullopt;
    if (clave) {
      encolar(*conexion, *clave);
    }
  }
}

void Servidor::avanzar(Conexion& conexion) {
  conexion.quiere_escribir = false;
  conexion.datos_guardados = false;
  if (conexion.estado == Estado::cifrando) {
    if (!conexion.turno_global) {
      const auto veredicto = proteccion_.empezar_saludo(conexion.origen.ipv4, Reloj::now());
      if (veredicto.decision != Decision::aceptar) {
        if (veredicto.avisar) {
          avisar(Evento::ocupado, conexion.origen);
        }
        conexion.terminar = true;
        return;
      }
      conexion.turno_global = true;
    }
    switch (conexion.tls->apreton()) {
      case cifrado::Paso::listo:
        conexion.estado = Estado::saludando;
        break;
      case cifrado::Paso::esperar_lectura:
        revisar_lo_recibido(conexion);
        return;
      case cifrado::Paso::esperar_escritura:
        conexion.quiere_escribir = true;
        revisar_lo_recibido(conexion);
        return;
      case cifrado::Paso::cerrado:
      case cifrado::Paso::error:
        avisar(Evento::rechazo_cifrado, conexion.origen);
        falla(conexion.origen);
        conexion.terminar = true;
        return;
    }
  }
  if (!enviar_pendiente(conexion)) {
    return;
  }

  std::array<std::uint8_t, 4096> recibido{};
  int lecturas = 0;
  bool corto_por_tope = false;
  while (!conexion.terminar && conexion.estado != Estado::cerrando) {
    if (lecturas++ == kLecturasPorVuelta) {
      corto_por_tope = true;
      break;
    }
    std::size_t leidos = 0;
    const auto paso = conexion.tls->leer(recibido, leidos);
    if (paso == cifrado::Paso::listo) {
      conexion.ultimo_dato = Reloj::now();
      auto datos = gsl::span<const std::uint8_t>(recibido).first(leidos);
      while (auto marco = conexion.ensamblador.siguiente(datos)) {
        atender(conexion, *marco);
        if (conexion.terminar || conexion.estado == Estado::cerrando) {
          break;
        }
      }
      if (conexion.ensamblador.error()) {
        avisar(conexion.estado == Estado::conectada ? Evento::desconectada : Evento::rechazo_saludo,
               conexion.origen);
        falla(conexion.origen);
        conexion.terminar = true;
      }
      continue;
    }
    if (paso == cifrado::Paso::esperar_escritura) {
      conexion.quiere_escribir = true;
    } else if (paso != cifrado::Paso::esperar_lectura) {
      if (conexion.estado != Estado::conectada) {
        avisar(Evento::rechazo_saludo, conexion.origen);
        falla(conexion.origen);
      }
      conexion.terminar = true;
    }
    break;
  }
  conexion.datos_guardados = corto_por_tope && !conexion.terminar &&
                             conexion.tls->hay_datos_guardados();
  enviar_pendiente(conexion);
  revisar_lo_recibido(conexion);
}

void Servidor::revisar_lo_recibido(Conexion& conexion) {
  if (!conexion.terminar && conexion.pendiente() &&
      conexion.tls->bytes_recibidos() > kMaximoAntesDelSaludo) {
    avisar(conexion.estado == Estado::cifrando ? Evento::rechazo_cifrado : Evento::rechazo_saludo,
           conexion.origen);
    falla(conexion.origen);
    conexion.terminar = true;
  }
}

void Servidor::atender(Conexion& conexion, const protocolo::Marco& marco) {
  switch (conexion.estado) {
    case Estado::saludando:
      atender_hola(conexion, marco);
      return;
    case Estado::presentando:
      atender_presentacion(conexion, marco);
      return;
    case Estado::conectada:
      atender_pedido(conexion, marco);
      return;
    case Estado::cifrando:
    case Estado::comprobando:
    case Estado::cerrando:
      return;
  }
}

void Servidor::atender_hola(Conexion& conexion, const protocolo::Marco& marco) {
  std::optional<protocolo::Hola> hola;
  if (marco.tipo == static_cast<std::uint16_t>(TipoMensaje::hola)) {
    hola = protocolo::leer_hola(marco.cuerpo);
  }
  const auto respuesta =
      hola ? protocolo::negociar(*hola, opciones_.protocolo)
           : std::variant<protocolo::Bienvenida, protocolo::Rechazo>(
                 protocolo::Rechazo{protocolo::MotivoRechazo::saludo_invalido, {}});
  if (const auto* bienvenida = std::get_if<protocolo::Bienvenida>(&respuesta)) {
    protocolo::Escritor cuerpo;
    protocolo::escribir(cuerpo, *bienvenida);
    std::vector<std::uint8_t> marco_bienvenida;
    if (!cuerpo.ok() ||
        !protocolo::armar_marco(static_cast<std::uint16_t>(TipoMensaje::bienvenida),
                                cuerpo.datos(), marco_bienvenida)) {
      conexion.terminar = true;
      return;
    }
    encolar_bytes(conexion, marco_bienvenida);
    if (!marco_cadena_.empty()) {
      encolar_bytes(conexion, marco_cadena_);
    }
    conexion.protocolo = bienvenida->protocolo;
    conexion.estado = Estado::presentando;
    conexion.ensamblador.cambiar_maximo(protocolo::kMaximoCuerpoPresentacion);
    avisar(Evento::conectada, conexion.origen);
    return;
  }
  const auto& rechazo = std::get<protocolo::Rechazo>(respuesta);
  if (rechazo.motivo == protocolo::MotivoRechazo::saludo_invalido) {
    falla(conexion.origen);
  }
  protocolo::Escritor cuerpo;
  protocolo::escribir(cuerpo, rechazo);
  std::vector<std::uint8_t> marco_rechazo;
  if (cuerpo.ok() && protocolo::armar_marco(static_cast<std::uint16_t>(TipoMensaje::rechazo),
                                            cuerpo.datos(), marco_rechazo)) {
    encolar_bytes(conexion, marco_rechazo);
  }
  cerrar_con_respuesta(conexion);
  avisar(Evento::rechazo_saludo, conexion.origen);
}

void Servidor::atender_presentacion(Conexion& conexion, const protocolo::Marco& marco) {
  auto presentacion =
      marco.tipo == static_cast<std::uint16_t>(TipoMensaje::presentarse)
          ? protocolo::leer_mensaje<protocolo::Presentarse>(marco.cuerpo)
          : std::nullopt;
  if (!presentacion) {
    avisar(Evento::rechazo_saludo, conexion.origen);
    falla(conexion.origen);
    conexion.terminar = true;
    return;
  }
  std::array<std::uint8_t, protocolo::kBytesValorDeConexion> valor{};
  auto firmado = conexion.tls->valor_para_identidad(valor)
                     ? protocolo::lo_que_se_firma(*presentacion, conexion.protocolo, identidad_.huella().bytes(),
                                                  valor)
                     : std::vector<std::uint8_t>();
  const bool firmada =
      !firmado.empty() && cifrado::verificar_firma(presentacion->clave, firmado, presentacion->firma);
  mbedtls_platform_zeroize(firmado.data(), firmado.size());
  cifrado::Secreto contrasena = sacar_contrasena(presentacion->contrasena);
  if (!firmada) {
    rechazar_presentacion(conexion, protocolo::MotivoPresentacion::firma_invalida);
    avisar(Evento::firma_invalida, conexion.origen);
    falla(conexion.origen);
    return;
  }
  continuar_presentacion(conexion, std::move(*presentacion), std::move(contrasena));
}

void Servidor::continuar_presentacion(Conexion& conexion, protocolo::Presentarse presentacion,
                                      std::optional<cifrado::Secreto> contrasena) {
  auto yo = std::find_if(registrados_.begin(), registrados_.end(), [&presentacion](const Registrado& registrado) {
    return registrado.guardado.clave == presentacion.clave;
  });
  const bool ya_es_dueno = yo != registrados_.end() && yo->guardado.dueno;
  const bool reclama = presentacion.codigo_dueno && !ya_es_dueno;
  if (reclama && (tiene_dueno() || !opciones_.codigo_dueno ||
                  !(*opciones_.codigo_dueno == cifrado::CodigoDueno(*presentacion.codigo_dueno)))) {
    rechazar_presentacion(conexion, protocolo::MotivoPresentacion::codigo_incorrecto);
    avisar(Evento::codigo_incorrecto, conexion.origen);
    falla(conexion.origen);
    return;
  }
  if (contrasena_ && contrasena && yo == registrados_.end() && !reclama) {
    comprobar_contrasena(conexion, std::move(presentacion), std::move(*contrasena));
    return;
  }
  if (yo != registrados_.end() && yo->segundo_paso && pide_segundo_paso(yo->guardado, reclama) &&
      !comprobar_segundo_paso(conexion, *yo, presentacion)) {
    return;
  }
  const std::wstring comparable = nombre_para_comparar(presentacion.nombre);
  if (comparable.empty() ||
      std::any_of(registrados_.begin(), registrados_.end(), [&](const Registrado& registrado) {
        return registrado.comparable == comparable && registrado.guardado.clave != presentacion.clave;
      })) {
    rechazar_presentacion(conexion, protocolo::MotivoPresentacion::nombre_en_uso);
    return;
  }
  bool nueva = false;
  if (!admitir_identidad(conexion, presentacion, yo, reclama, nueva)) {
    return;
  }
  UsuarioGuardado actualizado = yo->guardado;
  actualizado.nombre = presentacion.nombre;
  actualizado.dueno = actualizado.dueno || reclama;
  const std::uint64_t hoy = ahora_ms();
  const bool otro_dia = nueva || hoy >= actualizado.visto_ms + kMsPorDia;
  actualizado.visto_ms = std::max(actualizado.visto_ms, hoy);
  actualizado.creado_ms = nueva ? actualizado.visto_ms : actualizado.creado_ms;
  actualizado.entradas = otro_dia ? std::max(actualizado.entradas, actualizado.entradas + 1) : actualizado.entradas;
  conexion.id = proximo_id_++;
  if (!guardar_entrada(conexion.id, actualizado, presentacion.equipo, nueva)) {
    if (nueva) {
      registrados_.erase(yo);
    }
    rechazar_presentacion(conexion, protocolo::MotivoPresentacion::lleno);
    return;
  }
  yo->guardado = std::move(actualizado);
  yo->comparable = comparable;
  if (reclama) {
    opciones_.codigo_dueno.reset();
  }

  const auto ahora = Reloj::now();
  conexion.clave = presentacion.clave;
  conexion.nombre = presentacion.nombre;
  conexion.comparable = comparable;
  conexion.dueno = yo->guardado.dueno;
  conexion.admin = yo->guardado.admin;
  conexion.sin_segundo_paso = pide_segundo_paso(yo->guardado) && !(yo->segundo_paso && conexion.segundo_paso_hecho);
  conexion.estado = Estado::conectada;
  conexion.ultimo_dato = ahora;
  conexion.chat = Cubeta(opciones_.rafaga_chat, opciones_.chat_por_segundo, ahora);
  conexion.pedidos = Cubeta(opciones_.rafaga_pedidos, opciones_.pedidos_por_segundo, ahora);
  conexion.historial = Cubeta(opciones_.rafaga_historial, opciones_.historial_por_segundo, ahora);
  conexion.entrar_a_la_voz = Cubeta(opciones_.rafaga_entrar_a_la_voz, opciones_.entrar_a_la_voz_por_segundo, ahora);
  conexion.ensamblador.cambiar_maximo(protocolo::kMaximoCuerpo);

  protocolo::Presentado presentado;
  presentado.tu_id = conexion.id;
  presentado.rol = conexion.rol();
  presentado.servidor = nombre_;
  for (const Canal& canal : canales_) {
    presentado.canales.push_back(info_de(canal));
  }
  for (const auto& otra : conexiones_) {
    if (otra->estado == Estado::conectada && !otra->terminar) {
      presentado.usuarios.push_back(otra->info());
    }
  }
  encolar(conexion, presentado);
  difundir(protocolo::UsuarioEntro{conexion.info()}, conexion.id);
  proteccion_.saludo_completo(conexion.origen.ipv4, ahora);
  avisar(Evento::presentada, conexion);
  if (reclama) {
    avisar(Evento::dueno_reclamado, conexion);
  }
}

void Servidor::atender_pedido(Conexion& conexion, const protocolo::Marco& marco) {
  const auto tipo = static_cast<TipoMensaje>(marco.tipo);
  if (tipo == TipoMensaje::enviar_chat) {
    enviar_chat(conexion, marco);
    return;
  }
  if (!conexion.pedidos.tomar(Reloj::now()) ||
      (tipo == TipoMensaje::pedir_historial && !conexion.historial.tomar(Reloj::now()))) {
    negar(conexion, tipo, MotivoNegado::demasiado_rapido);
    return;
  }
  switch (tipo) {
    case TipoMensaje::entrar_a_canal:
      entrar_a_canal(conexion, marco);
      return;
    case TipoMensaje::silenciar:
      silenciar(conexion, marco);
      return;
    case TipoMensaje::silenciar_personas:
      silenciar_personas(conexion, marco);
      return;
    case TipoMensaje::destinos_de_susurro:
      destinos_de_susurro(conexion, marco);
      return;
    case TipoMensaje::pedir_historial:
      pedir_historial(conexion, marco);
      return;
    case TipoMensaje::crear_canal:
      crear_canal(conexion, marco);
      return;
    case TipoMensaje::cambiar_canal:
      cambiar_canal(conexion, marco);
      return;
    case TipoMensaje::mirar_canal:
      mirar_canal(conexion, marco);
      return;
    case TipoMensaje::borrar_canal:
      borrar_canal(conexion, marco);
      return;
    case TipoMensaje::nombrar_admin:
      nombrar_admin(conexion, marco);
      return;
    case TipoMensaje::liberar_nombre:
      liberar_nombre(conexion, marco);
      return;
    case TipoMensaje::segundo_paso:
      segundo_paso(conexion, marco);
      return;
    case TipoMensaje::quitar_segundo_paso:
      quitar_segundo_paso(conexion, marco);
      return;
    case TipoMensaje::pedir_config_seguridad:
      pedir_config_seguridad(conexion, marco);
      return;
    case TipoMensaje::cambiar_config_seguridad:
      cambiar_config_seguridad(conexion, marco);
      return;
    case TipoMensaje::ping:
      if (const auto ping = protocolo::leer_mensaje<protocolo::Ping>(marco.cuerpo)) {
        encolar(conexion, protocolo::Pong{ping->marca});
        return;
      }
      break;
    default:
      break;
  }
  error_de_protocolo(conexion);
}

void Servidor::entrar_a_canal(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::EntrarACanal>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (const auto motivo = pedido->canal == 0 ? std::nullopt : puede_entrar(canales_, pedido->canal)) {
    negar(conexion, TipoMensaje::entrar_a_canal, *motivo);
    return;
  }
  if (conexion.canal == pedido->canal) {
    return;
  }
  if (pedido->canal == 0) {
    voz_->baja(conexion.id);
  } else if (conexion.canal != 0) {
    voz_->mover(conexion.id, pedido->canal);
  } else if (!conexion.entrar_a_la_voz.tomar(Reloj::now())) {
    negar(conexion, TipoMensaje::entrar_a_canal, MotivoNegado::demasiado_rapido);
    return;
  } else if (const auto clave = voz_->alta(conexion.id, conexion.origen.ipv4, pedido->canal, Reloj::now(),
                                           derivar_de(*conexion.tls))) {
    voz_->silencio(conexion.id, conexion.silencio);
    voz_->silenciados(conexion.id, conexion.silenciados);
    voz_->susurros(conexion.id, conexion.susurros);
    encolar(conexion, *clave);
  } else {
    negar(conexion, TipoMensaje::entrar_a_canal, MotivoNegado::lleno);
    return;
  }
  conexion.canal = pedido->canal;
  difundir(protocolo::UsuarioCambioCanal{conexion.id, conexion.canal});
}

void Servidor::silenciar(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::Silenciar>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (conexion.silencio == pedido->silencio) {
    return;
  }
  conexion.silencio = pedido->silencio;
  voz_->silencio(conexion.id, conexion.silencio);
  difundir(protocolo::UsuarioSilencio{conexion.id, conexion.silencio});
}

void Servidor::silenciar_personas(Conexion& conexion, const protocolo::Marco& marco) {
  auto pedido = protocolo::leer_mensaje<protocolo::SilenciarPersonas>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  conexion.silenciados = std::move(pedido->usuarios);
  voz_->silenciados(conexion.id, conexion.silenciados);
}

void Servidor::destinos_de_susurro(Conexion& conexion, const protocolo::Marco& marco) {
  auto pedido = protocolo::leer_mensaje<protocolo::DestinosDeSusurro>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  const auto motivo = pedido->canal == 0 ? std::nullopt : puede_entrar(canales_, pedido->canal);
  conexion.susurros[pedido->lugar - 1U] = motivo ? protocolo::DestinosDeSusurro{} : std::move(*pedido);
  voz_->susurros(conexion.id, conexion.susurros);
  if (motivo) {
    negar(conexion, TipoMensaje::destinos_de_susurro, *motivo);
  }
}

void Servidor::enviar_chat(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::EnviarChat>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (!conexion.chat.tomar(Reloj::now()) ||
      !proteccion_.puede_chatear(conexion.origen.ipv4, Reloj::now())) {
    negar(conexion, TipoMensaje::enviar_chat, MotivoNegado::demasiado_rapido);
    return;
  }
  if (const auto motivo = puede_escribir(conexion.persona(), canales_, pedido->canal)) {
    negar(conexion, TipoMensaje::enviar_chat, *motivo);
    return;
  }
  Canal* canal = buscar_canal(canales_, pedido->canal);
  protocolo::MensajeChat mensaje;
  mensaje.canal = pedido->canal;
  mensaje.numero = ++canal->contador;
  mensaje.autor_id = conexion.id;
  mensaje.autor = conexion.nombre;
  mensaje.hora_ms = ahora_ms();
  mensaje.texto = pedido->texto;
  const bool guardar = retencion_ != Retencion::no_guardar;
  const bool pedido_aceptado = base_.pedir([this, mensaje, guardar](Base& base) mutable {
    const auto id = base.guardar_mensaje(mensaje.canal, mensaje.numero, mensaje.autor,
                                         mensaje.hora_ms, mensaje.texto, guardar);
    return std::function<void()>([this, mensaje, id]() mutable {
      if (id) {
        mensaje.id = *id;
        difundir_chat(mensaje);
      }
    });
  });
  if (!pedido_aceptado) {
    --canal->contador;
    negar(conexion, TipoMensaje::enviar_chat, MotivoNegado::lleno);
  } else if (!guardar) {
    mensaje.id = proximo_mensaje_sin_guardar_++;
    difundir_chat(mensaje);
  }
}

void Servidor::difundir_chat(const protocolo::MensajeChat& mensaje) {
  std::vector<std::uint8_t> marco;
  const Canal* canal = buscar_canal(canales_, mensaje.canal);
  if (canal == nullptr || !protocolo::armar_mensaje(mensaje, marco)) {
    return;
  }
  const bool texto = canal->tipo == TipoCanal::texto;
  for (auto& conexion : conexiones_) {
    if (conexion->estado != Estado::conectada || conexion->terminar) {
      continue;
    }
    if (texto ? conexion->mirando == mensaje.canal || conexion->id == mensaje.autor_id
              : conexion->canal == mensaje.canal) {
      encolar_bytes(*conexion, marco);
    } else if (texto && std::find(conexion->novedades.begin(), conexion->novedades.end(),
                                  mensaje.canal) == conexion->novedades.end()) {
      conexion->novedades.push_back(mensaje.canal);
    }
  }
}

void Servidor::avisar_novedades() {
  const auto ahora = Reloj::now();
  for (auto& conexion : conexiones_) {
    if (conexion->novedades.empty() || ahora - conexion->ultimo_aviso < kEntreAvisos) {
      continue;
    }
    protocolo::MensajesNuevos aviso;
    for (const std::uint32_t id : conexion->novedades) {
      if (const Canal* canal = buscar_canal(canales_, id)) {
        aviso.canales.push_back(protocolo::Contador{id, canal->contador});
      }
    }
    conexion->novedades.clear();
    conexion->ultimo_aviso = ahora;
    if (!aviso.canales.empty()) {
      encolar(*conexion, aviso);
    }
  }
}

void Servidor::mirar_canal(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::MirarCanal>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (const auto motivo = puede_mirar(canales_, pedido->canal)) {
    negar(conexion, TipoMensaje::mirar_canal, *motivo);
    return;
  }
  conexion.mirando = pedido->canal;
  conexion.novedades.erase(
      std::remove(conexion.novedades.begin(), conexion.novedades.end(), pedido->canal),
      conexion.novedades.end());
}

void Servidor::pedir_historial(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::PedirHistorial>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (const auto motivo = puede_leer(conexion.persona(), canales_, pedido->canal)) {
    negar(conexion, TipoMensaje::pedir_historial, *motivo);
    return;
  }
  const std::uint32_t quien = conexion.id;
  const bool pedido_aceptado = base_.pedir([this, quien, cual = *pedido](Base& base) {
    auto historial = base.historial(cual, kMaximoHistorial);
    return std::function<void()>([this, quien, historial = std::move(historial)]() {
      Conexion* conexion = buscar_conectada(quien);
      if (conexion != nullptr && !puede_leer(conexion->persona(), canales_, historial.canal)) {
        encolar(*conexion, historial);
      }
    });
  });
  if (!pedido_aceptado) {
    negar(conexion, TipoMensaje::pedir_historial, MotivoNegado::lleno);
  }
}

void Servidor::crear_canal(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::CrearCanal>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  auto motivo = puede_crear(conexion.persona(), canales_, pedido->nombre);
  if (!motivo && canales_.size() + canales_pedidos_ >= kMaximoCanales) {
    motivo = MotivoNegado::lleno;
  }
  if (motivo) {
    negar(conexion, TipoMensaje::crear_canal, *motivo);
    return;
  }
  const std::uint32_t quien = conexion.id;
  const bool pedido_aceptado = base_.pedir([this, quien, nuevo = *pedido](Base& base) {
    const auto id = base.crear_canal(nuevo.nombre, nuevo.tipo, nuevo.solo_admins);
    return std::function<void()>([this, quien, nuevo, id]() {
      --canales_pedidos_;
      if (!id) {
        if (Conexion* conexion = buscar_conectada(quien)) {
          negar(*conexion, TipoMensaje::crear_canal, MotivoNegado::nombre_en_uso);
        }
        return;
      }
      canales_.push_back(Canal{*id, nuevo.nombre, nombre_para_comparar(nuevo.nombre), nuevo.tipo,
                               nuevo.solo_admins});
      difundir(protocolo::CanalCreado{info_de(canales_.back())});
    });
  });
  if (pedido_aceptado) {
    ++canales_pedidos_;
  } else {
    negar(conexion, TipoMensaje::crear_canal, MotivoNegado::lleno);
  }
}

void Servidor::cambiar_canal(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::CambiarCanal>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (const auto motivo = puede_cambiar(conexion.persona(), canales_, *pedido)) {
    negar(conexion, TipoMensaje::cambiar_canal, *motivo);
    return;
  }
  const std::uint32_t quien = conexion.id;
  const bool pedido_aceptado = base_.pedir([this, quien, cambio = *pedido](Base& base) {
    const bool hecho = base.cambiar_canal(cambio.canal, cambio.nombre, cambio.solo_admins);
    return std::function<void()>([this, quien, cambio, hecho]() {
      Canal* encontrado = buscar_canal(canales_, cambio.canal);
      if (!hecho || encontrado == nullptr) {
        if (Conexion* conexion = buscar_conectada(quien)) {
          negar(*conexion, TipoMensaje::cambiar_canal, MotivoNegado::nombre_en_uso);
        }
        return;
      }
      encontrado->nombre = cambio.nombre;
      encontrado->comparable = nombre_para_comparar(cambio.nombre);
      encontrado->solo_admins = cambio.solo_admins;
      difundir(protocolo::CanalCambiado{info_de(*encontrado)});
    });
  });
  if (!pedido_aceptado) {
    negar(conexion, TipoMensaje::cambiar_canal, MotivoNegado::lleno);
  }
}

void Servidor::borrar_canal(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::BorrarCanal>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (const auto motivo = puede_borrar(conexion.persona(), canales_, pedido->canal)) {
    negar(conexion, TipoMensaje::borrar_canal, *motivo);
    return;
  }
  const std::uint32_t quien = conexion.id;
  const bool del_dueno = conexion.es_dueno();
  const protocolo::AvisoDeSeguridad aviso{protocolo::Seguridad::canal_borrado, ahora_ms(), conexion.nombre,
                                          buscar_canal(canales_, pedido->canal)->nombre};
  const bool pedido_aceptado =
      base_.pedir([this, quien, canal = pedido->canal, del_dueno, aviso](Base& base) {
        const bool hecho = base.borrar_canal(canal);
        const bool anotado = !hecho || base.anotar(aviso);
        return std::function<void()>([this, quien, canal, hecho, anotado, del_dueno, aviso]() {
          if (!anotado) {
            avisar(Evento::error_base, red::Direccion{});
          }
          if (!hecho) {
            if (Conexion* conexion = buscar_conectada(quien)) {
              negar(*conexion, TipoMensaje::borrar_canal, MotivoNegado::no_existe);
            }
            return;
          }
          canales_.erase(std::remove_if(canales_.begin(), canales_.end(),
                                        [canal](const Canal& c) { return c.id == canal; }),
                         canales_.end());
          for (auto& otra : conexiones_) {
            if (otra->canal == canal) {
              otra->canal = kCanalGeneral;
              voz_->mover(otra->id, kCanalGeneral);
            }
            bool sin_destino = false;
            for (auto& destino : otra->susurros) {
              sin_destino = std::exchange(destino.canal, destino.canal == canal ? 0 : destino.canal) == canal ||
                            sin_destino;
            }
            if (sin_destino) {
              voz_->susurros(otra->id, otra->susurros);
            }
          }
          difundir(protocolo::CanalBorrado{canal, kCanalGeneral});
          if (Conexion* conexion = buscar_conectada(quien)) {
            avisar(Evento::canal_borrado, *conexion);
          }
          if (!del_dueno && seguridad_.opciones[Opcion::avisos_de_admins] != 0) {
            avisar_al_dueno(aviso);
          }
        });
      });
  if (!pedido_aceptado) {
    negar(conexion, TipoMensaje::borrar_canal, MotivoNegado::lleno);
  }
}

void Servidor::nombrar_admin(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::NombrarAdmin>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  Conexion* otra = buscar_conectada(pedido->usuario);
  const auto registrado = std::find_if(registrados_.begin(), registrados_.end(), [otra](const Registrado& cual) {
    return otra != nullptr && cual.guardado.clave == otra->clave;
  });
  if (!conexion.es_dueno() || otra == nullptr || otra->dueno || registrado == registrados_.end()) {
    negar(conexion, TipoMensaje::nombrar_admin, MotivoNegado::sin_permiso);
    return;
  }
  if (otra->admin == pedido->admin) {
    return;
  }
  UsuarioGuardado cambiado = registrado->guardado;
  cambiado.admin = pedido->admin;
  if (!anotar({pedido->admin ? protocolo::Seguridad::admin_nombrado : protocolo::Seguridad::admin_quitado, ahora_ms(),
               conexion.nombre, otra->nombre},
              cambiado)) {
    negar(conexion, TipoMensaje::nombrar_admin, MotivoNegado::lleno);
    return;
  }
  registrado->guardado = std::move(cambiado);
  otra->admin = pedido->admin;
  revisar_segundo_paso(*otra);
  difundir(protocolo::UsuarioRol{otra->id, otra->rol()});
  avisar(pedido->admin ? Evento::admin_nombrado : Evento::admin_quitado, *otra);
}

void Servidor::liberar_nombre(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::LiberarNombre>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  const std::wstring comparable = nombre_para_comparar(pedido->nombre);
  const auto registrado = std::find_if(registrados_.begin(), registrados_.end(),
                                       [&comparable](const Registrado& cual) { return cual.comparable == comparable; });
  if (!conexion.es_dueno() || (registrado != registrados_.end() &&
                          (registrado->guardado.dueno || conectada(registrado->guardado.clave)))) {
    negar(conexion, TipoMensaje::liberar_nombre, MotivoNegado::sin_permiso);
    return;
  }
  if (registrado == registrados_.end()) {
    negar(conexion, TipoMensaje::liberar_nombre, MotivoNegado::nombre_libre);
    return;
  }
  const std::uint32_t quien = conexion.id;
  const protocolo::AvisoDeSeguridad aviso{protocolo::Seguridad::nombre_liberado, ahora_ms(), conexion.nombre,
                                          registrado->guardado.nombre};
  if (!base_.pedir([this, quien, aviso](Base& base) -> std::function<void()> {
        const bool hecho = base.liberar_nombre(aviso.detalle) && base.anotar(aviso);
        return [this, quien, aviso, hecho] {
          Conexion* conexion = buscar_conectada(quien);
          if (!hecho) {
            avisar(Evento::error_base, red::Direccion{});
          } else if (conexion != nullptr) {
            encolar(*conexion, aviso);
            avisar(Evento::nombre_liberado, *conexion);
          }
        };
      })) {
    negar(conexion, TipoMensaje::liberar_nombre, MotivoNegado::lleno);
    return;
  }
  registrados_.erase(registrado);
}

std::vector<Servidor::Registrado>::iterator Servidor::registrado_de(const ClaveDeUsuario& clave) {
  return std::find_if(registrados_.begin(), registrados_.end(),
                      [&clave](const Registrado& cual) { return cual.guardado.clave == clave; });
}

bool Servidor::pide_segundo_paso(const UsuarioGuardado& usuario, bool reclama) const {
  return pide_con(seguridad_, usuario, reclama);
}

bool Servidor::comprobar_segundo_paso(Conexion& conexion, Registrado& yo, const protocolo::Presentarse& presentacion) {
  using protocolo::MotivoPresentacion;
  SegundoPasoGuardado cambiado = *yo.segundo_paso;
  if (cambiado.fallas >= kFallasDeSegundoPaso || !presentacion.segundo_paso) {
    rechazar_presentacion(conexion, cambiado.fallas >= kFallasDeSegundoPaso ? MotivoPresentacion::segundo_paso_bloqueado
                                                                             : MotivoPresentacion::falta_segundo_paso);
    return false;
  }
  if (const std::uint16_t espera = segundos_de_espera(conexion.origen.ipv4, presentacion.clave)) {
    rechazar_presentacion(conexion, MotivoPresentacion::esperar, espera);
    return false;
  }
  const auto secreto = cifrado::abrir_sellado(clave_segundo_paso_.bytes(), presentacion.clave, cambiado.sellado);
  if (!secreto) {
    avisar(Evento::no_se_pudo_comprobar, conexion.origen);
    rechazar_presentacion(conexion, MotivoPresentacion::ocupado);
    return false;
  }
  const std::uint64_t hoy = ahora_ms();
  const auto paso = cifrado::verificar_totp(secreto->bytes(), *presentacion.segundo_paso, hoy / 1000, cambiado.paso);
  if (!paso && hoy >= cambiado.desde_ms + kMsPorDia) {
    cambiado.fallas = 0;
    cambiado.desde_ms = hoy;
  }
  cambiado.fallas += paso ? 0 : 1;
  cambiado.paso = paso.value_or(cambiado.paso);
  if (!guardar_segundo_paso(cambiado)) {
    rechazar_presentacion(conexion, MotivoPresentacion::lleno);
    return false;
  }
  yo.segundo_paso = std::move(cambiado);
  if (paso) {
    esperas_por_clave_.erase(presentacion.clave);
    conexion.segundo_paso_hecho = true;
    return true;
  }
  const bool bloqueado = yo.segundo_paso->fallas >= kFallasDeSegundoPaso;
  const std::uint16_t espera = anotar_fallo(conexion.origen.ipv4, presentacion.clave);
  if (avisar_) {
    avisar_(bloqueado ? Evento::segundo_paso_bloqueado : Evento::segundo_paso_incorrecto, conexion.origen,
            yo.guardado.nombre);
  }
  falla(conexion.origen);
  rechazar_presentacion(conexion,
                        bloqueado ? MotivoPresentacion::segundo_paso_bloqueado
                                  : MotivoPresentacion::segundo_paso_incorrecto,
                        espera);
  return false;
}

void Servidor::revisar_segundo_paso(Conexion& conexion) {
  const auto yo = registrado_de(conexion.clave);
  conexion.sin_segundo_paso = yo != registrados_.end() && pide_segundo_paso(yo->guardado) &&
                              !(yo->segundo_paso && conexion.segundo_paso_hecho);
  if (!conexion.sin_segundo_paso) {
    conexion.ofrecido.reset();
  }
  ofrecer_segundo_paso(conexion);
}

void Servidor::ofrecer_segundo_paso(Conexion& conexion) {
  const auto yo = registrado_de(conexion.clave);
  if (!conexion.sin_segundo_paso || !conexion.puede_configurar || conexion.ofrecido || yo == registrados_.end() ||
      yo->segundo_paso) {
    return;
  }
  conexion.ofrecido = cifrado::secreto_totp_nuevo();
  protocolo::ConfigurarSegundoPaso oferta;
  std::vector<std::uint8_t> marco;
  if (conexion.ofrecido) {
    std::copy(conexion.ofrecido->bytes().begin(), conexion.ofrecido->bytes().end(), oferta.secreto.begin());
    if (protocolo::armar_mensaje(oferta, marco)) {
      encolar_bytes(conexion, marco);
    }
  }
  mbedtls_platform_zeroize(oferta.secreto.data(), oferta.secreto.size());
  mbedtls_platform_zeroize(marco.data(), marco.size());
}

bool Servidor::guardar_segundo_paso(const SegundoPasoGuardado& guardado) {
  return base_.pedir([this, guardado](Base& base) -> std::function<void()> {
    if (base.guardar_segundo_paso(guardado)) {
      return {};
    }
    return [this, clave = guardado.clave] {
      avisar(Evento::error_base, red::Direccion{});
      const auto yo = registrado_de(clave);
      if (yo != registrados_.end() && yo->segundo_paso) {
        yo->segundo_paso->fallas = kFallasDeSegundoPaso;
      }
    };
  });
}

void Servidor::segundo_paso(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::SegundoPaso>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  const auto yo = registrado_de(conexion.clave);
  if (!conexion.ofrecido || yo == registrados_.end() || yo->segundo_paso) {
    negar(conexion, TipoMensaje::segundo_paso, MotivoNegado::sin_permiso);
    return;
  }
  const auto paso = cifrado::verificar_totp(conexion.ofrecido->bytes(), pedido->codigo, ahora_ms() / 1000, 0);
  if (!paso) {
    negar(conexion, TipoMensaje::segundo_paso, MotivoNegado::codigo_incorrecto);
    return;
  }
  auto sellado = cifrado::sellar(clave_segundo_paso_.bytes(), conexion.clave, conexion.ofrecido->bytes());
  const SegundoPasoGuardado nuevo{conexion.clave, sellado.value_or(std::vector<std::uint8_t>()), *paso, 0, 0};
  const protocolo::AvisoDeSeguridad aviso{protocolo::Seguridad::segundo_paso_configurado, ahora_ms(), conexion.nombre,
                                          {}};
  if (!sellado || !base_.pedir([this, nuevo, aviso](Base& base) -> std::function<void()> {
        if (base.guardar_segundo_paso(nuevo) && base.anotar(aviso)) {
          return {};
        }
        return [this] { avisar(Evento::error_base, red::Direccion{}); };
      })) {
    negar(conexion, TipoMensaje::segundo_paso, MotivoNegado::lleno);
    return;
  }
  yo->segundo_paso = nuevo;
  conexion.ofrecido.reset();
  conexion.sin_segundo_paso = false;
  conexion.segundo_paso_hecho = true;
  encolar(conexion, aviso);
  difundir(protocolo::UsuarioRol{conexion.id, conexion.rol()});
  avisar(Evento::segundo_paso_configurado, conexion);
}

void Servidor::quitar_segundo_paso(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::QuitarSegundoPaso>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  const std::wstring comparable = nombre_para_comparar(pedido->nombre);
  const auto registrado = std::find_if(registrados_.begin(), registrados_.end(),
                                       [&comparable](const Registrado& cual) { return cual.comparable == comparable; });
  if (!conexion.es_dueno()) {
    negar(conexion, TipoMensaje::quitar_segundo_paso, MotivoNegado::sin_permiso);
    return;
  }
  if (registrado == registrados_.end() || !registrado->segundo_paso) {
    negar(conexion, TipoMensaje::quitar_segundo_paso, MotivoNegado::no_existe);
    return;
  }
  const std::uint32_t quien = conexion.id;
  const protocolo::AvisoDeSeguridad aviso{protocolo::Seguridad::segundo_paso_quitado, ahora_ms(), conexion.nombre,
                                          registrado->guardado.nombre};
  if (!base_.pedir([this, quien, aviso, clave = registrado->guardado.clave](Base& base) -> std::function<void()> {
        const bool hecho = base.quitar_segundo_paso(clave) && base.anotar(aviso);
        return [this, quien, aviso, hecho] {
          Conexion* conexion = buscar_conectada(quien);
          if (!hecho) {
            avisar(Evento::error_base, red::Direccion{});
          } else if (conexion != nullptr) {
            encolar(*conexion, aviso);
          }
        };
      })) {
    negar(conexion, TipoMensaje::quitar_segundo_paso, MotivoNegado::lleno);
    return;
  }
  registrado->segundo_paso.reset();
  avisar(Evento::segundo_paso_quitado, conexion);
  for (auto& otra : conexiones_) {
    if (otra->estado == Estado::conectada && !otra->terminar && otra->clave == registrado->guardado.clave) {
      revisar_segundo_paso(*otra);
      difundir(protocolo::UsuarioRol{otra->id, otra->rol()});
    }
  }
}

void Servidor::pedir_config_seguridad(Conexion& conexion, const protocolo::Marco& marco) {
  if (!protocolo::leer_mensaje<protocolo::PedirConfigSeguridad>(marco.cuerpo)) {
    error_de_protocolo(conexion);
    return;
  }
  if (!conexion.es_dueno()) {
    negar(conexion, TipoMensaje::pedir_config_seguridad, MotivoNegado::sin_permiso);
    return;
  }
  encolar(conexion, protocolo::ConfigSeguridad{seguridad_, opciones_.en_xp});
}

void Servidor::cambiar_config_seguridad(Conexion& conexion, const protocolo::Marco& marco) {
  const auto pedido = protocolo::leer_mensaje<protocolo::CambiarConfigSeguridad>(marco.cuerpo);
  if (!pedido) {
    error_de_protocolo(conexion);
    return;
  }
  if (!conexion.es_dueno()) {
    negar(conexion, TipoMensaje::cambiar_config_seguridad, MotivoNegado::sin_permiso);
    return;
  }
  const auto yo = registrado_de(conexion.clave);
  if (yo != registrados_.end() && pide_con(pedido->seguridad, yo->guardado, false) &&
      !(yo->segundo_paso && conexion.segundo_paso_hecho) && !conexion.puede_configurar) {
    negar(conexion, TipoMensaje::cambiar_config_seguridad, MotivoNegado::sin_permiso);
    return;
  }
  const std::uint32_t quien = conexion.id;
  const protocolo::AvisoDeSeguridad aviso{protocolo::Seguridad::config_cambiada, ahora_ms(), conexion.nombre, {}};
  const auto trabajo = [this, quien, aviso, pedida = pedido->seguridad](Base& base) -> std::function<void()> {
    const bool convertido = aplicar_en_la_base(base, pedida);
    const Seguridad real = como_quedo(base, pedida);
    const bool hecho = convertido && base.guardar_seguridad(real) && base.anotar(aviso);
    return [this, quien, real, hecho, retencion = base.retencion(), cifra = base.historial_cifrado()] {
      retencion_ = retencion;
      historial_cifrado_ = cifra;
      Conexion* conexion = buscar_conectada(quien);
      aplicar_seguridad(real);
      if (!hecho) {
        avisar(Evento::error_base, red::Direccion{});
        if (conexion != nullptr) {
          negar(*conexion, TipoMensaje::cambiar_config_seguridad, MotivoNegado::lleno);
        }
      }
      if (conexion != nullptr) {
        avisar(Evento::config_cambiada, *conexion);
      } else {
        avisar(Evento::config_cambiada, red::Direccion{});
      }
      if (maximo_en_xp()) {
        avisar(Evento::maximo_en_xp, red::Direccion{});
      }
      for (auto& otra : conexiones_) {
        if (otra->estado == Estado::conectada && !otra->terminar && otra->es_dueno()) {
          encolar(*otra, protocolo::ConfigSeguridad{seguridad_, opciones_.en_xp});
        }
      }
    };
  };
  if (!base_.pedir(trabajo, true)) {
    negar(conexion, TipoMensaje::cambiar_config_seguridad, MotivoNegado::lleno);
  } else if (pedido->seguridad.opciones[Opcion::sin_historial] != 0) {
    retencion_ = Retencion::no_guardar;
  }
}

void Servidor::aplicar_seguridad(const Seguridad& nueva) {
  const bool fija_antes = seguridad_.opciones[Opcion::voz_fija] != 0;
  const bool fija = nueva.opciones[Opcion::voz_fija] != 0;
  const bool renovar_voz = fija != fija_antes || nueva.opciones[Opcion::algoritmo] != seguridad_.opciones[Opcion::algoritmo];
  seguridad_ = nueva;
  opciones_.protocolo.programa_minimo = std::max(programa_minimo_fijo_, version_minima(nueva.opciones));
  const bool rotacion_propia = opciones_.voz.rotacion != OpcionesVoz{}.rotacion;
  voz_->ajustar(rotacion_propia ? opciones_.voz.rotacion : std::chrono::seconds(segundos_de_renovacion(nueva.opciones)),
                static_cast<Algoritmo>(nueva.opciones[Opcion::algoritmo]), fija, Reloj::now());
  std::vector<std::uint32_t> con_voz;
  for (auto& conexion : conexiones_) {
    if (conexion->estado != Estado::conectada || conexion->terminar) {
      continue;
    }
    const protocolo::Rol antes = conexion->rol();
    revisar_segundo_paso(*conexion);
    if (conexion->rol() != antes) {
      difundir(protocolo::UsuarioRol{conexion->id, conexion->rol()});
    }
    if (conexion->canal != 0 && renovar_voz) {
      con_voz.push_back(conexion->id);
    }
  }
  renovar(con_voz);
}

void Servidor::avisar_al_dueno(const protocolo::AvisoDeSeguridad& aviso) {
  bool avisado = false;
  for (auto& conexion : conexiones_) {
    if (conexion->estado == Estado::conectada && !conexion->terminar && conexion->es_dueno()) {
      encolar(*conexion, aviso);
      avisado = true;
    }
  }
  const auto dueno = std::find_if(registrados_.begin(), registrados_.end(),
                                  [](const Registrado& cual) { return cual.guardado.dueno; });
  if (avisado || dueno == registrados_.end()) {
    return;
  }
  if (!base_.pedir([this, clave = dueno->guardado.clave, aviso](Base& base) -> std::function<void()> {
        if (base.guardar_aviso(clave, std::nullopt, aviso)) {
          return {};
        }
        return [this] { avisar(Evento::error_base, red::Direccion{}); };
      })) {
    avisar(Evento::error_base, red::Direccion{});
  }
}

bool Servidor::anotar(const protocolo::AvisoDeSeguridad& aviso, const std::optional<UsuarioGuardado>& usuario) {
  return base_.pedir([this, aviso, usuario](Base& base) -> std::function<void()> {
    if ((!usuario || base.guardar_usuario(*usuario)) && base.anotar(aviso)) {
      return {};
    }
    return [this] { avisar(Evento::error_base, red::Direccion{}); };
  });
}

bool Servidor::guardar_entrada(std::uint32_t id, const UsuarioGuardado& usuario, const Equipo& equipo, bool nueva) {
  return base_.pedir([this, id, usuario, equipo, nueva](Base& base) -> std::function<void()> {
    const auto conocido = nueva ? std::optional<bool>(false) : base.conoce_equipo(usuario.clave, equipo);
    const bool equipo_nuevo = !nueva && conocido == false;
    const protocolo::AvisoDeSeguridad aviso{nueva ? protocolo::Seguridad::entrada_nueva : protocolo::Seguridad::equipo_nuevo,
                                            usuario.visto_ms, usuario.nombre, {}};
    const bool bien = conocido && base.guardar_usuario(usuario) &&
                      base.anotar_equipo(usuario.clave, equipo, usuario.visto_ms, equipo_nuevo) &&
                      (!(nueva || equipo_nuevo) || base.anotar(aviso)) &&
                      (!equipo_nuevo || base.guardar_aviso(usuario.clave, equipo, aviso));
    const bool de_confianza = nueva || base.es_de_confianza(usuario.clave, equipo) == true;
    return [this, id, bien, equipo_nuevo, de_confianza, clave = usuario.clave, equipo,
            pendientes = base.avisos_para(usuario.clave, equipo)] {
      if (!bien) {
        avisar(Evento::error_base, red::Direccion{});
      }
      Conexion* conexion = buscar_conectada(id);
      if (conexion == nullptr) {
        return;
      }
      conexion->puede_configurar = de_confianza;
      ofrecer_segundo_paso(*conexion);
      if (equipo_nuevo) {
        avisar(Evento::equipo_nuevo, *conexion);
      }
      if (conexion->sin_segundo_paso) {
        return;
      }
      for (const auto& aviso : pendientes.lista) {
        encolar(*conexion, aviso);
      }
      if (!pendientes.lista.empty()) {
        base_.pedir([clave, equipo, hasta = pendientes.hasta](Base& base) -> std::function<void()> {
          base.entregados(clave, equipo, hasta);
          return {};
        });
      }
    };
  });
}

void Servidor::error_de_protocolo(Conexion& conexion) {
  falla(conexion.origen);
  conexion.terminar = true;
}

void Servidor::negar(Conexion& conexion, TipoMensaje pedido, MotivoNegado motivo) {
  encolar(conexion, protocolo::Negado{pedido, motivo});
}

void Servidor::comprobar_contrasena(Conexion& conexion, protocolo::Presentarse presentacion,
                                    cifrado::Secreto contrasena) {
  if (contrasena.size() == 0) {
    rechazar_presentacion(conexion, protocolo::MotivoPresentacion::falta_contrasena);
    return;
  }
  if (const std::uint16_t espera = segundos_de_espera(conexion.origen.ipv4, presentacion.clave)) {
    rechazar_presentacion(conexion, protocolo::MotivoPresentacion::esperar, espera);
    return;
  }
  conexion.marca = ++proxima_marca_;
  if (!comprobador_.pedir({conexion.marca, conexion.origen.ipv4, presentacion.clave}, *contrasena_,
                          std::move(contrasena))) {
    rechazar_presentacion(conexion, protocolo::MotivoPresentacion::ocupado);
    return;
  }
  conexion.estado = Estado::comprobando;
  conexion.esperando = std::move(presentacion);
}

void Servidor::terminar_comprobaciones() {
  for (const Comprobacion& hecha : comprobador_.tomar()) {
    const auto encontrada = std::find_if(conexiones_.begin(), conexiones_.end(), [&hecha](const auto& conexion) {
      return conexion->estado == Estado::comprobando && conexion->marca == hecha.marca && !conexion->terminar &&
             conexion->esperando;
    });
    Conexion* conexion = encontrada == conexiones_.end() ? nullptr : encontrada->get();
    std::optional<protocolo::Presentarse> presentacion;
    if (conexion != nullptr) {
      presentacion = std::move(conexion->esperando);
      conexion->esperando.reset();
      conexion->estado = Estado::presentando;
    }
    const red::Direccion origen = conexion != nullptr ? conexion->origen : red::Direccion{hecha.ip, 0};
    switch (hecha.resultado) {
      case Comprobado::distinta: {
        const std::uint16_t espera = anotar_fallo(hecha.ip, hecha.clave);
        avisar(Evento::contrasena_incorrecta, origen);
        falla(origen);
        if (conexion != nullptr) {
          rechazar_presentacion(*conexion, protocolo::MotivoPresentacion::contrasena_incorrecta, espera);
        }
        break;
      }
      case Comprobado::fallo:
        avisar(Evento::no_se_pudo_comprobar, origen);
        if (conexion != nullptr) {
          rechazar_presentacion(*conexion, protocolo::MotivoPresentacion::ocupado);
        }
        break;
      case Comprobado::coincide:
        esperas_por_ip_.erase(hecha.ip);
        esperas_por_clave_.erase(hecha.clave);
        if (conexion != nullptr) {
          continuar_presentacion(*conexion, std::move(*presentacion), std::nullopt);
        }
        break;
    }
  }
}

std::uint16_t Servidor::segundos_de_espera(std::uint32_t ip, const ClaveDeUsuario& clave) const {
  const auto ahora = Reloj::now();
  const auto falta = [ahora](const auto& esperas, const auto& llave) -> std::chrono::milliseconds {
    const auto encontrada = esperas.find(llave);
    return encontrada == esperas.end() || encontrada->second.hasta <= ahora
               ? std::chrono::milliseconds(0)
               : std::chrono::duration_cast<std::chrono::milliseconds>(encontrada->second.hasta - ahora);
  };
  const auto mayor = std::max(falta(esperas_por_ip_, ip), falta(esperas_por_clave_, clave));
  return static_cast<std::uint16_t>(std::min<std::chrono::milliseconds::rep>((mayor.count() + 999) / 1000, 65535));
}

std::uint16_t Servidor::anotar_fallo(std::uint32_t ip, const ClaveDeUsuario& clave) {
  const auto ahora = Reloj::now();
  const auto anotar = [this, ahora](auto& esperas, const auto& llave) {
    for (auto vieja = esperas.begin(); vieja != esperas.end();) {
      vieja = vieja->second.hasta + opciones_.espera_maxima < ahora ? esperas.erase(vieja) : std::next(vieja);
    }
    if (esperas.size() >= kMaximoIpsAnotadas && esperas.count(llave) == 0) {
      return;
    }
    Espera& espera = esperas[llave];
    espera.fallas = std::min(espera.fallas + 1, 20);
    const auto crecida = opciones_.espera_tras_fallo * (std::int64_t{1} << (espera.fallas - 1));
    espera.hasta = ahora + std::min<std::chrono::milliseconds>(
                               crecida, std::chrono::duration_cast<std::chrono::milliseconds>(opciones_.espera_maxima));
  };
  anotar(esperas_por_ip_, ip);
  anotar(esperas_por_clave_, clave);
  return std::max<std::uint16_t>(1, segundos_de_espera(ip, clave));
}

bool Servidor::tiene_dueno() const {
  return std::any_of(registrados_.begin(), registrados_.end(),
                     [](const Registrado& registrado) { return registrado.guardado.dueno; });
}

void Servidor::rechazar_presentacion(Conexion& conexion, protocolo::MotivoPresentacion motivo,
                                     std::uint16_t espera) {
  encolar(conexion, protocolo::PresentacionRechazada{motivo, espera});
  cerrar_con_respuesta(conexion);
}

bool Servidor::admitir_identidad(Conexion& conexion, const protocolo::Presentarse& presentacion,
                                 std::vector<Registrado>::iterator& yo, bool reclama, bool& nueva) {
  const auto ahora = Reloj::now();
  if (yo == registrados_.end()) {
    if (nuevas_por_ip_.size() >= kMaximoIpsAnotadas) {
      for (auto anotada = nuevas_por_ip_.begin(); anotada != nuevas_por_ip_.end();) {
        anotada = ahora - anotada->second.first >= kVentanaDeIdentidadesNuevas ? nuevas_por_ip_.erase(anotada)
                                                                                : std::next(anotada);
      }
    }
    auto desde_la_ip = nuevas_por_ip_.find(conexion.origen.ipv4);
    const bool sin_lugar_para_la_ip = desde_la_ip == nuevas_por_ip_.end() && nuevas_por_ip_.size() >= kMaximoIpsAnotadas;
    if (desde_la_ip != nuevas_por_ip_.end() && ahora - desde_la_ip->second.first >= kVentanaDeIdentidadesNuevas) {
      desde_la_ip->second = {ahora, 0};
    }
    if (ahora - nuevas_en_total_.first >= kVentanaDeIdentidadesNuevas) {
      nuevas_en_total_ = {ahora, 0};
    }
    const bool cerca_del_tope = registrados_.size() * 2 >= opciones_.maximo_identidades;
    const bool demasiadas = sin_lugar_para_la_ip ||
                            (cerca_del_tope && nuevas_en_total_.second >= opciones_.identidades_nuevas_por_hora) ||
                            (desde_la_ip != nuevas_por_ip_.end() &&
                             desde_la_ip->second.second >= opciones_.identidades_nuevas_por_ip);
    if (!reclama && demasiadas) {
      rechazar_presentacion(conexion, protocolo::MotivoPresentacion::demasiadas);
      return false;
    }
    if (registrados_.size() >= opciones_.maximo_identidades && !desalojar_una() && !reclama) {
      rechazar_presentacion(conexion, protocolo::MotivoPresentacion::lleno);
      return false;
    }
    if (!reclama) {
      ++nuevas_en_total_.second;
      ++nuevas_por_ip_.emplace(conexion.origen.ipv4, std::make_pair(ahora, 0)).first->second.second;
    }
    registrados_.push_back(Registrado{UsuarioGuardado{presentacion.clave}});
    yo = std::prev(registrados_.end());
    nueva = true;
    return true;
  }
  const auto anterior = std::find_if(conexiones_.begin(), conexiones_.end(), [&presentacion](const auto& otra) {
    return otra->estado == Estado::conectada && !otra->terminar && otra->clave == presentacion.clave;
  });
  if (anterior == conexiones_.end()) {
    return true;
  }
  if (ahora - yo->desde_reemplazos >= opciones_.ventana_de_reemplazos) {
    yo->desde_reemplazos = ahora;
    yo->reemplazos = 0;
  }
  if (yo->reemplazos >= opciones_.reemplazos_por_identidad) {
    rechazar_presentacion(conexion, protocolo::MotivoPresentacion::demasiadas);
    return false;
  }
  ++yo->reemplazos;
  desplazar(**anterior);
  return true;
}

bool Servidor::conectada(const ClaveDeUsuario& clave) const {
  return std::any_of(conexiones_.begin(), conexiones_.end(), [&clave](const auto& conexion) {
    return conexion->estado == Estado::conectada && !conexion->terminar && conexion->clave == clave;
  });
}

bool Servidor::desalojar_una() {
  const std::uint64_t ahora = ahora_ms();
  auto elegido = registrados_.end();
  for (auto registrado = registrados_.begin(); registrado != registrados_.end(); ++registrado) {
    if (se_puede_desalojar(registrado->guardado, ahora) && !conectada(registrado->guardado.clave) &&
        (elegido == registrados_.end() || registrado->guardado.visto_ms < elegido->guardado.visto_ms)) {
      elegido = registrado;
    }
  }
  if (elegido == registrados_.end() ||
      !base_.pedir([clave = elegido->guardado.clave](Base& base) -> std::function<void()> {
        base.borrar_usuarios({clave});
        return {};
      })) {
    return false;
  }
  registrados_.erase(elegido);
  return true;
}

bool Servidor::guardar(const UsuarioGuardado& usuario) {
  return base_.pedir([this, usuario](Base& base) -> std::function<void()> {
    if (base.guardar_usuario(usuario)) {
      return {};
    }
    return [this] { avisar(Evento::error_base, red::Direccion{}); };
  });
}

void Servidor::anotar_salida(const ClaveDeUsuario& clave) {
  const auto registrado = std::find_if(registrados_.begin(), registrados_.end(),
                                       [&clave](const Registrado& cual) { return cual.guardado.clave == clave; });
  if (registrado != registrados_.end()) {
    registrado->guardado.visto_ms = std::max(registrado->guardado.visto_ms, ahora_ms());
    guardar(registrado->guardado);
  }
}

void Servidor::vencer_identidades() {
  const auto ahora = Reloj::now();
  if (ahora < proxima_limpieza_) {
    return;
  }
  proxima_limpieza_ = ahora + kCadaCuantoVencer;
  const std::uint64_t hoy = ahora_ms();
  const bool reloj_atrasado = std::any_of(registrados_.begin(), registrados_.end(), [hoy](const Registrado& registrado) {
    return registrado.guardado.visto_ms > hoy;
  });
  if (reloj_atrasado) {
    return;
  }
  std::set<ClaveDeUsuario> conectadas;
  for (const auto& conexion : conexiones_) {
    if (conexion->estado == Estado::conectada) {
      conectadas.insert(conexion->clave);
    }
  }
  const auto vence = [hoy, &conectadas](const Registrado& registrado) {
    return esta_vencido(registrado.guardado, hoy) && conectadas.count(registrado.guardado.clave) == 0;
  };
  std::vector<ClaveDeUsuario> vencidas;
  for (const Registrado& registrado : registrados_) {
    if (vence(registrado)) {
      vencidas.push_back(registrado.guardado.clave);
    }
  }
  if (vencidas.empty() || !base_.pedir([vencidas](Base& base) -> std::function<void()> {
        base.borrar_usuarios(vencidas);
        return {};
      })) {
    return;
  }
  registrados_.erase(std::remove_if(registrados_.begin(), registrados_.end(), vence), registrados_.end());
}

void Servidor::desplazar(Conexion& conexion) {
  const std::uint32_t id = conexion.id;
  encolar(conexion, protocolo::Desplazado{});
  avisar(Evento::desplazada, conexion);
  cerrar_con_respuesta(conexion);
  voz_->baja(id);
  difundir(protocolo::UsuarioSalio{id});
}

void Servidor::cerrar_con_respuesta(Conexion& conexion) {
  conexion.estado = Estado::cerrando;
  conexion.limite = Reloj::now() + opciones_.tiempo_para_cerrar;
}

Servidor::Conexion* Servidor::buscar_conectada(std::uint32_t id) {
  for (auto& conexion : conexiones_) {
    if (conexion->estado == Estado::conectada && !conexion->terminar && conexion->id == id) {
      return conexion.get();
    }
  }
  return nullptr;
}

template <typename Mensaje>
void Servidor::encolar(Conexion& conexion, const Mensaje& mensaje) {
  std::vector<std::uint8_t> marco;
  if (!protocolo::armar_mensaje(mensaje, marco)) {
    conexion.terminar = true;
    return;
  }
  encolar_bytes(conexion, marco);
}

template <typename Mensaje>
void Servidor::difundir(const Mensaje& mensaje, std::uint32_t excepto) {
  std::vector<std::uint8_t> marco;
  if (!protocolo::armar_mensaje(mensaje, marco)) {
    return;
  }
  for (auto& conexion : conexiones_) {
    if (conexion->estado == Estado::conectada && !conexion->terminar && conexion->id != excepto) {
      encolar_bytes(*conexion, marco);
    }
  }
}

void Servidor::encolar_bytes(Conexion& conexion, const std::vector<std::uint8_t>& marco) {
  if (conexion.terminar) {
    return;
  }
  if (conexion.por_enviar() + marco.size() > opciones_.maximo_salida) {
    avisar(Evento::no_lee, conexion);
    conexion.terminar = true;
    return;
  }
  conexion.salida.insert(conexion.salida.end(), marco.begin(), marco.end());
}

bool Servidor::enviar_pendiente(Conexion& conexion) {
  while (conexion.por_enviar() > 0 && !conexion.terminar) {
    std::size_t escritos = 0;
    const auto paso = conexion.tls->escribir(
        gsl::span<const std::uint8_t>(conexion.salida).subspan(conexion.enviado), escritos);
    if (paso == cifrado::Paso::listo) {
      conexion.enviado += escritos;
      conexion.ultimo_dato = Reloj::now();
      continue;
    }
    if (paso == cifrado::Paso::esperar_escritura) {
      conexion.quiere_escribir = true;
    } else if (paso != cifrado::Paso::esperar_lectura) {
      conexion.terminar = true;
    }
    break;
  }
  if (conexion.enviado >= 64 * 1024 || conexion.enviado * 2 >= conexion.salida.size()) {
    conexion.salida.erase(conexion.salida.begin(),
                          conexion.salida.begin() + static_cast<std::ptrdiff_t>(conexion.enviado));
    conexion.enviado = 0;
  }
  if (conexion.por_enviar() > 0 || conexion.terminar) {
    return false;
  }
  if (conexion.estado == Estado::cerrando) {
    conexion.tls->cerrar();
    conexion.terminar = true;
  }
  return !conexion.terminar;
}

}
