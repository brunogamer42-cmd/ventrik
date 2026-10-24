#pragma once

#include <string>
#include <vector>

#include "common/textos.h"

namespace ventrik::ui {

int abrir_programa(Idioma idioma, const std::wstring& titulo, int como_mostrar,
                   const std::vector<std::string>& entrar = {});

}
