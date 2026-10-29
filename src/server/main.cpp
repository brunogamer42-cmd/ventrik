#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
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
#include "common/texto.h"
#include "common/textos.h"
#include "common/utf.h"
#include "common/version.h"
#include "server/mudanza.h"
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
    case Evento::voz_conectada: return Clave::servidor_voz_conectada;
    case Evento::firma_invalida: return Clave::servidor_firma_invalida;
    case Evento::dueno_reclamado: return Clave::servidor_dueno_reclamado;
    case Evento::desplazada: return Clave::servidor_desplazada;
    case Evento::error_base: return Clave::servidor_error_guardar;
    case Evento::contrasena_incorrecta: return Clave::servidor_contrasena_incorrecta;
    case Evento::no_se_pudo_comprobar: return Clave::servidor_no_se_pudo_comprobar;
    case Evento::admin_nombrado: return Clave::servidor_admin_nombrado;
    case Evento::admin_quitado: return Clave::servidor_admin_quitado;
    case Evento::canal_borrado: return Clave::servidor_canal_borrado;
    case Evento::nombre_liberado: return Clave::servidor_nombre_liberado;
    case Evento::equipo_nuevo: return Clave::servidor_equipo_nuevo;
  }
  return Clave::servidor_conectada;
}

ventrik::Clave clave_de(ventrik::cifrado::ResultadoIdentidad resultado) {
  using ventrik::Clave;
  using ventrik::cifrado::ResultadoIdentidad;
  switch (resultado) {
    case ResultadoIdentidad::archivo_invalido: return Clave::servidor_error_clave_ilegible;
    case ResultadoIdentidad::error_lectura: return Clave::servidor_error_clave_lectura;
    case ResultadoIdentidad::error_escritura: return Clave::servidor_error_clave_escritura;
    case ResultadoIdentidad::falta_contrasena: return Clave::servidor_arranque_sin_consola;
    case ResultadoIdentidad::contrasena_incorrecta: return Clave::servidor_arranque_incorrecta;
    case ResultadoIdentidad::sin_memoria: return Clave::servidor_arranque_sin_memoria;
    case ResultadoIdentidad::boveda_de_otro_windows: return Clave::servidor_error_boveda_windows;
    case ResultadoIdentidad::boveda_danada: return Clave::servidor_error_boveda_danada;
    case ResultadoIdentidad::falta_boveda: return Clave::servidor_error_falta_boveda;
    case ResultadoIdentidad::falta_clave: return Clave::servidor_error_falta_clave;
    case ResultadoIdentidad::claves_distintas: return Clave::servidor_error_claves_distintas;
    case ResultadoIdentidad::cargada:
    case ResultadoIdentidad::creada:
    case ResultadoIdentidad::convertida:
    case ResultadoIdentidad::error_cifrado: break;
  }
  return Clave::servidor_error_cifrado;
}

ventrik::Clave clave_de(ventrik::servidor::Mudanza resultado) {
  using ventrik::Clave;
  using ventrik::servidor::Mudanza;
  switch (resultado) {
    case Mudanza::sin_servidor: return Clave::servidor_error_sin_servidor;
    case Mudanza::ya_hay_servidor: return Clave::servidor_error_ya_hay_servidor;
    case Mudanza::archivo_existe: return Clave::servidor_error_archivo_existe;
    case Mudanza::error_lectura: return Clave::servidor_error_respaldo_lectura;
    case Mudanza::error_escritura: return Clave::servidor_error_respaldo_escritura;
    case Mudanza::error_base: return Clave::servidor_error_base_respaldo;
    case Mudanza::base_grande: return Clave::servidor_error_base_grande;
    case Mudanza::contrasena_corta: return Clave::servidor_respaldo_corta;
    case Mudanza::contrasena: return Clave::servidor_respaldo_incorrecta;
    case Mudanza::sin_memoria: return Clave::servidor_respaldo_sin_memoria;
    case Mudanza::respaldo_danado: return Clave::servidor_respaldo_danado;
    case Mudanza::base_danada: return Clave::servidor_respaldo_base_danada;
    case Mudanza::base_nueva: return Clave::servidor_respaldo_base_nueva;
    case Mudanza::otro_esquema: return Clave::servidor_respaldo_otro_esquema;
    case Mudanza::cadena_danada: return Clave::servidor_cadena_danada;
    case Mudanza::cadena_de_otra_clave: return Clave::servidor_cadena_de_otra_clave;
    case Mudanza::clave_vieja_sin_borrar: return Clave::servidor_vieja_sin_borrar;
    case Mudanza::hecha:
    case Mudanza::error_cifrado: break;
  }
  return Clave::servidor_error_cifrado;
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

std::optional<ventrik::cifrado::Secreto> pedir_contrasena(std::string_view pregunta) {
  const HANDLE entrada = GetStdHandle(STD_INPUT_HANDLE);
  DWORD modo = 0;
  if (GetConsoleMode(entrada, &modo) == 0) {
    return std::nullopt;
  }
  escribir_linea(pregunta);
  if (SetConsoleMode(entrada, (modo & ~static_cast<DWORD>(ENABLE_ECHO_INPUT)) | ENABLE_LINE_INPUT) == 0) {
    return std::nullopt;
  }
  std::array<wchar_t, ventrik::protocolo::kMaximoBytesContrasena + 2> letras{};
  DWORD leidas = 0;
  bool bien = ReadConsoleW(entrada, letras.data(), static_cast<DWORD>(letras.size()), &leidas, nullptr) != 0;
  const std::wstring_view leido(letras.data(), bien ? std::min<std::size_t>(leidas, letras.size()) : 0);
  const std::wstring_view linea = leido.substr(0, leido.find_first_of(L"\r\n"));
  const int largo = static_cast<int>(linea.size());
  const int bytes = largo == 0 ? 0 : WideCharToMultiByte(CP_UTF8, 0, linea.data(), largo, nullptr, 0, nullptr, nullptr);
  std::optional<ventrik::cifrado::Secreto> escrita;
  if (bien && (largo == 0 || bytes > 0)) {
    escrita.emplace(static_cast<std::size_t>(bytes));
    WideCharToMultiByte(CP_UTF8, 0, linea.data(), largo, reinterpret_cast<char*>(escrita->bytes().data()), bytes,
                        nullptr, nullptr);
  }
  for (bool termino = leido.find(L'\n') != std::wstring_view::npos; bien && !termino;) {
    bien = ReadConsoleW(entrada, letras.data(), static_cast<DWORD>(letras.size()), &leidas, nullptr) != 0;
    termino = std::wstring_view(letras.data(), std::min<std::size_t>(leidas, letras.size())).find(L'\n') !=
              std::wstring_view::npos;
  }
  SetConsoleMode(entrada, modo);
  mbedtls_platform_zeroize(letras.data(), sizeof(letras));
  return escrita;
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
  ventrik::proteger_el_monton();

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
  bool local = false;
  bool nuevo_codigo = false;
  bool cambiar_contrasena = false;
  bool cambiar_arranque = false;
  bool cambiar_clave = false;
  std::optional<bool> cifrar_historial;
  std::string nombre;
  std::string liberar;
  std::wstring exportar;
  std::wstring importar;
  for (int i = 1; i < cantidad; ++i) {
    const std::wstring_view opcion = argumentos[i];
    const bool hay_valor = i + 1 < cantidad;
    const std::wstring_view valor = hay_valor ? argumentos[i + 1] : L"";
    if (opcion == L"--carpeta" && hay_valor) {
      carpeta = argumentos[++i];
      continue;
    }
    if (opcion == L"--exportar" && hay_valor && !valor.empty()) {
      exportar = argumentos[++i];
      continue;
    }
    if (opcion == L"--importar" && hay_valor && !valor.empty()) {
      importar = argumentos[++i];
      continue;
    }
    if (opcion == L"--cambiar-clave") {
      cambiar_clave = true;
      continue;
    }
    if (opcion == L"--nombre" && hay_valor && ventrik::es_nombre_valido(ventrik::a_utf8(argumentos[i + 1]))) {
      nombre = ventrik::a_utf8(argumentos[++i]);
      continue;
    }
    if (opcion == L"--liberar-nombre" && hay_valor && ventrik::es_nombre_valido(ventrik::a_utf8(argumentos[i + 1]))) {
      liberar = ventrik::a_utf8(argumentos[++i]);
      continue;
    }
    if (opcion == L"--nuevo-codigo-dueno") {
      nuevo_codigo = true;
      continue;
    }
    if (opcion == L"--contrasena") {
      cambiar_contrasena = true;
      continue;
    }
    if (opcion == L"--contrasena-arranque") {
      cambiar_arranque = true;
      continue;
    }
    if (opcion == L"--cifrar-historial" && (valor == L"si" || valor == L"sí" || valor == L"no")) {
      cifrar_historial = valor != L"no";
      ++i;
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
    if (opcion == L"--local") {
      local = true;
      continue;
    }
    escribir_linea(texto(ventrik::Clave::servidor_uso));
    escribir_linea(texto(ventrik::Clave::servidor_uso_huella));
    escribir_linea(texto(ventrik::Clave::servidor_uso_local));
    escribir_linea(texto(ventrik::Clave::servidor_uso_nombre));
    escribir_linea(texto(ventrik::Clave::servidor_uso_nuevo_codigo));
    escribir_linea(texto(ventrik::Clave::servidor_uso_liberar));
    escribir_linea(texto(ventrik::Clave::servidor_uso_contrasena));
    escribir_linea(texto(ventrik::Clave::servidor_uso_arranque));
    escribir_linea(texto(ventrik::Clave::servidor_uso_historial));
    escribir_linea(texto(ventrik::Clave::servidor_uso_exportar));
    escribir_linea(texto(ventrik::Clave::servidor_uso_importar));
    escribir_linea(texto(ventrik::Clave::servidor_uso_cambiar_clave));
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

  const auto pedir_dos_veces = [&texto](ventrik::Clave pregunta) -> std::optional<ventrik::cifrado::Secreto> {
    auto primera = pedir_contrasena(texto(pregunta));
    const auto segunda = primera ? pedir_contrasena(texto(ventrik::Clave::servidor_repetir_contrasena)) : std::nullopt;
    if (!primera || !segunda) {
      escribir_linea(texto(ventrik::Clave::servidor_contrasena_sin_consola));
      return std::nullopt;
    }
    if (ventrik::cifrado::texto_de(*primera) != ventrik::cifrado::texto_de(*segunda)) {
      escribir_linea(texto(ventrik::Clave::servidor_contrasenas_distintas));
      return std::nullopt;
    }
    return primera;
  };
  const auto escribir_huella = [&texto](ventrik::Clave titulo, const ventrik::cifrado::Huella& huella) {
    escribir_linea("");
    escribir_linea(texto(titulo));
    escribir_linea("    " + huella.a_texto());
    escribir_linea(texto(ventrik::Clave::servidor_huella_explicacion));
  };
  using ventrik::servidor::Mudanza;
  const auto avisar_mudanza = [&texto](Mudanza resultado, const std::wstring& ruta) {
    const std::string minimo = std::to_string(ventrik::cifrado::kMinimoContrasena);
    escribir_linea(ventrik::con_datos(texto(clave_de(resultado)), {minimo}) +
                   (ruta.empty() ? std::string() : " " + ventrik::a_utf8(ruta)));
  };

  if (!importar.empty()) {
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
    if (!ventrik::servidor::se_puede_importar(carpeta)) {
      avisar_mudanza(Mudanza::ya_hay_servidor, carpeta);
      return 1;
    }
    const auto contrasena = pedir_contrasena(texto(ventrik::Clave::servidor_pedir_respaldo_importar));
    if (!contrasena) {
      escribir_linea(texto(ventrik::Clave::servidor_contrasena_sin_consola));
      return 1;
    }
    const auto importado = ventrik::servidor::importar_servidor(
        carpeta, importar, ventrik::cifrado::texto_de(*contrasena), texto(ventrik::Clave::canal_general));
    const Mudanza resultado = importado.resultado;
    if (resultado != Mudanza::hecha) {
      avisar_mudanza(resultado, resultado == Mudanza::error_lectura ? importar
                                : resultado == Mudanza::ya_hay_servidor || resultado == Mudanza::error_escritura
                                    ? carpeta
                                    : std::wstring());
      return 1;
    }
    escribir_linea(texto(ventrik::Clave::servidor_importado));
    escribir_huella(ventrik::Clave::servidor_huella, *importado.huella);
    return 0;
  }

  const std::wstring archivo_base = carpeta + ventrik::servidor::kArchivoBase;
  const auto con_datos = ventrik::servidor::tiene_datos_guardados(archivo_base);
  if (!con_datos) {
    escribir_linea(texto(ventrik::Clave::servidor_error_base) + " " + ventrik::a_utf8(archivo_base));
    return 1;
  }
  if ((!exportar.empty() || cambiar_clave) && !ventrik::servidor::hay_servidor(carpeta)) {
    escribir_linea(texto(ventrik::Clave::servidor_error_sin_servidor) + " " + ventrik::a_utf8(carpeta));
    return 1;
  }
  auto cargada = ventrik::cifrado::cargar_o_crear_identidad(carpeta, std::nullopt, *con_datos);
  if (cargada.resultado == ventrik::cifrado::ResultadoIdentidad::falta_contrasena) {
    const auto contrasena = pedir_contrasena(texto(ventrik::Clave::servidor_pedir_arranque));
    if (contrasena) {
      cargada =
          ventrik::cifrado::cargar_o_crear_identidad(carpeta, ventrik::cifrado::texto_de(*contrasena), *con_datos);
    }
  }
  if (!cargada.identidad || !cargada.boveda) {
    escribir_linea(texto(clave_de(cargada.resultado)) +
                   (cargada.archivo.empty() ? std::string() : " " + ventrik::a_utf8(cargada.archivo)));
    return 1;
  }
  if (cargada.vieja_sin_borrar) {
    escribir_linea(texto(ventrik::Clave::servidor_vieja_sin_borrar) + " " +
                   ventrik::a_utf8(carpeta + ventrik::cifrado::kArchivoClaveVieja));
  } else if (cargada.resultado != ventrik::cifrado::ResultadoIdentidad::cargada) {
    escribir_linea(texto(cargada.resultado == ventrik::cifrado::ResultadoIdentidad::creada
                             ? ventrik::Clave::servidor_clave_creada
                             : ventrik::Clave::servidor_clave_convertida));
  }
  if (!ventrik::servidor::limpiar_restos(carpeta)) {
    escribir_linea(texto(ventrik::Clave::servidor_anterior_sin_borrar) + " " + ventrik::a_utf8(carpeta));
  }
  if (cambiar_arranque) {
    const auto nueva = pedir_dos_veces(ventrik::Clave::servidor_nueva_arranque);
    if (!nueva) {
      return 1;
    }
    const std::string_view escrita = ventrik::cifrado::texto_de(*nueva);
    if (!escrita.empty() && ventrik::cantidad_de_letras(escrita) < ventrik::cifrado::kMinimoContrasena) {
      escribir_linea(ventrik::con_datos(texto(ventrik::Clave::servidor_arranque_corta),
                                        {std::to_string(ventrik::cifrado::kMinimoContrasena)}));
      return 1;
    }
    const std::wstring archivo_boveda = carpeta + ventrik::cifrado::kArchivoBoveda;
    const auto boveda = cargada.boveda->armar_archivo(
        escrita.empty() ? std::nullopt : std::optional<std::string_view>(escrita));
    if (!boveda || !ventrik::escribir_archivo_completo(archivo_boveda, *boveda)) {
      escribir_linea(texto(ventrik::Clave::servidor_error_arranque) + " " + ventrik::a_utf8(archivo_boveda));
      return 1;
    }
    escribir_linea(texto(escrita.empty() ? ventrik::Clave::servidor_arranque_quitada
                                         : ventrik::Clave::servidor_arranque_puesta));
    if (!escrita.empty()) {
      escribir_linea(texto(ventrik::Clave::servidor_arranque_copias_viejas));
    }
  }
  if (cambiar_clave) {
    const auto cambio = ventrik::servidor::cambiar_clave(carpeta, *cargada.identidad, *cargada.boveda);
    const Mudanza resultado = cambio.resultado;
    if (resultado != Mudanza::hecha) {
      escribir_linea(texto(ventrik::Clave::servidor_clave_sin_cambiar));
      const wchar_t* archivo = resultado == Mudanza::clave_vieja_sin_borrar ? ventrik::cifrado::kArchivoClaveVieja
                               : resultado == Mudanza::error_escritura      ? L""
                                                                            : ventrik::servidor::kArchivoCadena;
      avisar_mudanza(resultado, resultado == Mudanza::error_cifrado ? std::wstring() : carpeta + archivo);
      return 1;
    }
    escribir_linea(texto(ventrik::Clave::servidor_clave_cambiada));
    escribir_linea(texto(ventrik::Clave::servidor_clave_cambiada_respaldos));
    if (cambio.anterior_sin_borrar) {
      escribir_linea(texto(ventrik::Clave::servidor_anterior_sin_borrar) + " " + ventrik::a_utf8(carpeta));
    }
    escribir_huella(ventrik::Clave::servidor_huella_nueva, *cambio.huella);
    return 0;
  }
  escribir_huella(ventrik::Clave::servidor_huella, cargada.identidad->huella());
  if (solo_huella) {
    return 0;
  }
  auto cadena = ventrik::servidor::leer_cadena_guardada(carpeta, cargada.identidad->huella());
  if (cadena.resultado != Mudanza::hecha) {
    avisar_mudanza(cadena.resultado, carpeta + ventrik::servidor::kArchivoCadena);
  }
  if (!exportar.empty()) {
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
    if (GetFileAttributesW(exportar.c_str()) != INVALID_FILE_ATTRIBUTES) {
      avisar_mudanza(Mudanza::archivo_existe, exportar);
      return 1;
    }
    const auto contrasena = pedir_dos_veces(ventrik::Clave::servidor_pedir_respaldo);
    if (!contrasena) {
      return 1;
    }
    const Mudanza resultado = ventrik::servidor::exportar_servidor(carpeta, *cargada.identidad, *cargada.boveda,
                                                                   exportar, ventrik::cifrado::texto_de(*contrasena));
    if (resultado != Mudanza::hecha) {
      avisar_mudanza(resultado, resultado == Mudanza::archivo_existe || resultado == Mudanza::error_escritura
                                    ? exportar
                                : resultado == Mudanza::error_base ? archivo_base
                                                                   : std::wstring());
      return 1;
    }
    escribir_linea(texto(ventrik::Clave::servidor_exportado) + " " + ventrik::a_utf8(exportar));
    return 0;
  }
  escribir_linea("");
  auto clave_historial = cargada.boveda->derivar(ventrik::cifrado::kEtiquetaHistorial);
  cargada.boveda.reset();
  if (!clave_historial) {
    escribir_linea(texto(ventrik::Clave::servidor_error_cifrado));
    return 1;
  }

  ventrik::servidor::Opciones opciones;
  if (cambiar_contrasena) {
    const auto nueva = pedir_dos_veces(ventrik::Clave::servidor_pedir_contrasena);
    if (!nueva) {
      return 1;
    }
    opciones.poner_contrasena = nueva->size() == 0
                                    ? std::optional<std::string>(std::string())
                                    : ventrik::servidor::guardar_contrasena(ventrik::cifrado::texto_de(*nueva));
    if (!opciones.poner_contrasena) {
      escribir_linea(texto(ventrik::Clave::servidor_error_contrasena));
      return 1;
    }
  }
  const std::wstring archivo_codigo = carpeta + L"\\codigo_dueno.txt";
  opciones.escuchar = {local ? 0x7F000001u : 0u, puerto};
  opciones.archivo_base = archivo_base;
  opciones.cifrar_historial = cifrar_historial;
  opciones.nombre_general = texto(ventrik::Clave::canal_general);
  opciones.nombre_servidor = nombre;
  opciones.nombre_de_fabrica = texto(ventrik::Clave::servidor_nombre_de_fabrica);
  opciones.quitar_dueno = nuevo_codigo;
  opciones.liberar_nombre = liberar;
  opciones.cadena = std::move(cadena.cadena);
  SYSTEM_INFO sistema = {};
  GetSystemInfo(&sistema);
  opciones.proteccion = ventrik::servidor::proteccion_para(sistema.dwNumberOfProcessors);
  if (local) {
    opciones.identidades_nuevas_por_ip = opciones.identidades_nuevas_por_hora = INT_MAX;
  }
  if (nuevo_codigo && !DeleteFileW(archivo_codigo.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
    escribir_linea(texto(ventrik::Clave::servidor_error_codigo) + " " + ventrik::a_utf8(archivo_codigo));
    return 1;
  }
  ventrik::servidor::Registro registro(escribir_linea,
                                      texto(ventrik::Clave::servidor_avisos_perdidos));
  const auto avisar = [&texto, &registro, &archivo_codigo](ventrik::servidor::Evento evento,
                                                           const ventrik::red::Direccion& origen,
                                                           const std::string& nombre) {
    if (evento == ventrik::servidor::Evento::conectada) {
      return;
    }
    if (evento == ventrik::servidor::Evento::dueno_reclamado) {
      DeleteFileW(archivo_codigo.c_str());
    }
    if (evento == ventrik::servidor::Evento::error_base) {
      registro.agregar(texto(clave_de(evento)));
      return;
    }
    const std::string ip = ventrik::red::a_texto(origen);
    registro.agregar(nombre.empty() ? texto(clave_de(evento)) + " " + ip
                                    : ventrik::con_datos(texto(clave_de(evento)), {nombre, ip}));
  };
  ventrik::servidor::Servidor servidor(*cargada.identidad, opciones, avisar);
  servidor.clave_del_historial(std::move(*clave_historial));
  if (!servidor.escuchar()) {
    escribir_linea(servidor.base_abierta()
                       ? texto(ventrik::Clave::servidor_error_puerto) + " " + std::to_string(puerto)
                       : texto(ventrik::Clave::servidor_error_base) + " " + ventrik::a_utf8(opciones.archivo_base));
    if (servidor.base_abierta() && opciones.poner_contrasena) {
      escribir_linea(texto(ventrik::Clave::servidor_contrasena_guardada));
    }
    if (!servidor.base_abierta() && opciones.cifrar_historial) {
      escribir_linea(texto(ventrik::Clave::servidor_error_historial));
    }
    return 1;
  }
  escribir_linea(texto(ventrik::Clave::servidor_nombre) + " " + servidor.nombre());
  escribir_linea(texto(servidor.pide_contrasena() ? ventrik::Clave::servidor_con_contrasena
                                                  : ventrik::Clave::servidor_sin_contrasena));
  escribir_linea(texto(servidor.historial_cifrado() ? ventrik::Clave::servidor_historial_cifrado
                                                    : ventrik::Clave::servidor_historial_sin_cifrar));
  if (servidor.tiene_dueno()) {
    DeleteFileW(archivo_codigo.c_str());
  } else {
    bool codigo_creado = false;
    const auto codigo = cargar_o_crear_codigo(archivo_codigo, codigo_creado);
    if (!codigo) {
      escribir_linea(texto(ventrik::Clave::servidor_error_codigo) + " " + ventrik::a_utf8(archivo_codigo));
      return 1;
    }
    servidor.codigo_para_reclamar(codigo);
    escribir_linea(codigo_creado ? texto(ventrik::Clave::servidor_codigo_dueno)
                                 : texto(ventrik::Clave::servidor_codigo_dueno_en));
    escribir_linea("    " + (codigo_creado ? codigo->a_texto() : ventrik::a_utf8(archivo_codigo)));
  }
  escribir_linea("");
  escribir_linea(texto(ventrik::Clave::servidor_puerto) + " " + std::to_string(servidor.puerto()));
  escribir_linea(texto(ventrik::Clave::servidor_salir));
  SetConsoleCtrlHandler(al_cortar, TRUE);
  servidor.correr(seguir_corriendo);
  registro.agregar(texto(ventrik::Clave::servidor_cerrando));
  return 0;
}
