#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>

#include <gsl/gsl>
#include <sqlite3.h>

#include "common/archivos.h"
#include "common/cifrado/inicio.h"
#include "common/cifrado/sobre.h"
#include "common/texto.h"
#include "server/base.h"

using ventrik::servidor::Base;

namespace {
std::unique_ptr<ventrik::cifrado::Inicio> cifrado;

void exigir(bool condicion) {
  if (!condicion) {
    std::abort();
  }
}

bool esta_sana(const std::wstring& archivo) {
  sqlite3* base = nullptr;
  sqlite3_stmt* consulta = nullptr;
  const bool sana = sqlite3_open16(archivo.c_str(), &base) == SQLITE_OK &&
                    sqlite3_prepare_v2(base, "PRAGMA integrity_check", -1, &consulta, nullptr) == SQLITE_OK &&
                    sqlite3_step(consulta) == SQLITE_ROW && sqlite3_column_text(consulta, 0) != nullptr &&
                    std::string_view(reinterpret_cast<const char*>(sqlite3_column_text(consulta, 0))) == "ok";
  sqlite3_finalize(consulta);
  sqlite3_close(base);
  return sana;
}

std::int64_t contar(const std::wstring& archivo, const char* sql) {
  sqlite3* base = nullptr;
  sqlite3_stmt* consulta = nullptr;
  const bool bien = sqlite3_open16(archivo.c_str(), &base) == SQLITE_OK &&
                    sqlite3_prepare_v2(base, sql, -1, &consulta, nullptr) == SQLITE_OK &&
                    sqlite3_step(consulta) == SQLITE_ROW;
  const std::int64_t cuantos = bien ? sqlite3_column_int64(consulta, 0) : -1;
  sqlite3_finalize(consulta);
  sqlite3_close(base);
  return cuantos;
}

bool ejecutar_en(const std::wstring& archivo, const char* sql) {
  sqlite3* base = nullptr;
  const bool bien = sqlite3_open16(archivo.c_str(), &base) == SQLITE_OK &&
                    sqlite3_exec(base, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
  sqlite3_close(base);
  return bien;
}

ventrik::cifrado::Secreto clave_fija() {
  ventrik::cifrado::Secreto clave(ventrik::cifrado::kBytesClaveSimetrica);
  std::fill(clave.bytes().begin(), clave.bytes().end(), std::uint8_t{7});
  return clave;
}

std::vector<ventrik::protocolo::Historial> historiales(Base& base, const std::vector<std::uint32_t>& canales) {
  std::vector<ventrik::protocolo::Historial> lista;
  for (const std::uint32_t canal : canales) {
    lista.push_back(base.historial({canal, 0, ventrik::protocolo::Sentido::viejos, 256},
                                   ventrik::protocolo::kMaximoCuerpo - 64));
  }
  return lista;
}

bool iguales(const std::vector<ventrik::protocolo::Historial>& unos,
             const std::vector<ventrik::protocolo::Historial>& otros) {
  return std::equal(unos.begin(), unos.end(), otros.begin(), otros.end(), [](const auto& uno, const auto& otro) {
    return uno.hay_mas == otro.hay_mas &&
           std::equal(uno.mensajes.begin(), uno.mensajes.end(), otro.mensajes.begin(), otro.mensajes.end(),
                      [](const auto& a, const auto& b) {
                        return a.id == b.id && a.numero == b.numero && a.autor == b.autor &&
                               a.hora_ms == b.hora_ms && a.texto == b.texto;
                      });
  });
}

void revisar_cifrado(Base& base, const std::wstring& archivo, bool sana) {
  const bool sin_trucos =
      sana && contar(archivo, "SELECT COUNT(*) FROM sqlite_master WHERE type IN ('trigger', 'view')") == 0 &&
      ejecutar_en(archivo, "VACUUM");
  std::vector<std::uint32_t> canales;
  for (const auto& canal : base.canales()) {
    canales.push_back(canal.id);
  }
  const auto antes = historiales(base, canales);
  if (base.cifrar_historial(true)) {
    exigir(!sin_trucos || (base.historial_cifrado() && iguales(antes, historiales(base, canales)) &&
                           contar(archivo, "SELECT COUNT(*) FROM mensajes WHERE sellado IS NULL") == 0));
    if (sin_trucos && ejecutar_en(archivo, "UPDATE mensajes SET numero = numero + 2")) {
      for (const auto& historial : historiales(base, canales)) {
        exigir(historial.mensajes.empty());
      }
    }
  }
  base.cifrar_historial(false);
}

std::wstring archivo_de_prueba() {
  wchar_t carpeta[MAX_PATH] = {};
  exigir(GetTempPathW(MAX_PATH, carpeta) > 0);
  return std::wstring(carpeta) + L"ventrik_fuzz_base_" + std::to_wstring(GetCurrentProcessId()) +
         L".db";
}

void revisar(Base& base, const ventrik::protocolo::PedirHistorial& pedido) {
  const auto historial = base.historial(pedido, ventrik::protocolo::kMaximoCuerpo - 64);
  exigir(historial.mensajes.size() <= pedido.cantidad);
  exigir(historial.canal == pedido.canal && historial.desde == pedido.desde && historial.sentido == pedido.sentido);
  const bool nuevos = pedido.sentido == ventrik::protocolo::Sentido::nuevos;
  std::uint64_t anterior = !nuevos && pedido.desde == 0 ? UINT64_MAX : pedido.desde;
  for (const auto& mensaje : historial.mensajes) {
    exigir(nuevos ? mensaje.id > anterior : mensaje.id < anterior);
    exigir(ventrik::es_nombre_valido(mensaje.autor) && ventrik::es_chat_valido(mensaje.texto));
    anterior = mensaje.id;
  }
  std::vector<std::uint8_t> marco;
  exigir(ventrik::protocolo::armar_mensaje(historial, marco));
  if (!historial.mensajes.empty() && pedido.desde == 0) {
    revisar(base, {pedido.canal, historial.mensajes.back().id, pedido.sentido, pedido.cantidad});
  }
}
}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  cifrado = std::make_unique<ventrik::cifrado::Inicio>();
  exigir(cifrado->ok());
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* datos, std::size_t largo) {
  static const std::wstring archivo = archivo_de_prueba();
  ventrik::borrar_temporales_de(archivo);
  for (const wchar_t* sufijo : {L"-journal", L"-wal", L"-shm"}) {
    DeleteFileW((archivo + sufijo).c_str());
  }
  if (!ventrik::escribir_archivo_completo(archivo, gsl::make_span(datos, largo))) {
    return 0;
  }
  const bool sana = esta_sana(archivo);
  auto base = Base::abrir(archivo, "General", clave_fija());
  if (!base) {
    return 0;
  }
  const auto canales = base->canales();
  exigir(canales.size() <= ventrik::protocolo::kMaximoCanalesEnLista);
  std::vector<std::uint8_t> marco;
  for (const auto& canal : canales) {
    exigir(canal.id > 0 && ventrik::es_nombre_valido(canal.nombre) &&
           ventrik::protocolo::es_tipo_valido(canal.tipo, canal.solo_admins));
    exigir(ventrik::protocolo::armar_mensaje(ventrik::protocolo::CanalCreado{canal}, marco));
    revisar(*base, {canal.id, 0, ventrik::protocolo::Sentido::viejos, 256});
    revisar(*base, {canal.id, 0, ventrik::protocolo::Sentido::nuevos, 7});
  }
  base->retencion();
  base->historial_cifrado();
  const bool tanda = base->empezar_tanda();
  base->crear_canal("Nuevo");
  base->crear_canal("Texto", ventrik::protocolo::TipoCanal::texto, true);
  base->cambiar_canal(ventrik::servidor::kCanalGeneral, "General", false);
  base->guardar_mensaje(ventrik::servidor::kCanalGeneral, 1, "Ana", 1, "hola");
  base->guardar_mensaje(99, 1, "Ana", 1, "hola");
  base->guardar_mensaje(ventrik::servidor::kCanalGeneral, 2, "Ana", 1, "hola", false);
  if (tanda) {
    base->confirmar_tanda();
  }
  base->asentar();
  const auto despues = base->canales();
  for (const auto& canal : canales) {
    for (const auto& nuevo : despues) {
      exigir(!sana || nuevo.id != canal.id || nuevo.contador >= canal.contador);
    }
  }
  const auto avisos_validos = [&marco](const std::vector<ventrik::protocolo::AvisoDeSeguridad>& avisos,
                                       std::size_t maximo) {
    exigir(avisos.size() <= maximo);
    for (const auto& aviso : avisos) {
      exigir(ventrik::protocolo::armar_mensaje(aviso, marco));
    }
  };
  avisos_validos(base->registro(SIZE_MAX), ventrik::servidor::kMaximoRegistroPorTipo * 8);
  if (const auto usuarios = base->usuarios(UINT64_MAX / 4)) {
    exigir(usuarios->size() <= ventrik::servidor::kMaximoUsuariosGuardados);
    for (const auto& usuario : *usuarios) {
      exigir(usuario.clave[0] == 0x04 && ventrik::es_nombre_valido(usuario.nombre));
      ventrik::servidor::Equipo equipo{};
      base->conoce_equipo(usuario.clave, equipo);
      const auto pendientes = base->avisos_para(usuario.clave, equipo);
      avisos_validos(pendientes.lista, ventrik::servidor::kMaximoAvisosPorUsuario);
      base->entregados(usuario.clave, equipo, pendientes.hasta);
      base->es_de_confianza(usuario.clave, equipo);
    }
  }
  if (const auto segundos = base->segundos_pasos()) {
    exigir(segundos->size() <= ventrik::servidor::kMaximoUsuariosGuardados);
    for (const auto& segundo : *segundos) {
      exigir(segundo.fallas <= ventrik::servidor::kFallasDeSegundoPaso);
      exigir(segundo.sellado.empty() || segundo.sellado.size() == ventrik::servidor::kBytesSegundoPasoSellado);
    }
  }
  if (const auto llaves = base->llaves()) {
    exigir(llaves->size() <= ventrik::servidor::kMaximoUsuariosGuardados);
    for (const auto& suyas : *llaves) {
      exigir(suyas.llaves.size() <= ventrik::servidor::kMaximoLlavesPorUsuario);
      base->guardar_llaves(suyas);
    }
  }
  std::optional<ventrik::Seguridad> seguridad;
  if (base->leer_seguridad(seguridad) && seguridad) {
    exigir(ventrik::es_valida(*seguridad));
    base->guardar_seguridad(*seguridad);
  }
  ventrik::servidor::UsuarioGuardado nuevo;
  nuevo.clave.fill(4);
  nuevo.nombre = "Ana";
  nuevo.visto_ms = 1;
  base->guardar_usuario(nuevo);
  base->anotar_equipo(nuevo.clave, {}, 1, true);
  base->es_de_confianza(nuevo.clave, {});
  base->confiar_equipos(nuevo.clave);
  base->anotar({ventrik::protocolo::Seguridad::canal_borrado, 1, "Ana", "Juegos"});
  base->guardar_aviso(nuevo.clave, std::nullopt, {ventrik::protocolo::Seguridad::canal_borrado, 1, "Ana", "Juegos"});
  avisos_validos(base->avisos_para(nuevo.clave, {}).lista, ventrik::servidor::kMaximoAvisosPorUsuario);
  base->guardar_segundo_paso({nuevo.clave, std::vector<std::uint8_t>(ventrik::servidor::kBytesSegundoPasoSellado, 3), 9, 1, 1});
  base->guardar_llaves({nuevo.clave, 3, {{nuevo.clave, 7, true}}});
  base->guardar_texto("nombre", "Taberna");
  base->texto_guardado("nombre");
  base->quitar_dueno();
  base->liberar_nombre("Ana");
  base->quitar_segundo_paso(nuevo.clave);
  base->borrar_usuarios({nuevo.clave});
  base->borrar_viejos(UINT64_MAX / 2);
  revisar_cifrado(*base, archivo, sana);
  return 0;
}
