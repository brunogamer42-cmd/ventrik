#include <string>

#include "common/texto.h"
#include "common/textos.h"
#include <doctest/doctest.h>

using namespace ventrik;

namespace {
std::string repetir(const std::string& texto, int veces) {
  std::string resultado;
  while (veces-- > 0) {
    resultado += texto;
  }
  return resultado;
}
}

TEST_CASE("Nombres que se aceptan") {
  for (const std::string& nombre : {std::string("Bruno"), std::string("José"),
                                    std::string("Ana María"), std::string("日本語"),
                                    std::string("😀 x"), std::string("a-b_c.d"), repetir("a", 32),
                                    repetir("ñ", 32), repetir("😀", 16)}) {
    CAPTURE(nombre);
    CHECK(es_nombre_valido(nombre));
  }
}

TEST_CASE("Nombres con trampas no se aceptan") {
  for (const std::string& nombre :
       {std::string(), std::string(" Bruno"), std::string("Bruno "), std::string("Bru  no"),
        repetir("a", 33), repetir("😀", 17), repetir("ñ", 33),
        std::string("Bru\tno"), std::string("Bru\nno"), std::string("a\0b", 3),
        std::string("\x7F" "a"), std::string("a\xC2\x85"), std::string("\xC3\x28"),
        std::string("\xED\xA0\x80"), std::string("\xE2\x80\xAE" "abc"), std::string("a\xE2\x81\xA6" "b"),
        std::string("a\xE2\x80\x8B" "b"), std::string("a\xE2\x80\x8D" "b"), std::string("a\xE2\x80\x8E" "b"),
        std::string("a\xD8\x9C" "b"), std::string("a\xE3\x85\xA4" "b"), std::string("a\xC2\xA0" "b"),
        std::string("a\xC2\xAD" "b"), std::string("a\xE2\x80\x83" "b"), std::string("a\xEF\xBB\xBF" "b"),
        std::string("a\xEF\xB8\x8F" "b"), std::string("a\U000E0041b"), std::string("a\xEF\xBF\xBF" "b")}) {
    CAPTURE(nombre);
    CHECK_FALSE(es_nombre_valido(nombre));
  }
}

TEST_CASE("Mensajes de chat") {
  CHECK(es_chat_valido("hola"));
  CHECK(es_chat_valido("línea 1\nlínea 2"));
  CHECK(es_chat_valido("👨\xE2\x80\x8D👩\xE2\x80\x8D👧"));
  CHECK(es_chat_valido(std::string(kMaximoBytesChat, 'x')));
  CHECK_FALSE(es_chat_valido(std::string(kMaximoBytesChat + 1, 'x')));
  CHECK_FALSE(es_chat_valido(""));
  CHECK_FALSE(es_chat_valido("   "));
  CHECK_FALSE(es_chat_valido("\n\n"));
  CHECK_FALSE(es_chat_valido("a\rb"));
  CHECK_FALSE(es_chat_valido("a\tb"));
  CHECK_FALSE(es_chat_valido(std::string("a\0b", 3)));
  CHECK_FALSE(es_chat_valido("abc\xE2\x80\xAE" "def"));
  CHECK_FALSE(es_chat_valido("abc\xE2\x81\xA7" "def"));
  CHECK_FALSE(es_chat_valido("\xFF"));
}

TEST_CASE("Los nombres se comparan sin mayúsculas") {
  CHECK(nombre_para_comparar("bruno") == nombre_para_comparar("BRUNO"));
  CHECK(nombre_para_comparar("josé") == nombre_para_comparar("JOSÉ"));
  CHECK(nombre_para_comparar("Ñandú") == nombre_para_comparar("ñANDÚ"));
  CHECK(nombre_para_comparar("Bruno") != nombre_para_comparar("Bruna"));
  CHECK(nombre_para_comparar("Jos\xC3\xA9") == nombre_para_comparar("Jose\xCC\x81"));
  CHECK(nombre_para_comparar("\xEF\xBC\xA1\xEF\xBD\x8E\xEF\xBD\x81") == nombre_para_comparar("ana"));
  CHECK_FALSE(es_nombre_valido("Ana\xE2\xA0\x80"));
  CHECK_FALSE(es_nombre_valido("\xE2\xA0\x80"));
  CHECK(nombre_para_comparar("\xFF").empty());
}

TEST_CASE("con_datos completa {1} a {9} y deja lo demás como está") {
  CHECK(con_datos("{1} pasó a {2}.", {"Ana", "General"}) == "Ana pasó a General.");
  CHECK(con_datos("{2}{1}{2}", {"a", "b"}) == "bab");
  CHECK(con_datos("{3} {0} {x} {1", {"a", "b"}) == "{3} {0} {x} {1");
  CHECK(con_datos("{1}", {"{2}", "b"}) == "{2}");
  CHECK(con_datos("", {}) == "");
}
