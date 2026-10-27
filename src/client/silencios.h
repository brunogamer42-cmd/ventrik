#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace ventrik::cliente {
constexpr std::uint64_t kHastaQueLoActive = std::numeric_limits<std::uint64_t>::max();
constexpr std::size_t kArrobasPorMensaje = 32;

enum class Avisar : std::uint8_t { como_el_servidor, todos, menciones, nada };
enum class Mencion : std::uint8_t { ninguna, todos, directa };

struct AjusteDeAvisos {
  std::uint64_t silenciado_hasta = 0;
  bool aun_con_mencion = true;
  Avisar avisar = Avisar::como_el_servidor;
};

struct AvisosDeCanal {
  std::uint32_t canal = 0;
  AjusteDeAvisos ajuste;
};

struct AvisosDelServidor {
  AjusteDeAvisos servidor{0, true, Avisar::menciones};
  bool contar_todos = false;
  bool sonidos_de_voz = true;
  std::vector<AvisosDeCanal> canales;
};

bool silenciado(const AjusteDeAvisos& ajuste, std::uint64_t ahora);
Mencion mencion_en(std::string_view texto, std::string_view nombre);
AjusteDeAvisos ajuste_de(const AvisosDelServidor& avisos, std::uint32_t canal);
bool avisa(const AvisosDelServidor& avisos, std::uint32_t canal, Mencion mencion, std::uint64_t ahora);
void cambiar_canal(AvisosDelServidor& avisos, std::uint32_t canal, const AjusteDeAvisos& ajuste);
void normalizar(AvisosDelServidor& avisos, std::uint64_t ahora);
bool son_validos(const AvisosDelServidor& avisos);

}
