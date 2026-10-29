# Licencias

Ventrik se publica con la **GPL versión 3** ([LICENSE](../LICENSE)). Todo lo que va **dentro** de sus ejecutables (`ventrik.exe`, `ventrik_servidor.exe`) tiene que tener una licencia compatible con la GPLv3. Lo que no es compatible solo puede usarse como **programa aparte**, en su propio proceso, hablando con Ventrik por línea de comandos o por un socket; nunca enlazado ni cargado dentro del proceso de Ventrik.

Revisado el 2026-10-29 contra los archivos de licencia de cada paquete en `third_party/` y la [lista de licencias de la FSF](https://www.gnu.org/licenses/license-list.html). Versiones y huellas: `third_party/VERSIONS.md`.

## Bibliotecas de third_party/

Todas se compilan desde el código y se enlazan estáticamente dentro del `.exe` (un solo archivo, sin DLL aparte).

| Biblioteca | Licencia | ¿Compatible con GPLv3? | Cómo se usa |
| --- | --- | --- | --- |
| Opus | BSD de 3 cláusulas | Sí | Enlazada (programa y servidor) |
| RNNoise | BSD de 3 cláusulas | Sí | Enlazada (programa) |
| SpeexDSP | BSD de 3 cláusulas | Sí | Enlazada (programa) |
| libwebp | BSD de 3 cláusulas, con la concesión de patentes de Google (`PATENTS`) | Sí | Enlazada (programa) |
| miniupnpc | BSD de 3 cláusulas | Sí | Enlazada (programa) |
| Mbed TLS | Apache 2.0 o GPL 2.0 o posterior (a elección) | Sí: se usa bajo Apache 2.0, que la GPLv3 acepta | Enlazada (programa y servidor) |
| Argon2 (implementación de referencia) | CC0 1.0 o Apache 2.0 (a elección) | Sí | Enlazada (programa y servidor) |
| Dear ImGui | MIT | Sí | Enlazada (programa) |
| Microsoft GSL | MIT | Sí | Solo encabezados, compilada adentro |
| miniz | MIT | Sí | Enlazada (programa) |
| jsmn | MIT | Sí | Solo encabezado, compilada adentro (programa) |
| qrcodegen (Nayuki) | MIT | Sí | Enlazada (programa), desde el paso 4.7 |
| miniaudio | Dominio público (Unlicense) o MIT-0 (a elección) | Sí | Enlazada (programa) |
| stb_image | Dominio público o MIT (a elección) | Sí | Enlazada (programa) |
| SQLite | Dominio público | Sí | Enlazada (servidor) |
| doctest | MIT | Sí | Solo en `ventrik_pruebas.exe`, que no se publica |

**Letras:** Barlow y Barlow Semi Condensed, con la SIL Open Font License 1.1. Van como datos dentro del `.exe` (no son código) y se distribuyen con su `OFL.txt`. La OFL permite incluirlas junto con cualquier programa mientras no se vendan solas. Las que se suman más adelante para los nombres (Bungee, Pacifico, Press Start 2P y Righteous, con OFL, y Permanent Marker, con Apache 2.0) siguen la misma regla.

**Avisos de autoría:** cada licencia exige conservar su aviso. Van en "Acerca de" y en el instalador (etapa 11).

## Programas aparte

| Programa | Licencia | ¿Compatible con GPLv3 dentro del mismo ejecutable? | Cómo se usa |
| --- | --- | --- | --- |
| ClamAV: `clamscan`, `clamd`, `freshclam` y la biblioteca `libclamav` (etapa 5) | GPL versión 2 **únicamente**: sus archivos dicen "version 2", sin "or any later version" | **No** | Siempre como proceso aparte. Ventrik lo abre (`clamscan` por cada análisis, o `clamd` solo mientras dura) y se comunica por línea de comandos o por un socket local. **Nunca** se enlaza `libclamav`, ni estáticamente ni cargando su DLL dentro de Ventrik. |
| Adaptación de ClamAV de Netfarm para XP (versión clásica, etapa 5) | GPL versión 2 (la de ClamAV) | No | Igual que ClamAV: proceso aparte |

El instalador incluye la licencia de ClamAV y el código fuente exacto de la versión que se distribuye, también el de la adaptación de Netfarm, como pide la GPLv2.

## Partes de Windows

- **DLL del sistema** (kernel32, user32, d3d9, crypt32 y las demás) y las que se cargan con `GetProcAddress` cuando existen (por ejemplo `webauthn.dll` para las llaves de seguridad, desde el paso 4.11). Son parte del sistema operativo, y la GPLv3 las excluye como "bibliotecas del sistema".
- **Biblioteca de C de Visual Studio**, enlazada estáticamente (`/MT`). Es la biblioteca del compilador con el que se arma el programa, y la GPLv3 también la cuenta como biblioteca del sistema.

## Lo propio

Todo el código de `src/`, `tests/`, `fuzz/`, `tools/` y `third_party/integracion/` y los sonidos de fábrica (los hace el autor) son de Ventrik, con la GPLv3.

## Antes de sumar algo nuevo

1. Leer la licencia en los archivos del paquete, no solo en la página del proyecto.
2. Compatibles para enlazar: MIT, BSD, ISC, zlib, Apache 2.0, CC0, dominio público, LGPL, GPLv3 y "GPLv2 o posterior".
3. No compatibles para enlazar: GPL versión 2 **únicamente**, licencias con cláusulas de uso ("no comercial", "solo para el bien") y las que prohíben modificar. Esas solo pueden ir como programa aparte, o no se usan.
4. Anotarla acá y en `third_party/VERSIONS.md`, y preguntarle al dueño antes de agregarla (CLAUDE.md).
