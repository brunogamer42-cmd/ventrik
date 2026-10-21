# Ventrik

Programa de voz para grupos, liviano, con servidor propio gratuito.
Diseño: @docs/diseno.md. Plan de etapas: @docs/plan.md.

## Con quién trabajás
- El dueño del proyecto no programa. Hablale en español, simple y sin jerga.
- Al terminar cada paso: decí qué cambió en dos frases, mostrá la salida de tools\check.ps1 y dale una prueba a mano para hacer él.
- Si algo contradice docs/diseno.md, pará y preguntá antes de seguir.

## Compilar y probar
- IMPORTANTE: nada está terminado si tools\check.ps1 no pasa. Compila las dos versiones y corre todas las pruebas.
- Moderna: Windows 10 y 11, x64, Visual Studio 2022 (v143).
- Clásica: Win32, toolset v141_xp, para Windows XP SP3 a 8.1 y Windows 10 de 32 bits. Solo lo de C++17 que soporte el compilador de VS 2017. Se enlaza con /SAFESEH y activa DEP al arrancar con SetProcessDEPPolicy (cargada con GetProcAddress).
- Las dos con la biblioteca de C estática (/MT): un solo .exe, sin instaladores extra.
- Advertencias como errores (/W4 /WX).

## Reglas de código
- Sobre datos de red o archivos, prohibido: memcpy, strcpy, sprintf y new/delete a mano. Usar std::vector, std::string_view, gsl::span y punteros inteligentes.
- Todo mensaje de red pasa por el lector único de src/common/protocol, que valida cada longitud antes de copiar.
- Protocolo: campos de tamaño fijo, little-endian, texto en UTF-8. Nunca mandar structs tal como están en memoria.
- Cifrado: solo Mbed TLS, nunca algoritmos propios. En la versión clásica, los números aleatorios salen de RtlGenRandom.
- En el código común, solo funciones de Windows que existan en XP. Lo de Windows moderno se carga con GetProcAddress y tiene alternativa.
- Bibliotecas: solo las de third_party/, con su versión en third_party/VERSIONS.md. No agregar ninguna sin preguntar.
- Textos visibles: siempre en lang/, nunca escritos en el código.
- Código que lea datos de red o archivos lleva prueba de fuzzing.
- Cambios en cifrado, protocolo o archivos: revisión con un subagente antes de darlos por terminados.
- Comentarios en el código: pocos y de una línea, solo para explicar por qué (una decisión de seguridad o un arreglo para XP). Nunca para contar lo que el código hace.

## Git
- Un commit por paso aprobado, con un mensaje en español que diga qué hace.
- Nunca push --force ni reescribir el historial.
