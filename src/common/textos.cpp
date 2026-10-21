#include "common/textos.h"

#include <string>

#include <windows.h>

namespace ventrik {

namespace {

// LOCALE_SISO639LANGNAME devuelve hasta 9 caracteres, contando el final.
constexpr int kLargoCodigoWindows = 9;

const char* buscar(std::size_t indice_idioma, Clave clave) {
  const auto indice_clave = static_cast<std::size_t>(clave);
  if (indice_idioma >= generado::kCantidadIdiomas ||
      indice_clave >= generado::kCantidadClaves) {
    return "";
  }
  return generado::kTextos[indice_idioma][indice_clave];
}

}  // namespace

std::size_t cantidad_idiomas() { return generado::kCantidadIdiomas; }

std::size_t cantidad_claves() { return generado::kCantidadClaves; }

Idioma elegir_idioma(std::string_view codigo_iso) {
  for (std::size_t i = 0; i < generado::kCantidadIdiomas; ++i) {
    if (codigo_iso == generado::kCodigosIdiomas[i]) {
      return Idioma{i};
    }
  }
  return Idioma{generado::kIdiomaRespaldo};
}

Idioma idioma_de_windows() {
  const LCID local = MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT);
  wchar_t codigo[kLargoCodigoWindows] = {};
  const int largo =
      GetLocaleInfoW(local, LOCALE_SISO639LANGNAME, codigo, kLargoCodigoWindows);
  if (largo <= 1) {
    return Idioma{generado::kIdiomaRespaldo};
  }
  // El código ISO 639 son letras minúsculas; cualquier otra cosa se descarta.
  std::string ascii;
  for (int i = 0; i < largo - 1; ++i) {
    const wchar_t letra = codigo[i];
    if (letra < L'a' || letra > L'z') {
      return Idioma{generado::kIdiomaRespaldo};
    }
    ascii.push_back(static_cast<char>(letra));
  }
  return elegir_idioma(ascii);
}

std::string_view codigo_de(Idioma idioma) {
  if (idioma.indice >= generado::kCantidadIdiomas) {
    return {};
  }
  return generado::kCodigosIdiomas[idioma.indice];
}

std::string_view nombre_de_clave(Clave clave) {
  const auto indice = static_cast<std::size_t>(clave);
  if (indice >= generado::kCantidadClaves) {
    return {};
  }
  return generado::kNombresClaves[indice];
}

std::string_view texto(Idioma idioma, Clave clave) {
  const std::string_view propio = buscar(idioma.indice, clave);
  if (!propio.empty()) {
    return propio;
  }
  return buscar(generado::kIdiomaRespaldo, clave);
}

bool falta_traduccion(Idioma idioma, Clave clave) {
  return std::string_view(buscar(idioma.indice, clave)).empty();
}

}  // namespace ventrik
