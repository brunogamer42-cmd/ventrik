# Ventrik — Plan de desarrollo

Versión para Claude Code del plan "Ventrik — Plan de desarrollo con Claude Code". El diseño completo está en docs/diseno.md.

## Cómo se trabaja

1. Una etapa por sesión, un paso por pedido.
2. Cada paso empieza en modo Plan: se propone, el dueño aprueba y recién ahí se implementa.
3. Un paso está terminado cuando compilan las dos versiones (moderna y clásica), pasan todas las pruebas de tools\check.ps1 y la prueba a mano da lo esperado.
4. Un commit por paso aprobado.
5. Lo marcado como delicado (cifrado, protocolo, archivos, imágenes) se revisa con un subagente que no escribió ese código.
6. El diseño manda: si algo lo contradice, se pregunta. Si cambia una decisión, se actualiza docs/diseno.md en el mismo paso.

## Estructura del repositorio

```text
ventrik/
  CLAUDE.md            reglas que Claude Code lee en cada sesión
  LICENSE              GPL versión 3
  CMakeLists.txt       cómo se compila todo
  CMakePresets.json    las dos versiones: moderna (x64) y clásica (Win32, v141_xp)
  docs/diseno.md       el documento de diseño
  docs/plan.md         este plan
  docs/mediciones.md   resultados de la etapa 1
  docs/aspecto.md      colores, medidas y piezas de la interfaz
  docs/pantallas/      capturas de cada pantalla del boceto
  third_party/         bibliotecas con versión fija (VERSIONS.md dice cuál y de dónde)
  src/common/          protocolo, cifrado y utilidades compartidas
  src/client/          el programa
  src/server/          el servidor
  src/relay/           el relay (etapa 6)
  src/directory/       el directorio de la lista pública (etapa 7)
  lang/                textos en español e inglés
  tests/               pruebas automáticas
  fuzz/                pruebas con datos rotos
  tools/               check.ps1 y otros comandos
```

## Bibliotecas

Se guardan dentro del proyecto con versión fija, en su última versión estable al momento de agregarlas (Mbed TLS: la rama de soporte largo). Versión y origen en third_party/VERSIONS.md.

| Biblioteca | Para qué | Licencia |
| --- | --- | --- |
| Opus | Comprimir la voz | BSD |
| Mbed TLS (rama de soporte largo) | TLS 1.3 y DTLS 1.2 | Apache 2.0 |
| miniaudio | Micrófono y parlantes | Dominio público o MIT-0 |
| Dear ImGui | Interfaz | MIT |
| RNNoise | Limpiar el ruido de fondo | BSD |
| SpeexDSP | Cancelar el eco (sin auriculares) | BSD |
| Argon2 (implementación de referencia) | Contraseñas | CC0 o Apache 2.0 |
| SQLite | Base de datos del servidor | Dominio público |
| stb_image y libwebp | GIF y WebP del modo Visual | Dominio público o MIT; BSD |
| miniupnpc | Abrir el puerto con UPnP | BSD |
| Microsoft GSL | gsl::span y utilidades de memoria segura | MIT |
| doctest | Pruebas automáticas | MIT |

ClamAV no va en third_party/: es un programa aparte que se suma en la etapa 5 (versión oficial en la moderna, adaptación de Netfarm en la clásica).

## Etapa 0: el proyecto

1. Estructura de carpetas, LICENSE (GPL versión 3), .gitignore para Visual Studio y CMake, Git inicializado y primer commit.
2. CMakePresets.json con dos versiones: moderna (x64, Visual Studio 2022, v143) y clásica (Win32, toolset v141_xp), las dos con /MT, /W4 y /WX. Programa mínimo que abre una ventana Win32 vacía, una prueba con doctest y tools\check.ps1, que compila las dos versiones y corre las pruebas. Prueba a mano: los dos .exe abren en Windows 11 y el clásico en XP SP3. La clásica se enlaza con /SAFESEH y activa DEP al arrancar con SetProcessDEPPolicy.
3. Bibliotecas de la tabla en third_party/, compilando en las dos versiones. En la clásica, Mbed TLS usa RtlGenRandom en lugar de BCryptGenRandom.
4. Repositorio público "ventrik" en GitHub con GitHub CLI, y una acción que compila la versión moderna y corre las pruebas en cada envío.

Terminada cuando: las dos versiones compilan, abren en Windows 11 y en XP, y GitHub compila solo.

## Etapa 1: prueba de voz

Prueba descartable para medir: sin cifrado, nunca se publica, se aparta al terminar.

1. Eco local: micrófono con miniaudio a 48 kHz, Opus a 24 kbps en pedazos de 20 ms, descompresión y reproducción mientras se aprieta una tecla. Muestra demora en ms, procesador y RAM. Corre en las dos versiones.
2. Voz entre dos PCs por UDP en la red local, con un buffer simple para ordenar paquetes y la IP destino elegible.
3. Registro de demora, procesador y RAM cada segundo; resultados en docs/mediciones.md comparados con las metas de docs/diseno.md ("Rendimiento"). Prueba de 10 minutos en la PC, la netbook y XP: confirma si miniaudio anda en XP.

Terminada cuando: la demora en red local está cerca de las 80 ms de la meta y la netbook aguanta. Si no, se corrige el diseño antes de seguir.

## Etapa 2: base

1. (Delicado) src/common/protocol: lector y escritor único de mensajes, tipos básicos y negociación de versión y funciones ("Dos versiones que se entienden"). Pruebas y un fuzzer que corre 10 minutos sin fallas.
2. (Delicado) El servidor genera su certificado y muestra la huella; el programa se conecta por TLS 1.3 con Mbed TLS y compara la huella. Pruebas de huella correcta, huella incorrecta e intento de bajar a un cifrado más débil.
3. Canales, entrar y salir, chat por canal guardado en SQLite con consultas preparadas, permisos básicos verificados en el servidor en cada acción y panel de diagnóstico (procesador, RAM, ping y pérdida de paquetes).
4. (Delicado) Voz por DTLS 1.2 a través del servidor, que la reenvía sin decodificar y decide quién habla según la conexión.
5. Ventana principal en modo Rendimiento y pantalla Inicio con Direct3D 9 y Dear ImGui, con el aspecto de docs/aspecto.md. Textos en lang/. Captura del resultado comparada con docs/pantallas/Rendimiento.png e Inicio.png.
6. Prueba en check.ps1 que levanta servidor y programa en las cuatro combinaciones (clásica y moderna en cada lado) y verifica conexión, chat y voz.

Terminada cuando: un servidor con dos programas conectados (uno desde XP) que chatean y hablan, y la revisión independiente sin fallas de seguridad pendientes.

## Etapas 3 a 11

Cada una empieza en una sesión nueva: se divide en pasos chicos, cada uno con su prueba automática y una prueba a mano, marcando los delicados.

| Etapa | Pasos principales | Pantallas | Prueba a mano al final |
| --- | --- | --- | --- |
| 3. Voz completa | Varias voces y mezcla; RNNoise; cancelación de eco con SpeexDSP; tecla o voz con sensibilidad; volumen por persona; susurro con los permisos básicos (los permisos por rol llegan en la etapa 9) | Audio y micrófono; Ajustes → Teclas | Tres personas hablando, ruido de fondo de prueba y un susurro |
| 4. Identidad y seguridad (delicada) | Clave de identidad; contraseñas con Argon2id; DPAPI y contraseña maestra; niveles de seguridad; historial cifrado; bloqueo automático; aviso de equipo nuevo; avisos de acciones de admins; TOTP para admins; llaves en la moderna; respaldo, mudanza y cambio de clave del servidor | Ajustes → Seguridad; Hostear → Seguridad | Exportar la identidad, borrarla, importarla y volver a entrar; mudar un servidor de prueba a otra PC |
| 5. Archivos y antivirus (delicada) | Subida, descarga y reanudación con prioridad de la voz; cuarentena y ClamAV en el servidor; base de virus compartida por el servidor; comprimidos con contraseña en el cliente | Enviar archivo con contraseña | Subir el archivo de prueba EICAR y ver que se bloquea |
| 6. Red | Panel de red y puertos (placa, puertos, UPnP, firewall, prueba desde internet); búsqueda en la red local; relay (el oficial y agregar otros); invitaciones | Hostear → Red; Explorar → Red local | Alguien de afuera entra con una invitación |
| 7. Lista pública | Directorio (puerto 7702 y DNS dinámico) y agregar otros directorios; búsqueda por nombre, IP y región; copia guardada cuando está apagado; buzón de reportes y botón Reportar, sin captura todavía | Explorar servidores; Reportar | Apagar el directorio y entrar igual a un servidor guardado |
| 8. Servidores grandes | Simulador de 512 usuarios; tope de voces; calidad dinámica; capacidad estimada; canal escenario; modo canal grande; chat lento; reconexiones escalonadas | Hostear → Calidad y Capacidad | El informe del simulador muestra cómo baja la calidad con poca subida |
| 9. Moderación | Roles y permisos por rol y canal (también para susurrar); baneos con huella del equipo; barreras de entrada | Panel de administración | Banear una prueba y ver que vuelve silenciada |
| 10. Modo Visual (delicada) | Perfiles y tarjeta con fondo animado, efecto por fuera y animación al abrirse; marcos de avatar y editor de efectos con su código para compartir (validado y con fuzzing); Inicio en Visual; personalización (icono animado, fondo del chat, color del tema), GIFs y stickers; imágenes en un proceso aparte con fuzzing; captura en los reportes | Visual, Inicio en Visual, Perfil, Editor de efectos, Personalizar servidor | Cambiar de modo y ver que en Rendimiento no se descarga nada |
| 11. Cierre | Modo servidor limitado; idiomas; el resto de Ajustes; instaladores con ClamAV; firma con SignPath; actualizaciones firmadas; pruebas de 8 horas | Ajustes (general, apariencia, avisos, descargas, red, acerca de) | Instalar desde cero en XP, en la netbook y en Windows 11 |
