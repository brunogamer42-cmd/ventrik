#include "client/guardados.h"

#include <algorithm>

#include <windows.h>

#include "common/archivos.h"
#include "common/protocol/bytes.h"
#include "common/texto.h"

namespace ventrik::cliente {
namespace {
constexpr std::uint32_t kMarca = 0x52504B56;
constexpr std::uint16_t kVersion = 1;

bool nombre_opcional(const std::string& nombre) { return nombre.empty() || es_nombre_valido(nombre); }

bool es_valido(const ServidorGuardado& servidor) {
  return servidor.direccion.ipv4 != 0 && servidor.direccion.puerto != 0 && es_nombre_valido(servidor.nombre) &&
         nombre_opcional(servidor.nombre_canal);
}

bool son_validos(const Guardados& guardados) {
  const auto& servidores = guardados.servidores;
  for (auto servidor = servidores.begin(); servidor != servidores.end(); ++servidor) {
    if (!es_valido(*servidor) ||
        std::any_of(servidores.begin(), servidor, [&servidor](const ServidorGuardado& otro) {
          return otro.direccion == servidor->direccion;
        })) {
      return false;
    }
  }
  return servidores.size() <= kMaximoServidoresGuardados && nombre_opcional(guardados.nombre) &&
         es_tecla_valida(guardados.tecla_hablar);
}

std::wstring ruta() {
  const std::wstring carpeta = carpeta_de_datos(L"Programa");
  return carpeta.empty() ? carpeta : carpeta + L"\\programa.dat";
}

}

LecturaGuardados leer_guardados(gsl::span<const std::uint8_t> datos, Guardados& guardados) {
  protocolo::Lector lector(datos);
  if (lector.leer_u32() != kMarca) {
    return LecturaGuardados::danados;
  }
  const std::uint16_t version = lector.leer_u16();
  if (lector.ok() && version > kVersion) {
    return LecturaGuardados::version_nueva;
  }
  Guardados leidos;
  leidos.tecla_hablar = lector.leer_u8();
  leidos.nombre = lector.leer_texto(kMaximoBytesNombre);
  const std::size_t cantidad = lector.leer_u8();
  for (std::size_t i = 0; i < cantidad && lector.ok(); ++i) {
    ServidorGuardado servidor;
    servidor.direccion.ipv4 = lector.leer_u32();
    servidor.direccion.puerto = lector.leer_u16();
    const auto huella = lector.leer_bytes(servidor.huella.size());
    if (lector.ok()) {
      std::copy(huella.begin(), huella.end(), servidor.huella.begin());
    }
    servidor.nombre = lector.leer_texto(kMaximoBytesNombre);
    servidor.canal = lector.leer_u32();
    servidor.nombre_canal = lector.leer_texto(kMaximoBytesNombre);
    servidor.ultima_vez_ms = lector.leer_u64();
    leidos.servidores.push_back(std::move(servidor));
  }
  if (version != kVersion || !lector.terminado() || !son_validos(leidos)) {
    return LecturaGuardados::danados;
  }
  guardados = std::move(leidos);
  return LecturaGuardados::leidos;
}

std::vector<std::uint8_t> armar_guardados(const Guardados& guardados) {
  if (!son_validos(guardados)) {
    return {};
  }
  protocolo::Escritor escritor;
  escritor.escribir_u32(kMarca);
  escritor.escribir_u16(kVersion);
  escritor.escribir_u8(guardados.tecla_hablar);
  escritor.escribir_texto(guardados.nombre, kMaximoBytesNombre);
  escritor.escribir_u8(static_cast<std::uint8_t>(guardados.servidores.size()));
  for (const ServidorGuardado& servidor : guardados.servidores) {
    escritor.escribir_u32(servidor.direccion.ipv4);
    escritor.escribir_u16(servidor.direccion.puerto);
    escritor.escribir_bytes(servidor.huella);
    escritor.escribir_texto(servidor.nombre, kMaximoBytesNombre);
    escritor.escribir_u32(servidor.canal);
    escritor.escribir_texto(servidor.nombre_canal, kMaximoBytesNombre);
    escritor.escribir_u64(servidor.ultima_vez_ms);
  }
  return escritor.datos();
}

bool es_tecla_valida(std::uint8_t tecla) {
  return tecla == 0 || (tecla >= VK_MBUTTON && tecla != VK_ESCAPE && (tecla < VK_SHIFT || tecla > VK_MENU) &&
                        tecla != 0xFF);
}

std::uint8_t tecla_para_hablar(std::uint8_t guardada) {
  return guardada == 0 ? kTeclaHablarPorDefecto : guardada;
}

bool recordar(Guardados& guardados, ServidorGuardado servidor) {
  if (!es_valido(servidor)) {
    return false;
  }
  auto& servidores = guardados.servidores;
  servidores.erase(std::remove_if(servidores.begin(), servidores.end(),
                                  [&servidor](const ServidorGuardado& otro) {
                                    return otro.direccion == servidor.direccion;
                                  }),
                   servidores.end());
  servidores.insert(servidores.begin(), std::move(servidor));
  if (servidores.size() > kMaximoServidoresGuardados) {
    servidores.resize(kMaximoServidoresGuardados);
  }
  return true;
}

LecturaGuardados cargar_guardados(Guardados& guardados) {
  const std::wstring archivo = ruta();
  if (archivo.empty()) {
    return LecturaGuardados::error;
  }
  borrar_temporales_de(archivo);
  cifrado::Secreto contenido;
  switch (leer_archivo(archivo, kMaximoArchivoGuardados, contenido)) {
    case LecturaArchivo::leido:
      return leer_guardados(contenido.bytes(), guardados);
    case LecturaArchivo::no_existe:
      return LecturaGuardados::leidos;
    case LecturaArchivo::demasiado_grande:
    case LecturaArchivo::error:
      break;
  }
  return LecturaGuardados::error;
}

bool guardar_guardados(const Guardados& guardados) {
  Guardados en_disco;
  const LecturaGuardados lectura = cargar_guardados(en_disco);
  const std::vector<std::uint8_t> datos = armar_guardados(guardados);
  return (lectura == LecturaGuardados::leidos || lectura == LecturaGuardados::danados) && !datos.empty() &&
         crear_carpetas(carpeta_de_datos(L"Programa")) && escribir_archivo_completo(ruta(), datos);
}

}
