#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <windows.h>

#include <gsl/gsl>

#include "common/cifrado/huella.h"
#include "common/protocol/hostear.h"

namespace ventrik::cliente {
constexpr std::size_t kMaximoMisServidores = 32;

struct MiServidorEnDisco {
  std::wstring carpeta;
  std::string nombre;
  std::uint16_t puerto = 0;
  std::optional<cifrado::Huella::Bytes> huella;
  bool exportada = false;
};

class ProcesoDelServidor {
 public:
  static std::optional<ProcesoDelServidor> lanzar(const std::wstring& exe, const std::vector<std::wstring>& argumentos,
                                                  gsl::span<const std::uint8_t> entrada,
                                                  const std::wstring& carpeta = {});
  std::optional<protocolo::SalidaServidor> salida() const;

 private:
  explicit ProcesoDelServidor(HANDLE proceso) : proceso_(proceso, &CloseHandle) {}

  std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)> proceso_;
};

std::wstring exe_del_servidor();
std::wstring carpeta_de_mis_servidores(const std::wstring& carpeta_del_programa, bool propia);
std::vector<MiServidorEnDisco> leer_mis_servidores(const std::wstring& raiz);
std::wstring carpeta_nueva(const std::wstring& raiz, std::string_view nombre);
void borrar_carpeta_vacia(const std::wstring& carpeta);
bool esta_andando(const std::wstring& carpeta);
bool detener_servidor(const std::wstring& carpeta);
bool pide_arranque(const std::wstring& carpeta);

}
