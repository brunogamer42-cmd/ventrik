#pragma once

// Marca algo provisorio para revisar cuando el programa esté terminado.
// Aparece en cada compilación como aviso (no frena /WX) y tools\check.ps1
// lista todos al final. Texto sin acentos: va a la consola del compilador.

#define VENTRIK_TEXTO_INTERNO(x) #x
#define VENTRIK_TEXTO(x) VENTRIK_TEXTO_INTERNO(x)
#define VENTRIK_PENDIENTE(texto) \
  __pragma(message(__FILE__ "(" VENTRIK_TEXTO(__LINE__) "): PENDIENTE: " texto))
