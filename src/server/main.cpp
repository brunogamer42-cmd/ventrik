// Servidor de Ventrik: por ahora, en una ventana de consola.

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

#include <windows.h>

#include "common/archivos.h"
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
    // Redirigida a un archivo: UTF-8, que no depende de la página de códigos.
    const std::string linea = std::string(texto) + "\r\n";
    WriteFile(salida, linea.data(), static_cast<DWORD>(linea.size()), &escritos, nullptr);
  }
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

}  // namespace

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

  ventrik::servidor::Opciones opciones;
  opciones.escuchar.puerto = puerto;
  ventrik::servidor::Registro registro(escribir_linea,
                                      texto(ventrik::Clave::servidor_avisos_perdidos));
  const auto avisar = [&texto, &registro](ventrik::servidor::Evento evento,
                                          const ventrik::red::Direccion& origen) {
    ventrik::Clave clave = ventrik::Clave::servidor_conectada;
    switch (evento) {
      case ventrik::servidor::Evento::conectada:
        clave = ventrik::Clave::servidor_conectada;
        break;
      case ventrik::servidor::Evento::desconectada:
        clave = ventrik::Clave::servidor_desconectada;
        break;
      case ventrik::servidor::Evento::rechazo_cifrado:
        clave = ventrik::Clave::servidor_rechazo_cifrado;
        break;
      case ventrik::servidor::Evento::rechazo_saludo:
        clave = ventrik::Clave::servidor_rechazo_saludo;
        break;
      case ventrik::servidor::Evento::tiempo_vencido:
        clave = ventrik::Clave::servidor_tiempo_vencido;
        break;
      case ventrik::servidor::Evento::demasiadas_pendientes:
        clave = ventrik::Clave::servidor_demasiadas_pendientes;
        break;
      case ventrik::servidor::Evento::demasiadas_por_ip:
        clave = ventrik::Clave::servidor_demasiadas_por_ip;
        break;
      case ventrik::servidor::Evento::lleno:
        clave = ventrik::Clave::servidor_lleno;
        break;
      case ventrik::servidor::Evento::demasiado_rapido:
        clave = ventrik::Clave::servidor_demasiado_rapido;
        break;
      case ventrik::servidor::Evento::bloqueada:
        clave = ventrik::Clave::servidor_bloqueada;
        break;
      case ventrik::servidor::Evento::rechazo_bloqueada:
        clave = ventrik::Clave::servidor_rechazo_bloqueada;
        break;
      case ventrik::servidor::Evento::ocupado:
        clave = ventrik::Clave::servidor_ocupado;
        break;
    }
    registro.agregar(texto(clave) + " " + ventrik::red::a_texto(origen));
  };
  ventrik::servidor::Servidor servidor(*cargada.identidad, opciones, avisar);
  if (!servidor.escuchar()) {
    escribir_linea(texto(ventrik::Clave::servidor_error_puerto) + " " + std::to_string(puerto));
    return 1;
  }
  escribir_linea(texto(ventrik::Clave::servidor_puerto) + " " + std::to_string(servidor.puerto()));
  escribir_linea(texto(ventrik::Clave::servidor_salir));
  SetConsoleCtrlHandler(al_cortar, TRUE);
  servidor.correr(seguir_corriendo);
  registro.agregar(texto(ventrik::Clave::servidor_cerrando));
  return 0;
}
