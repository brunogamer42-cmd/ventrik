#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

#include <windows.h>

#include "common/utf.h"
#include <doctest/doctest.h>

namespace {

// Para que doctest muestre en qué caso falló.
std::string en_hexadecimal(std::string_view texto) {
  std::string salida;
  const char* const digitos = "0123456789ABCDEF";
  for (const char c : texto) {
    const auto b = static_cast<unsigned char>(c);
    salida.push_back(digitos[b >> 4]);
    salida.push_back(digitos[b & 0x0F]);
    salida.push_back(' ');
  }
  return salida;
}

}  // namespace

TEST_CASE("a_utf16 convierte texto común") {
  CHECK(ventrik::a_utf16("Ventrik") == std::wstring(L"Ventrik"));
}

TEST_CASE("a_utf16 respeta acentos, eñes y emojis") {
  // "canción ñandú 🎤", escrito byte por byte en UTF-8.
  CHECK(ventrik::a_utf16("canci\xC3\xB3n \xC3\xB1"
                         "and\xC3\xBA \xF0\x9F\x8E\xA4") ==
        std::wstring(L"canción ñandú \U0001F3A4"));
}

TEST_CASE("a_utf16 de texto vacío da vacío") {
  CHECK(ventrik::a_utf16("").empty());
}

TEST_CASE("UTF-8 válido, en los bordes de cada largo") {
  for (const std::string_view caso : {
           std::string_view("\x7F", 1),      // U+007F, último de 1 byte
           std::string_view("\xC2\x80"),     // U+0080, primero de 2 bytes
           std::string_view("\xDF\xBF"),     // U+07FF, último de 2 bytes
           std::string_view("\xE0\xA0\x80"),  // U+0800, primero de 3 bytes
           std::string_view("\xED\x9F\xBF"),  // U+D7FF, justo antes de los pares
           std::string_view("\xEE\x80\x80"),  // U+E000, justo después
           std::string_view("\xEF\xBF\xBF"),  // U+FFFF, último de 3 bytes
           std::string_view("\xF0\x90\x80\x80"),  // U+10000, primero de 4 bytes
           std::string_view("\xF4\x8F\xBF\xBF"),  // U+10FFFF, el último que existe
       }) {
    const std::string bytes = en_hexadecimal(caso);
    CAPTURE(bytes);
    CHECK(ventrik::es_utf8_valido(caso));
    CHECK_FALSE(ventrik::a_utf16(caso).empty());
  }
}

TEST_CASE("UTF-8 roto se rechaza igual en todos los Windows") {
  for (const std::string_view caso : {
           std::string_view("\xC3"),              // falta el segundo byte
           std::string_view("\xE2\x82"),          // falta el tercero
           std::string_view("\xF0\x9F\x8E"),      // falta el cuarto
           std::string_view("\x80"),              // un byte del medio, suelto
           std::string_view("a\xFF" "b"),         // 0xFF nunca es válido
           std::string_view("\xC0\x80"),          // forma larga de U+0000
           std::string_view("\xC1\xBF"),          // forma larga de U+007F
           std::string_view("\xE0\x80\x80"),      // forma larga de U+0000
           std::string_view("\xE0\x9F\xBF"),      // forma larga de U+07FF
           std::string_view("\xF0\x8F\xBF\xBF"),  // forma larga de U+FFFF
           std::string_view("\xED\xA0\x80"),      // mitad de par UTF-16 (U+D800)
           std::string_view("\xED\xBF\xBF"),      // mitad de par UTF-16 (U+DFFF)
           std::string_view("\xF4\x90\x80\x80"),  // U+110000, se pasa del máximo
           std::string_view("\xF5\x80\x80\x80"),  // nunca empieza un carácter
           std::string_view("\xC3\x28"),          // el segundo byte no es del medio
       }) {
    const std::string bytes = en_hexadecimal(caso);
    CAPTURE(bytes);
    CHECK_FALSE(ventrik::es_utf8_valido(caso));
    CHECK(ventrik::a_utf16(caso).empty());
  }
}

#if defined(VENTRIK_MODERNA)
// Desde Vista, Windows sigue la regla de Unicode al pasar UTF-8 a UTF-16
// (XP no). En la moderna se compara es_utf8_valido con Windows en todas las
// combinaciones de 1, 2 y 3 bytes: unos 16 millones de casos.
TEST_CASE("es_utf8_valido dice lo mismo que Windows 10 en todos los casos cortos") {
  wchar_t destino[4];
  const auto windows_acepta = [&destino](const char* bytes, int largo) {
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, largo, destino, 4) > 0;
  };
  std::size_t distintos = 0;
  std::string primer_distinto;
  char bytes[3];
  for (int largo = 1; largo <= 3; ++largo) {
    const long total = 1L << (8 * largo);
    for (long n = 0; n < total; ++n) {
      for (int k = 0; k < largo; ++k) {
        bytes[k] = static_cast<char>((n >> (8 * k)) & 0xFF);
      }
      const std::string_view caso(bytes, static_cast<std::size_t>(largo));
      if (ventrik::es_utf8_valido(caso) != windows_acepta(bytes, largo)) {
        if (distintos == 0) {
          primer_distinto = en_hexadecimal(caso);
        }
        ++distintos;
      }
    }
  }
  CAPTURE(primer_distinto);
  CHECK(distintos == 0u);
}
#endif

TEST_CASE("a_utf8 pasa texto de Windows a UTF-8, con acentos y emojis") {
  const std::wstring texto = L"canción ñandú \U0001F3A4";
  CHECK(ventrik::a_utf16(ventrik::a_utf8(texto)) == texto);
  CHECK(ventrik::a_utf8(L"").empty());
}

TEST_CASE("a_utf8 cambia las mitades de par sueltas por U+FFFD, también en XP") {
  const wchar_t alta_suelta[] = {L'a', 0xD800, L'b', 0};
  const wchar_t baja_suelta[] = {0xDC00, L'c', 0};
  CHECK(ventrik::a_utf8(alta_suelta) == "a\xEF\xBF\xBD" "b");
  CHECK(ventrik::a_utf8(baja_suelta) == "\xEF\xBF\xBD" "c");
  CHECK(ventrik::es_utf8_valido(ventrik::a_utf8(alta_suelta)));
}
