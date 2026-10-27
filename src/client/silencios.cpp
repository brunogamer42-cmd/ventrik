#include "client/silencios.h"

#include <algorithm>
#include <array>
#include <string>

#include "common/protocol/pedidos.h"
#include "common/texto.h"

namespace ventrik::cliente {
namespace {
bool termina(std::string_view resto) {
  if (resto.empty()) {
    return true;
  }
  const auto letra = static_cast<unsigned char>(resto.front());
  return letra < 0x80 && letra != '_' && !(letra >= '0' && letra <= '9') && !(letra >= 'a' && letra <= 'z') &&
         !(letra >= 'A' && letra <= 'Z');
}

std::size_t letras(std::string_view texto) {
  return static_cast<std::size_t>(std::count_if(
      texto.begin(), texto.end(), [](char byte) { return (static_cast<unsigned char>(byte) & 0xC0) != 0x80; }));
}

std::string_view primeras(std::string_view texto, std::size_t cuantas) {
  std::size_t fin = 0;
  for (std::size_t vistas = 0; fin < texto.size(); ++fin) {
    if ((static_cast<unsigned char>(texto[fin]) & 0xC0) != 0x80 && vistas++ == cuantas) {
      break;
    }
  }
  return texto.substr(0, fin);
}

bool es_comun(const AjusteDeAvisos& ajuste) {
  return ajuste.silenciado_hasta == 0 && ajuste.aun_con_mencion && ajuste.avisar == Avisar::como_el_servidor;
}

bool menor(const AvisosDeCanal& uno, std::uint32_t canal) { return uno.canal < canal; }

}

bool silenciado(const AjusteDeAvisos& ajuste, std::uint64_t ahora) { return ajuste.silenciado_hasta > ahora; }

Mencion mencion_en(std::string_view texto, std::string_view nombre) {
  if (texto.find('@') == std::string_view::npos) {
    return Mencion::ninguna;
  }
  const std::wstring buscado = nombre_para_comparar(nombre);
  const std::array<std::string_view, 3> generales = {"todos", "aqu\xC3\xAD", "aqui"};
  Mencion hallada = Mencion::ninguna;
  std::size_t arroba = texto.find('@');
  for (std::size_t vistas = 0; arroba != std::string_view::npos && vistas < kArrobasPorMensaje;
       arroba = texto.find('@', arroba + 1), ++vistas) {
    const std::string_view resto = texto.substr(arroba + 1);
    const auto nombra = [&resto](std::string_view palabra, const std::wstring& comparada) {
      const std::string_view pedazo = primeras(resto, letras(palabra));
      return !comparada.empty() && termina(resto.substr(pedazo.size())) && nombre_para_comparar(pedazo) == comparada;
    };
    if (nombra(nombre, buscado)) {
      return Mencion::directa;
    }
    if (std::any_of(generales.begin(), generales.end(),
                    [&nombra](std::string_view palabra) { return nombra(palabra, nombre_para_comparar(palabra)); })) {
      hallada = Mencion::todos;
    }
  }
  return hallada;
}

AjusteDeAvisos ajuste_de(const AvisosDelServidor& avisos, std::uint32_t canal) {
  const auto propio = std::lower_bound(avisos.canales.begin(), avisos.canales.end(), canal, menor);
  return propio != avisos.canales.end() && propio->canal == canal ? propio->ajuste : AjusteDeAvisos{};
}

bool avisa(const AvisosDelServidor& avisos, std::uint32_t canal, Mencion mencion, std::uint64_t ahora) {
  const bool nombrado = mencion == Mencion::directa || (mencion == Mencion::todos && avisos.contar_todos);
  const AjusteDeAvisos propio = ajuste_de(avisos, canal);
  const AjusteDeAvisos& manda =
      silenciado(propio, ahora) || propio.avisar != Avisar::como_el_servidor ? propio : avisos.servidor;
  if (silenciado(manda, ahora)) {
    return nombrado && manda.aun_con_mencion;
  }
  return manda.avisar == Avisar::todos || (manda.avisar == Avisar::menciones && nombrado);
}

void cambiar_canal(AvisosDelServidor& avisos, std::uint32_t canal, const AjusteDeAvisos& ajuste) {
  auto& canales = avisos.canales;
  const auto lugar = std::lower_bound(canales.begin(), canales.end(), canal, menor);
  const bool esta = lugar != canales.end() && lugar->canal == canal;
  if (es_comun(ajuste)) {
    if (esta) {
      canales.erase(lugar);
    }
  } else if (esta) {
    lugar->ajuste = ajuste;
  } else if (canal != 0 && canales.size() < protocolo::kMaximoCanales) {
    canales.insert(lugar, {canal, ajuste});
  }
}

void normalizar(AvisosDelServidor& avisos, std::uint64_t ahora) {
  const auto vencer = [ahora](AjusteDeAvisos& ajuste) {
    ajuste.silenciado_hasta = silenciado(ajuste, ahora) ? ajuste.silenciado_hasta : 0;
  };
  vencer(avisos.servidor);
  for (auto& canal : avisos.canales) {
    vencer(canal.ajuste);
  }
  avisos.canales.erase(std::remove_if(avisos.canales.begin(), avisos.canales.end(),
                                      [](const AvisosDeCanal& canal) { return es_comun(canal.ajuste); }),
                       avisos.canales.end());
}

bool son_validos(const AvisosDelServidor& avisos) {
  const auto valido = [](const AjusteDeAvisos& ajuste) { return ajuste.avisar <= Avisar::nada; };
  const auto& canales = avisos.canales;
  for (std::size_t i = 0; i < canales.size(); ++i) {
    if (canales[i].canal == 0 || !valido(canales[i].ajuste) || (i > 0 && canales[i - 1].canal >= canales[i].canal)) {
      return false;
    }
  }
  return valido(avisos.servidor) && avisos.servidor.avisar != Avisar::como_el_servidor &&
         canales.size() <= protocolo::kMaximoCanales;
}

}
