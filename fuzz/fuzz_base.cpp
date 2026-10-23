#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include <windows.h>

#include <gsl/gsl>

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

std::wstring archivo_de_prueba() {
  wchar_t carpeta[MAX_PATH] = {};
  exigir(GetTempPathW(MAX_PATH, carpeta) > 0);
  return std::wstring(carpeta) + L"ventrik_fuzz_base_" + std::to_wstring(GetCurrentProcessId()) +
         L".db";
}

void revisar(const ventrik::protocolo::Historial& historial, std::uint64_t antes_de) {
  exigir(historial.mensajes.size() <= ventrik::protocolo::kMaximoMensajesEnHistorial);
  std::uint64_t anterior = antes_de == 0 ? UINT64_MAX : antes_de;
  for (const auto& mensaje : historial.mensajes) {
    exigir(mensaje.id < anterior);
    exigir(ventrik::es_nombre_valido(mensaje.autor) && ventrik::es_chat_valido(mensaje.texto));
    anterior = mensaje.id;
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
  auto base = Base::abrir(archivo, "General");
  if (!base) {
    return 0;
  }
  const auto canales = base->canales();
  exigir(canales.size() <= ventrik::protocolo::kMaximoCanalesEnLista);
  for (const auto& canal : canales) {
    exigir(canal.id > 0 && ventrik::es_nombre_valido(canal.nombre));
    const auto historial = base->historial(canal.id, 0, 65536);
    revisar(historial, 0);
    if (!historial.mensajes.empty()) {
      revisar(base->historial(canal.id, historial.mensajes.back().id, 65536),
              historial.mensajes.back().id);
    }
  }
  base->retencion();
  base->crear_canal("Nuevo");
  base->guardar_mensaje(ventrik::servidor::kCanalGeneral, "Ana", 1, "hola");
  base->borrar_viejos(UINT64_MAX / 2);
  return 0;
}
