#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <windows.h>

#include "common/archivos.h"
#include "common/cifrado/codigo_dueno.h"
#include "common/cifrado/identidad.h"
#include "common/cifrado/inicio.h"
#include "common/proceso.h"
#include "common/red/red.h"
#include "common/textos.h"
#include "common/utf.h"
#include "common/version.h"
#include "server/registro.h"
#include "server/servidor.h"

namespace {

std::atomic<bool> seguir_corriendo{true};

BOOL WINAPI al_cortar(DWORD) {
  seguir_corriendo = false;
  return TRUE;
}

void escribir_linea(std::string_view texto) {
  const HANDLE salida = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD modo = 0;
  DWORD escritos = 0;
  if (GetConsoleMode(salida, &modo) != 0) {
    const std::wstring ancho = ventrik::a_utf16(texto) + L"\r\n";
    WriteConsoleW(salida, ancho.data(), static_cast<DWORD>(ancho.size()), &escritos, nullptr);
  } else {
    const std::string linea = std::string(texto) + "\r\n";
    WriteFile(salida, linea.data(), static_cast<DWORD>(linea.size()), &escritos, nullptr);
  }
}

ventrik::Clave clave_de(ventrik::servidor::Evento evento) {
  using ventrik::Clave;
  using ventrik::servidor::Evento;
  switch (evento) {
    case Evento::conectada: return Clave::servidor_conectada;
    case Evento::desconectada: return Clave::servidor_desconectada;
    case Evento::rechazo_cifrado: return Clave::servidor_rechazo_cifrado;
    case Evento::rechazo_saludo: return Clave::servidor_rechazo_saludo;
    case Evento::tiempo_vencido: return Clave::servidor_tiempo_vencido;
    case Evento::demasiadas_pendientes: return Clave::servidor_demasiadas_pendientes;
    case Evento::demasiadas_por_ip: return Clave::servidor_demasiadas_por_ip;
    case Evento::lleno: return Clave::servidor_lleno;
    case Evento::demasiado_rapido: return Clave::servidor_demasiado_rapido;
    case Evento::bloqueada: return Clave::servidor_bloqueada;
    case Evento::rechazo_bloqueada: return Clave::servidor_rechazo_bloqueada;
    case Evento::ocupado: return Clave::servidor_ocupado;
    case Evento::presentada: return Clave::servidor_presentada;
    case Evento::salio: return Clave::servidor_salio;
    case Evento::sin_respuesta: return Clave::servidor_sin_respuesta;
    case Evento::no_lee: return Clave::servidor_no_lee;
    case Evento::codigo_incorrecto: return Clave::servidor_codigo_incorrecto;
  }
  return Clave::servidor_conectada;
}

std::optional<ventrik::cifrado::CodigoDueno> cargar_o_crear_codigo(const std::wstring& archivo,
                                                                   bool& creado) {
  ventrik::cifrado::Secreto leido;
  const auto lectura = ventrik::leer_archivo(archivo, 256, leido);
  if (lectura == ventrik::LecturaArchivo::leido) {
    const std::string_view texto(reinterpret_cast<const char*>(leido.bytes().data()), leido.size());
    return ventrik::cifrado::leer_codigo_dueno(texto.substr(0, texto.find_last_not_of(" \r\n") + 1));
  }
  auto nuevo = ventrik::cifrado::CodigoDueno::generar();
  if (lectura != ventrik::LecturaArchivo::no_existe || !nuevo) {
    return std::nullopt;
  }
  const std::string texto = nuevo->a_texto();
  const auto bytes = gsl::make_span(reinterpret_cast<const std::uint8_t*>(texto.data()), texto.size());
  creado = ventrik::escribir_archivo_nuevo(archivo, bytes) == ventrik::EscrituraNueva::escrito;
  return creado ? nuevo : std::nullopt;
}

bool leer_puerto(std::wstring_view texto, std::uint16_t& puerto) {
  std::uint32_t valor = 0;
  if (texto.empty() || texto.size() > 5 || texto.front() == L'0') {
    return false;
  }
  for (const wchar_t letra : texto) {
    if (letra < L'0' || letra > L'9') {
      return false;
    }
    valor = valor * 10 + static_cast<std::uint32_t>(letra - L'0');
  }
  if (valor > 65535) {
    return false;
  }
  puerto = static_cast<std::uint16_t>(valor);
  return true;
}

}

int wmain(int cantidad, wchar_t** argumentos) {
  ventrik::endurecer_carga_de_dll();
  ventrik::activar_dep();

  const ventrik::Idioma idioma = ventrik::idioma_de_windows();
  const auto texto = [&idioma](ventrik::Clave clave) {
    return std::string(ventrik::texto(idioma, clave));
  };
  if (!ventrik::windows_es_compatible()) {
    escribir_linea(texto(ventrik::Clave::aviso_windows_viejo));
    return 1;
  }

  std::wstring carpeta;
  std::uint16_t puerto = ventrik::red::kPuertoPredeterminado;
  bool solo_huella = false;
  for (int i = 1; i < cantidad; ++i) {
    const std::wstring_view opcion = argumentos[i];
    const bool hay_valor = i + 1 < cantidad;
    if (opcion == L"--carpeta" && hay_valor) {
      carpeta = argumentos[++i];
      continue;
    }
    if (opcion == L"--puerto" && hay_valor && leer_puerto(argumentos[i + 1], puerto)) {
      ++i;
      continue;
    }
    if (opcion == L"--huella") {
      solo_huella = true;
      continue;
    }
    escribir_linea(texto(ventrik::Clave::servidor_uso));
    escribir_linea(texto(ventrik::Clave::servidor_uso_huella));
    return 2;
  }
  if (carpeta.empty()) {
    carpeta = ventrik::carpeta_de_datos(L"Servidor");
  }

  ventrik::cifrado::Inicio cifrado;
  if (!cifrado.ok()) {
    escribir_linea(texto(ventrik::Clave::servidor_error_cifrado));
    return 1;
  }
  ventrik::red::Inicio red;
  if (!red.ok()) {
    escribir_linea(texto(ventrik::Clave::servidor_error_red));
    return 1;
  }
  if (carpeta.empty() || !ventrik::crear_carpetas(carpeta)) {
    escribir_linea(texto(ventrik::Clave::servidor_error_carpeta) + " " + ventrik::a_utf8(carpeta));
    return 1;
  }

  escribir_linea(texto(ventrik::Clave::servidor_titulo) + " " +
                 ventrik::a_texto(ventrik::kVersionPrograma));
  escribir_linea(texto(ventrik::Clave::servidor_carpeta) + " " + ventrik::a_utf8(carpeta));

  const std::wstring archivo = carpeta + L"\\clave_servidor.der";
  auto cargada = ventrik::cifrado::cargar_o_crear_identidad(archivo);
  switch (cargada.resultado) {
    case ventrik::cifrado::ResultadoIdentidad::cargada:
      break;
    case ventrik::cifrado::ResultadoIdentidad::creada:
      escribir_linea(texto(ventrik::Clave::servidor_clave_creada));
      break;
    case ventrik::cifrado::ResultadoIdentidad::archivo_invalido:
      escribir_linea(texto(ventrik::Clave::servidor_error_clave_ilegible) + " " +
                     ventrik::a_utf8(archivo));
      return 1;
    case ventrik::cifrado::ResultadoIdentidad::error_lectura:
      escribir_linea(texto(ventrik::Clave::servidor_error_clave_lectura) + " " +
                     ventrik::a_utf8(archivo));
      return 1;
    case ventrik::cifrado::ResultadoIdentidad::error_escritura:
      escribir_linea(texto(ventrik::Clave::servidor_error_clave_escritura) + " " +
                     ventrik::a_utf8(archivo));
      return 1;
    case ventrik::cifrado::ResultadoIdentidad::error_cifrado:
      escribir_linea(texto(ventrik::Clave::servidor_error_cifrado));
      return 1;
  }
  escribir_linea("");
  escribir_linea(texto(ventrik::Clave::servidor_huella));
  escribir_linea("    " + cargada.identidad->huella().a_texto());
  escribir_linea(texto(ventrik::Clave::servidor_huella_explicacion));
  if (solo_huella) {
    return 0;
  }
  escribir_linea("");

  const std::wstring archivo_codigo = carpeta + L"\\codigo_dueno.txt";
  ventrik::servidor::Opciones opciones;
  opciones.escuchar.puerto = puerto;
  opciones.archivo_base = carpeta + L"\\ventrik.db";
  opciones.nombre_general = texto(ventrik::Clave::canal_general);
  bool codigo_creado = false;
  opciones.codigo_dueno = cargar_o_crear_codigo(archivo_codigo, codigo_creado);
  if (!opciones.codigo_dueno) {
    escribir_linea(texto(ventrik::Clave::servidor_error_codigo) + " " + ventrik::a_utf8(archivo_codigo));
    return 1;
  }
  escribir_linea(codigo_creado ? texto(ventrik::Clave::servidor_codigo_dueno)
                               : texto(ventrik::Clave::servidor_codigo_dueno_en));
  escribir_linea("    " + (codigo_creado ? opciones.codigo_dueno->a_texto()
                                         : ventrik::a_utf8(archivo_codigo)));
  escribir_linea("");
  ventrik::servidor::Registro registro(escribir_linea,
                                      texto(ventrik::Clave::servidor_avisos_perdidos));
  const auto avisar = [&texto, &registro](ventrik::servidor::Evento evento,
                                          const ventrik::red::Direccion& origen,
                                          const std::string& nombre) {
    if (evento == ventrik::servidor::Evento::conectada) {
      return;
    }
    const std::string ip = ventrik::red::a_texto(origen);
    registro.agregar(nombre.empty() ? texto(clave_de(evento)) + " " + ip
                                    : ventrik::con_datos(texto(clave_de(evento)), {nombre, ip}));
  };
  ventrik::servidor::Servidor servidor(*cargada.identidad, opciones, avisar);
  if (!servidor.escuchar()) {
    escribir_linea(servidor.base_abierta()
                       ? texto(ventrik::Clave::servidor_error_puerto) + " " + std::to_string(puerto)
                       : texto(ventrik::Clave::servidor_error_base) + " " + ventrik::a_utf8(opciones.archivo_base));
    return 1;
  }
  escribir_linea(texto(ventrik::Clave::servidor_puerto) + " " + std::to_string(servidor.puerto()));
  escribir_linea(texto(ventrik::Clave::servidor_salir));
  SetConsoleCtrlHandler(al_cortar, TRUE);
  servidor.correr(seguir_corriendo);
  registro.agregar(texto(ventrik::Clave::servidor_cerrando));
  return 0;
}
