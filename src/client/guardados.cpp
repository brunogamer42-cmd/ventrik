#include "client/guardados.h"

#include <algorithm>
#include <array>

#include <windows.h>

#include "common/archivos.h"
#include "common/protocol/bytes.h"
#include "common/texto.h"

namespace ventrik::cliente {
namespace {
constexpr std::uint32_t kMarca = 0x52504B56;
constexpr std::uint16_t kVersion = 7;
constexpr std::uint16_t kVersionConLeidos = 2;
constexpr std::uint16_t kVersionConModo = 3;
constexpr std::uint16_t kVersionConSilencio = 4;
constexpr std::uint16_t kVersionConPersonas = 5;
constexpr std::uint16_t kVersionConAvisos = 6;
constexpr std::uint16_t kVersionConAjustes = 7;
constexpr std::size_t kTexto = 2 + kMaximoBytesNombre;
constexpr std::size_t kAjuste = 10;
static_assert(kMaximoArchivoGuardados >=
              13 + kTexto + kMaximoServidoresGuardados *
                                (6 + std::tuple_size<cifrado::Huella::Bytes>::value + 2 * kTexto + 14 +
                                 kMaximoLeidosPorServidor * 8 + kMaximoPersonasPorServidor * (kTexto + 2) +
                                 kAjuste + 3 + protocolo::kMaximoCanales * (4 + kAjuste)) +
                  2 + kMaximoAjustes * 4 + kMaximoBloqueDeAjustes);

struct MaximoDeAjuste {
  Ajuste ajuste;
  std::uint8_t maximo;
};

constexpr std::array<MaximoDeAjuste, 4> kMaximos = {{{Ajuste::ruido, static_cast<std::uint8_t>(Ruido::alta)},
                                                     {Ajuste::eco, static_cast<std::uint8_t>(Eco::prendido)},
                                                     {Ajuste::volumen_automatico, 1},
                                                     {Ajuste::bajar_otros, static_cast<std::uint8_t>(Bajar::mucho)}}};

bool son_validos(const std::map<std::uint16_t, std::vector<std::uint8_t>>& ajustes) {
  std::size_t total = 0;
  for (const auto& [clave, valor] : ajustes) {
    total += valor.size();
    const std::uint16_t esta = clave;
    const auto conocido = std::find_if(kMaximos.begin(), kMaximos.end(), [esta](const MaximoDeAjuste& m) {
      return static_cast<std::uint16_t>(m.ajuste) == esta;
    });
    if (valor.size() > kMaximoLargoDeAjuste ||
        (conocido != kMaximos.end() && (valor.size() != 1 || valor[0] > conocido->maximo))) {
      return false;
    }
  }
  return ajustes.size() <= kMaximoAjustes && total <= kMaximoBloqueDeAjustes;
}

bool nombre_opcional(const std::string& nombre) { return nombre.empty() || es_nombre_valido(nombre); }

bool leer_ajuste(protocolo::Lector& lector, AjusteDeAvisos& ajuste) {
  ajuste.silenciado_hasta = lector.leer_u64();
  const std::uint8_t aun = lector.leer_u8();
  const std::uint8_t avisar = lector.leer_u8();
  ajuste.aun_con_mencion = aun == 1;
  ajuste.avisar = static_cast<Avisar>(avisar);
  return aun <= 1 && avisar <= static_cast<std::uint8_t>(Avisar::nada);
}

void escribir_ajuste(protocolo::Escritor& escritor, const AjusteDeAvisos& ajuste) {
  escritor.escribir_u64(ajuste.silenciado_hasta);
  escritor.escribir_u8(ajuste.aun_con_mencion ? 1 : 0);
  escritor.escribir_u8(static_cast<std::uint8_t>(ajuste.avisar));
}

bool leer_avisos(protocolo::Lector& lector, AvisosDelServidor& avisos) {
  bool bien = leer_ajuste(lector, avisos.servidor);
  const std::uint8_t contar_todos = lector.leer_u8();
  const std::uint8_t sonidos = lector.leer_u8();
  avisos.contar_todos = contar_todos == 1;
  avisos.sonidos_de_voz = sonidos == 1;
  const std::size_t canales = lector.leer_u8();
  for (std::size_t i = 0; i < canales && lector.ok(); ++i) {
    AvisosDeCanal canal;
    canal.canal = lector.leer_u32();
    bien = leer_ajuste(lector, canal.ajuste) && bien;
    avisos.canales.push_back(canal);
  }
  return bien && contar_todos <= 1 && sonidos <= 1;
}

bool es_valida(const PersonaGuardada& persona) {
  return es_nombre_valido(persona.nombre) && persona.volumen <= kVolumenMaximo;
}

bool es_valido(const ServidorGuardado& servidor) {
  const auto& leidos = servidor.leidos;
  for (std::size_t i = 0; i < leidos.size(); ++i) {
    if (leidos[i].canal == 0 || (i > 0 && leidos[i - 1].canal >= leidos[i].canal)) {
      return false;
    }
  }
  const auto& personas = servidor.personas;
  for (auto persona = personas.begin(); persona != personas.end(); ++persona) {
    if (!es_valida(*persona) || std::any_of(personas.begin(), persona, [&persona](const PersonaGuardada& otra) {
          return otra.nombre == persona->nombre;
        })) {
      return false;
    }
  }
  return servidor.direccion.ipv4 != 0 && servidor.direccion.puerto != 0 && es_nombre_valido(servidor.nombre) &&
         nombre_opcional(servidor.nombre_canal) && leidos.size() <= kMaximoLeidosPorServidor &&
         personas.size() <= kMaximoPersonasPorServidor && son_validos(servidor.avisos);
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
         son_validos(guardados.ajustes) &&
         es_tecla_valida(guardados.tecla_hablar) && guardados.sensibilidad <= kSensibilidadMaxima &&
         protocolo::es_silencio_valido(guardados.silencio) && guardados.volumen_microfono <= kVolumenMaximo &&
         guardados.volumen_salida <= kVolumenMaximo;
}

std::wstring ruta(const std::wstring& carpeta) {
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
  if (version >= kVersionConModo) {
    const std::uint8_t modo = lector.leer_u8();
    leidos.por_voz = modo == 1;
    leidos.sensibilidad = lector.leer_u8();
    if (modo > 1) {
      return LecturaGuardados::danados;
    }
  }
  if (version >= kVersionConSilencio) {
    leidos.silencio = lector.leer_u8();
    leidos.volumen_microfono = lector.leer_u8();
    leidos.volumen_salida = lector.leer_u8();
  }
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
    servidor.texto = lector.leer_u32();
    servidor.nombre_canal = lector.leer_texto(kMaximoBytesNombre);
    servidor.ultima_vez_ms = lector.leer_u64();
    const std::size_t cuantos = version >= kVersionConLeidos ? lector.leer_u8() : 0;
    for (std::size_t j = 0; j < cuantos && lector.ok(); ++j) {
      const std::uint32_t canal = lector.leer_u32();
      servidor.leidos.push_back({canal, lector.leer_u32()});
    }
    const std::size_t personas = version >= kVersionConPersonas ? lector.leer_u8() : 0;
    for (std::size_t j = 0; j < personas && lector.ok(); ++j) {
      PersonaGuardada persona;
      persona.nombre = lector.leer_texto(kMaximoBytesNombre);
      persona.volumen = lector.leer_u8();
      const std::uint8_t silenciada = lector.leer_u8();
      persona.silenciada = silenciada == 1;
      if (silenciada > 1) {
        return LecturaGuardados::danados;
      }
      servidor.personas.push_back(std::move(persona));
    }
    if (version >= kVersionConAvisos && !leer_avisos(lector, servidor.avisos)) {
      return LecturaGuardados::danados;
    }
    leidos.servidores.push_back(std::move(servidor));
  }
  const std::size_t ajustes = version >= kVersionConAjustes ? lector.leer_u16() : 0;
  if (ajustes > kMaximoAjustes) {
    return LecturaGuardados::danados;
  }
  for (std::size_t i = 0; i < ajustes && lector.ok(); ++i) {
    const std::uint16_t clave = lector.leer_u16();
    const auto valor = lector.leer_bytes(lector.leer_u16());
    if (lector.ok() && !leidos.ajustes.empty() && leidos.ajustes.rbegin()->first >= clave) {
      return LecturaGuardados::danados;
    }
    leidos.ajustes.emplace(clave, std::vector<std::uint8_t>(valor.begin(), valor.end()));
  }
  if (version == 0 || !lector.terminado() || !son_validos(leidos)) {
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
  escritor.escribir_u8(guardados.por_voz ? 1 : 0);
  escritor.escribir_u8(guardados.sensibilidad);
  escritor.escribir_u8(guardados.silencio);
  escritor.escribir_u8(guardados.volumen_microfono);
  escritor.escribir_u8(guardados.volumen_salida);
  escritor.escribir_texto(guardados.nombre, kMaximoBytesNombre);
  escritor.escribir_u8(static_cast<std::uint8_t>(guardados.servidores.size()));
  for (const ServidorGuardado& servidor : guardados.servidores) {
    escritor.escribir_u32(servidor.direccion.ipv4);
    escritor.escribir_u16(servidor.direccion.puerto);
    escritor.escribir_bytes(servidor.huella);
    escritor.escribir_texto(servidor.nombre, kMaximoBytesNombre);
    escritor.escribir_u32(servidor.texto);
    escritor.escribir_texto(servidor.nombre_canal, kMaximoBytesNombre);
    escritor.escribir_u64(servidor.ultima_vez_ms);
    escritor.escribir_u8(static_cast<std::uint8_t>(servidor.leidos.size()));
    for (const Leido& leido : servidor.leidos) {
      escritor.escribir_u32(leido.canal);
      escritor.escribir_u32(leido.numero);
    }
    escritor.escribir_u8(static_cast<std::uint8_t>(servidor.personas.size()));
    for (const PersonaGuardada& persona : servidor.personas) {
      escritor.escribir_texto(persona.nombre, kMaximoBytesNombre);
      escritor.escribir_u8(persona.volumen);
      escritor.escribir_u8(persona.silenciada ? 1 : 0);
    }
    escribir_ajuste(escritor, servidor.avisos.servidor);
    escritor.escribir_u8(servidor.avisos.contar_todos ? 1 : 0);
    escritor.escribir_u8(servidor.avisos.sonidos_de_voz ? 1 : 0);
    escritor.escribir_u8(static_cast<std::uint8_t>(servidor.avisos.canales.size()));
    for (const AvisosDeCanal& canal : servidor.avisos.canales) {
      escritor.escribir_u32(canal.canal);
      escribir_ajuste(escritor, canal.ajuste);
    }
  }
  escritor.escribir_u16(static_cast<std::uint16_t>(guardados.ajustes.size()));
  for (const auto& [clave, valor] : guardados.ajustes) {
    escritor.escribir_u16(clave);
    escritor.escribir_u16(static_cast<std::uint16_t>(valor.size()));
    escritor.escribir_bytes(valor);
  }
  return escritor.datos();
}

std::optional<std::uint8_t> ajuste_u8(const Guardados& guardados, Ajuste ajuste) {
  const auto valor = guardados.ajustes.find(static_cast<std::uint16_t>(ajuste));
  return valor != guardados.ajustes.end() && valor->second.size() == 1 ? std::optional<std::uint8_t>(valor->second[0])
                                                                       : std::nullopt;
}

void poner_ajuste(Guardados& guardados, Ajuste ajuste, std::uint8_t valor) {
  guardados.ajustes[static_cast<std::uint16_t>(ajuste)] = {valor};
}

AjustesDeLimpieza limpieza_de(const Guardados& guardados, bool un_nucleo) {
  const auto valor = [&guardados](Ajuste ajuste, std::uint8_t de_fabrica) {
    return ajuste_u8(guardados, ajuste).value_or(de_fabrica);
  };
  return {static_cast<Ruido>(valor(Ajuste::ruido, un_nucleo ? 0 : 2)),
          static_cast<Eco>(valor(Ajuste::eco, un_nucleo ? 0 : 1)), valor(Ajuste::volumen_automatico, 1) == 1,
          static_cast<Bajar>(valor(Ajuste::bajar_otros, 0))};
}

bool es_tecla_valida(std::uint8_t tecla) {
  return tecla == 0 || (tecla >= VK_MBUTTON && tecla != VK_ESCAPE && (tecla < VK_SHIFT || tecla > VK_MENU) &&
                        tecla != 0xFF);
}

std::uint8_t tecla_para_hablar(std::uint8_t guardada) {
  return guardada == 0 ? kTeclaHablarPorDefecto : guardada;
}

ServidorGuardado* buscar_guardado(Guardados& guardados, const red::Direccion& direccion) {
  const auto encontrado = std::find_if(
      guardados.servidores.begin(), guardados.servidores.end(),
      [&direccion](const ServidorGuardado& servidor) { return servidor.direccion == direccion; });
  return encontrado == guardados.servidores.end() ? nullptr : &*encontrado;
}

bool recordar(Guardados& guardados, ServidorGuardado servidor) {
  if (!es_valido(servidor)) {
    return false;
  }
  auto& servidores = guardados.servidores;
  if (ServidorGuardado* anterior = buscar_guardado(guardados, servidor.direccion)) {
    if (servidor.leidos.empty() && anterior->huella == servidor.huella) {
      servidor.leidos = std::move(anterior->leidos);
    }
    servidores.erase(servidores.begin() + (anterior - servidores.data()));
  }
  servidores.insert(servidores.begin(), std::move(servidor));
  if (servidores.size() > kMaximoServidoresGuardados) {
    servidores.resize(kMaximoServidoresGuardados);
  }
  return true;
}

void ajustar_persona(std::vector<PersonaGuardada>& personas, PersonaGuardada persona) {
  if (!es_valida(persona)) {
    return;
  }
  personas.erase(std::remove_if(personas.begin(), personas.end(),
                                [&persona](const PersonaGuardada& otra) { return otra.nombre == persona.nombre; }),
                 personas.end());
  if (persona.volumen != kVolumenPorDefecto || persona.silenciada) {
    personas.insert(personas.begin(), std::move(persona));
  }
  if (personas.size() > kMaximoPersonasPorServidor) {
    personas.resize(kMaximoPersonasPorServidor);
  }
}

AjustesDeVoz ajustes_de_voz(const std::vector<PersonaGuardada>& personas,
                            const std::map<std::uint32_t, protocolo::UsuarioInfo>& usuarios) {
  AjustesDeVoz ajustes;
  for (auto usuario = usuarios.begin();
       usuario != usuarios.end() && ajustes.volumenes.size() < kMaximoPersonasPorServidor; ++usuario) {
    const auto persona = std::find_if(personas.begin(), personas.end(), [&usuario](const PersonaGuardada& guardada) {
      return guardada.nombre == usuario->second.nombre;
    });
    if (persona != personas.end()) {
      ajustes.volumenes.emplace_back(usuario->first, persona->silenciada ? 0 : persona->volumen);
      if (persona->silenciada) {
        ajustes.silenciados.push_back(usuario->first);
      }
    }
  }
  return ajustes;
}

LecturaGuardados cargar_guardados(Guardados& guardados, const std::wstring& carpeta) {
  const std::wstring archivo = ruta(carpeta);
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

bool guardar_guardados(const Guardados& guardados, const std::wstring& carpeta) {
  Guardados en_disco;
  const LecturaGuardados lectura = cargar_guardados(en_disco, carpeta);
  const std::vector<std::uint8_t> datos = armar_guardados(guardados);
  return (lectura == LecturaGuardados::leidos || lectura == LecturaGuardados::danados) && !datos.empty() &&
         crear_carpetas(carpeta) && escribir_archivo_completo(ruta(carpeta), datos);
}

}
