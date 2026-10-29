#include "client/ui/qr.h"

#include <cstdint>
#include <string>

#include <gsl/gsl>
#include <mbedtls/platform_util.h>
#include <qrcodegen.h>

namespace ventrik::ui {
std::optional<Qr> armar_qr(std::string_view texto) {
  std::string terminado(texto);
  std::vector<std::uint8_t> temporal(qrcodegen_BUFFER_LEN_MAX);
  std::vector<std::uint8_t> modulos(qrcodegen_BUFFER_LEN_MAX);
  const auto borrar = gsl::finally([&] {
    mbedtls_platform_zeroize(&terminado[0], terminado.size());
    mbedtls_platform_zeroize(temporal.data(), temporal.size());
    mbedtls_platform_zeroize(modulos.data(), modulos.size());
  });
  if (terminado.find('\0') != std::string::npos ||
      !qrcodegen_encodeText(terminado.c_str(), temporal.data(), modulos.data(), qrcodegen_Ecc_MEDIUM,
                            qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, false)) {
    return std::nullopt;
  }
  const int tamano = qrcodegen_getSize(modulos.data());
  Qr qr;
  qr.lado = tamano + 2 * kBordeQr;
  for (int y = 0; y < tamano; ++y) {
    int inicio = 0;
    for (int x = 0; x <= tamano; ++x) {
      if (qrcodegen_getModule(modulos.data(), x, y)) {
        continue;
      }
      if (x > inicio) {
        qr.tramos.push_back({y + kBordeQr, inicio + kBordeQr, x - inicio});
      }
      inicio = x + 1;
    }
  }
  return qr;
}

void dibujar_qr(ImDrawList& dibujo, const Qr& qr, ImVec2 desde, float modulo) {
  const float lado = static_cast<float>(qr.lado) * modulo;
  dibujo.AddRectFilled(desde, ImVec2(desde.x + lado, desde.y + lado), IM_COL32_WHITE);
  for (const TramoQr& tramo : qr.tramos) {
    const ImVec2 esquina(desde.x + static_cast<float>(tramo.columna) * modulo,
                         desde.y + static_cast<float>(tramo.fila) * modulo);
    dibujo.AddRectFilled(esquina,
                         ImVec2(esquina.x + static_cast<float>(tramo.largo) * modulo, esquina.y + modulo),
                         IM_COL32_BLACK);
  }
}

}
