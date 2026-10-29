#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "client/silencios.h"
#include "common/protocol/pedidos.h"
#include "common/textos.h"

namespace ventrik::ui {
struct EleccionDeServidor {
  std::optional<cliente::AvisosDelServidor> avisos;
  bool abrir_avisos = false;
  bool marcar_todo = false;
  bool desconectar = false;
  bool liberar_nombre = false;
  bool quitar_segundo_paso = false;
  bool configurar = false;
};

struct EleccionDeAvisos {
  std::optional<cliente::AvisosDelServidor> avisos;
  bool cerrado = false;
};

std::string estado_de_silencio(Idioma idioma, const cliente::AjusteDeAvisos& ajuste);
std::optional<cliente::AjusteDeAvisos> menu_silenciar(Idioma idioma, Clave etiqueta,
                                                      const cliente::AjusteDeAvisos& actual, std::uint64_t ahora);
void titulo_de_menu(std::string_view titulo);
void nota_de_menu(std::string_view nota);
std::optional<cliente::Avisar> menu_avisarme(Idioma idioma, Clave etiqueta, cliente::Avisar actual);
EleccionDeServidor menu_servidor(Idioma idioma, const cliente::AvisosDelServidor& avisos, bool conectado, protocolo::Rol rol,
                                 std::uint64_t ahora);
std::optional<cliente::AvisosDelServidor> menu_de_canal(Idioma idioma, const cliente::AvisosDelServidor& avisos,
                                                        const protocolo::CanalInfo& canal, std::uint64_t ahora);
EleccionDeAvisos dialogo_avisos(Idioma idioma, std::string_view servidor, const cliente::AvisosDelServidor& avisos,
                                const std::vector<protocolo::CanalInfo>* canales, std::uint64_t ahora, bool& abrir);

}
