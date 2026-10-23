#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ventrik {
constexpr std::size_t kMaximoCaracteresNombre = 32;
constexpr std::size_t kMaximoBytesNombre = 64;
constexpr std::size_t kMaximoBytesChat = 4000;

bool es_nombre_valido(std::string_view nombre);

bool es_chat_valido(std::string_view texto);

std::wstring nombre_para_comparar(std::string_view nombre);

}
