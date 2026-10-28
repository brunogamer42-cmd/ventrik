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

}

struct Servidor::Conexion {
  bool pendiente() const {
    return estado == Estado::cifrando || estado == Estado::saludando ||
           estado == Estado::presentando || estado == Estado::comprobando;
  }
  bool tiene_limite() const { return pendiente() || estado == Estado::cerrando; }
  Persona persona() const { return Persona{dueno, canal}; }
  protocolo::UsuarioInfo info() const { return {id, nombre, canal, silencio, dueno}; }
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
  opciones_.conexiones_maximas =
      std::min(opciones_.conexiones_maximas, static_cast<std::size_t>(FD_SETSIZE - 3));
}

Servidor::~Servidor() = default;

bool Servidor::escuchar() {
  config_ = cifrado::ConfigTls::para_servidor(identidad_);
  if (!config_ || !despertador_.ok() ||
      !base_.arrancar(opciones_.archivo_base, opciones_.nombre_general, despertador_)) {
    return false;
  }
  struct Cargado {
    std::vector<protocolo::CanalInfo> canales;
    Retencion retencion = Retencion::siempre;
    std::optional<std::vector<UsuarioGuardado>> usuarios;
    std::string nombre;
    std::string contrasena;
    bool bien = false;
  };
  std::promise<Cargado> cargado;
  auto futuro = cargado.get_future();
  if (!base_.pedir([&cargado, this](Base& base) -> std::function<void()> {
        const bool bien = (!opciones_.quitar_dueno || base.quitar_dueno()) &&
                          (opciones_.liberar_nombre.empty() || base.liberar_nombre(opciones_.liberar_nombre)) &&
                          (!es_nombre_valido(opciones_.nombre_servidor) ||
                           base.guardar_texto("nombre", opciones_.nombre_servidor)) &&
                          (!opciones_.poner_contrasena || base.guardar_texto("contrasena", *opciones_.poner_contrasena));
        cargado.set_value({base.canales(), base.retencion(), base.usuarios(ahora_ms()), base.texto_guardado("nombre"),
                           base.texto_guardado("contrasena"), bien});
        return {};
      })) {
    return false;
  }
  auto [canales, retencion, usuarios, nombre, contrasena, bien] = futuro.get();
  contrasena_ = contrasena.empty() ? std::nullopt : leer_contrasena(contrasena);
  if (!bien || !usuarios || (!contrasena.empty() && !contrasena_)) {
    return false;
  }
  if (contrasena_) {
    comprobador_.arrancar();
  }
  retencion_ = retencion;
  nombre_ = es_nombre_valido(nombre) ? nombre : opciones_.nombre_de_fabrica;
  for (auto& usuario : *usuarios) {
    std::wstring comparable = nombre_para_comparar(usuario.nombre);
    registrados_.push_back(Registrado{std::move(usuario), std::move(comparable)});
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
    conexion->tls = std::make_unique<cifrado::SesionTls>(*config_, conexion->socket.get());
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
  for (const std::uint32_t id : voz_->revisar(ahora)) {
    Conexion* conexion = buscar_conectada(id);
    const auto clave = conexion != nullptr ? voz_->renovar(id, ahora, derivar_de(*conexion->tls))
                                           : std::nullopt;
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
  if (!guardar(actualizado, presentacion.equipo)) {
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
  conexion.id = proximo_id_++;
  conexion.clave = presentacion.clave;
  conexion.nombre = presentacion.nombre;
  conexion.comparable = comparable;
  conexion.dueno = yo->guardado.dueno;
  conexion.estado = Estado::conectada;
  conexion.ultimo_dato = ahora;
  conexion.chat = Cubeta(opciones_.rafaga_chat, opciones_.chat_por_segundo, ahora);
  conexion.pedidos = Cubeta(opciones_.rafaga_pedidos, opciones_.pedidos_por_segundo, ahora);
  conexion.historial = Cubeta(opciones_.rafaga_historial, opciones_.historial_por_segundo, ahora);
  conexion.entrar_a_la_voz = Cubeta(opciones_.rafaga_entrar_a_la_voz, opciones_.entrar_a_la_voz_por_segundo, ahora);
  conexion.ensamblador.cambiar_maximo(protocolo::kMaximoCuerpo);

  protocolo::Presentado presentado;
  presentado.tu_id = conexion.id;
  presentado.dueno = conexion.dueno;
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
  const bool pedido_aceptado =
      base_.pedir([this, quien, canal = pedido->canal](Base& base) {
        const bool hecho = base.borrar_canal(canal);
        return std::function<void()>([this, quien, canal, hecho]() {
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
        });
      });
  if (!pedido_aceptado) {
    negar(conexion, TipoMensaje::borrar_canal, MotivoNegado::lleno);
  }
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

bool Servidor::guardar(const UsuarioGuardado& usuario, const std::optional<Equipo>& equipo) {
  return base_.pedir([this, usuario, equipo](Base& base) -> std::function<void()> {
    const bool bien = base.guardar_usuario(usuario) && (!equipo || base.anotar_equipo(usuario.clave, *equipo, usuario.visto_ms));
    if (bien) {
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
    guardar(registrado->guardado, std::nullopt);
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
