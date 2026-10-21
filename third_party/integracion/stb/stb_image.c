/*
 * stb_image viene como un solo encabezado: acá se compila su código.
 *
 * - Solo GIF: el diseño usa stb_image para GIF y libwebp para WebP; los
 *   demás formatos quedan afuera y no se pueden usar para atacar.
 * - Sin archivos: siempre se decodifica desde memoria.
 */
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
