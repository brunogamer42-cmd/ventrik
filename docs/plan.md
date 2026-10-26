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
   - Segunda parte (delicada, antes de publicar; hecha): el paquete de medios por DTLS lleva el tipo de flujo (voz; los demás reservados) y el número de flujo; el servidor reenvía según el tipo y descarta los que no conoce. Se reserva el bit "pantalla" en las funciones del protocolo, sin comportamiento. Prepara compartir pantalla (etapa 12).
5. Ventana principal en modo Rendimiento y pantalla Inicio con Direct3D 9 y Dear ImGui, con el aspecto de docs/aspecto.md. Textos en lang/. En tres partes:
   - Parte 1 (hecha): la ventana y la pantalla Inicio, comparada con docs/pantallas/Inicio.png.
   - Parte 2 (delicada; hecha): canales de texto en el protocolo, la base y los permisos. Tipo de canal y marca "solo los admins escriben", contador de mensajes, aviso de mensajes nuevos sin el texto y con tope de ritmo, y el servidor rechaza usar un canal de texto como canal de voz. La base de prueba se actualiza sola.
   - Parte 3a (hecha): la pantalla del servidor con los dos grupos de canales, la barra de la lista, el chat, los contadores de nuevos mientras estás conectado, la tecla para hablar y la barra de estado con el gráfico de ping. Lo que en la captura no tiene etapa va apagado con "Pronto", igual que "Compartir pantalla". Captura comparada con docs/pantallas/Rendimiento.png y Rendimiento-canal-texto.png.
   - Parte 3b (hecha): hasta dónde leíste guardado en la PC (programa.dat versión 2), los botones + y el clic derecho del dueño para crear, cambiar y borrar canales, el panel de diagnóstico que abre el gráfico, y borrar la consola de prueba.
   - Medición del consumo (hecha): `ventrik.exe --registro`.
   - Consumo de la ventana y microcortes en XP (hecho): dibujar solo lo que cambia, teclas repetidas y mouse quieto sin dibujos, hacia los parlantes se tira solo lo callado.
   - Optimización integral de lo que ya existe, de un solo tirón (hecha): docs/optimizacion.md, sección 17.
   - Hablar por voz, adelantado de la etapa 3 por pedido del dueño (hecho): el selector "Tecla para hablar / Por voz" y la sensibilidad en el panel "Tu audio", guardados en programa.dat (versión 3). Capturas comparadas con docs/pantallas/Rendimiento-por-voz.png.
   - Barra fija en "Tu audio", idea del dueño (hecha): en Rendimiento, una barra naranja prendida o apagada en lugar del medidor, y la sensibilidad detrás de "Ajustar" y "Guardar". Con el arreglo de los caracteres repetidos que seguían pidiendo cuadros con la tecla apretada.
   - Entrar a un servidor sin entrar a la voz (delicado: cambia el protocolo y el servidor; hecho y probado en la HP): estar en el servidor y estar en la voz quedan separados, con el ajuste "Al entrar a un servidor", "Salir de la voz", el panel "Sin canal de voz" y el grupo "Sin canal de voz · N"; sin voz no se abren el micrófono, la salida de la voz ni el UDP (docs/diseno.md, "Entrar a un servidor y a la voz"). Captura comparada con docs/pantallas/Rendimiento-sin-voz.png y Rendimiento.png.
   - Panel de audio nuevo (diseño del dueño del 2026-10-25; delicado: silenciar cambia el protocolo y el servidor; hecho y probado en la HP): la cabecera sin "Micrófono" ni "Audio"; abajo a la izquierda, tres filas (canal con "Salir del canal", medidor, y cuatro botones: silenciar micrófono con Ctrl+M, silenciar audio y micrófono con Ctrl+D, modo de transmisión y salida), con los paneles flotantes "Cómo transmitís" (volumen del micrófono de 0 a 200 %) y "Volumen de salida"; el silencio se ve en tu fila para los demás y en la cabecera (docs/diseno.md, "Panel de audio"). Capturas comparadas con las nueve de docs/pantallas (Rendimiento y Visual).
6. Prueba en check.ps1 que levanta servidor y programa en las cuatro combinaciones (clásica y moderna en cada lado) y verifica conexión, chat y voz.

Terminada cuando: un servidor con dos programas conectados (uno desde XP) que chatean en un canal de voz y en uno de texto, y hablan, y la revisión independiente sin fallas de seguridad pendientes.

## Etapas 3 a 14

Cada una empieza en una sesión nueva: se divide en pasos chicos, cada uno con su prueba automática y una prueba a mano, marcando los delicados. Cada etapa cumple las reglas de consumo de docs/optimizacion.md (secciones 2 y 15).

| Etapa | Pasos principales | Pantallas | Prueba a mano al final |
| --- | --- | --- | --- |
| 3. Voz completa | Notificaciones como primer paso: las de Windows 10 y 11 en la moderna y una ventana propia en la clásica, configurables; varias voces y mezcla; RNNoise; cancelación de eco con SpeexDSP; Ajustes → Audio y micrófono con "Seguir transmitiendo al soltar" (tecla o voz con sensibilidad ya está desde el 2.5); volumen por persona; susurro con los permisos básicos (los permisos por rol llegan en la etapa 9); sonidos de los avisos (los 27, con "Sonar por", No molestar, sonidos propios y paquetes .vksonidos; decodificar WAV y MP3 con dr_wav y dr_mp3, que ya vienen en miniaudio, y OGG con stb_vorbis, del mismo paquete de miniaudio; el zip y el JSON del paquete piden sumar bibliotecas a third_party, con permiso del dueño; fuzzing de los audios y del paquete; mezcla sin saturar) | Ajustes, una sección por vez con menú y buscador, con las secciones General, Teclas, Audio y micrófono y Avisos y sonidos (Ajustes-*.png); las de etapas futuras con "PRONTO" | Tres personas hablando, ruido de fondo de prueba y un susurro; una notificación con Ventrik minimizado, en la PC y en XP; un sonido propio y un paquete importado, y los avisos sonando por otro dispositivo |
| 4. Identidad y seguridad (delicada) | Clave de identidad; contraseñas con Argon2id; DPAPI y contraseña maestra; niveles de seguridad; historial cifrado; bloqueo automático; aviso de equipo nuevo; avisos de acciones de admins; TOTP para admins; llaves en la moderna; respaldo, mudanza y cambio de clave del servidor | Ajustes → Seguridad; Hostear: el asistente por pasos, con los pasos 1 (Nombre y acceso), 4 (Seguridad) y 7 (Revisar e iniciar), y el 6 apagado "En desarrollo" | Exportar la identidad, borrarla, importarla y volver a entrar; mudar un servidor de prueba a otra PC |
| 5. Archivos y antivirus (delicada) | Subida, descarga y reanudación con prioridad de la voz; cuarentena y ClamAV en el servidor; base de virus compartida por el servidor; comprimidos con contraseña en el cliente | Enviar archivo con contraseña; Hostear, paso 5 (Chat y archivos) | Subir el archivo de prueba EICAR y ver que se bloquea |
| 6. Red | Panel de red y puertos (placa, puertos, UPnP, firewall, prueba desde internet); búsqueda en la red local; relay (el oficial y agregar otros); invitaciones | Hostear, paso 2 (Conexión) y la pantalla final Invitar gente; Explorar → Red local | Alguien de afuera entra con una invitación |
| 7. Lista pública | Directorio (puerto 7702 y DNS dinámico) y agregar otros directorios; búsqueda por nombre, IP y región; copia guardada cuando está apagado; buzón de reportes y botón Reportar, sin captura todavía | Explorar servidores; Reportar | Apagar el directorio y entrar igual a un servidor guardado |
| 8. Servidores grandes | Simulador de 512 usuarios; tope de voces; calidad dinámica; capacidad estimada; canal escenario; modo canal grande; chat lento; reconexiones escalonadas; la subida del servidor repartida en un solo lugar por prioridades (primero la voz) | Hostear, paso 3 (Capacidad y voz) | El informe del simulador muestra cómo baja la calidad con poca subida |
| 9. Moderación | Roles y permisos por rol y canal (también para susurrar); baneos con huella del equipo; barreras de entrada; el permiso "Compartir pantalla" en la lista, apagado y "en desarrollo" | Panel de administración | Banear una prueba y ver que vuelve silenciada |
| 10. Modo Visual (delicada) | Perfiles y tarjeta con fondo animado, efecto por fuera y animación al abrirse; marcos de avatar y editor de efectos con su código para compartir (validado y con fuzzing); Inicio en Visual; personalización (icono animado, fondo del chat, color del tema), GIFs y stickers; imágenes en el proceso de medios (proceso aparte, con fuzzing, listo para sumarle video); captura en los reportes; en el panel de audio, el medidor con divisiones (con la marca por voz), hasta 30 imágenes por segundo (en Rendimiento queda la barra fija) | Visual, Inicio en Visual, Perfil, Editor de efectos, Personalizar servidor | Cambiar de modo y ver que en Rendimiento no se descarga nada |
| 11. Cierre | Modo servidor limitado; idiomas; el resto de Ajustes; instaladores con ClamAV; firma con SignPath; actualizaciones firmadas; pruebas de 8 horas | Ajustes (general, apariencia, descargas, red, acerca de) | Instalar desde cero en XP, en la netbook y en Windows 11 |
| 12a. Compartir pantalla (delicada) | VP8 con libvpx en todos los equipos, incluido XP (libvpx entra a third_party/); captura con DXGI desde Windows 8 y GDI en XP y 7, solo lo que cambió y el cursor aparte; reenvío sin recomprimir, solo a quien mira; copia chica y ajuste a la conexión; topes y opciones de Hostear; descompresor en el proceso de medios con fuzzing; "No mirar pantallas en esta PC" | Compartir pantalla; Hostear, paso 6 (Pantallas compartidas) | Medir como en la etapa 1: la HP 2133 compartiendo y mirando |
| 12b. Pantalla en alta calidad | Copia H.264 por la placa de video con Media Foundation (desde Windows 7); cuadros en capas para bajar de 60 a 30 sin recomprimir; repetidor opcional | Las mismas | Una PC moderna compartiendo en 1080p y 1440p a 60 cuadros |
| 14a. Amigos y llamadas de a dos (delicada) | ID (la identidad de siempre, con parte antispam y código corto) y solicitudes; red repartida (DHT) para encontrarse sin servidor; conexión directa (UDP hole punching) o por otro participante o el relay; cifrado de punta a punta con MLS (antes, verificar una implementación que compile para XP); chat de a dos con mensajes que esperan al otro; archivos en partes; llamadas de voz de a dos; base local; copias cifradas en Google Drive desde el servicio de Windows y Recuperar. No depende de las etapas 12 y 13 | Inicio → Amigos; Agregar amigo; Llamada entrante; Ajustes → Amigos y privacidad; Ajustes → Copias de seguridad; Recuperar | Dos PCs detrás de módems distintos y una con CGNAT; con el directorio apagado se siguen encontrando; un extraño con tu ID no ve si estás conectado ni tu IP; mensaje y archivo a un amigo desconectado; formatear y recuperar desde Drive en Windows 10 y en XP, con una contraseña equivocada que falla sin romper nada |
| 14b. Grupos y llamadas grupales | Grupos con admins y mensajes que faltan pasados por otro miembro; llamadas grupales con reparto (usa el algoritmo de la etapa 13) y pantalla en árbol (usa la etapa 12); panel de la llamada (personas, chat, cómo viaja); fuzzing de los mensajes entre PCs y de la DHT | Chat de grupo; Llamada grupal | En una llamada grupal se van el que coordina y uno que reenvía pantalla: la voz no se corta y la pantalla vuelve en segundos; alguien sale del grupo y no lee lo nuevo; un adjunto en varias copias se sube una vez |

La etapa 13 (varias PCs, un servidor sostenido entre varias PCs) se suma a esta tabla cuando esté su diseño. Desde que exista Ajustes (etapa 3), sus secciones de etapas futuras (Prestar esta PC, Amigos y privacidad, Copias de seguridad) aparecen desactivadas y marcadas "EN DESARROLLO".
