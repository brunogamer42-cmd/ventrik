#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include <windows.h>

#include <gsl/gsl>
#include <sqlite3.h>

#include "common/archivos.h"
#include "common/texto.h"
#include "server/base.h"

using ventrik::servidor::Base;

namespace {
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
  auto base = Base::abrir(archivo, "General");
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
    }
  }
  ventrik::servidor::UsuarioGuardado nuevo;
  nuevo.clave.fill(4);
  nuevo.nombre = "Ana";
  nuevo.visto_ms = 1;
  base->guardar_usuario(nuevo);
  base->anotar_equipo(nuevo.clave, {}, 1);
  base->anotar({ventrik::protocolo::Seguridad::canal_borrado, 1, "Ana", "Juegos"});
  base->guardar_aviso(nuevo.clave, std::nullopt, {ventrik::protocolo::Seguridad::canal_borrado, 1, "Ana", "Juegos"});
  avisos_validos(base->avisos_para(nuevo.clave, {}).lista, ventrik::servidor::kMaximoAvisosPorUsuario);
  base->guardar_texto("nombre", "Taberna");
  base->texto_guardado("nombre");
  base->quitar_dueno();
  base->liberar_nombre("Ana");
  base->borrar_usuarios({nuevo.clave});
  base->borrar_viejos(UINT64_MAX / 2);
  return 0;
}
