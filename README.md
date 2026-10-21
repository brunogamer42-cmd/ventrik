# Ventrik

[![Compilar y probar](https://github.com/brunogamer42-cmd/ventrik/actions/workflows/compilar.yml/badge.svg)](https://github.com/brunogamer42-cmd/ventrik/actions/workflows/compilar.yml)

Programa de voz para grupos, liviano, con servidor propio gratuito. Funciona desde Windows XP SP3 hasta Windows 11. La voz, el chat y los archivos viajan cifrados.

*Lightweight group voice chat with a free self-hosted server, for Windows XP SP3 to Windows 11.*

**Estado:** en desarrollo. Todavía no hay una versión para usar.

- Diseño: [docs/diseno.md](docs/diseno.md)
- Plan de trabajo: [docs/plan.md](docs/plan.md)
- Bibliotecas usadas: [third_party/VERSIONS.md](third_party/VERSIONS.md)

## Compilar

Hace falta Visual Studio 2022 (alcanza con las Build Tools) con:
- "Desarrollo para el escritorio con C++";
- las herramientas v141 y la "Compatibilidad de C++ con Windows XP para las herramientas de VS 2017 (v141)", para la versión clásica.

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check.ps1
```

Compila las dos versiones, corre las pruebas y deja los programas en `build\moderna\Release` (Windows 10 y 11, 64 bits) y `build\clasica\Release` (Windows XP SP3 a 8.1 y Windows 10 de 32 bits).

## Licencia

GPL versión 3 ([LICENSE](LICENSE)). Cada biblioteca de `third_party/` tiene su propia licencia, compatible con la GPL v3.
