#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <windows.h>

#include "client/avisos.h"

namespace ventrik::ui {

class Notificador {
 public:
  virtual ~Notificador() = default;
  virtual bool mostrar(const std::vector<cliente::Notificacion>& avisos) = 0;
  virtual void ocultar() = 0;
  virtual std::optional<cliente::Notificacion> tocado() = 0;
  virtual std::uint32_t mostrados() const = 0;
};

std::unique_ptr<Notificador> notificaciones_de_windows(HWND principal, const std::wstring& carpeta,
                                                       bool prueba = false);
void escuchar_clics_de_windows(HWND principal);
void soltar_clics_de_windows();
long registro_de_clics_de_windows();
std::optional<cliente::Notificacion> tomar_clic_de_windows();
std::size_t notificaciones_en_windows(bool prueba);
void olvidar_notificaciones_de_windows(bool prueba);
std::wstring xml_de_aviso(const cliente::Notificacion& aviso);

}
