#include <string>

#include "common/utf.h"
#include <doctest/doctest.h>

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

TEST_CASE("a_utf16 rechaza UTF-8 roto") {
  CHECK(ventrik::a_utf16("\xC3").empty());       // falta el segundo byte
  CHECK(ventrik::a_utf16("a\xFF"
                         "b").empty());          // 0xFF nunca es válido
  CHECK(ventrik::a_utf16("\xED\xA0\x80").empty());  // mitad de un par UTF-16
}
