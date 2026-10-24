#pragma once

#include <cstdint>
#include <string>

namespace ventrik::cliente {
std::uint64_t ahora_ms();
std::string hora_local(std::uint64_t milisegundos);
std::string fecha_local(std::uint64_t milisegundos);
std::string fecha_y_hora_local(std::uint64_t milisegundos);
std::uint32_t dia_local(std::uint64_t milisegundos);

}
