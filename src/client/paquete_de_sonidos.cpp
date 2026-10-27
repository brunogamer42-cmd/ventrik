#include "client/paquete_de_sonidos.h"

#include <algorithm>
#include <array>
#include <memory>

#include <miniz.h>

#define JSMN_HEADER
#include <jsmn.h>

#include "common/utf.h"

namespace ventrik::cliente {
namespace {
constexpr unsigned kMaximoPiezasDelIndice = 128;
constexpr std::string_view kIndice = "paquete.json";

std::string_view texto(std::string_view json, const jsmntok_t& pieza) {
  return json.substr(static_cast<std::size_t>(pieza.start), static_cast<std::size_t>(pieza.end - pieza.start));
}

std::size_t saltar(const std::vector<jsmntok_t>& piezas, std::size_t cual) {
  std::size_t siguiente = cual + 1;
  for (int hijo = 0; hijo < piezas[cual].size && siguiente < piezas.size(); ++hijo) {
    siguiente = saltar(piezas, siguiente);
  }
  return siguiente;
}

bool nombre_valido(std::string_view nombre) {
  return !nombre.empty() && nombre.size() <= kMaximoNombreEnPaquete && nombre.find_first_of("/\\:") == std::string_view::npos &&
         nombre.find("..") == std::string_view::npos && es_utf8_valido(nombre);
}

}

std::optional<std::vector<std::pair<Sonido, std::string>>> leer_indice(std::string_view json) {
  std::vector<jsmntok_t> piezas(kMaximoPiezasDelIndice);
  jsmn_parser lector;
  jsmn_init(&lector);
  const int cuantas = jsmn_parse(&lector, json.data(), json.size(), piezas.data(), kMaximoPiezasDelIndice);
  if (cuantas < 1 || piezas[0].type != JSMN_OBJECT) {
    return std::nullopt;
  }
  piezas.resize(static_cast<std::size_t>(cuantas));
  bool version = false;
  std::vector<std::pair<Sonido, std::string>> sonidos;
  for (std::size_t clave = 1, vistas = 0; vistas < static_cast<std::size_t>(piezas[0].size) && clave + 1 < piezas.size();
       ++vistas, clave = saltar(piezas, clave)) {
    const std::string_view nombre = texto(json, piezas[clave]);
    const jsmntok_t& valor = piezas[clave + 1];
    if (nombre == "version") {
      version = valor.type == JSMN_PRIMITIVE && texto(json, valor) == "1";
    } else if (nombre == "sonidos" && valor.type == JSMN_OBJECT) {
      for (std::size_t par = clave + 2, hechos = 0; hechos < static_cast<std::size_t>(valor.size) && par + 1 < piezas.size();
           ++hechos, par = saltar(piezas, par)) {
        const std::string_view aviso = texto(json, piezas[par]);
        const std::string_view archivo = texto(json, piezas[par + 1]);
        const auto cual = std::find(kNombresDeSonidos.begin(), kNombresDeSonidos.end(), aviso);
        const auto sonido = static_cast<Sonido>(cual - kNombresDeSonidos.begin());
        if (piezas[par + 1].type == JSMN_STRING && cual != kNombresDeSonidos.end() && nombre_valido(archivo) &&
            std::none_of(sonidos.begin(), sonidos.end(), [sonido](const auto& visto) { return visto.first == sonido; })) {
          sonidos.emplace_back(sonido, std::string(archivo));
        }
      }
    }
  }
  return version ? std::optional<std::vector<std::pair<Sonido, std::string>>>(std::move(sonidos)) : std::nullopt;
}

std::optional<std::vector<SonidoDePaquete>> leer_paquete(gsl::span<const std::uint8_t> datos) {
  mz_zip_archive zip{};
  if (datos.empty() || datos.size() > kMaximoPaquete || !mz_zip_reader_init_mem(&zip, datos.data(), datos.size(), 0)) {
    return std::nullopt;
  }
  const auto cerrar = gsl::finally([&zip] { mz_zip_reader_end(&zip); });
  const mz_uint cuantos = mz_zip_reader_get_num_files(&zip);
  if (cuantos > kMaximoArchivosEnPaquete) {
    return std::nullopt;
  }
  std::vector<std::string> nombres;
  for (mz_uint cual = 0; cual < cuantos; ++cual) {
    mz_zip_archive_file_stat archivo{};
    if (!mz_zip_reader_file_stat(&zip, cual, &archivo) || archivo.m_is_directory || !archivo.m_is_supported ||
        archivo.m_is_encrypted || archivo.m_uncomp_size > kMaximoArchivoDeSonido || !nombre_valido(archivo.m_filename)) {
      return std::nullopt;
    }
    nombres.emplace_back(archivo.m_filename);
  }
  const auto extraer = [&zip, &nombres](std::string_view nombre, std::size_t maximo) {
    const auto cual = std::find(nombres.begin(), nombres.end(), nombre);
    mz_zip_archive_file_stat archivo{};
    std::vector<std::uint8_t> contenido;
    if (cual == nombres.end() ||
        !mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(cual - nombres.begin()), &archivo) ||
        archivo.m_uncomp_size == 0 || archivo.m_uncomp_size > maximo) {
      return std::optional<std::vector<std::uint8_t>>();
    }
    contenido.resize(static_cast<std::size_t>(archivo.m_uncomp_size));
    return mz_zip_reader_extract_to_mem(&zip, static_cast<mz_uint>(cual - nombres.begin()), contenido.data(),
                                        contenido.size(), 0)
               ? std::optional<std::vector<std::uint8_t>>(std::move(contenido))
               : std::nullopt;
  };
  const auto indice = extraer(kIndice, kMaximoIndiceDePaquete);
  const auto pedidos =
      indice ? leer_indice(std::string_view(reinterpret_cast<const char*>(indice->data()), indice->size()))
             : std::nullopt;
  if (!pedidos) {
    return std::nullopt;
  }
  std::vector<SonidoDePaquete> sonidos;
  for (const auto& [sonido, archivo] : *pedidos) {
    const auto contenido = extraer(archivo, kMaximoArchivoDeSonido);
    auto muestras = contenido ? decodificar_archivo(*contenido, maximo_de_muestras(sonido)) : std::nullopt;
    if (muestras) {
      sonidos.push_back({sonido, archivo, std::move(*muestras)});
    }
  }
  return sonidos;
}

std::vector<std::uint8_t> armar_paquete(const std::vector<SonidoDePaquete>& sonidos) {
  mz_zip_archive zip{};
  if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
    return {};
  }
  const auto cerrar = gsl::finally([&zip] { mz_zip_writer_end(&zip); });
  std::string indice = "{\"version\":1,\"sonidos\":{";
  for (const SonidoDePaquete& sonido : sonidos) {
    const auto cual = static_cast<std::size_t>(sonido.sonido);
    const std::string nombre = (cual < 9 ? "0" : "") + std::to_string(cual + 1) + "_" + kNombresDeSonidos[cual] + ".wav";
    const auto wav = armar_wav(sonido.muestras);
    if (!mz_zip_writer_add_mem(&zip, nombre.c_str(), wav.data(), wav.size(), MZ_DEFAULT_LEVEL)) {
      return {};
    }
    indice += (indice.back() == '{' ? "\"" : ",\"") + std::string(kNombresDeSonidos[cual]) + "\":\"" + nombre + "\"";
  }
  indice += "}}";
  void* armado = nullptr;
  std::size_t largo = 0;
  if (!mz_zip_writer_add_mem(&zip, kIndice.data(), indice.data(), indice.size(), MZ_DEFAULT_LEVEL) ||
      !mz_zip_writer_finalize_heap_archive(&zip, &armado, &largo)) {
    return {};
  }
  const std::unique_ptr<void, decltype(&mz_free)> dueno(armado, &mz_free);
  const auto* bytes = static_cast<const std::uint8_t*>(armado);
  return std::vector<std::uint8_t>(bytes, bytes + largo);
}

}
