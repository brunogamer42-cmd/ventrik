#include "client/consola.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

#include "client/audio.h"
#include "client/diagnostico.h"
#include "client/motor_voz.h"
#include "client/sesion.h"
#include "common/cifrado/inicio.h"
#include "common/pendiente.h"
#include "common/red/red.h"
#include "common/texto.h"
#include "common/utf.h"

namespace ventrik::cliente {
namespace {
using namespace std::chrono_literals;

class Consola {
 public:
  explicit Consola(Idioma idioma)
      : idioma_(idioma),
        salida_(CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, 0, nullptr)),
        entrada_(CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, 0, nullptr)) {}

  Idioma idioma() const { return idioma_; }
  std::string texto_de(Clave clave) const { return std::string(texto(idioma_, clave)); }

  void decir(Clave clave, std::initializer_list<std::string_view> datos = {}) const {
    escribir(con_datos(texto_de(clave), datos));
  }

  void escribir(std::string_view linea) const {
    const std::wstring ancha = a_utf16(linea) + L"\r\n";
    DWORD escritos = 0;
    WriteConsoleW(salida_, ancha.data(), static_cast<DWORD>(ancha.size()), &escritos, nullptr);
  }

  std::optional<std::string> leer_linea() const {
    std::array<wchar_t, 4096> leido{};
    DWORD cantidad = 0;
    if (!ReadConsoleW(entrada_, leido.data(), static_cast<DWORD>(leido.size()), &cantidad, nullptr)) {
      return std::nullopt;
    }
    std::wstring linea(leido.data(), cantidad);
    linea.erase(linea.find_last_not_of(L"\r\n") + 1);
    return a_utf8(linea);
  }

 private:
  Idioma idioma_;
  HANDLE salida_;
  HANDLE entrada_;
};

class Lineas {
 public:
  void agregar(std::string linea) {
    const std::lock_guard<std::mutex> candado(candado_);
    lineas_.push_back(std::move(linea));
  }
  std::deque<std::string> tomar() {
    const std::lock_guard<std::mutex> candado(candado_);
    return std::exchange(lineas_, {});
  }

 private:
  std::mutex candado_;
  std::deque<std::string> lineas_;
};

Clave clave_de(protocolo::MotivoNegado motivo) {
  switch (motivo) {
    case protocolo::MotivoNegado::sin_permiso: return Clave::consola_negado_sin_permiso;
    case protocolo::MotivoNegado::no_existe: return Clave::consola_negado_no_existe;
    case protocolo::MotivoNegado::texto_invalido: return Clave::consola_negado_texto_invalido;
    case protocolo::MotivoNegado::demasiado_rapido: return Clave::consola_negado_demasiado_rapido;
    case protocolo::MotivoNegado::lleno: return Clave::consola_negado_lleno;
    case protocolo::MotivoNegado::nombre_en_uso: return Clave::consola_negado_nombre_en_uso;
  }
  return Clave::consola_negado_sin_permiso;
}

std::string hora_local(std::uint64_t hora_ms, std::string sin_hora) {
  const std::uint64_t desde_1601 = hora_ms * 10000 + 116444736000000000ull;
  FILETIME universal = {static_cast<DWORD>(desde_1601), static_cast<DWORD>(desde_1601 >> 32)};
  FILETIME local = {};
  SYSTEMTIME partes = {};
  if (!FileTimeToLocalFileTime(&universal, &local) || !FileTimeToSystemTime(&local, &partes)) {
    return sin_hora;
  }
  const auto dos_cifras = [](int valor) {
    return std::string(valor < 10 ? "0" : "") + std::to_string(valor);
  };
  return dos_cifras(partes.wHour) + ":" + dos_cifras(partes.wMinute);
}

class Charla {
 public:
  Charla(const Consola& consola, Sesion& sesion, PuenteVoz& puente, MotorVoz& motor, const Audio& audio,
         std::uint16_t puerto)
      : consola_(consola), sesion_(sesion), puente_(puente), motor_(motor), audio_(audio), puerto_(puerto) {}

  void revisar_voz() {
    const auto estado = puente_.estado.load();
    if ((estado == EstadoVoz::lista || estado == EstadoVoz::sin_conexion) && estado != anunciado_) {
      consola_.decir(estado == EstadoVoz::lista ? Clave::consola_voz_lista
                                                : Clave::consola_voz_sin_conexion,
                     {std::to_string(puerto_)});
      anunciado_ = estado;
    }
    const auto hablante = puente_.hablando.load();
    if (hablante != 0 && hablante != hablante_mostrado_) {
      consola_.decir(Clave::consola_habla, {nombre_de_usuario(hablante)});
    }
    hablante_mostrado_ = hablante;
  }

  bool terminada() const { return terminada_; }

  void mostrar(const Novedad& novedad) {
    std::visit([this](const auto& cual) { atender(cual); }, novedad);
  }

  void ordenar(const std::string& linea) {
    if (linea.empty()) {
      return;
    }
    if (linea.front() != '/') {
      if (!es_chat_valido(linea)) {
        consola_.decir(Clave::consola_mensaje_invalido);
        return;
      }
      sesion_.ordenar(protocolo::EnviarChat{linea});
      return;
    }
    const std::size_t espacio = linea.find(' ');
    const std::string comando = linea.substr(0, espacio);
    const std::string resto = espacio == std::string::npos ? std::string() : linea.substr(espacio + 1);
    if (comando == "/ayuda") {
      consola_.decir(Clave::consola_ayuda);
    } else if (comando == "/canales") {
      mostrar_canales();
    } else if (comando == "/quien") {
      mostrar_quien();
    } else if (comando == "/historial") {
      sesion_.ordenar(protocolo::PedirHistorial{mi_canal(), mas_viejo_});
    } else if (comando == "/diag") {
      const auto medicion = diagnostico_.medir();
      const std::uint64_t perdidos = puente_.perdidos;
      const std::uint64_t total = perdidos + puente_.recibidos;
      consola_.decir(Clave::consola_diag,
                     {ping_ ? std::to_string(*ping_) : consola_.texto_de(Clave::consola_sin_ping),
                      std::to_string(static_cast<int>(medicion.procesador + 0.5)),
                      std::to_string(medicion.ram_bytes / (1024 * 1024)),
                      std::to_string(total == 0 ? 0 : perdidos * 100 / total),
                      std::to_string(audio_.espera_de_salida_ms())});
    } else if (comando == "/hablar") {
      motor_.transmitir(!motor_.transmitiendo());
      consola_.decir(motor_.transmitiendo() ? Clave::consola_hablando_vos : Clave::consola_callado);
    } else if (comando == "/salir") {
      terminada_ = true;
    } else if (comando == "/entrar") {
      if (const auto canal = buscar(resto)) {
        sesion_.ordenar(protocolo::EntrarACanal{*canal});
      }
    } else if (comando == "/crear") {
      if (es_nombre_valido(resto)) {
        sesion_.ordenar(protocolo::CrearCanal{resto});
      } else {
        consola_.decir(Clave::consola_nombre_canal_invalido);
      }
    } else if (comando == "/renombrar") {
      const std::size_t igual = resto.find(" = ");
      const auto canal = igual == std::string::npos ? std::nullopt : buscar(resto.substr(0, igual));
      const std::string nuevo = igual == std::string::npos ? std::string() : resto.substr(igual + 3);
      if (canal && es_nombre_valido(nuevo)) {
        sesion_.ordenar(protocolo::RenombrarCanal{*canal, nuevo});
      } else if (canal) {
        consola_.decir(Clave::consola_nombre_canal_invalido);
      }
    } else if (comando == "/borrar") {
      if (const auto canal = buscar(resto)) {
        sesion_.ordenar(protocolo::BorrarCanal{*canal});
      }
    } else {
      consola_.decir(Clave::consola_comando_desconocido);
    }
  }

 private:
  void atender(const IntentoDeConexion& intento) {
    if (intento.estado != EstadoConexion::conectado || !intento.presentado) {
      consola_.escribir(explicar(consola_.idioma(), intento));
      terminada_ = true;
      return;
    }
    mi_id_ = intento.presentado->tu_id;
    canales_ = intento.presentado->canales;
    for (const auto& usuario : intento.presentado->usuarios) {
      usuarios_[usuario.id] = usuario;
    }
    consola_.decir(Clave::consola_entraste, {nombre_de_usuario(mi_id_), nombre_de_canal(mi_canal())});
    if (intento.presentado->dueno) {
      consola_.decir(Clave::consola_sos_dueno);
    }
    consola_.decir(Clave::consola_ayuda);
    pedir_historial_nuevo();
  }

  void atender(const protocolo::UsuarioEntro& entro) {
    usuarios_[entro.usuario.id] = entro.usuario;
    consola_.decir(Clave::consola_entro, {entro.usuario.nombre});
  }

  void atender(const protocolo::UsuarioSalio& salio) {
    consola_.decir(Clave::consola_salio, {nombre_de_usuario(salio.id)});
    usuarios_.erase(salio.id);
  }

  void atender(const protocolo::UsuarioCambioCanal& cambio) {
    usuarios_[cambio.id].canal = cambio.canal;
    if (cambio.id == mi_id_) {
      consola_.decir(Clave::consola_estas_en, {nombre_de_canal(cambio.canal)});
      pedir_historial_nuevo();
    } else {
      consola_.decir(Clave::consola_paso_a, {nombre_de_usuario(cambio.id), nombre_de_canal(cambio.canal)});
    }
  }

  void atender(const protocolo::CanalCreado& creado) {
    canales_.push_back(creado.canal);
    consola_.decir(Clave::consola_canal_creado, {creado.canal.nombre});
  }

  void atender(const protocolo::CanalRenombrado& renombrado) {
    for (auto& canal : canales_) {
      if (canal.id == renombrado.canal.id) {
        consola_.decir(Clave::consola_canal_renombrado, {canal.nombre, renombrado.canal.nombre});
        canal.nombre = renombrado.canal.nombre;
      }
    }
  }

  void atender(const protocolo::CanalBorrado& borrado) {
    const bool estaba = mi_canal() == borrado.id;
    consola_.decir(Clave::consola_canal_borrado,
                   {nombre_de_canal(borrado.id), nombre_de_canal(borrado.movidos_a)});
    canales_.erase(std::remove_if(canales_.begin(), canales_.end(),
                                  [&](const protocolo::CanalInfo& c) { return c.id == borrado.id; }),
                   canales_.end());
    for (auto& [id, usuario] : usuarios_) {
      if (usuario.canal == borrado.id) {
        usuario.canal = borrado.movidos_a;
      }
    }
    if (estaba) {
      consola_.decir(Clave::consola_estas_en, {nombre_de_canal(mi_canal())});
      pedir_historial_nuevo();
    }
  }

  void atender(const protocolo::MensajeChat& mensaje) {
    if (mensaje.canal == mi_canal()) {
      mostrar_mensaje(mensaje.hora_ms, mensaje.autor, mensaje.texto);
    }
  }

  void atender(const protocolo::Historial& historial) {
    if (historial.canal != mi_canal()) {
      return;
    }
    consola_.decir(Clave::consola_historial, {nombre_de_canal(historial.canal)});
    std::for_each(historial.mensajes.rbegin(), historial.mensajes.rend(), [this](const auto& guardado) {
      mostrar_mensaje(guardado.hora_ms, guardado.autor, guardado.texto);
    });
    if (!historial.mensajes.empty()) {
      mas_viejo_ = historial.mensajes.back().id;
    }
    consola_.decir(historial.hay_mas ? Clave::consola_hay_mas : Clave::consola_sin_mas);
  }

  void atender(const protocolo::Negado& negado) { consola_.decir(clave_de(negado.motivo)); }

  void atender(const PingMedido& ping) { ping_ = ping.milisegundos; }

  void atender(const Fin& fin) {
    consola_.escribir(explicar(consola_.idioma(), fin));
    terminada_ = true;
  }

  void mostrar_mensaje(std::uint64_t hora_ms, const std::string& autor, const std::string& texto) const {
    consola_.decir(Clave::consola_mensaje,
                   {hora_local(hora_ms, consola_.texto_de(Clave::consola_sin_hora)), autor, texto});
  }

  std::uint32_t mi_canal() const {
    const auto yo = usuarios_.find(mi_id_);
    return yo == usuarios_.end() ? 0 : yo->second.canal;
  }

  std::string nombre_de_usuario(std::uint32_t id) const {
    const auto usuario = usuarios_.find(id);
    return usuario == usuarios_.end() ? consola_.texto_de(Clave::consola_desconocido)
                                      : usuario->second.nombre;
  }

  std::string nombre_de_canal(std::uint32_t id) const {
    const auto canal = std::find_if(canales_.begin(), canales_.end(),
                                    [id](const protocolo::CanalInfo& c) { return c.id == id; });
    return canal == canales_.end() ? consola_.texto_de(Clave::consola_desconocido) : canal->nombre;
  }

  std::optional<std::uint32_t> buscar(const std::string& nombre) const {
    const std::wstring buscado = nombre_para_comparar(nombre);
    const auto canal = std::find_if(canales_.begin(), canales_.end(), [&](const protocolo::CanalInfo& c) {
      return !buscado.empty() && nombre_para_comparar(c.nombre) == buscado;
    });
    if (canal == canales_.end()) {
      consola_.decir(Clave::consola_no_existe);
      return std::nullopt;
    }
    return canal->id;
  }

  void pedir_historial_nuevo() {
    mas_viejo_ = 0;
    sesion_.ordenar(protocolo::PedirHistorial{mi_canal(), 0});
  }

  void mostrar_canales() const {
    consola_.decir(Clave::consola_canales);
    for (const auto& canal : canales_) {
      const auto personas = std::count_if(usuarios_.begin(), usuarios_.end(),
                                          [&](const auto& u) { return u.second.canal == canal.id; });
      std::string linea = "  " + con_datos(consola_.texto_de(Clave::consola_canal_linea),
                                           {canal.nombre, std::to_string(personas)});
      if (canal.id == mi_canal()) {
        linea += " " + consola_.texto_de(Clave::consola_aca);
      }
      consola_.escribir(linea);
    }
  }

  void mostrar_quien() const {
    for (const auto& canal : canales_) {
      std::string nombres;
      for (const auto& [id, usuario] : usuarios_) {
        if (usuario.canal == canal.id) {
          nombres += (nombres.empty() ? "" : ", ") + usuario.nombre;
        }
      }
      if (!nombres.empty()) {
        consola_.decir(Clave::consola_quien_linea, {canal.nombre, nombres});
      }
    }
  }

  const Consola& consola_;
  Sesion& sesion_;
  PuenteVoz& puente_;
  MotorVoz& motor_;
  const Audio& audio_;
  std::uint16_t puerto_;
  Diagnostico diagnostico_;
  EstadoVoz anunciado_ = EstadoVoz::sin_clave;
  std::uint32_t hablante_mostrado_ = 0;
  std::uint32_t mi_id_ = 0;
  std::vector<protocolo::CanalInfo> canales_;
  std::map<std::uint32_t, protocolo::UsuarioInfo> usuarios_;
  std::uint64_t mas_viejo_ = 0;
  std::optional<std::uint32_t> ping_;
  bool terminada_ = false;
};

}

int abrir_consola(Idioma idioma, int cantidad, wchar_t** argumentos) {
  VENTRIK_PENDIENTE("consola de prueba provisoria hasta la ventana del paso 2.5");
  AllocConsole();
  const Consola consola(idioma);
  SetConsoleTitleW(a_utf16(consola.texto_de(Clave::consola_titulo)).c_str());
  const auto esperar_enter = [&consola](int codigo) {
    consola.decir(Clave::consola_enter_para_cerrar);
    consola.leer_linea();
    return codigo;
  };

  const bool con_codigo = cantidad == 7 && std::wstring_view(argumentos[5]) == L"--dueno";
  if (cantidad != 5 && !con_codigo) {
    consola.decir(Clave::consola_uso);
    return esperar_enter(2);
  }
  const auto direccion = red::leer_direccion(a_utf8(argumentos[2]));
  const auto huella = cifrado::leer_huella(a_utf8(argumentos[3]));
  Presentacion quien{a_utf8(argumentos[4]), std::nullopt};
  if (con_codigo) {
    quien.codigo_dueno = cifrado::leer_codigo_dueno(a_utf8(argumentos[6]));
  }
  const Clave problema = !direccion                      ? Clave::conectar_direccion_invalida
                         : !huella                       ? Clave::conectar_huella_invalida
                         : !es_nombre_valido(quien.nombre) ? Clave::conectar_nombre_invalido
                         : con_codigo && !quien.codigo_dueno ? Clave::conectar_codigo_invalido
                                                             : Clave::consola_titulo;
  if (problema != Clave::consola_titulo) {
    consola.decir(problema);
    return esperar_enter(2);
  }

  cifrado::Inicio cifrado;
  red::Inicio red;
  if (!cifrado.ok() || !red.ok()) {
    consola.decir(Clave::conectar_error_interno);
    return esperar_enter(1);
  }
  consola.decir(Clave::consola_conectando, {red::a_texto(*direccion)});
  const auto lineas = std::make_shared<Lineas>();
  std::thread([consola, lineas] {
    while (auto linea = consola.leer_linea()) {
      lineas->agregar(std::move(*linea));
    }
  }).detach();

  const auto puente = std::make_shared<PuenteVoz>();
  MotorVoz motor(*puente, calidad_automatica());
  Audio audio;
  if (!motor.ok() || !audio.abrir(motor.frecuencia(), MotorVoz::procesar_audio, &motor)) {
    consola.decir(Clave::consola_sin_audio);
  }
  OpcionesSesion opciones;
  opciones.voz = puente;
  Sesion sesion(*direccion, *huella, quien, opciones);
  Charla charla(consola, sesion, *puente, motor, audio, direccion->puerto);
  while (!charla.terminada()) {
    for (const Novedad& novedad : sesion.esperar_novedades(100ms)) {
      charla.mostrar(novedad);
    }
    charla.revisar_voz();
    for (const std::string& linea : lineas->tomar()) {
      charla.ordenar(linea);
    }
  }
  consola.decir(Clave::consola_enter_para_cerrar);
  while (lineas->tomar().empty()) {
    std::this_thread::sleep_for(100ms);
  }
  return 0;
}

}
