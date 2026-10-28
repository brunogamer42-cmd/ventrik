# Ventrik — Documento de diseño técnico

Oct 20, 2026 · @Jorge Salvaje

## Qué es Ventrik

Ventrik es un programa de chat de voz para grupos, liviano como el Ventrilo original, donde cualquiera puede abrir su propio servidor gratis. La voz, el chat y los archivos viajan cifrados.

En este documento, **programa** (o cliente) es lo que usa cada persona, y **servidor** es lo que hostea el grupo. El nombre definitivo es Ventrik. Antes de publicarlo conviene una búsqueda de marcas en el INPI, porque se parece a Ventrilo, una marca de terceros del mismo rubro.

Principios:

- **Liviano.** Consume lo mínimo, incluso en PCs viejas.
- **Simple.** Una ventana con canales y chat. Abrir un servidor lleva un par de clics.
- **Gratis y propio.** El servidor lo hostea quien quiera, sin pagar y sin un límite artificial de usuarios.
- **Seguro.** Todo viaja cifrado entre cada programa y el servidor.

Lo que no hace en la primera versión: video de cámara, compartir pantalla, app para celular, bots, búsqueda de GIFs en servicios externos, grabación de la voz ni overlay dentro de los juegos (ver "Viabilidad como programa gratuito"). Compartir pantalla llega después, en la etapa 12 (ver "Compartir pantalla"), los amigos con chats y llamadas directas, en la etapa 14 (ver "Amigos y llamadas directas"), y los bots, en la etapa 15 (ver "Bots").

## Cómo funciona

Cada persona usa el programa en su PC y se conecta a un solo servidor, directo o a través de un relay cuando hay CGNAT.

&#91;embedded content: Arquitectura: programas, relay y servidor\]

El servidor reparte la voz y guarda el chat, los archivos y los permisos. Las imágenes se procesan aparte, para que un archivo malicioso no tumbe al servidor.

## Equipos y sistemas

Ventrik funciona en Windows XP (con Service Pack 3), Vista, 7, 8, 10 y 11. La versión moderna es para Windows 10 y 11 de 64 bits; la clásica, de 32 bits, sirve para XP a 8.1 y para Windows 10 de 32 bits. La PC mínima de referencia es la netbook HP 2133 (procesador VIA C7-M de un núcleo, Windows 7): si anda bien ahí, anda en cualquier PC.

| Uso | Equipo | Cómo funciona |
| --- | --- | --- |
| Programa en una PC común | Cualquier PC con Windows XP o posterior | Todo disponible, modo Visual incluido |
| Programa en la netbook | VIA C7-M o similar | Arranca en modo Rendimiento. El modo Visual se puede activar, con un aviso de que puede ir lento |
| Servidor | Recomendado: 2 núcleos o más y buena velocidad de subida | Todo disponible |
| Servidor en la netbook | VIA C7-M o similar | Modo servidor limitado, bajo riesgo del dueño |

### Modo servidor limitado

Si el servidor detecta un equipo débil (un solo núcleo o menos de 2 GB de RAM), se activa solo este modo:

| Qué | Límite inicial |
| --- | --- |
| Usuarios conectados a la vez | 8 |
| Archivos | Hasta 25 MB cada uno, una transferencia a la vez, velocidad limitada |
| Personalización del modo Visual | Apagada: no acepta imágenes animadas, fondos ni stickers |
| Historial del chat | 7 días |
| Tareas pesadas | Ninguna: no recodifica imágenes |

Los límites son valores iniciales, para ajustar después de probar en la netbook. El dueño puede subirlos con la opción "Quitar límites bajo mi responsabilidad", que avisa que el servidor puede trabarse. Aun sin límites, la voz conserva siempre la prioridad.

Windows XP y 7 ya no reciben parches de seguridad (desde 2014 y 2020). Un servidor en esos sistemas no debería abrir puertos a internet: conviene usar el relay (ver "Conectarse desde internet").

## Interfaz

La ventana tiene una sola organización y dos modos: los modos cambian cuánto hace el programa, no dónde está cada cosa. Los bocetos están en el [lienzo de diseño](https://claude.ai/artifact/8HLYD5VpXCLnrNvKdTKGe4).

Organización de la ventana:

- **Arriba:** el servidor (su nombre lleva ▾ y abre su menú; silenciado, muestra la campana tachada y debajo "Silenciado hasta las HH:MM"; ver "Silenciar avisos por servidor y por canal"), los botones (servidores, perfil, personalizar para los administradores, ajustes, desconectar), el selector de modo y el estado de la tecla para hablar ("Hablar con <tecla>" y "Transmitiendo"; silenciado, en lugar de "Transmitiendo" un aviso rojo, "Micrófono silenciado" o "Sin audio ni micrófono"; sin canal de voz, "Sin voz · micrófono apagado"). El micrófono y el audio ya no son botones de la cabecera: están en el panel de audio (ver "Panel de audio").
- **Izquierda:** la lista de canales en dos grupos, "Canales de texto" arriba y "Canales de voz" abajo (ver "Canales de voz y de texto"), y al final el grupo "Sin canal de voz". Los de voz muestran las personas adentro y su estado: habla, micrófono apagado, audio apagado, ausente o no molestar (sin sonidos de aviso), y se pliegan. La lista se desplaza con su propia barra; abajo queda fijo el panel de audio: tu canal con "Salir del canal", el medidor y cuatro botones (ver "Panel de audio"). Sin canal de voz, el panel dice "Sin canal de voz" (ver "Entrar a un servidor y a la voz").
- **Derecha:** el chat del canal de voz donde estás o del canal de texto que elegiste, con los archivos. Desde la etapa 12, ahí también se mira una pantalla compartida.
- **Abajo:** la barra de estado con la conexión, el ping, el cifrado, la voz y la verificación del servidor.

|  | Modo Rendimiento | Modo Visual |
| --- | --- | --- |
| Para qué | Voz, chat y archivos, nada más | Todo lo anterior, más la personalización |
| Inicio | Tus servidores con iniciales | Encabezado e icono animados de cada servidor, nombres con su fuente y tu tarjeta |
| Imágenes de perfil | No se descargan: se ven iniciales | Fotos y animaciones |
| Tarjeta de perfil | Plana, con iniciales | Fondo animado, efecto por fuera y animación al abrirse |
| GIFs y stickers | Sin selector para elegirlos; los que llegan no se descargan hasta que tocás Ver | Se ven en el chat |
| Fuentes y efectos de nombres | Apagados: letra normal | Encendidos |
| Fondos de perfil y del servidor | No se descargan | Se ven, animados |
| Color del tema del servidor | No se usa | Tiñe el canal donde estás, las barras de volumen y los bordes al pasar el mouse. El celeste de quien habla y el naranja de transmitir no cambian |
| Fondo del chat del servidor | No se descarga | Se ve detrás de los mensajes, oscurecido para que el texto se lea siempre. Cada persona lo puede apagar en Ajustes |
| Marcos de avatar | No hay | Se ven alrededor de la imagen, en la lista de canales y en el chat. Pueden ser del catálogo, hechos en el editor o una imagen animada propia |
| Transmitiendo | Etiqueta fija | La etiqueta late mientras transmitís |
| Animaciones | Ninguna | Solo mientras se ven; en pausa con la ventana minimizada o sin foco |
| Ritmo de la interfaz | Lo que se mueve solo (quién habla, el medidor del panel "Cómo transmitís" mientras está abierto) a 4 imágenes por segundo; el medidor del panel de audio es una barra fija (ver Voz, paso 3) | Hasta 30 imágenes por segundo, para que los elementos visuales se vean fluidos |

La tabla muestra los modos como vienen de fábrica; cada persona puede cambiar qué prende cada uno (ver "Qué prende y qué apaga cada modo"). Lo apagado se apaga, no se oculta: el programa no lo descarga ni lo procesa. Los dos modos tienen los mismos botones, canales, chat, tarjeta de perfil y controles de audio; solo cambia lo que se descarga y se anima. La única excepción es "Ocultar sus efectos" en la tarjeta de otro, que solo aparece en Visual porque en Rendimiento no hay efectos. En Ajustes hay un tercer modo, Automático: usa Visual, salvo en PCs débiles o con un juego en pantalla completa abierto, donde pasa solo a Rendimiento. Las PCs débiles arrancan en Rendimiento.

### Qué prende y qué apaga cada modo

Diseño del dueño del 2026-10-27. Capturas: docs/pantallas/Ajustes-apariencia.png (como viene, con la vista previa de Rendimiento), Ajustes-apariencia-visual.png (la vista previa de Visual) y Ajustes-apariencia-personalizado.png (Rendimiento con 5 cosas prendidas). Se cambia en Ajustes → Apariencia.

- **Cada modo es una lista guardada de interruptores:** qué se baja y qué se muestra.
  - De fábrica, **Rendimiento** tiene todo apagado y **Visual** todo prendido.
  - Cada persona puede cambiar las dos listas.
  - **Automático** y "Pasar a Rendimiento al abrir un juego en pantalla completa" siguen igual: pasan de una lista a la otra.
  - **Apagado quiere decir apagado, no oculto:** no se baja, no se decodifica y no ocupa memoria.
- **Los interruptores**, con una columna por modo:

| Grupo | Interruptor | Depende de | Pesado |
| --- | --- | --- | --- |
| Perfiles de la gente | Imágenes de perfil (apagado: iniciales) | | |
| | Animadas (GIF o WebP) | Imágenes de perfil | sí |
| | Marcos del avatar y efectos del editor | | |
| | Fondos de la tarjeta de perfil | | |
| | Animados | Fondos de la tarjeta | sí |
| | Efectos alrededor de la tarjeta y al abrirla | | sí |
| | Fuentes y efectos en los nombres | | |
| Servidores | Icono y fondo del encabezado (apagado: solo el nombre) | | |
| | Animados | Icono y fondo | sí |
| | Fondo del chat (se oscurece para que el texto se lea) | | |
| | Color de cada servidor (tiñe el canal donde estás y las barras) | | |
| | Stickers del servidor (apagado: se ve el nombre del sticker) | | |
| Chat | Imágenes y GIFs que manda la gente (apagado: se bajan solo si tocás "Ver") | | |
| | GIFs en movimiento (apagado: se mueven al pasar el mouse) | Imágenes y GIFs | sí |
| El programa | Animaciones del programa (Inicio, transiciones y brillos) | | sí |

- **Dependencias:** si el de arriba está apagado, el que depende de él se ve gris y no se puede tocar. Su valor queda guardado y vuelve a valer cuando se prende el de arriba.
- **"Pesado"** marca lo que más procesador y memoria gasta. Es solo una etiqueta, no un número estimado.
- **Botones de cada columna:** "Apagar todo" en Rendimiento y "Prender todo" en Visual; los dos dejan esa lista como venía de fábrica.
- **La tarjeta de cada modo dice cómo está:** "Todo apagado: consume lo mínimo", "Todo prendido…" o "Personalizado: 5 de 15 prendidas".
- **Vista previa en vivo:** a la derecha, cambia al tocar cualquier interruptor; debajo dice qué se mueve ("Se mueven: …" o "Nada se mueve").
- **Ajustes que salen de la pantalla** porque ahora están en la tabla: "Animaciones en modo Visual" y "Fondo del chat de cada servidor".
- **Ajuste nuevo:** "Frenar las animaciones con la ventana minimizada o sin foco", prendido de entrada. Ya era el comportamiento de Visual; ahora se puede apagar.
- **El buscador de Ajustes** encuentra cada interruptor por su nombre.
- **Reglas:**
  - **Solo cambia lo que ves vos.** Los demás te siguen viendo según lo que eligieron ellos.
  - **Orden de prioridad:**
    1. lo que el servidor no permite ("Qué se permite en este servidor") no se ve aunque lo prendas;
    2. después cuentan tus interruptores;
    3. después, "Ocultar sus efectos" desde la tarjeta de esa persona.
  - **Los cambios valen al instante, sin reiniciar.** Al prender algo, se baja solo lo que está a la vista, usando lo que ya esté guardado en la PC. Al apagar algo, se libera de la memoria enseguida; lo guardado en el disco se queda.
  - **Un interruptor nuevo de una versión nueva** arranca apagado en Rendimiento y prendido en Visual, aunque la persona haya cambiado los otros. Así Rendimiento sigue siendo lo más liviano después de cada actualización.
  - **Se guarda** con el resto de la configuración local, con número de versión del formato.
  - **Rendimiento de fábrica no anima nada**, tampoco lo que trae ImGui por su cuenta (el oscurecido de los cuadros, Ctrl+Tab, abrir y cerrar en dos pasos): lo vigilan las pruebas de pantalla (docs/estado.md, paso 3.3b). "Animaciones del programa" es lo único que las prende, y solo si la persona lo elige.

Aspecto, tomado de la imagen de referencia del autor: grafito azulado con acento naranja; botones con esquinas biseladas y borde oscuro; botones secundarios marcados solo por esquineros; triángulos claros en las esquinas de los paneles; rayado diagonal y cuadrícula tenue en los encabezados; avatares con una esquina cortada; medidores segmentados; selector de modo enmarcado en naranja, y las tipografías Barlow y Barlow Semi Condensed. El celeste marca a quien habla y el naranja, cuando transmitís vos. La ventana principal mide 1376 × 768 en el boceto y es la base de todas las demás pantallas. Medidas y colores exactos: docs/aspecto.md.

**Idiomas:** el programa sale en español e inglés. Todos los textos están en archivos aparte desde el principio, así sumar otro idioma no exige tocar el código y la comunidad puede traducirlo.

### Pantallas

El programa abre en **Inicio**. Ventrik se organiza por servidores; desde la etapa 14, Inicio suma arriba las pestañas "Servidores | Amigos" (ver "Amigos y llamadas directas").

| Pantalla | Qué tiene |
| --- | --- |
| Inicio (en los dos modos) | Tus servidores con su estado (en línea, cuántos hay, ping, tu último canal, solicitudes pendientes, tu servidor apagado), entrar con invitación, tu estado, el aviso de copia de tu identidad y el botón Reportar. En Visual suma el encabezado y el icono animados de cada servidor y tu tarjeta. "Entrar" respeta el ajuste "Al entrar a un servidor" |
| Servidor (Rendimiento o Visual) | Canales de voz y de texto, chat, archivos, tu audio y tarjetas de perfil. Capturas, cada una en docs/pantallas/Rendimiento*.png y Visual*.png: la base (en un canal de voz), -sin-voz (solo con el chat), -canal-texto, -por-voz (transmitiendo por voz), -microfono y -por-voz-microfono (el panel flotante "Cómo transmitís"), -salida (el volumen de salida), -silenciado y -sin-audio; y en Rendimiento, -silenciar-servidor, -silenciar-canal y -silenciar-chat-voz (los menús de silenciar), -avisos-servidor (el diálogo "Avisos de <servidor>") y -silenciados (cómo se ve lo silenciado). Todas muestran abierta la tarjeta de una persona con su volumen para vos |
| Explorar servidores | Lista pública con búsqueda por nombre, IP o región; red local; guardados. En Visual, al elegir un servidor se ven su fondo e icono animados |
| Hostear un servidor | Asistente de 7 pasos (nombre y acceso, conexión, capacidad y voz, seguridad, chat y archivos, pantallas y varias PCs, revisar e iniciar) y la pantalla final Invitar gente (ver "Hostear: asistente por pasos") |
| Ajustes | Una sección por vez, con un menú a la izquierda con buscador (ver "Ajustes"): General (con el idioma y "Al entrar a un servidor"), Apariencia, Teclas (hablar y susurros), Audio y micrófono, Avisos y sonidos (ver "Sonidos de los avisos"), Red, Descargas, Seguridad y Acerca de (con Reportar, la licencia y las bibliotecas). Las secciones de etapas futuras (Prestar esta PC, Amigos y privacidad, Copias de seguridad) llevan "PRONTO" en el menú y aparecen desactivadas y marcadas "EN DESARROLLO". Capturas: docs/pantallas/Ajustes-*.png, una por sección |
| Amigos (etapa 14) | Solicitudes, grupos y amigos; chat de a dos y de grupo; agregar amigo; llamada entrante; llamada grupal con personas, chat y cómo viaja; recuperar desde Google Drive (ver "Amigos y llamadas directas") |
| Bots (etapa 15) | Los bots del servidor, los oficiales para agregar y "Bot de otra persona", con sus permisos y ajustes; solo para quien tiene "Manejar bots" (ver "Bots") |
| Enviar archivo con contraseña | Pide la contraseña, analiza el comprimido con ClamAV en tu PC y lo envía; la contraseña no sale de tu PC |
| Reportar un error o sugerir | Error o idea, datos técnicos que se pueden ver antes de enviar, captura opcional y contacto opcional |
| Perfil y Personalizar servidor | Lo del modo Visual, incluida la tarjeta de perfil |
| Editor de efectos | Capas, vista previa ampliada y en tamaño real, límites y código para compartir, para el marco del avatar y el de la tarjeta |
| Notificaciones | Mensajes nuevos y conexión cortada mientras Ventrik no está al frente: las de Windows 10 y 11 en la versión moderna, una ventana propia en la clásica (ver "Notificaciones") |
| Compartir pantalla (etapa 12) | Elegir qué compartir (pantalla, ventana o una parte), la resolución, los cuadros y el cursor, con el lugar que hay hoy en el servidor; mirar en el lugar del chat (ver "Compartir pantalla") |

### Notificaciones

Avisan de lo que pasa mientras Ventrik no está al frente o está minimizado. Nunca avisan de tus propios mensajes, y tocarlas abre Ventrik en ese canal.

- **De qué avisan:** mensajes nuevos en canales de texto, mensajes del chat de tu canal de voz y conexión cortada (el servidor se cortó o dejó de responder). Los mensajes avisan según "Avisarme de" de cada servidor y canal: por defecto, solo las menciones (decisión del dueño, 2026-10-26; ver "Silenciar avisos por servidor y por canal"). En Ajustes → Avisos se apagan todas juntas o cada tipo por separado. No molestar las silencia.
- **Qué dicen:** si hay un solo mensaje, quién lo escribió y el texto ("Tano en #general: ¿Arrancamos la segunda?"). Si hay varios, se amontonan en una sola notificación por canal, que se va actualizando: "3 mensajes en #general de Tano", o "de Tano, Sofi y 2 más".
- **De dónde sale el texto:** de los canales de texto que no estás mirando, el servidor solo avisa el contador (ver "Canales de voz y de texto"). Cuando va a mostrar una notificación, el programa le pide esos mensajes nuevos al servidor como cualquier historial, con sus permisos y topes.
- **Windows 10 y 11 (versión moderna):** las notificaciones del sistema, las que salen abajo a la derecha y quedan en el Centro de actividades, con el nombre y el ícono de Ventrik. Se cargan con GetProcAddress; si no están, se usa la ventana propia.
- **Versión clásica (XP a 8.1 y Windows 10 de 32 bits):** una ventana propia, chica, abajo a la derecha, parecida a la de Windows 10 y con el aspecto de Ventrik (docs/aspecto.md). No le quita el foco a lo que estés haciendo, se cierra sola a los pocos segundos y se dibuja sin Direct3D, para no gastar en la netbook.
- **Para que Windows acepte las de un .exe sin instalador** (decisión del dueño, 2026-10-26):
  - Ventrik deja en el registro del usuario (no del sistema) su identidad de aplicación (`HKCU\Software\Classes\AppUserModelId\Ventrik`, con nombre, ícono y activador) y el activador COM que abre Ventrik al tocar una notificación, aunque esté cerrado;
  - escribe el ícono en %APPDATA%\Ventrik;
  - hay una por canal, que se reemplaza al actualizarse;
  - si algo de esto falla, se usa la ventana propia.
- **Privacidad:** Windows guarda sus notificaciones en un archivo propio, fuera de Ventrik y sin cifrar. Por eso, en Ajustes → Avisos se puede elegir que digan solo el canal y la cantidad, sin quién ni el texto.

### Silenciar avisos por servidor y por canal

Diseño del dueño del 2026-10-26. Capturas: docs/pantallas/Rendimiento-silenciar-servidor.png, -silenciar-canal.png, -silenciar-chat-voz.png, -avisos-servidor.png y -silenciados.png. Va con los avisos, en la etapa 3.

- **Tres niveles:**
  - **Servidor:** su nombre, con ▾, abre el menú:
    - Invitar gente;
    - Avisos del servidor…;
    - Silenciar servidor ▸;
    - Marcar todo como leído;
    - Bots y Configuración del servidor (solo admins);
    - Desconectar.

    El mismo menú sale con clic derecho en la tarjeta del servidor en Inicio; ahí, sin conexión, "Marcar todo como leído" va apagado y no está Desconectar. Hasta que el nombre del servidor viaje en el protocolo (etapa 4, con Hostear), el ▾ va junto a la dirección, que es lo que muestra la cabecera. Lo que todavía no existe va apagado: Invitar gente y Configuración del servidor (etapa 4) y Bots (etapa 15), con "PRONTO" para el dueño y "admins" para los demás.
  - **Canal de texto,** con clic derecho (para todos): arriba el nombre del canal, Marcar como leído (apagado si no hay nuevos), Silenciar canal ▸, Avisarme de ▸ y Editar canal (admins; el dueño también tiene ahí Borrar canal).
  - **Chat de un canal de voz,** con clic derecho en el canal: Entrar al canal, Ver el chat del canal, Silenciar el chat ▸ y Avisarme del chat ▸, y abajo la nota "Silenciar el chat no apaga la voz". Silenciar el chat no toca la voz: si entrás, escuchás a todos igual. Hoy el chat de un canal de voz se ve solo estando adentro, así que "Ver el chat del canal" se usa para volver a él desde un canal de texto, y va apagado en los demás.
- **Silenciar ▸:**
  - por 15 minutos, 1 hora, 3 horas, 8 horas o 24 horas, o "Hasta que lo vuelva a activar";
  - una casilla "Avisarme igual si me mencionan", tildada por defecto;
  - si ya está silenciado, arriba "Volver a activar los avisos" (no está en la captura, pero hace falta para deshacerlo).
- **Avisarme de ▸:** Igual que el servidor (lo que viene en los canales), Todos los mensajes, Solo menciones o Nada, con ✓ en lo elegido y lo elegido a la derecha de "Avisarme de". En el servidor, "Solo menciones" viene elegido (decisión del dueño, 2026-10-26): un mensaje común solo sube el contador.
- **Menciones:** tu nombre con @ adelante, y @todos y @aquí (también "@aqui", "@everyone" y "@here"), salvo que el servidor tenga tildado "No contar @todos ni @aquí como mención" (viene tildado).
  - No importan las mayúsculas, pero sí los acentos: en un servidor "José" y "Jose" pueden ser dos personas distintas, así que "@Jose" no nombra a José.
  - Después del nombre tiene que venir un espacio, un signo o el final: "@Tanoo" no nombra a Tano.
  - Se miran como mucho 32 "@" por mensaje.
  - En un canal que no estás mirando, el servidor solo manda el contador: para saber si te nombraron, el programa pide esos mensajes (como para mostrar el aviso), también con la privacidad prendida; el texto se usa solo para buscar la mención.
- **Silenciado:**
  - sin sonido ni aviso de Windows;
  - campana tachada junto al nombre;
  - el canal se ve apagado y sin contador de no leídos;
  - el servidor muestra "Silenciado hasta las HH:MM" debajo del nombre.
- **Diálogo "Avisos de <servidor>":**
  - "Silenciar el servidor" (No, o los mismos plazos) y "Avisarme igual si me mencionan";
  - Avisarme de: Todos los mensajes ("Suena y avisa con cada uno"), Solo menciones ("Cuando te nombran a vos", elegido por defecto) o Nada ("Solo el contador de no leídos");
  - "No contar @todos ni @aquí como mención";
  - "Sonidos de la voz de este servidor (entra o sale gente)";
  - la lista "Canales con su propio ajuste", con Quitar en cada uno y "Agregar canal" (un menú con los canales, cada uno con Silenciar ▸ y Avisarme de ▸); si no entra en la pantalla, la lista se desplaza con su propia barra. Desde Inicio, sin conexión, los nombres de los canales no se conocen: la lista dice cuántos tienen ajuste propio;
  - abajo, la nota: el canal manda sobre el servidor, y esto se guarda en tu PC (y en tu copia de seguridad), el servidor no sabe qué silenciaste.
- **Reglas:**
  - el ajuste del canal manda sobre el del servidor: un canal en "Todos los mensajes" avisa aunque el servidor esté silenciado;
  - el vencimiento se guarda con fecha y hora, así sobrevive a cerrar el programa;
  - todo se guarda en la base local (y en la copia de Drive), nunca en el servidor.
- **Ajustes → Avisos y sonidos:** la línea "Servidores y canales silenciados", con lo silenciado y "Ver y cambiar".
- En modo Visual va igual. En Amigos (etapa 14), el mismo menú sirve para silenciar a una persona o a un grupo.

### Ajustes

Diseño del dueño del 2026-10-26. Capturas: docs/pantallas/Ajustes-*.png, una por sección:
- general, apariencia, teclas, audio;
- avisos, más un grupo de avisos por captura: -avisos-chat, -avisos-llamadas y -avisos-servidor;
- red, descargas, prestar, seguridad, amigos, copias y acerca.

- **Una sección por vez.** A la izquierda va un menú con buscador ("Buscar un ajuste") y estos grupos:
  - PROGRAMA: General, Apariencia, Teclas;
  - AUDIO: Audio y micrófono, Avisos y sonidos;
  - CONEXIÓN: Red, Descargas, Prestar esta PC;
  - PRIVACIDAD Y SEGURIDAD: Seguridad, Amigos y privacidad, Copias de seguridad;
  - Acerca de, separado.

  Tocar una sección la muestra sola, a todo el ancho. Los cambios se guardan solos, como antes ("Los cambios se guardan solos", junto al título).
- **Secciones en desarrollo:** llevan "PRONTO" en el menú. Adentro solo tienen el título, "EN DESARROLLO" y "Llega en la etapa N", sin controles de mentira (decisión del dueño, 2026-10-27). Son Apariencia (10), Red (6), Descargas (5), Prestar (13), Seguridad (4), Amigos (14), Copias (14) y Acerca de (11).
- **Lo que hay desde el paso 3.5:**
  - **General:**
    - **la bandeja:** "Al cerrar la ventana" viene en "Seguir en la bandeja", como en la captura. El ícono junto al reloj aparece cuando la ventana se esconde y se va cuando vuelve. "Salir", del menú del ícono, cierra Ventrik de verdad;
    - **"Entrar solo al último servidor"** viene tildada: al abrir sin `--entrar`, entra al primero de la lista;
    - **"Al entrar a un servidor"**, con "Volver al último canal de voz", entra a ese canal si existe;
    - **el idioma** es el de Windows o uno elegido, y cambia al momento;
    - **un solo Ventrik:** abrirlo otra vez, con Ventrik ya abierto (aunque esté en la bandeja), muestra el que ya estaba.
  - **Teclas:**
    - Ctrl+M y Ctrl+D se ven, pero por ahora no se cambian;
    - "Pasar entre Rendimiento y Visual" y "Ir a los canales 1 a 9" llegan más adelante;
    - sin tildar "Las teclas funcionan aunque la ventana no esté activa", la tecla de hablar anda solo con Ventrik al frente.
  - **Audio y micrófono:**
    - el micrófono y la salida se eligen por nombre; si el elegido no está, se usa el de Windows;
    - **"Probar micrófono":** fuera de la voz abre el micrófono solo mientras se prueba. Dentro de la voz usa el medidor de la voz, que con la tecla se mueve al apretarla;
    - **"Escucharme mientras pruebo"** anda fuera de la voz;
    - **"Probar salida"** toca un tono de 1,2 s, sin abrir el micrófono;
    - **"Seguir transmitiendo al soltar":** No, 0,2 (de fábrica), 0,5 o 1 s;
    - **"Calidad de la voz"** es solo el texto de la captura (la decide cada servidor).
  - **Avisos y sonidos:**
    - qué avisa (canales de texto, chat de tu canal de voz, conexión cortada);
    - avisos privados;
    - "Mostrar avisos de Windows con la ventana minimizada";
    - **"Servidores y canales silenciados":** "Ver y cambiar" lista los servidores, y cada uno abre su diálogo "Avisos de…";
    - **los sonidos** llegan en el paso 3.7.
- **El buscador** filtra el menú y salta a la sección que tiene el ajuste buscado.
- **Apariencia** (capturas Ajustes-apariencia.png, -apariencia-visual.png y -apariencia-personalizado.png): la tabla de interruptores de cada modo, con la vista previa a la derecha (ver "Qué prende y qué apaga cada modo").
- **General** (captura Ajustes-general.png): "Abrir Ventrik al iniciar Windows", "Entrar solo al último servidor al abrir el programa", "Al cerrar la ventana" (seguir en la bandeja, junto al reloj, o salir), idioma y "Al entrar a un servidor".
  - **"Al minimizar la ventana"** (pedido del dueño del 2026-10-26; no está en la captura): una lista como la de "Al cerrar la ventana" y junto a ella, con "Dejarla en la barra de tareas" (de fábrica) o "Pasarla a la bandeja, junto al reloj". Con la segunda, minimizar esconde también el botón de la barra de tareas (el ícono grande en Windows 10 y 11, la barra larga en XP), y queda solo el ícono de Ventrik junto al reloj, al lado de los de volumen y red.
  - **El ícono junto al reloj** es el mismo al cerrar y al minimizar:
    - un clic abre Ventrik como estaba;
    - clic derecho muestra un menú con "Abrir Ventrik" y "Salir";
    - los avisos siguen igual, y tocarlos también abre la ventana escondida.
  - Va con `Shell_NotifyIcon`, que existe desde Windows 2000; en XP, con el tamaño de estructura que XP acepta. Si se reinicia el Explorador de Windows, el ícono se vuelve a poner (mensaje "TaskbarCreated"). No suma hilos ni despertares.
- **Audio y micrófono** ya no es una pantalla aparte: es una sección de Ajustes, en dos columnas (captura Ajustes-audio.png).
  - **Izquierda:**
    - Dispositivos: micrófono, salida de audio y "Probar salida". Si se desconecta un dispositivo, se pasa solo al de Windows.
    - Volúmenes: micrófono, voces que escuchás y sonidos de aviso. Son los mismos valores que el panel de audio y "Avisos y sonidos"; el de cada persona se cambia desde su tarjeta.
    - Calidad de la voz: la decide cada servidor.
  - **Derecha:**
    - Cómo transmitís: tecla para hablar o por voz; el medidor del micrófono, que se mueve solo mientras dura "Probar micrófono"; "Escucharme mientras pruebo"; la tecla y "Seguir transmitiendo al soltar".
    - Limpieza de la voz: quitar ruido de fondo con su nivel, cancelación de eco, ajustar solo el volumen del micrófono, y bajar el volumen de otros programas cuando alguien habla.
- **Avisos y sonidos:** arriba van las opciones generales, la línea "Servidores y canales silenciados" con "Ver y cambiar" (ver "Silenciar avisos por servidor y por canal") y el paquete de sonidos. Abajo, un selector de grupo (Voz · Chat · Llamadas y amigos · Servidor, cada uno con su cantidad) muestra un grupo por vez, con los avisos en dos columnas (ver "Sonidos de los avisos").

### Sonidos de los avisos

Diseño del dueño del 2026-10-26. No es para la etapa en curso: va con las notificaciones, en la etapa 3. Capturas: docs/pantallas/Ajustes-avisos.png (las opciones generales y el grupo Voz), Ajustes-avisos-chat.png, Ajustes-avisos-llamadas.png y Ajustes-avisos-servidor.png (los otros tres grupos).

- **27 avisos con sonido, en 4 grupos.** La lista, con cuándo suena cada uno y si viene encendido, está en esas cuatro capturas:
  - **Voz (14):**
    - entrás a un canal de voz;
    - salís del canal;
    - alguien entra a tu canal;
    - alguien sale de tu canal;
    - silenciás el micrófono;
    - activás el micrófono;
    - silenciás el audio;
    - activás el audio;
    - empezás a transmitir (tecla), apagado de fábrica;
    - dejás de transmitir (tecla), apagado de fábrica;
    - un admin te movió o te silenció;
    - te llega un susurro, suave para no tapar la voz;
    - se cortó la conexión;
    - volvió la conexión.
  - **Chat (4):**
    - mensaje nuevo en un canal;
    - te mencionan;
    - mensaje de un amigo;
    - terminó de bajar un archivo.
  - **Llamadas y amigos (5), "EN DESARROLLO" hasta la etapa 14:**
    - te están llamando;
    - estás llamando (espera);
    - termina la llamada;
    - solicitud de amistad;
    - un amigo se conectó, apagado de fábrica.
  - **Servidor (4):**
    - alguien pide entrar (admins);
    - alerta de seguridad (admins);
    - alguien comparte pantalla en tu canal, "PRONTO" hasta la etapa 12;
    - no se pudo (error).
- **Los sonidos de fábrica** los produce el autor en WAV. Van en assets/sonidos/, numerados en el orden de la lista (01_entrar_canal.wav … 27_error.wav).
  - **El programa los lleva en Opus, no en OGG** (decisión del dueño del 2026-10-27): `tools/sonidos.cpp` los pasa una vez a 48 kHz mono, les saca el silencio del principio y del final (menos a los dos de llamada, que se repiten cada 4 s) y los guarda en el formato propio VKSN, en recursos/sonidos/. Los 27 pesan unos 250 KB y van dentro del .exe. Los WAV originales (37 MB) no entran a git.
  - **VKSN:** "VKSN", versión 1, la cantidad de muestras (u32), la cantidad de paquetes (u16) y cada paquete de 20 ms de Opus a 48 kbps con su largo (u16). Se lee con el lector único y se rechaza si algo no cierra.
- **Reglas al sonar:**
  - "Mensaje nuevo" suena solo si ese canal no está a la vista.
  - Los de llamada son bucles de 4 s hasta atender o rechazar.
  - En No molestar no suena nada, salvo las llamadas de amigos si está tildado "Dejar sonar las llamadas de amigos en No molestar".
  - "Sonar por" permite un dispositivo de salida distinto del de la voz.
  - La mezcla del programa no agrega saturación cuando se superponen sonidos o se juntan con la voz. Eso es responsabilidad del programa, no del archivo.
- **Ajustes → Avisos y sonidos:**
  - volumen de los avisos;
  - "Sonar por";
  - No molestar, con la casilla de las llamadas de amigos;
  - "Mostrar avisos de Windows con la ventana minimizada";
  - paquete de sonidos: importar, exportar y volver a los predeterminados;
  - un selector de grupo que muestra un grupo por vez, con los avisos en dos columnas;
  - por cada aviso: casilla para encenderlo, botón para probarlo y "Predeterminado / Propio / Elegir un archivo…".
- **Sonidos propios:**
  - **Formatos y límites:** WAV o MP3; hasta 1 MB y 5 s (los de llamada, hasta 10 s). **OGG quedó apagado** el 2026-10-27: el fuzzing encontró que stb_vorbis 1.22 escribe fuera de su memoria con un OGG armado a propósito, y el proyecto no tiene arreglos desde 2021. El dueño decidió el mismo día dejarlo así: solo WAV y MP3, y stb_vorbis salió de third_party.
  - **Al elegir uno**, se decodifica con los decodificadores que trae miniaudio (dr_wav y dr_mp3), se rechaza si está dañado o se pasa de los límites, se pasa a 48 kHz mono y se guarda una copia en la carpeta del programa.
  - **Volumen:** no se nivela ni se limita, porque lo escucha solo el usuario. Solo aplica el volumen general de los avisos.
  - **Son locales:** nunca se mandan a otros ni a los servidores.
- **Paquete .vksonidos:** un zip con los audios y un JSON que dice a qué aviso va cada uno. Al importarlo se valida cada archivo con las mismas reglas, y se ignora cualquier archivo que no sea audio.
  - **El índice** se llama `paquete.json`: `{"version":1,"sonidos":{"mencion":"16_mencion.wav", …}}`. Los nombres de los avisos son los de los WAV de fábrica sin el número (entrar_canal, mencion, error…).
  - **Topes al importar:** el paquete hasta 30 MB y 40 archivos; cada archivo hasta 1 MB ya descomprimido (contra las bombas de zip); el índice hasta 64 KB. Un nombre con carpeta, ruta, "..", dos puntos o barra hace rechazar todo el paquete.
  - **Exportar** arma lo mismo con los sonidos propios, en WAV de 48 kHz mono, que cualquier programa abre.
- **Lo que suena desde el paso 3.7:** 1 a 10 (respetando "Sonidos de la voz de este servidor"), el 12 (susurro), el 13 (se cortó la conexión), el 15 (mensaje nuevo, solo si ese canal no está a la vista o la ventana no está al frente), el 16 (mención, también con el canal a la vista) y el 27 (no se pudo, cuando el servidor rechaza algo que pediste: entrar a un canal, escribir, crear, cambiar o borrar uno). Los demás están en la lista apagados, con PRONTO o "en desarrollo", hasta su etapa.
- **No molestar** viene en "Mientras juego en pantalla completa", como en la captura. En "Siempre" no suena nada.

### Hostear: asistente por pasos

Hostear un servidor es un asistente de 7 pasos y una pantalla final. Capturas: docs/pantallas/Hostear-1.png a Hostear-8.png, una por paso; el paso 4 tiene además Hostear-4-opciones.png, y el 6 va en Hostear-6-pantallas.png y Hostear-6-varias-pcs.png.

1. **Nombre y acceso.**
2. **Conexión:** puerto, placa de red, IPs, UPnP, probar desde internet y relay (ver "Red y puertos al hostear").
3. **Capacidad y voz:** subida, voces a la vez, máximo de usuarios, tablas de cuántos entran y calidad.
4. **Seguridad:** el nivel, con las opciones sueltas plegadas en "Qué incluye y cada opción por separado", y la identidad del servidor.
5. **Chat y archivos.**
6. **Pantallas y varias PCs:** opcional y en desarrollo (ver "Compartir pantalla"). "Varias PCs", un servidor sostenido entre varias PCs, todavía no está en este documento.
7. **Revisar e iniciar:** un resumen con "Cambiar" en cada paso, las opciones al iniciar y un aviso si no se exportó la identidad del servidor.

Después de iniciar, **Invitar gente** muestra el estado del servidor y las invitaciones; la primera invitación se crea sola al iniciar.

- **La lista de pasos va a la izquierda,** siempre visible, con el valor elegido debajo de cada uno. Se puede saltar a cualquier paso.
- **Abajo:** Atrás · "Dejar el resto como recomendado y revisar" · "Siguiente: <paso>". Nada se aplica hasta "Iniciar servidor".
- **"Probar desde internet" funciona antes de iniciar:** abre el puerto un momento.
- **Con el servidor ya creado,** "Configuración del servidor" muestra los mismos pasos como pestañas, sin Siguiente, y cada cambio se guarda solo.

## Voz

La voz se comprime con Opus, el estándar actual para voz por internet, y cada persona que habla usa unos 50 kbps de conexión, contando los encabezados del cifrado.

Recorrido de la voz:

1. El micrófono capta el sonido a 48 kHz, la calidad con la que trabaja Opus, así no hay que convertirlo. En los niveles de consumo Bajo y Mínimo capta a 16 u 8 kHz (ver "Consumo de la voz").
2. Se limpia el ruido de fondo y, si no usás auriculares, se cancela el eco (si Windows no informa qué tipo de salida es, queda encendida). Las dos cosas son opcionales porque son lo que más procesador gasta.
   - **El orden** (paso 3.4), en bloques de 10 ms y en el hilo de la voz:
     1. el volumen del micrófono;
     2. el eco (SpeexDSP, con lo que sale por los parlantes como referencia);
     3. el ruido;
     4. "Ajustar solo el volumen del micrófono" (el control automático de SpeexDSP). Aprende solo de los bloques con voz: los que están 10 dB sobre el ruido de fondo del micrófono sin limpiar y, con RNNoise, los que RNNoise también toma por voz. Entre palabras mantiene la ganancia, así que nunca sube un ventilador por sí solo;
     5. recién después se decide si se transmite, así que un ventilador o el teclado ya no abren "por voz". La decisión mira la voz ya sin ruido pero antes del volumen automático: así el volumen automático no abre "por voz" subiendo un ruido bajo. El medidor muestra el micrófono sin limpiar.
   - **"Quitar ruido de fondo" tiene tres niveles:**
     - Baja: SpeexDSP, −15 dB;
     - Media: RNNoise, y detrás SpeexDSP con −25 dB para el ruido parejo que RNNoise deja (el zumbido de un ventilador tiene tono, como la voz, y RNNoise lo deja pasar);
     - Alta: lo mismo con −35 dB, y además baja lo que RNNoise dice que no es voz;
     - RNNoise trabaja solo a 48 kHz: a 16 y 8 kHz, Media y Alta usan SpeexDSP con −25 y −35 dB.
   - **De fábrica:**
     - en PCs comunes, ruido Media, eco "Automática: se apaga con auriculares" y el volumen automático prendido;
     - en PCs de un núcleo, ruido y eco apagados (docs/optimizacion.md, etapa 3).
   - **Auriculares:** en Vista y más nuevos se reconocen por el tipo de salida que informa Windows. En XP no hay forma de saberlo y el eco queda prendido, salvo en PCs de un núcleo, que lo tienen apagado.
   - **"Bajar el volumen de otros programas cuando alguien habla":**
     - desde Windows 7, con las sesiones de audio de Windows; en XP no existe;
     - no baja nada con tu audio apagado;
     - los devuelve después de 1 s sin voces (para que la música no suba y baje con cada pausa), al salir de la voz y al cerrar Ventrik;
     - si mientras estaban bajos cambiaste el volumen de un programa, ese no lo toca.
   - **Medido en la PC de prueba**, cada 10 ms a 48 kHz: eco 1,8 % de un núcleo, ruido Media 2,5 % (RNNoise con SSE4.1 o AVX2 en la moderna), todo junto 4,6 %. A 16 kHz, todo junto 0,65 %.
3. Se transmite solo mientras apretás la tecla para hablar, o, en el modo por voz, mientras tu voz supera la sensibilidad que marcás en el medidor. El modo se elige en el panel flotante "Cómo transmitís" del panel de audio (ver "Panel de audio"), en los dos modos de la interfaz. En silencio no se envía casi nada.
   - **Por voz** (capturas: docs/pantallas/Rendimiento-por-voz.png, -por-voz-microfono.png y sus Visual): la cabecera dice "Activación por voz" en lugar de "Hablar con <tecla>"; el botón del modo dice "Por voz"; en "Cómo transmitís", en lugar de la tecla va un medidor propio con una marca (el umbral) y el control "Sensibilidad", que mueve esa marca, con el texto "Transmite cuando tu voz pasa la marca del medidor". "Transmitiendo" se prende solo mientras el nivel pasa la marca, y sigue 0,4 s después de callarte para no cortar el final de las palabras. Con la voz pasando la marca se manda también el pedazo de 20 ms anterior, para no comerse el principio. La tecla no hace nada en este modo.
   - **El medidor del panel de audio según el modo de la interfaz** (decisión del dueño, 2026-10-25; las capturas de Rendimiento muestran el medidor con divisiones y moviéndose: manda este texto):
     - **Rendimiento:** la fila del medidor es una barra naranja sin divisiones, fija: prendida mientras te escuchan y apagada si no, con la tecla y por voz, sin marca. El medidor en vivo con la marca se ve solo en el panel flotante "Cómo transmitís" mientras está abierto, que hace lo que antes hacían "Ajustar" y "Guardar"; al cerrarlo vuelve la barra fija. Así la ventana no se redibuja por el medidor.
     - **Visual:** la fila del medidor muestra el nivel con divisiones, con la marca en modo por voz, y se mueve hasta 30 imágenes por segundo.
   - Por debajo de la marca no se comprime ni se manda nada: solo se mide el volumen del micrófono, que es barato. El medidor en vivo se mueve solo con la ventana al frente; minimizada o con un juego adelante, la voz sigue igual pero el medidor no se dibuja.
   - El modo y la sensibilidad se guardan en la PC.
4. Opus comprime la voz en pedazos de 20 milésimas de segundo, a 24 kbps.
5. La voz se cifra y viaja al servidor.
6. El servidor la reenvía a los demás del canal tal como llegó: no la decodifica ni la mezcla.
7. Cada programa espera unas milésimas para ordenar los pedazos que llegan desordenados, los decodifica y mezcla las voces: como mucho las 4 más fuertes (por el nivel que trae cada paquete), sin saturar nunca. La espera se ajusta sola entre 40 y 200 ms según cuánto varía la red: sube enseguida con una traba y baja de a poco, acelerando o estirando la voz sin cambiar el tono, nunca tirando pedazos.

Ahorros automáticos:

- Sin canal de voz no se abren el micrófono, la salida de audio de la voz ni la conexión de la voz (UDP): queda solo la conexión de control y chat (TLS). Se abren al entrar a un canal de voz y se cierran al salir (ver "Entrar a un servidor y a la voz").
- Si silenciás a alguien, el servidor deja de mandarte su voz.
- Si apagás el audio, no recibís voz.
- La corrección de errores de Opus se activa solo cuando se pierden paquetes.

Meta de demora: hasta 80 milésimas de segundo entre que alguien habla y lo escuchás, en red local.

### Consumo de la voz

Comprimir la propia voz es lo que más procesador gasta, y en una PC débil puede ser mucho. Por eso cada programa elige cuánto gasta, en Audio y micrófono. Todos los niveles son Opus, así que todos se entienden entre sí: el que escucha descomprime cualquier nivel, y el servidor no tiene que imponer uno.

| Nivel | Cómo comprime | Netbook de referencia, hablando | PC común, hablando |
| --- | --- | --- | --- |
| Alto | Banda completa (48 kHz), complejidad 10 | 31 % de un núcleo | 2 % |
| Normal | Banda completa (48 kHz), complejidad 5 | 18 % | 1 % |
| Bajo | Banda ancha (16 kHz), complejidad 0 | 7 % | 0,5 % |
| Mínimo | Banda angosta (8 kHz), complejidad 0 | 4 % | 0,3 % |
| Automático (predeterminado) | Alto en PCs comunes; Bajo en las débiles (un núcleo o poca velocidad) | | |

- **Escuchar es barato:** descomprimir una voz en banda completa cuesta 3,7 % de un núcleo en la netbook (2,4 % si sale a 16 kHz, lo que hacen los niveles Bajo y Mínimo) y menos de 0,1 % en una PC común.
- **Por qué no otros códecs:** códecs más viejos como GSM gastan todavía menos, pero sumarlos obliga a que cada programa traiga todos (para entender a cualquiera), agrega bibliotecas y más puntos de ataque. Con Opus en Mínimo, la netbook queda holgada.
- Números medidos en la etapa 1 (ver docs/mediciones.md).

### Calidad dinámica

La calidad de las voces sube o baja sola según lo que aguante la subida del servidor. Es la opción predeterminada al hostear; el host puede elegir una calidad fija.

| Nivel | Opus | Paquete | Subida por oyente | Demora extra | Cómo suena (estimación) |
| --- | --- | --- | --- | --- | --- |
| Alta | 32 kbps | 20 ms | 60 kbps | Ninguna | Voz muy clara |
| Normal | 24 kbps | 20 ms | 50 kbps | Ninguna | Voz clara; la predeterminada |
| Media | 16 kbps | 40 ms | 30 kbps | 20 ms | Buena para voz |
| Baja | 12 kbps | 60 ms | 21 kbps | 40 ms | Se entiende bien, menos natural |
| Mínima | 8 kbps | 60 ms | 17 kbps | 40 ms | Como una llamada telefónica |

- **El servidor no recodifica:** sigue reenviando la voz tal como llega. Para bajar la calidad, le pide al programa de quien habla que comprima a menos kbps, y Opus cambia de nivel entre un paquete y el siguiente sin cortes. Como todos los oyentes reciben la misma voz, bajarla en quien habla baja la subida hacia todos.
- **Cómo mide la capacidad:** parte de la subida que anotó el host, sacada de una prueba de velocidad. Mientras funciona, vigila la cola de envío, la demora y las pérdidas que informan los programas: si se acumulan paquetes sin salir, baja la calidad aunque el número anotado diga otra cosa. Apunta a usar hasta el 85 % de la subida (valor inicial).
- **Baja rápido, sube despacio:** baja un nivel apenas detecta saturación y sube de a uno después de unos 20 segundos estables, para que la calidad no salte a cada rato.
- **Primero los canales grandes:** bajar la calidad en un canal de 100 personas ahorra 99 veces más que en uno de 2. Los canales chicos conservan la calidad el mayor tiempo posible.
- **Límites del host:** elige la calidad máxima y la mínima. Si con la mínima no alcanza, el servidor reduce las voces simultáneas por canal y, como último paso, no deja entrar a más gente, con un aviso claro.
- **A la vista:** la barra de estado muestra la calidad actual y avisa si está reducida por la conexión del servidor.

Los paquetes más largos (40 y 60 ms) ahorran encabezados, pero cada paquete perdido se lleva más audio; la corrección de errores de Opus compensa en parte.

### Susurro

Con una tecla aparte, hablás a una persona, a un grupo o a otro canal sin moverte del tuyo, como en Mumble o TeamSpeak.

- Hasta 4 teclas de susurro, cada una con su destino, en Ajustes → Teclas.
- Quien recibe un susurro lo escucha marcado, con el nombre de quien habla y un sonido distinto.
- El servidor decide quién puede susurrar a quién (permiso por rol y canal) y le aplica el tope de voces y la calidad dinámica igual que al resto.
- Gasta lo mismo que hablar: el servidor reenvía la voz solo a los destinatarios.
- **Lo que hay desde el paso 3.6:**
  - cada susurro va a personas (hasta 8, estén en el canal que estén) o a todos los de un canal de voz;
  - los destinos se guardan por nombre en esta PC y valen en cualquier servidor donde haya alguien con ese nombre o un canal con ese nombre;
  - todos pueden susurrar hasta que lleguen los permisos por rol (etapa 9);
  - el susurro sale aunque estés solo en tu canal, y gana sobre la tecla de hablar si se aprietan las dos;
  - quien recibe ve "Te susurra: Sofi" en el panel de audio, sin animación. El sonido es el del paso 3.7.

## Compartir pantalla (en desarrollo, etapa 12)

Llega en la etapa 12, con el programa ya completo. Desde antes, lo que ya se hace se arma de la forma que después sirve, para sumarla sin rehacer nada (ver "Preparación desde ahora"). Requisito del dueño: tiene que andar también en XP, aunque cueste más hacerlo. Capturas: docs/pantallas/PantallaCompartida.png (mirando), PantallaCompartida-dialogo.png (qué compartir) y Hostear-6-pantallas.png.

- **Cómo se ve:** quien comparte aparece con "EN VIVO" en la lista de canales. Quien toca "Mirar" ve la pantalla en el lugar del chat, con quién la comparte, en qué calidad sale y en cuál le llega, y los botones Compartir la tuya, Pantalla completa y Dejar de mirar. "Volver al chat" vuelve al chat del canal.
- **Formatos de video:**
  - Base para todos: VP8 con libvpx, comprimido por el procesador. Es libre de regalías (concesión de patentes de Google) y con licencia BSD, compatible con la GPL versión 3. Todo Ventrik lo trae, así cualquiera mira lo que comparte cualquiera, incluido XP. Hay que comprobar que ande en la versión clásica.
  - Copia de mejor calidad, opcional: H.264 comprimido por la placa de video con Media Foundation, para quienes pueden descomprimirla (desde Windows 7).
  - A quien mira desde XP le llega la copia VP8, de hasta 480p.
- **Captura:** con la duplicación de escritorio de DXGI desde Windows 8, y con GDI en XP y 7. Solo se comprime si la pantalla cambió, comparando por bloques; el cursor viaja aparte.
- **Qué se elige:** pantalla completa, una ventana o una parte; la resolución, de la original a 480p; 15, 30 o 60 cuadros por segundo; y si se ve el cursor. En XP, o sin una placa que comprima, el programa baja solo a lo que aguante el procesador.
- **Ritmo de la pantalla y de la interfaz:** la pantalla compartida va a su propio ritmo, el que se configuró, o si la PC que mira no da, uno más bajo que igual deje verla. El resto de la interfaz sigue a su ritmo (4 imágenes por segundo en Rendimiento): solo la zona de la pantalla compartida se redibuja a su ritmo, no toda la ventana.
- **Red del host:**
  - el servidor reenvía sin recomprimir, y solo a quien tocó "Mirar";
  - si nadie la tiene a la vista, se deja de enviar y de comprimir;
  - cuadros en capas, para bajar de 60 a 30 cuadros sin recomprimir;
  - una copia chica para poca conexión, PC débil, ventana chica o XP;
  - imágenes completas solo cuando entra alguien o se pierden datos, y de a pedazos;
  - ajuste a la conexión de cada uno.
- **La voz manda:** las pantallas usan solo lo que sobra de la voz, medido y con margen. Si no entra ni la copia chica, la persona espera su lugar.
- **Hostear (paso 6):** subida para pantallas "lo que sobra de la voz" (predeterminado), con tope fijo o apagada; 1 transmisión por canal y 2 por servidor por defecto; quién puede compartir; cuántos entran en cada calidad; y un repetidor opcional, un relay que recibe una sola copia y la multiplica.
- **Nunca de persona a persona:** dejaría a la vista la IP de cada uno. Entre amigos, en las llamadas directas de la etapa 14, sí se va directo, con aviso y la opción "Siempre por el relay".
- **Seguridad:** el descompresor va en el proceso de medios, con fuzzing, versión fija y actualización rápida: libvpx tuvo el CVE-2023-5217, usado en ataques reales. En XP hay más riesgo porque no tiene ASLR, así que Ajustes trae "No mirar pantallas en esta PC".
- **Medición, como en la etapa 1:** la HP 2133 compartiendo y mirando, y una PC moderna en 1080p y 1440p a 60 cuadros.
- **Pendiente de decidir:** compartir el sonido de la PC (en XP no hay una forma simple de capturarlo).

### Preparación desde ahora

Sin código que todavía no se use: solo la forma correcta de lo que ya se hace.

- **Paquete de medios (paso 2.4, segunda parte):** lo que va por DTLS lleva un tipo de flujo (voz; los demás quedan reservados) y un número de flujo. El servidor reenvía según el tipo y descarta los tipos que no conoce. La voz sale primero.
- **Funciones del protocolo:** un bit reservado para "pantalla", todavía sin comportamiento.
- **Etapa 8:** la subida del servidor se reparte en un solo lugar y por prioridades: primero la voz, después lo demás. Las pantallas se suman después como lo de menor prioridad.
- **Etapa 9:** el permiso "Compartir pantalla" figura en la lista de permisos, apagado y marcado "en desarrollo".
- **Etapa 10:** el proceso aparte para imágenes se arma como un "proceso de medios", para sumarle después el descompresor de video.
- **Interfaz:** en el chat de voz, el botón "Compartir pantalla" aparece apagado con "Pronto" (paso 2.5). En Hostear, el paso 6 muestra "Pantallas compartidas" apagado con "En desarrollo". Los textos van en lang/.
- **libvpx** no se agrega a third_party/ hasta la etapa 12.

## Chat y archivos

Cada canal tiene su chat, y los mensajes y archivos quedan guardados en el servidor el tiempo que elija el dueño: no guardar, 7 días, 30 días o siempre.

- **Historial:** quien entra ve el historial completo. Se carga por partes, primero lo más nuevo.
- **Topes del chat:** hasta 4000 bytes por mensaje, con saltos de línea; 10 mensajes cada 10 segundos por persona y 20 seguidos (después 2 por segundo) sumando todas las conexiones de una misma IP. Sin caracteres de control ni marcas que dan vuelta el texto.
- **Por defecto se guarda siempre,** hasta que el dueño elija otra cosa.
- **Archivos:** se suben al servidor y quedan en el chat. Viajan por partes, se reanudan si se corta la conexión y se guardan directo en el disco.
- **La voz va primero:** los archivos viajan con menor prioridad y un límite de velocidad configurable, así una descarga grande no mete retraso en la voz.
- **Tamaño máximo:** lo decide el servidor. Por ejemplo, 500 MB por archivo en un servidor normal y 25 MB en el modo limitado.

### Canales de voz y de texto

Hay dos tipos de canal, y el tipo no se cambia después de crearlo:

- **De voz:** tocarlo te hace entrar (o te mueve si estabas en otro), hablás ahí y tiene su chat.
- **De texto:** solo chat. No se entra: se lee y se escribe sin salir del canal de voz donde estás, o sin estar en ninguno.

En la lista de la izquierda van en dos grupos, "Canales de texto" arriba y "Canales de voz" abajo. Cada grupo tiene su botón + para crear un canal, solo para el dueño (y los admins desde la etapa 9). Los de texto llevan #, la cantidad de mensajes nuevos y el nombre en negrita si hay nuevos.

- **Elegir un canal de texto:** se marca en la lista, el chat pasa a ese canal y el encabezado dice "Canal de texto · seguís hablando en <canal de voz>", o "Canal de texto · no estás en un canal de voz". Tu canal de voz sigue con "estás acá", y tocarlo vuelve a su chat.
- **"Solo los admins escriben":** marca opcional de cada canal de texto, por ejemplo #anuncios, con un candado en la lista. Se elige al crear el canal y el dueño la cambia después con clic derecho, junto con el nombre. El resto lo lee con el campo de mensaje bloqueado, y el servidor lo verifica en cada mensaje. Hasta la etapa 9, "admins" es solo el dueño; los permisos por rol y canal siguen en la etapa 9.
- **Mismo chat que los de voz:** mismo guardado en SQLite, mismos topes, historial y tiempo de guardado. Para escribir en un canal de texto no hace falta estar adentro, pero el pedido pasa por los permisos del servidor (permisos.cpp) como cualquier otro.
- **Quién recibe cada mensaje:** el programa le dice al servidor qué canal de texto está mirando, uno por vez. El mensaje completo va a quienes están en ese canal de voz o miran ese canal de texto. A los demás que pueden leerlo, el servidor solo les avisa qué canal tiene mensajes nuevos y su contador, nunca el texto: como mucho un aviso por segundo por persona, con todos los canales que cambiaron juntos.
- **Mensajes nuevos:** cada canal tiene un contador que solo sube con cada mensaje; borrar mensajes viejos no lo baja, y sigue subiendo aunque el servidor esté en "no guardar". Cada programa guarda en su PC hasta qué número leyó en cada canal, y los nuevos son la diferencia. La primera vez que ves un servidor, todo cuenta como leído.
- **Protocolo y base:** cada canal lleva su tipo (voz o texto), la marca de "solo admins" y su contador. Se suma antes de publicar la primera versión, así no hay que negociarlo entre versiones. El servidor rechaza usar un canal de texto como canal de voz, con un motivo propio ("tipo de canal").
- **Borrar un canal de texto** borra sus mensajes, y quien lo estaba mirando vuelve al chat de su canal de voz.
- **Un servidor nuevo** arranca solo con "General", que es de voz. Los canales de texto los crea el dueño; un "general" de texto no puede convivir con "General" de voz, porque los nombres no distinguen mayúsculas.
- **En los dos modos es igual,** porque es texto.

### Entrar a un servidor y a la voz

Estar en un servidor y estar en la voz son dos cosas separadas. Capturas: docs/pantallas/Rendimiento-sin-voz.png y Visual-sin-voz.png (sin voz), Rendimiento.png y Visual.png (en un canal de voz), y Ajustes-general.png (el ajuste).

- **Al entrar a un servidor no se entra a un canal de voz:** por defecto quedás solo con el chat, en el último canal de texto que miraste ahí, o en #general si no hay. Lo elige Ajustes → General → "Al entrar a un servidor":
  - "Quedarme solo en el chat, sin entrar a un canal de voz" (predeterminado);
  - "Volver al último canal de voz en el que estuve".

  El botón "Entrar" de Inicio respeta este ajuste. Hasta que exista la pantalla Ajustes (etapa 3) vale el predeterminado.
  - Si el último canal de texto ya no existe, se mira #general; si no hay, el primer canal de texto. Si el servidor no tiene ningún canal de texto, en el lugar del chat va el cartel "Este servidor todavía no tiene canales de texto. Tocá un canal de voz para entrar." (decisión del dueño).
- **Entrar a la voz:** tocar un canal de voz te hace entrar. Con voz, la primera fila del panel de audio dice "<canal> · <ping>" con el botón "Salir del canal", que te saca de la voz sin salir del servidor. "Desconectar" (en la cabecera) sigue siendo salir del servidor.
- **Sin voz:**
  - la primera fila del panel de audio dice "Sin canal de voz", con "Volver a <último canal>"; abajo, la línea "Micrófono y audio apagados hasta que entres a un canal. Si te silenciás antes, entrás silenciado." y los mismos cuatro botones (ver "Panel de audio");
  - en la cabecera, en lugar de "Hablar con <tecla>" y "Transmitiendo", va "Sin voz · micrófono apagado";
  - la tecla para hablar no hace nada;
  - la barra de abajo dice "Voz: no estás en un canal";
  - el subtítulo de un canal de texto dice "no estás en un canal de voz".
- **Sin voz no se usa nada de la voz:** ni el micrófono, ni la salida de audio de la voz, ni la conexión de la voz (UDP). Solo queda la conexión de control y chat (TLS). Todo eso se abre al entrar a un canal de voz y se cierra al salir.
- **Lista de canales:** al final, el grupo "Sin canal de voz · N" con quienes están en el servidor sin voz, sin mostrar qué canal de texto leen. "N conectados" cuenta a todos.
- **En el servidor:** los que están sin voz cuentan para el "Máximo de usuarios", pero no para la capacidad de voz (la que se calcula con la subida, ver "Servidores grandes"). Mandar al canal AFK por inactividad sigue siendo solo para quien está en un canal de voz.

### Panel de audio

Abajo a la izquierda, fijo debajo de la lista de canales. Capturas: docs/pantallas/Rendimiento.png (la base), -por-voz, -microfono, -por-voz-microfono, -salida, -silenciado, -sin-audio y -sin-voz, cada una también en Visual. En Rendimiento, la fila del medidor es la barra fija de "Voz", paso 3, aunque las capturas muestren el medidor con divisiones.

- **Tres filas:**
  1. "<canal> · <ping>" y el botón "Salir del canal", que te saca de la voz sin salir del servidor. Sin voz: "Sin canal de voz" y "Volver a <último canal>".
  2. El medidor del micrófono en una barra fina; en modo "Por voz" muestra la marca de la sensibilidad (en Rendimiento, barra fija: ver "Voz", paso 3). Sin voz, en su lugar va la línea "Micrófono y audio apagados hasta que entres a un canal. Si te silenciás antes, entrás silenciado."
  3. Cuatro botones:
     - silenciar el micrófono (Ctrl+M);
     - silenciar el audio y el micrófono (Ctrl+D);
     - el modo de transmisión: muestra la tecla (por ejemplo "Alt izq.") o "Por voz", con ▴; abre el panel flotante "Cómo transmitís";
     - la salida (por ejemplo "85 %"), con ▴; abre el panel flotante del volumen de salida.
- **Paneles flotantes:** se abren encima de la fila y se cierran al tocar afuera.
  - **"Cómo transmitís":** "Tecla para hablar | Por voz"; con la tecla, "Tecla <tecla>" y "Cambiar tecla"; por voz, la sensibilidad con su propio medidor y la marca. Abajo, "Volumen del micrófono" de 0 a 200 % y la nota "Dispositivo y supresión de ruido: Ajustes → Audio y micrófono".
  - **Salida:** "Volumen de salida" y la nota "El volumen de cada persona se cambia tocando su nombre".
  - **Por persona** (capturas Rendimiento*.png, con la tarjeta de Tano abierta): tocar el nombre de otra persona en la lista de canales abre su tarjeta (ver "Tarjeta de perfil") al lado de la lista. Abajo de la tarjeta van "Volumen de <nombre> para vos", de 0 a 200 %, y el botón "Silenciar para mí".
    - Silenciada para vos, su fila muestra el parlante tachado y el servidor deja de mandarte su voz.
    - Se recuerda por servidor y por nombre, hasta 16 personas por servidor. Por nombre porque hasta la etapa 4 no hay identidad fija; ahí pasa a la identidad.
- **Silenciado:**
  - el botón queda rojo con el ícono tachado;
  - en tu fila del canal se ve el micrófono o los auriculares tachados, también para los demás;
  - en la cabecera, "Transmitiendo" pasa a ser un aviso rojo: "Micrófono silenciado" o "Sin audio ni micrófono".
- **Como en Discord:** silenciar el audio silencia también el micrófono, y activar el micrófono estando sin audio activa las dos cosas. Sin audio no recibís voz (ver "Ahorros automáticos").
- **Sin canal de voz** se ven los mismos cuatro botones: si te silenciás antes de entrar, entrás silenciado.

### Canales y nombres

- **Canales:** el servidor arranca con "General", que se puede renombrar pero no borrar. Al borrar un canal de voz, quienes estaban ahí pasan a General. Hasta 60 canales, sumando los de voz y los de texto, con nombres únicos entre todos.
- **Nombres de personas y canales:** de 1 a 32 caracteres, sin espacios al principio, al final ni dobles, y sin controles, caracteres invisibles ni marcas de dirección de texto. Dos nombres que solo cambian en mayúsculas, en letras anchas o en cómo está escrita una tilde cuentan como el mismo. Las letras parecidas de otros alfabetos (como la "а" cirílica) quedan para la etapa 9.
- **Dueño, provisorio hasta las identidades (etapa 4):** la primera vez que arranca, el servidor crea un código de dueño de 128 bits al azar y lo muestra en su consola esa sola vez; después muestra solo dónde está guardado, para que no salga en una captura junto con la huella. Quien se presenta con ese código administra los canales desde adentro del programa, como en Discord. Un código equivocado cuenta como intento fallido para la protección contra ataques.

### Antivirus antes de publicar

El antivirus es uno solo y no se puede cambiar: **ClamAV, incluido en el programa y en el servidor**. Se activa con los niveles Reforzado, Máximo o Personalizado.

**Firmas blindadas.** Antes de cada análisis, el programa verifica la firma digital de ClamAV y la de su base de virus (las bases oficiales de ClamAV ya vienen firmadas). Si algo fue modificado o reemplazado, no se analiza nada: en la PC de un usuario, no puede enviar comprimidos con contraseña; en el servidor, se aplica la regla "Si no se puede analizar". Así, un programa falso que se haga pasar por antivirus no sirve, porque no tiene la firma.

**Se abre solo cuando hace falta.** ClamAV no queda corriendo: se abre para analizar y se cierra al terminar. Mientras corre usa más de 1,2 GB de memoria ([documentación de ClamAV](https://docs.clamav.net/manual/Installing/Docker.html)) y tarda unos segundos en cargar la base. En el modo servidor limitado está apagado.

**Archivos sin contraseña** (los analiza el servidor):

1. El archivo llega a una carpeta de cuarentena: nadie lo puede ver ni descargar. Quien lo subió ve "Analizando".
2. El servidor verifica las firmas de su ClamAV y lo analiza.
3. Si está limpio, se publica en el chat. Si ClamAV encuentra algo, se borra y se avisa a quien lo subió y a los administradores.

**Comprimidos con contraseña (RAR, ZIP, 7z)** (los analiza quien los envía):

1. El programa pide la contraseña.
2. Verifica las firmas de su ClamAV, abre el archivo en una carpeta temporal aislada de esa PC y lo analiza.
3. Si está limpio, lo envía y borra la contraseña y la carpeta temporal. La contraseña nunca llega al servidor, así que el dueño no puede ver el contenido.

El chat lo marca como "Con contraseña: lo analizó quien lo envió". En el nivel Máximo se rechazan, porque el servidor no puede comprobar ese análisis: la verificación de firmas la hace el mismo programa, y un programa modificado podría saltearla. En un ZIP, los nombres de los archivos de adentro quedan a la vista aunque tenga contraseña; en RAR (cifrando nombres) y 7z (cifrando encabezados), no.

- **Si no se puede analizar** (firmas que no coinciden, poca memoria libre, tarda demasiado o falla), el dueño elige: no publicarlo, o publicarlo marcado como "sin analizar".
- **Windows XP:** la versión oficial de ClamAV para Windows funciona desde Windows 7 ([preguntas frecuentes de ClamAV](https://docs.clamav.net/faq/faq-win32.html)). El instalador clásico (XP a 8.1) trae la adaptación de [Netfarm](https://oss.netfarm.it/clamav), que funciona desde Windows NT4, compilada y firmada por nosotros a partir de su código. Usa la misma base de virus oficial que la versión moderna:
  - Se sigue la rama de soporte largo de ClamAV (hoy la 1.4, con descargas de la base permitidas hasta agosto de 2028, según su [política de fin de vida](https://docs.clamav.net/faq/faq-eol.html)). Las versiones viejas quedan bloqueadas para bajar la base, así que hay que actualizar ClamAV antes de esa fecha.
  - La base se baja con su actualizador (freshclam), con su propia biblioteca de cifrado, sin depender del cifrado viejo de XP. Si eso falla, el programa la pide al servidor al que está conectado, y vale igual porque la base viene firmada por ClamAV.
  - La firma de ClamAV la verifica el programa con una clave propia, sin depender de Windows, así que funciona igual en XP.
  - Riesgos: la adaptación es experimental (su autor la llama beta) y, al ser de 32 bits, puede quedarse corta de memoria con la base completa. El pico de memoria es momentáneo y no depende del tamaño del archivo, sino de cargar la base. En la práctica, una PC con XP solo usa ClamAV al enviar comprimidos con contraseña a un servidor con protección; los demás archivos los analiza el servidor. ClamAV se abre solo para eso, nunca recargando la base en caliente, y hay que probarla en XP real antes de publicar.
- **Límite honesto:** un antivirus no detecta todo, sobre todo lo muy nuevo. Es una capa más, no una garantía.

## Perfiles y personalización

Tu perfil se guarda en tu PC y se manda a cada servidor al que entrás: no hay cuentas ni registro central.

**Qué tiene un perfil:** nombre visible, imagen (puede ser animada), fondo vertical, descripción de hasta 190 caracteres, fuente del nombre, efecto y colores. En modo Visual se muestra como una credencial: el fondo a la izquierda con la imagen encima, y el nombre, la descripción y las insignias a la derecha.

- **Fuentes:** un catálogo fijo que viene con el programa: Normal, Bungee, Pacifico, Pixel, Marcador y Righteous. Nadie sube fuentes propias, así no hay que descargar fuentes ajenas y los nombres siguen siendo legibles.
- **Efectos:** ninguno, degradado, brillo, destello o glitch.

### Tarjeta de perfil

Al tocar el nombre de alguien se abre su tarjeta: su avatar, su nombre, dónde está ("En Partida 1"), su descripción y sus insignias ("Fundador del servidor", "Miembro desde 2025"); abajo, "Volumen de <nombre> para vos" y "Silenciar para mí" (ver "Panel de audio"). Lo que todavía no existe (descripción, insignias, ausente) se suma cuando llega su etapa. En modo Visual, cada persona elige cómo se ve la suya:

| Parte | Opciones | Cómo se dibuja |
| --- | --- | --- |
| Fondo animado de la tarjeta | Liso, Topográfico, Radar, Lluvia, Estática o una animación propia | Los del catálogo los dibuja el programa con dos colores a elección. La propia es un GIF o WebP de hasta 4 MB y 680 × 400, que el servidor recodifica |
| Efecto por fuera | Ninguno, Aura, Chispas, Marco giratorio, Rayos o uno propio hecho en el editor | Lo dibuja el programa, sea del catálogo o del editor |
| Al abrirse | Aparecer, Deslizar, Destello o Glitch | Dura menos de medio segundo |
| Marco del avatar | Ninguno, Circuito, Esquinas, Órbita, Brasas, uno propio hecho en el editor o una imagen animada propia | Los del catálogo y los del editor los dibuja el programa; la imagen propia es un WebP o GIF con transparencia de hasta 512 KB. Va por fuera de la imagen, así el aro celeste de "está hablando" se sigue viendo |

- **Por qué un catálogo:** lo que dibuja el programa no se descarga, pesa casi nada y no puede traer un archivo armado para atacar. Solo la animación propia es un archivo, y pasa por la misma recodificación en un proceso aparte que las demás imágenes.
- **Siempre se lee:** el programa oscurece el fondo detrás del texto, así el nombre y la descripción se leen con cualquier fondo.
- **Control:** el servidor decide si permite fondos animados, efectos por fuera, marcos de avatar y animaciones propias, y de qué tamaño. Cada persona puede tocar "Ocultar sus efectos" en la tarjeta de otro.
- **Consumo:** las animaciones corren solo mientras la tarjeta está abierta y a la vista, a 30 cuadros por segundo como máximo, y se pausan con la ventana minimizada o sin foco. Si en Windows están desactivadas las animaciones, se muestran quietas. En Rendimiento no existen: la tarjeta es plana.

### Editor de efectos

Cada persona puede armar sus propios marcos del avatar y de la tarjeta en un editor por capas. El resultado no es un GIF sino una receta que dibuja el programa: pesa menos de 1 KB, se ve nítida en cualquier tamaño y se comparte con un código.

| Capa | Qué hace | Dónde se puede usar |
| --- | --- | --- |
| Trazo del borde | Trazos que recorren el borde, con grosor, largo, velocidad y sentido | Avatar y tarjeta |
| Brillo que recorre | Una luz que da vueltas alrededor | Avatar y tarjeta |
| Esquinas | Escuadras que laten | Avatar y tarjeta |
| Partículas | Chispas, brasas o estrellas que suben, caen o rodean | Avatar y tarjeta |
| Órbita | De 1 a 4 puntos que giran | Avatar y tarjeta |
| Halo | Resplandor que late | Solo tarjeta: en el avatar se confundiría con el aro de "está hablando" |
| Imagen animada | WebP o GIF propio con transparencia, hasta 512 KB y 128 × 128 | Solo avatar: una del tamaño de la tarjeta pesaría varios MB |

- **Límites:** hasta 4 capas y 24 partículas por efecto, a 30 cuadros por segundo como máximo. Nada puede destellar más de 3 veces por segundo, el límite de las pautas de accesibilidad para no provocar convulsiones ([WCAG 2.3.1](https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold.html)).
- **El catálogo es lo mismo:** los marcos (Circuito, Esquinas, Órbita y Brasas) y los efectos por fuera (Aura, Chispas, Marco giratorio y Rayos) están hechos con el editor, así que se puede partir de uno y cambiarlo.
- **Código para compartir:** cada efecto se copia como un código de texto (por ejemplo VKFX1-A1C1-G1EQ-9XD4-K2PA) y se pega en otro programa. El programa y el servidor lo validan como cualquier mensaje de red: tamaño máximo fijo, cada valor dentro de su rango y prueba de fuzzing. La imagen animada no va en el código: viaja como archivo y pasa por la recodificación en un proceso aparte.
- **Control:** el servidor decide si permite los efectos hechos en el editor y las imágenes propias en los marcos, y de qué tamaño. Cada persona puede ocultar los efectos de otro, y si Windows tiene las animaciones desactivadas se ven quietos. En Rendimiento no hay marcos ni efectos.

### Personalización del servidor

**Qué personaliza un servidor** (solo los administradores): icono (también animado), nombre, descripción, color del tema, fondo del encabezado, fondo del chat, hasta 50 stickers propios, qué efectos se permiten y los límites de tamaño.

| Archivo | Límite inicial |
| --- | --- |
| Imagen de perfil | 2 MB, se recorta a 256 × 256 |
| Fondo de perfil | 5 MB, vertical, 400 × 600 |
| Animación propia de la tarjeta | 4 MB, 680 × 400 |
| Fondo del encabezado del servidor | 8 MB, imagen o animación (GIF o WebP) |
| Fondo del chat del servidor | 4 MB, imagen o animación lenta (GIF o WebP) |
| Icono del servidor | 1 MB, 256 × 256, imagen o animación (GIF o WebP) |
| Imagen del marco del avatar | 512 KB, 128 × 128, WebP o GIF con transparencia |
| Sticker | 512 KB |
| GIF en el chat | 8 MB |

El servidor recodifica cada imagen al recibirla: la lleva a un tamaño fijo, limita los cuadros por segundo y la duración, y guarda una versión quieta. Lo hace en un proceso aparte, sin permisos, por seguridad: el "proceso de medios", que en la etapa 12 suma el descompresor de video.

## Seguridad y cifrado

Todo viaja cifrado entre cada programa y el servidor. El servidor decide cómo se cifra, y el programa no acepta nada por debajo de un mínimo fijo.

No es cifrado de punta a punta: el servidor descifra para reenviar. Por eso quien controla el servidor puede leer y escuchar todo; el servidor tiene que ser propio o de alguien de confianza.

**Cómo se cifra:**

- **Chat, control y archivos:** TLS 1.3, el mismo estándar que usan los bancos en internet.
- **Voz:** DTLS 1.2, una versión de TLS pensada para paquetes de voz.
- **Algoritmo:** AES-GCM si el procesador acelera AES; ChaCha20-Poly1305 si no, como en procesadores viejos. Los dos son igual de seguros; solo cambia la velocidad. El servidor decide cuáles se permiten y, entre esos, el programa pone primero el que su procesador hace más rápido.
- **Igual en todos los sistemas:** la biblioteca de cifrado va dentro del programa y del servidor. Un servidor en Windows XP cifra igual que uno en Windows 11.

| Lo decide el servidor | Lo hace el programa |
| --- | --- |
| Versión de TLS y algoritmos permitidos | Cifra y descifra con lo que eligió el servidor |
| Certificado y huella | Comprueba que la huella coincida con la de la invitación |
| Cada cuánto se renuevan las claves de la voz | Aplica cada clave nueva |
| Versión mínima del programa | Avisa si hay que actualizar |

El mínimo del programa es TLS 1.3 o DTLS 1.2, con AES-GCM o ChaCha20. Sin ese mínimo, un servidor falso podría pedir conectarse sin cifrado o con uno débil; a eso se le llama ataque de degradación.

**La clave de la voz:**

- **No viaja:** al entrar, el servidor le manda al programa, por la conexión TLS 1.3 del chat, una identificación al azar (16 bytes). Los dos sacan la misma clave de esa conexión con el exportador de claves de TLS (RFC 8446), así que la clave no pasa por la red ni por los buffers de mensajes.
- **DTLS 1.2 con esa clave compartida (PSK):** solo AES-GCM o ChaCha20-Poly1305. Conectar la voz no cuesta una firma digital, algo importante si el servidor es una PC débil.
- **Se ata a la conexión:** el servidor sabe de quién es cada sesión de voz por la clave, y además exige que el UDP llegue desde la misma IP que la conexión TCP. Así decide quién habla según la conexión, y el paquete no dice quién es.
- **Se renueva cada hora:** el servidor manda una identificación nueva; la vieja deja de servir para conectar.
- **Antes de gastar nada:** el servidor solo atiende UDP de una IP que tiene una conexión presentada, pide una cookie antes del saludo (así nadie lo usa para atacar a otros), tiene tope de saludos a medio hacer por IP y en total, tope de paquetes por persona y descarta los repetidos.
- **Formato de los medios, dentro de DTLS:** `u16 tipo` y el cuerpo. Los paquetes de medios llevan el tipo de flujo (1 = voz; el 2 queda reservado para compartir pantalla) y el número de flujo (la voz va en el 0). El servidor reenvía según el tipo, descarta los tipos y flujos que no conoce sin cortar la sesión, y manda primero la voz. El programa manda `MedioSubida` (tipo, flujo, número de paquete de 16 bits, tiempo en milisegundos del reloj de audio del que habla, de 16 bits y con vuelta, nivel del paquete de 0 a 127 en dB sobre −127 dBov, y hasta 400 bytes de Opus); el servidor reenvía `MedioBajada` (quién habla, que lo pone el servidor, y el mismo medio) a los demás del mismo canal. `Ping` y `Pong` cada 5 segundos (cada 15 en ahorro: ventana minimizada, juego en pantalla completa o PC bloqueada) mantienen abierto el camino (NAT) y miden la conexión.
- **Límites conocidos de la regla de la IP:** casas con dos salidas a internet, algunos CGNAT y servidores con varias placas de red pueden quedarse sin voz. Se revisan con el relay (etapa 6).

La invitación lleva la huella del servidor. Si al conectar no coincide, el programa avisa y no se conecta; conviene pasar la huella por otro medio, como WhatsApp o en persona.

La huella es el SHA-256 de la clave pública del servidor, no de su certificado: el certificado se puede rehacer sin que cambie. Se escribe con 52 letras y números en grupos de 4 (base32, que no usa 0, 1, 8 ni 9 para que no se confundan con O, I, B o g), por ejemplo `CTBC-P44I-2FC4-...`.

### Medidas de seguridad del servidor

- **Acceso:** contraseña de servidor y de canales, con una espera cada vez más larga después de cada intento fallido. Roles y permisos por canal.
- **Contra saturación y ataques:** límites de mensajes, conexiones y archivos por usuario. El servidor no responde con datos grandes a quien no demostró su dirección, así nadie lo usa para atacar a otros. Los paquetes de voz inválidos se descartan antes de procesarlos.
- **Contra inundaciones de conexiones**, siempre activo, en todos los niveles y en todos los Windows:
  - **Ritmo por IP:** cada IP puede abrir unas pocas conexiones de golpe (16) y después una por segundo. Lo que se pase se corta antes de gastar procesador en el cifrado.
  - **Bloqueo temporal:** una IP que falla 5 veces en un minuto queda bloqueada 5 minutos. Cuentan como falla no hablar TLS, no saludar a tiempo, mandar un saludo ilegible, mandar demasiados datos antes de saludar o cifrar y cortar sin saludar; una versión vieja del programa no cuenta. Un saludo bueno no borra las fallas, para que no se pueda esquivar el bloqueo intercalándolos.
  - **Tope global de saludos:** 20 por segundo entre todas las IP, porque cada saludo cuesta una firma y la netbook se satura antes que la red. Se cobra cuando el otro empieza a hablar, no al conectarse. Los que ya están conectados siguen hablando, y las IP que saludaron bien en las últimas 24 horas tienen además una reserva de 10 por segundo, para que un ataque no deje afuera a los de siempre. La reserva también tiene techo, porque cualquiera puede saludar bien una vez, y una IP que falla pierde ese lugar.
  - **Antes de saludar,** nadie puede mandar más de 16 KiB (un programa de verdad manda menos de 2 KiB), y cada vuelta del servidor lee como mucho 64 veces por conexión, así nadie lo traba mandando datos sin parar.
  - **Límites conocidos:**
    - quien intenta 5 veces en un minuto con una huella equivocada queda bloqueado 5 minutos, y con él su IP: por eso el programa nunca reintenta solo después de ese aviso;
    - detrás de una IP compartida por muchos (CGNAT de los celulares), un atacante puede bloquear también a los demás;
    - el relay y las pruebas del directorio (etapas 6 y 7) van a necesitar su propia regla, porque muchas personas llegan desde la misma IP.
  - **Topes de conexiones:** 4 a medio saludar y 16 en total por IP (detrás de un módem puede haber varias personas), 64 a medio saludar en total y 512 conexiones.
  - Durante un ataque, la consola avisa cada IP una vez cada tanto, y no en cada intento.
  - **Límite honesto:** si el ataque llena la conexión a internet del host, ningún programa en esa PC lo puede frenar, porque el tráfico no llega a pasar por el servidor. Para eso está el relay (ver "Conectarse desde internet").
- **Archivos:** se valida el tipo real del archivo, no la extensión. Hay límites de tamaño y cuotas de disco. Las imágenes se recodifican en un proceso aparte, sin permisos: si un archivo malicioso rompe ese proceso, el servidor sigue funcionando.
- **Sistema:** el servidor corre sin permisos de administrador. Guarda un registro de seguridad (entradas, bloqueos, cambios de permisos) y copias de la configuración y la base de datos.

### Más medidas en el servidor

El servidor no confía en nada de lo que dice un programa: cualquiera puede modificar el suyo.

| Riesgo | Medida |
| --- | --- |
| Un programa modificado se da permisos | El servidor verifica los permisos en cada acción. Ocultar un botón en la interfaz no alcanza |
| Alguien manda voz haciéndose pasar por otro | El servidor decide quién habla según la conexión e ignora lo que diga el paquete |
| Descargar archivos de canales ajenos | Cada descarga verifica que puedas leer ese canal: estar en él, si es de voz |
| Escribir en un canal donde solo escriben los admins | El servidor verifica la marca en cada mensaje |
| Usar un canal de texto para hablar | El servidor rechaza entrar a un canal de texto como canal de voz |
| Inundar con avisos de mensajes nuevos | Los avisos no llevan el texto y salen como mucho una vez por segundo por persona, juntando los canales que cambiaron |
| Inyección SQL | Siempre consultas preparadas en SQLite |
| Conexiones que nunca terminan de saludar | Tiempo máximo para el saludo inicial y tope de conexiones pendientes por IP |
| Un programa que no lee lo que recibe | Colas con tope: si no lee, se descarta su voz en vez de acumularla en la memoria. Si lo pendiente de chat y control pasa de 1 MiB, se lo desconecta |
| Conexiones muertas | Ping cada 5 segundos (cada 15 con la ventana minimizada, un juego en pantalla completa o la PC bloqueada); sin datos recibidos ni enviados durante 30 segundos, se cortan |
| Imagen "bomba" (1 MB que declara 50.000 × 50.000 píxeles) | Se leen las dimensiones antes de decodificar, con tope de píxeles y de cuadros |
| Nombres de archivo con trampas | Cada archivo se guarda con un nombre aleatorio; el nombre original es solo texto |

### El programa desconfía del servidor

Como cualquiera puede abrir un servidor, uno malicioso puede mandar datos armados para atacar a quien se conecta. Por eso el programa trata todo lo que recibe como posible ataque.

| Riesgo | Medida |
| --- | --- |
| Imágenes, GIFs y stickers armados | Se decodifican en un proceso aparte con permisos recortados (token restringido y objeto de trabajo de Windows, que existen desde XP). En modo Rendimiento no se decodifica nada |
| Archivos peligrosos (.exe, .bat, .scr, .lnk) | Aviso antes de abrirlos. Se marcan como "descargados de internet" (Mark of the Web) para que Windows también avise |
| Nombres disfrazados con Unicode | Se quitan los caracteres invisibles y de dirección de texto, así `foto[RLO]gpj.exe` no se ve como `fotoexe.jpg`. El servidor rechaza nombres que se vean iguales a otro |
| DLL falsas junto al programa | Todo va dentro del .exe, salvo ClamAV, cuyos archivos se verifican por firma antes de usarlos. Las DLL de Windows se cargan con ruta completa |
| Enlaces en el chat | Se muestra la dirección real antes de abrir y nunca se abren solos |
| Tecla para hablar | Se lee solo la tecla elegida y el resto se descarta al instante, sin guardarlo |
| Claves en la memoria | Se borran apenas dejan de usarse |

### Contraseñas, identidad y llaves de seguridad

Lo más seguro es guardar la menor cantidad posible de secretos: la contraseña del servidor se usa una vez y después entrás con tu identidad.

- **En el servidor:** la contraseña se guarda como hash Argon2id, un código irreversible y lento de calcular a propósito. Si roban la base de datos, la contraseña no sale.
- **Primera entrada:** entrás con la contraseña y el servidor registra tu clave de identidad. Las veces siguientes entrás con esa clave, así que el programa no necesita guardar la contraseña.
- **Contraseñas recordadas:** si igual querés guardar alguna, por ejemplo la de un canal, se guarda cifrada y no como hash, porque el programa necesita recuperarla para mandarla.

**Protección en tu PC, por capas:**

| Capa | Qué hace | Protege contra | Sistemas |
| --- | --- | --- | --- |
| DPAPI de Windows | Cifra la identidad y las contraseñas guardadas con tu sesión de Windows | Otro usuario de la PC, o alguien que se lleve el disco | XP a 11 |
| Contraseña maestra (opcional) | Argon2id convierte tu contraseña en una clave de cifrado AES-256. Cada intento cuesta memoria y tiempo, con valores ajustados a cada equipo | Robo del archivo, aunque tengan tu sesión de Windows | XP a 11 |
| Llave de seguridad o Windows Hello | La clave de identidad vive dentro de la llave y nunca sale. Para entrar hay que tocarla, o poner el PIN o la huella | Programas que copian archivos: no hay nada que copiar | Windows 10 (versión 1903 en adelante) y 11 |

Ningún cifrado frena a un programa malicioso que ya controla tu sesión mientras Ventrik está abierto: puede usar lo mismo que usa el programa. Con una llave de seguridad, al menos no puede llevarse tu identidad a otra PC.

**XP y 7:** Windows incluye la función para usar llaves de seguridad ([WebAuthn](https://learn.microsoft.com/en-us/windows/security/identity-protection/hello-for-business/webauthn-apis)) recién desde Windows 10, versión 1903. En XP y 7 no existe; hablar con la llave directo por USB es posible, pero sin bibliotecas probadas para esos sistemas, así que no está en el plan. En Windows 10 y 11 el programa usa la función de Windows, porque desde la 1903 el acceso directo a la llave [pide permisos de administrador](https://github.com/tavrez/openssh-sk-winhello).

- **Administradores:** el servidor puede exigirles un segundo paso. En Windows 10 y 11, la llave de seguridad. En cualquier sistema, XP incluido, un código de 6 dígitos de una app como Google Authenticator o Aegis (TOTP).
- **Firma de la llave:** el servidor la verifica con Mbed TLS (ECDSA P-256), sin bibliotecas extra.
- **Respaldo:** DPAPI está atado a tu instalación de Windows; si reinstalás, la identidad se pierde. Por eso el programa permite exportarla a un archivo cifrado con contraseña. Con llaves, conviene registrar dos (una de repuesto) o guardar un código de recuperación.

**Cómo es la identidad (etapa 4, paso 4.1):**

- **Una clave ECDSA P-256,** la misma curva que la clave del servidor, la firma de las llaves de seguridad y las actualizaciones firmadas. Se crea sola la primera vez que se abre el programa.
- **Se muestra corta,** con los primeros cuatro grupos de su huella (por ejemplo `CTBC-P44I-2FC4-QX7M`), en el mismo base32 que la huella del servidor. La captura Ajustes-seguridad.png muestra números hexadecimales; vale el formato de la huella.
- **Archivos, en la carpeta del programa:**
  - `identidad.dat`: la clave, cifrada con DPAPI;
  - `equipo.dat`: un secreto propio de esta PC, también con DPAPI, que nunca se exporta (sirve para el aviso de equipo nuevo);
  - la copia `.vkid`: la clave cifrada con una contraseña (Argon2id y AES-256-GCM).
- **Contraseña de la copia:** por lo menos 8 caracteres, escrita dos veces. Argon2id usa 16, 32 o 64 MiB según la memoria libre y las vueltas que tarden cerca de 1 segundo en esa PC. Los valores van dentro del archivo, así una copia hecha en una PC rápida se abre en la netbook. Al abrir se rechaza la que pida más de 64 MiB (lo máximo que escribe Ventrik), más de la mitad de la memoria libre o más de 10 vueltas, así un archivo armado no traba la PC antes de saber si la contraseña es correcta.
- **Si `identidad.dat` está dañado** o no se puede abrir (Windows reinstalado, archivo copiado de otra PC), el programa no crea otra en silencio: avisa y ofrece importar la copia o crear una nueva.
- **Importar una identidad distinta de la actual** pide confirmación, mostrando las dos.

### Identidad del servidor: respaldo y mudanza

La clave del servidor es su identidad: de ella sale la huella que llevan las invitaciones completas y la firma con la que se anota en la lista pública. Si se pierde, el servidor arranca con una clave nueva, pero las invitaciones dejan de servir y los usuarios ven el aviso de que la huella cambió, el mismo que verían ante un servidor falso.

La clave está en la carpeta de datos del servidor (`%APPDATA%\Ventrik\Servidor`). Si el archivo existe pero está dañado, el servidor no arranca, en lugar de crear otra clave y cambiar la huella sin avisar.

- **Respaldo:** el panel de hosteo exporta la identidad del servidor (su clave, la clave del historial cifrado, la configuración y la base de datos) a un archivo cifrado con contraseña, y recuerda hacerlo al crear el servidor.
- **Mudanza:** en la PC nueva se importa ese archivo y el servidor conserva la huella, las invitaciones, los roles y los baneos.
- **Cambio de clave planeado:** el servidor anuncia la clave nueva firmada con la vieja, y los programas que ya lo conocían la aceptan solos. Si la clave fue robada, eso no alcanza, porque el ladrón también puede firmar: hay que avisar por otro medio y repartir invitaciones nuevas.
- **Si se pierde todo:** la huella vieja no se puede recuperar. El dueño avisa por otro medio y reparte invitaciones nuevas.

### Calidad del código y actualizaciones

C++ no protege la memoria por sí solo, así que las reglas de escritura y las pruebas son parte del diseño.

- **Reglas de escritura:** un único lector de mensajes controla cada longitud antes de copiar. Se usan contenedores de C++17 (`std::vector`, `std::string_view`, `gsl::span`) y punteros inteligentes. Están prohibidos `memcpy`, `strcpy`, `sprintf` y `new`/`delete` a mano sobre datos externos, y se verifica cada cálculo de tamaños.
- **Revisión automática:** las advertencias del compilador se tratan como errores en el código propio (las bibliotecas de terceros se compilan tal como vienen, sin modificarlas), y se usa análisis estático (`/analyze`, clang-tidy).
- **Fuzzing:** un programa le tira millones de paquetes y archivos rotos al código hasta encontrar dónde falla. Corre en una compilación de prueba con AddressSanitizer, que detecta cada error de memoria en el momento.
- **Protecciones del compilador:** `/GS`, `/NXCOMPAT`, `/DYNAMICBASE`, `/guard:cf` y `/SAFESEH`. /SAFESEH existe solo en 32 bits, así que va únicamente en la versión clásica. XP SP3 aprovecha /GS, /SAFESEH y DEP, que la versión clásica activa al arrancar con SetProcessDEPPolicy para no depender de la configuración de Windows. XP no tiene ASLR, que llegó con Vista, ni CFG, que llegó con Windows 8.1. Por eso hay una versión moderna aparte (ver "Tecnología elegida").
- **Actualizaciones firmadas:** cada actualización va firmada, y el programa trae la clave pública adentro y rechaza lo que no esté firmado. Firmar el .exe para Windows (Authenticode) suele requerir un certificado pago; como el proyecto es de código abierto, se puede pedir gratis a [SignPath Foundation](https://signpath.org/) (ver "Viabilidad como programa gratuito"). Cómo se buscan, se muestran y se instalan: "Actualizaciones firmadas" más abajo.
- **Bibliotecas:** versiones fijas, seguimiento de avisos de seguridad y actualización rápida. Las de imágenes ya tuvieron fallas graves: libwebp, una usada en ataques reales en 2023 ([CVE-2023-4863](https://db.gcve.eu/cve/CVE-2023-4863)), y stb\_image, varias al leer GIF ([CVE-2023-45661](https://ubuntu.com/security/CVE-2023-45661)).

### Actualizaciones firmadas

Diseño del dueño del 2026-10-26, para la etapa 11. Capturas: docs/pantallas/Actualizaciones*.png, Ajustes-acerca.png, Ajustes-acerca-al-dia.png, Inicio-actualizacion.png e InicioVisual-actualizacion.png (en Inicio.png e InicioVisual.png solo cambia el pie).

- **De dónde saca las versiones:**
  - **Nada de la API de GitHub para revisar:** sin cuenta, api.github.com deja 60 pedidos por hora por IP, y con CGNAT muchos usuarios comparten una IP.
  - **Un manifiesto firmado,** subido como archivo de cada versión publicada en GitHub:
    - estable: `https://github.com/<dueño>/<repo>/releases/latest/download/manifest.json` ("latest" es la última publicada que no es borrador ni pre-release);
    - pruebas: `https://github.com/<dueño>/<repo>/releases/download/canal-pruebas/manifest.json` (`canal-pruebas` es un pre-release fijo; cada versión de prueba reemplaza su manifiesto).
  - **Redirecciones:** GitHub responde 302 hacia su dominio de descargas; el programa las sigue.
  - **XP:** GitHub pide TLS 1.2 o más nuevo, que XP no trae. La clásica usa Mbed TLS, que ya está en el proyecto y trae TLS 1.2. Lo que falta es la lista de autoridades raíz dentro del programa, que también pide la etapa 14 para hablar con Google: queda pendiente para esa etapa.
- **El manifiesto:**
  - **Firma:** con la llave del proyecto, en **ECDSA P-256 con Mbed TLS**; la clave pública viene adentro del programa. El diseño del dueño pedía Ed25519, pero Mbed TLS no lo trae y el proyecto usa solo Mbed TLS para el cifrado: sumar otra biblioteca necesita su permiso. Es la misma curva que la firma de llaves de seguridad.
  - **Se descarta** si la firma no vale, si la versión es más vieja que la instalada (nadie te puede hacer bajar de versión) o si la fecha es más vieja que la del último manifiesto aceptado.
  - **Campos:**
    - versión y fecha;
    - por compilación (moderna de 64 bits y clásica de 32 bits): dirección del archivo, tamaño y SHA-256;
    - `seguridad`: si trae algún arreglo de seguridad;
    - `minima`: la versión más vieja que todavía puede conectarse a servidores de esta versión;
    - `ultima_para_xp`: si existe, la clásica se queda en esa versión y deja de ofrecer las siguientes;
    - `destacados`: de 2 a 4 frases cortas para "Novedades";
    - `cambios`: los de esta versión y las anteriores (por lo menos las últimas 20), cada una con su versión y fecha.
  - **Los cambios van en categorías fijas:** Seguridad, Nuevo, Arreglos, Cambios y Quitado. Escritos para el usuario: sin nombres de funciones ni de archivos.
  - **Salen de CHANGELOG.md,** una sección por versión con esas categorías. El script de publicación arma desde ahí el texto de la versión en GitHub y el campo `cambios`, y firma el manifiesto.
  - **El programa muestra siempre los cambios del manifiesto firmado,** nunca la página de GitHub.
- **Cuándo revisa:**
  - al abrir el programa, después de que aparece la ventana, sin demorar el arranque;
  - si queda abierto, una vez cada 24 horas (un temporizador más, que se justifica en docs/estado.md al programarlo);
  - a mano, con "Buscar actualizaciones" en Ajustes → Acerca de;
  - sin conexión: 10 segundos de límite y ningún error a la vista; el pie de Inicio dice "No se pudo revisar";
  - se apaga con "Buscar actualizaciones al abrir el programa" (activado de entrada);
  - privacidad: GitHub ve la IP y la versión, nada más; no se manda ninguna identificación. La ayuda lo aclara.
- **Qué ve el usuario:**
  - **Ajustes → Acerca de** (Ajustes-acerca.png y -al-dia.png): el aviso de la versión nueva o "Estás al día · revisado hoy a las 23:10" con "Buscar actualizaciones"; "Buscar al abrir"; "Cuando hay una nueva" (Avisarme y que yo elija / Instalarla al cerrar el programa / Solo los arreglos de seguridad, sin preguntar); Canal (Estable / Pruebas); "Ver todos los cambios".
  - **El pie de Inicio** (Inicio-actualizacion.png e InicioVisual-actualizacion.png): "Versión 0.2.0 disponible · Ver cambios" en amarillo, que abre la ventana de la actualización.
  - **La ventana de la versión nueva** (Actualizaciones.png): los cambios por categoría, las versiones que te saltaste ("También trae la 0.1.2"), "Ver en GitHub" y Actualizar ahora / Al cerrar el programa / Más tarde.
    - En un canal de voz (Actualizaciones-en-llamada.png), el botón principal pasa a "Actualizar al salir del canal": nunca se corta una llamada.
    - Bajando (Actualizaciones-bajando.png): barra, pasos (versión, bajada, firma y huella, reinicio) y "Seguir en segundo plano".
    - Al volver a abrirse (Actualizaciones-novedades.png): "Novedades de la 0.2.0", con los `destacados`. Se puede apagar.
    - Firma inválida (Actualizaciones-error.png): se borra el archivo y se sigue con la versión actual.
  - **Cuándo se abre sola:** con "Avisarme", una vez por versión, al encontrarla; si trae un arreglo de seguridad, en cada arranque hasta que se instale; si no, quedan el pie de Inicio y Acerca de.
  - **Debajo de la `minima`:** la ventana no tiene "Más tarde", y explica que los servidores nuevos no la van a aceptar; con servidores viejos se sigue usando. El rechazo ya existe hoy en la negociación del protocolo (un programa viejo recibe "versión vieja", ver "Dos versiones que se entienden"); en la etapa 11 suma el botón que abre la ventana de la actualización ("Actualizá Ventrik para entrar").
- **Cómo se instala:**
  1. Baja la compilación de esa PC (moderna o clásica) a una carpeta temporal del programa, en segundo plano, con prioridad baja y tope de velocidad (docs/optimizacion.md).
  2. Comprueba el SHA-256 del manifiesto firmado; si no coincide, borra el archivo y muestra el error. El .exe además tiene que traer la firma Authenticode del proyecto (SignPath).
  3. Espera el momento: nunca en medio de una llamada ni de una transferencia de archivos.
  4. **El ayudante** hace el reemplazo: Ventrik se cierra, el ayudante espera a que termine el proceso, guarda la versión actual en `anterior\`, pone la nueva y la abre con `--recien-actualizado`. Vuelve a comprobar la firma él mismo, sin confiar en lo que le pasa el programa. Para cumplir "un solo .exe", el ayudante es el mismo ventrik.exe copiado a la carpeta temporal y abierto con `--actualizar`, no un `ventrik-actualizar.exe` aparte.
  5. **Vuelta atrás automática:** si la versión nueva no avisa "arranqué bien" (ventana principal abierta y configuración leída) en 60 segundos, o se cierra con error dos veces seguidas, el ayudante restaura `anterior\` y muestra por qué.
  6. Al volver a abrirse, entra solo al servidor donde estaba y muestra las Novedades.
  7. **Permisos de Windows:** en una carpeta del usuario no hace falta pedir permiso; en "Archivos de programa", pide permiso (UAC) solo para el reemplazo. **Hoy no hay instalador:** el .exe se copia a mano (por ejemplo, `build\para_xp`) y los datos van en %APPDATA%\Ventrik. Los instaladores llegan en la etapa 11; la propuesta es que instalen en una carpeta del usuario, así actualizar no pide permiso (lo decide el dueño en esa etapa).
  8. **La configuración, los chats y los datos locales no se tocan.** Si una versión cambia el formato de la base, la migración hace una copia antes y la vuelta atrás la restaura.
- **El servidor y las PCs de nodos:**
  - el programa del servidor revisa igual, al arrancar y una vez por día;
  - el admin elige en Configuración del servidor: avisarme, instalar cuando no haya nadie conectado o instalar en un horario;
  - con varias PCs (nodos, etapa 13), se actualizan de a una: cada nodo vuelve a entrar y recién ahí sigue el siguiente, así el servidor nunca se cae entero;
  - un cliente y un servidor de versiones distintas se entienden mientras los dos estén por encima de la `minima` del otro;
  - a un cliente demasiado viejo el servidor le responde "Actualizá Ventrik para entrar", con un botón que abre la ventana de actualización.
- **Pruebas:**
  - manifiestos rechazados: firma alterada, versión más vieja que la instalada y fecha anterior a la última aceptada;
  - un archivo con un byte cambiado se rechaza y se borra; una descarga cortada se retoma o se reinicia, nunca se instala incompleta;
  - una versión que se cierra al arrancar vuelve sola a la anterior, con los datos intactos;
  - en un canal de voz, "Actualizar ahora" espera a que salgas;
  - sin conexión, el programa arranca igual de rápido y sin errores;
  - en XP, la clásica baja e instala desde GitHub (TLS 1.2 con Mbed TLS);
  - con nodos (etapa 13), la actualización de a una no corta a nadie.

### Capas extra y niveles de seguridad

Cada capa nueva tiene que cubrir una amenaza que ninguna otra cubre. Las que repiten protección contra lo mismo solo suman código que puede fallar, consumo y pasos molestos, así que no se agregan.

**Capas que se agregan:**

| Capa | Qué cubre | Costo |
| --- | --- | --- |
| Cifrado del historial y los archivos en el servidor (AES-256-GCM con Mbed TLS; la clave se protege con DPAPI o con una contraseña al iniciar el servidor) | Robo de la PC del host o de su disco: sin la clave, el historial y los archivos no se leen | Bajo. Si el host pierde la contraseña, pierde el historial |
| Bloqueo automático del programa, por inactividad o al bloquear Windows | Alguien usa tu PC mientras no estás | Ninguno |
| Aviso de equipo nuevo | Te enterás si tu identidad entra desde otra PC | Ninguno |
| Confirmación y aviso al dueño en acciones grandes de administrador (nombrar admins, borrar canales, baneos masivos) | Un admin robado no puede arrasar el servidor sin que el dueño se entere | Bajo |

**Capas que no se agregan:**

- **Cifrar dos veces lo mismo:** más procesador, que pesa en la netbook, sin ganancia real sobre TLS 1.3.
- **Cifrado inventado:** los algoritmos caseros suelen tener fallas; solo se usan estándares.
- **Ocultar el protocolo:** frena poco y complica encontrar errores.
- **Contraseña maestra, llave y código cada vez que abrís:** la gente termina desactivándolo o usando contraseñas débiles.
- **Protección a nivel del sistema, como los anti-trampas de juegos:** es invasiva, los antivirus la marcan y no anda en XP.

**Niveles por servidor**, que elige el dueño:

| Nivel | Qué activa |
| --- | --- |
| Básico (predeterminado) | TLS con huella, contraseñas con Argon2id, límites, procesos aparte y todo lo que el programa hace solo |
| Reforzado | Lo anterior, más historial cifrado, antivirus incorporado, aprobación de usuarios nuevos, segundo paso para administradores y avisos de acciones de administrador |
| Máximo | Lo anterior, más llave de seguridad o código TOTP para todos, invitaciones de un solo uso, sin historial guardado y rechazo de comprimidos con contraseña. En XP, solo funciona detrás del relay |
| Personalizado | Cada opción por separado. Si el dueño cambia una opción de otro nivel, el servidor pasa solo a Personalizado |

Cada usuario puede sumar por su cuenta la contraseña maestra, la llave de seguridad y el bloqueo automático, desde Ajustes.

## Conectarse desde internet

En la red local un servidor funciona siempre; desde internet depende de la conexión de quien hostea, y el panel de hosteo indica qué abrir y prueba si funciona.

Muchos proveedores de internet en Argentina comparten una misma IP pública entre varios clientes; eso se llama CGNAT. Con CGNAT, nadie de afuera puede entrar directo a tu servidor.

| Forma de conectarse | Qué hace falta | ¿Funciona con CGNAT? |
| --- | --- | --- |
| Red local | Nada | Sí, pero solo dentro de la misma red |
| IP pública con el puerto abierto en el módem, a mano o con UPnP | Que el proveedor te dé IP pública y el router lo permita | No |
| IPv6 | Que vos y los demás tengan IPv6 | Sí, si todos tienen |
| Red privada virtual (Tailscale, ZeroTier) | Instalarla en todas las PCs | Sí |
| Relay | Alguien que hostee el relay | Sí |

El **relay** es un intermediario que reenvía los datos cifrados hasta tu PC sin poder leerlos. Es la opción recomendada para servidores en Windows XP o 7. Falta decidir quién lo hostea.

### Red y puertos al hostear

El panel de hosting muestra todo lo necesario para abrir el servidor a mano, y después prueba que funcione.

- **Placa de red:** se elige qué placa usar (o todas) y se ve su IP local. Si se elige la de una red privada virtual (ZeroTier, Tailscale), el servidor solo se ve dentro de esa red.
- **Puerto del servidor:** 7700 por defecto, TCP y UDP (TCP para chat, control y archivos; UDP para la voz).
- **Puerto de descubrimiento:** 7701 UDP. Solo sirve en la red local y no se abre en el módem.
- **IP pública:** el panel la muestra, igual que cualquier página de "cuál es mi IP".
- **Abrir el puerto:** en el módem se reenvía el 7700 TCP y UDP a la IP local; el panel da la indicación exacta y, si el módem lo permite, lo intenta solo con UPnP. Conviene reservar la IP local en el módem para que no cambie. Con dos routers en cadena, hay que reenviarlo en los dos.
- **Firewall de Windows:** el servidor crea la regla para su puerto; pide permiso de administrador una sola vez.
- **Probar desde internet:** un servicio externo (el directorio o el relay) intenta conectarse a la IP pública y al puerto, y dice si responde. Es la única forma segura de saber que funciona, y anda antes de iniciar el servidor: el programa abre el puerto un momento para la prueba. Si el directorio y el relay están apagados, la prueba no está disponible, pero el servidor anda igual.
- **Si la prueba falla con todo bien configurado:** probablemente el proveedor usa CGNAT. La IP que muestran las páginas está compartida con otros clientes y el reenvío del módem no llega. Ahí sirven IPv6, pedirle una IP propia al proveedor, una red privada virtual o el relay.
- **Lista pública:** no necesita otro puerto abierto. El servidor se anota hacia afuera y el directorio comprueba el puerto del servidor.

**Búsqueda por puerto:** encuentra servidores en tu red local. Primero pregunta en el puerto de descubrimiento (7701) y los servidores responden solos; si alguno no responde, busca por puerto o por rango, por ejemplo del 7700 al 7710. No busca en internet: sería lento, no encontraría servidores detrás de CGNAT y los proveedores lo tratan como abuso.

### Lista pública de servidores

Además de la búsqueda en la red local, el programa muestra una lista de servidores públicos, como la de Mumble, y se busca sobre todo por nombre.

- **Cómo funciona:** el dueño marca "Aparecer en la lista pública" y el servidor se anota en un directorio: un servicio que solo guarda la lista (nombre, descripción, etiquetas, región, personas, acceso, huella y cómo conectarse). No pasa voz, chat ni imágenes: en modo Visual, el fondo y el icono animados de un servidor los pide el programa a ese mismo servidor cuando lo elegís en la lista, y quedan guardados en la PC. Así la PC del directorio no carga con ese tráfico.
- **Búsqueda:** por nombre primero, y también por IP o por región (país o ciudad). No distingue mayúsculas ni tildes. Lo que coincide por nombre aparece arriba, y cada resultado dice con qué coincidió.
- **Filtros y orden:** categoría, región, con lugar libre y sin contraseña; orden por ping, personas o nombre; favoritos.
- **Ping:** lo mide el programa contra cada servidor, directo o a través del relay.
- **Detalle:** al elegir un servidor se ven sus canales, reglas, acceso, nivel de seguridad y huella, antes de conectarse.
- **También con CGNAT y contraseña:** Mumble solo lista servidores sin contraseña y accesibles desde todo internet. Acá un servidor detrás de CGNAT figura a través del relay, y uno con contraseña figura con su candado.
- **Directorio oficial en una PC común:** lo hostea el creador del proyecto en su propia PC. Es un programa chico (incluido en el instalador del servidor) que corre junto a lo demás, sin servidor alquilado. Necesita IP pública con un puerto abierto (7702 TCP por ahora) y un nombre de DNS dinámico gratuito, para que los programas lo sigan encontrando aunque cambie la IP.
- **No es una dependencia:** si el directorio está apagado, se pausan la lista pública, los enlaces cortos de invitación y el buzón de reportes (los reportes esperan en cada PC). Todo lo demás anda igual: los servidores guardados, la conexión por IP, la búsqueda en la red local y las invitaciones con enlace completo. El programa muestra la última copia de la lista que descargó, marcada con su fecha, y cuando el directorio vuelve, los servidores se anotan solos de nuevo.
- **Varios directorios:** cualquiera puede montar otro con el mismo programa, y el programa permite agregarlos. Nadie controla la lista entera.
- **Seguridad del directorio:** el servidor se anota firmando con su clave, y el directorio comprueba que responde antes de listarlo, así nadie anota un servidor ajeno. Un servidor por relay no publica su IP. Hay límites contra servidores falsos en masa y contra nombres que imitan a otros. La huella la publica el directorio; si un conocido te pasó otra, vale la del conocido.

### Invitaciones

Las invitaciones funcionan como en Discord: un enlace con vencimiento y cantidad de usos, con la opción de un solo uso.

- **Contraseña del servidor:** es opcional. Una invitación puede dejar entrar sin ella. Con la opción "Solo con invitación", sin invitación no se entra aunque se sepa la contraseña.
- **Opciones de cada invitación:** vence en 30 minutos, 1 hora, 6 horas, 12 horas, 1 día, 7 días o nunca; usos máximos sin límite, 1, 5, 10, 25, 50 o 100; entrar sin contraseña; miembro temporal, que sale al desconectarse si nadie le dio un rol.
- **Un solo uso:** tiene su propio botón. Al usarse queda anulada, y la lista muestra quién la creó y quién la usó.
- **Anular:** cualquier invitación se puede anular en cualquier momento.
- **Enlace corto** (`ventrik://K7Q2-9XPL`): un código de 8 caracteres no alcanza para llevar la dirección y la huella, así que el relay o el directorio lo traducen. En ese caso, la huella depende de ese servicio, y si está apagado el enlace corto no funciona; el completo sí.
- **Enlace completo:** lleva la dirección (o el relay), la huella y el código. Funciona sin ningún servicio intermedio y es el más seguro, porque el programa comprueba la huella por su cuenta.
- **Seguridad:** los códigos se generan al azar, el servidor guarda solo su hash y limita los intentos con códigos inválidos.

## Moderación y baneos

Un baneo bloquea la identidad del usuario y la huella de su equipo; bloquear la IP es opcional.

- **Identidad:** cada usuario tiene una clave que se genera en su PC o que, en Windows 10 y 11, vive dentro de una llave de seguridad (ver "Contraseñas, identidad y llaves de seguridad"). No hay registro central.
- **Huella del equipo:** se arma con cinco piezas que la PC ya tiene: el identificador de la instalación de Windows, la serie del disco del sistema, la de la placa madre o BIOS, el procesador y las placas de red físicas. El programa no escribe nada en la PC para esto.
- **Nunca se mandan los valores reales:** cada pieza se mezcla con una clave propia de cada servidor en un código irreversible. Cada servidor ve códigos distintos para la misma PC, así nadie puede seguir a una persona entre servidores.
- **Coincidencia parcial:** si coinciden 3 de las 5 piezas, es la misma PC. Así cambiar un disco o una placa de red no rompe el reconocimiento.
- **Sin baneo automático por huella:** si alguien entra con identidad nueva desde una PC baneada, se avisa al administrador o entra silenciado hasta que lo aprueben. Evita castigar a otra persona que usa la misma PC.
- **IP:** es opcional. Con CGNAT, bloquear una IP puede bloquear también a otros clientes del mismo proveedor.
- **Transparencia:** antes de entrar, el programa avisa si ese servidor reconoce equipos.

**Barreras de entrada**, que frenan a los que vuelven más que la huella:

- Invitaciones de un solo uso, con vencimiento.
- Aprobación de usuarios nuevos, o un período de prueba sin poder hablar ni subir archivos.
- Límite de ingresos por minuto.

El permiso "Compartir pantalla" figura en la lista de permisos desde la etapa 9, apagado y marcado "en desarrollo" hasta la etapa 12.

Límite honesto: la huella se puede falsificar con un programa modificado. Frena al usuario común, no al que se lo propone.

Nota legal, que no reemplaza a un abogado: los identificadores de equipo pueden considerarse datos personales (Ley 25.326 en Argentina, RGPD en Europa). Los códigos distintos por servidor reducen ese riesgo, pero cada servidor debería explicarlo en su política.

## Amigos y llamadas directas (etapa 14)

Además de los servidores, cada usuario puede tener una lista de amigos y hablar con ellos de PC a PC, sin servidor: chat de a dos y en grupo, llamadas de voz de a dos y grupales, pantalla compartida y archivos. No hay base de datos central ni buscador de gente: a alguien se lo agrega con su ID. Todo se guarda en una base local en cada PC, y el servicio del programa hace copias en Google Drive.

| Captura (docs/pantallas) | Qué muestra |
| --- | --- |
| Amigos.png | Pestaña "Amigos" de Inicio: solicitudes, grupos y amigos a la izquierda, chat de a dos al medio, detalles a la derecha, y tu ID abajo a la izquierda |
| Amigos-sin-conexion.png | Chat con un amigo desconectado: mensajes y archivo "Esperando a que Rulo se conecte" |
| Amigos-grupo.png | Chat de grupo: aviso de llamada en curso con "Unirme", miembros y roles, archivo bajando en partes de varios miembros, mensajes recibidos de otro miembro |
| Amigos-agregar.png | Diálogo "Agregar amigo": ID o código corto, mensaje opcional, tu ID y tu código corto |
| Amigos-llamada-entrante.png | Aviso de llamada entrante: aceptar te silencia en el servidor donde estás |
| LlamadaGrupal.png | Llamada grupal: pantalla compartida y personas, con el rol de cada PC |
| LlamadaGrupal-chat.png | Chat de la llamada, con los avisos del sistema |
| LlamadaGrupal-como-viaja.png | Árbol de la pantalla compartida y explicación de la voz |
| Recuperar-1.png a Recuperar-3.png | Recuperar desde Google Drive: código de Google, contraseña de copias y resultado |
| Ajustes-amigos.png y Ajustes-copias.png | Las secciones "Amigos y privacidad" y "Copias de seguridad" de Ajustes |

Inicio suma arriba las pestañas "Servidores | Amigos", con la cantidad de solicitudes. Las dos secciones de Ajustes llevan "PRONTO" en el menú y adentro se ven desactivadas y marcadas "EN DESARROLLO" hasta la etapa 14, como "Prestar esta PC".

### ID y solicitudes

- **El ID es la identidad que ya tiene cada usuario** (Ajustes → Seguridad). Lleva:
  - la llave pública completa, no la huella corta, para que nadie pueda hacerse pasar por otro;
  - una parte antispam que se puede cambiar;
  - un dígito de control.

  Se muestra como VK1-XXXX-… y se comparte copiando y pegando.
- **Código corto opcional:** un código como K7Q2-9XPL, de un solo uso, que vence en 24 horas. Lo resuelve el directorio, igual que los enlaces cortos de invitación.
- **"Cambiar la parte antispam":** el ID viejo deja de recibir solicitudes, y los amigos actuales no se enteran.
- **Solicitudes:**
  - agregar a alguien pide que el otro acepte;
  - cada solicitud tiene Aceptar, Rechazar y Bloquear;
  - hay un límite de solicitudes por remitente y por hora;
  - en Ajustes se elige de quién se aceptan: de quien tenga tu ID, o solo con un código corto que creaste.
- **Bloquear:** rechaza solicitudes, mensajes y llamadas de esa persona. La lista de bloqueados está en Ajustes.

### Encontrar a un amigo, sin servidor central

- **Red repartida (DHT tipo Kademlia):** cada programa guarda una parte de la red.
- **Cómo entrar a la red,** en este orden:
  1. las PCs que ya conocía;
  2. la red local;
  3. el directorio y el relay de Ventrik, que hacen de puerta de entrada.

  Si el directorio está apagado, los que ya estaban adentro se siguen encontrando. La barra de abajo de Amigos dice por dónde entró.
- **Solo los amigos saben si estás conectado y tu dirección:**
  - cada par de amigos busca al otro en un lugar de la red que solo ellos dos pueden calcular: sale de un secreto compartido y cambia cada día;
  - lo que se publica ahí va cifrado para ese amigo y firmado;
  - un extraño que conoce tu ID solo puede dejarte una solicitud en la red. La buscás vos, y no ve tu IP.
- **Ping:** el programa le hace ping a cada amigo para saber si está conectado, y solo contesta pings de amigos aceptados.
- **"Quién ve si estás conectado":** todos mis amigos, solo los que elija, o nadie (aparecer desconectado).

### Conexión directa

- **Camino directo:** se abre el agujero en el módem desde los dos lados a la vez (UDP hole punching), coordinado por la red o por un amigo en común.
- **Si no se puede** (por ejemplo, con CGNAT):
  1. pasa por otro participante que sí pueda;
  2. si no hay ninguno, por el relay.

  El relay no puede leer nada, porque todo va cifrado de punta a punta.
- **IP:** una conexión directa le muestra tu IP a ese amigo. Se avisa en la cabecera del chat y en los detalles. Es la diferencia con los servidores, donde nada va de persona a persona.
- **"Siempre por el relay":** opción para no mostrarla, con más demora.

### Cifrado

- **De punta a punta, siempre:** no hay servidor que deba leer nada.
- **Protocolo:** uno estándar ya probado, no uno inventado. Propuesta: MLS (RFC 9420) para todo. Un chat de a dos es un grupo de 2, y cada PC de una persona es un miembro aparte.
- **La clave cambia** cuando alguien entra, sale o lo sacan. Si se roba una clave, no sirve para leer lo anterior.
- **Antes de la etapa:** verificar qué implementación de MLS compila para XP. Como cualquier biblioteca nueva, entra a third_party/ preguntándole antes al dueño.

### Chats

- **Mensajes a un amigo desconectado:**
  - quedan en la PC de quien envía, marcados "Esperando a que X se conecte";
  - salen solos cuando los dos están conectados a la vez;
  - un archivo pendiente se puede cancelar.
- **Grupos:**
  - **Qué es:** el grupo es fijo, con miembros, nombre y chat; la llamada es aparte.
  - **Administración:** el que lo crea es administrador y puede nombrar a otros. "Quién puede sumar gente": solo los admins o cualquier miembro.
  - **Mensajes que faltan:** el miembro que vuelve los recibe de cualquier otro miembro conectado, con un aviso "Recibiste N mensajes… (te los pasó X)". Los mensajes van firmados, con id y fecha, y se juntan sin duplicar.
  - **Sin tope** de miembros.
- **Archivos:**
  - van en partes, como torrent: cada uno comparte las partes que ya tiene;
  - el adjunto muestra de quién está bajando;
  - el ClamAV del que recibe lo analiza antes de abrirlo.

### Llamadas

- **Ciclo de la llamada:**
  - se crea al llamar y se destruye al cortar;
  - el chat de la llamada queda guardado en el chat de a dos o del grupo;
  - en el grupo, el aviso "Llamada en curso" tiene "Unirme".
- **Cada participante es un nodo temporal,** con el mismo algoritmo de reparto de la etapa 13 (varias PCs, todavía sin diseñar en este documento): subida, demora, pérdida, procesador y estabilidad.
- **Quién coordina:** el de mejor conexión. Si se va, toma su lugar el siguiente, y la barra de abajo lo dice.
- **Voz:**
  - va directa de cada uno a todos;
  - si a alguien no le alcanza la subida, su voz sale por un participante con buena subida que la reenvía, con un solo salto y dentro de la demora máxima;
  - se escuchan como mucho las 4 voces más fuertes.
- **Pantalla:**
  - **reparto en árbol:** el que comparte manda pocas copias y otros participantes las reenvían. Cada copia lleva su versión chica (480p) para las PCs lentas;
  - **si se va uno que reenviaba,** su rama pasa a otra PC en segundos, y la voz no se entera;
  - **"Dejar de mirar"** libera la bajada y, si esa PC reenviaba, le pasa su parte a otra;
  - depende de la etapa 12.
- **Sin tope de participantes:** el límite real es la subida que suman todos. Solo reenvían video las PCs con buena subida y procesador. Una netbook con XP recibe la versión chica y no reenvía.
- **Panel de la llamada:**
  - **Personas:** el rol de cada uno: comparte, coordina, reenvía a N, detrás de CGNAT, poca subida, Windows XP.
  - **Chat:** el de la llamada.
  - **Cómo viaja:** el árbol de la pantalla.
- **Barra de abajo:** cantidad de personas, cifrado, tu subida usada y quién coordinaría si se va el actual.
- **Llamada entrante:**
  - aparece un aviso con Aceptar y Rechazar;
  - si estás en un canal de voz de un servidor, al aceptar ahí quedás en silencio y sordo, con el estado "En una llamada", hasta cortar;
  - en Ajustes se puede elegir que no avise.

### Base local y copias en Google Drive

- **Base local:** cada PC tiene su base (amigos, grupos, chats y adjuntos), que se actualiza a medida que llega información.
- **El servicio de Windows hace las copias,** aunque Ventrik esté cerrado:
  - **Cuándo:** todos los días a la hora elegida (00:00, 04:00 o 12:00). Si la PC estaba apagada, al prenderla.
  - **Cómo se toma la copia:** una foto coherente sin frenar la base, por ejemplo con la copia en caliente de SQLite.
  - **Qué se sube:** una copia completa por semana y, en el medio, solo los cambios.
  - **Cuánto se guarda:** 2, 4 u 8 semanas (por defecto, 4).
  - **Adjuntos:** solo el texto, hasta 2 GB (por defecto), hasta 10 GB, o todos. Cada adjunto se sube una sola vez, por su huella, aunque aparezca en varias copias. Lo que pasa el tope queda como texto, "archivo no guardado en la copia".
  - **Cifrado:** todo se cifra antes de subir, con una clave que sale de la contraseña de copias (Argon2id). Google no puede leerlo.
- **La copia lleva también la identidad,** cifrada con esa contraseña. Así, para recuperar todo alcanza con la cuenta de Google y la contraseña de copias.
  - Se exige una contraseña larga.
  - Se avisa que, si se pierde, la copia no se puede abrir.
  - El archivo de identidad de siempre sigue existiendo.
  - Si la identidad vive en una llave de seguridad, nunca sale de la llave y la copia no la puede llevar: se recupera con la llave o con la de repuesto (ver "Contraseñas, identidad y llaves de seguridad").
- **Permiso en Drive:** solo `drive.file`, con una carpeta visible "Ventrik" que el usuario puede ver y borrar. Para Google es un permiso "no sensible", así que alcanza con la verificación básica de la app.
- **Iniciar sesión en Google** (Recuperar-1.png):
  - se usa el flujo para dispositivos con entrada limitada, que admite `drive.file`: el usuario entra a la dirección que da Google (hoy google.com/device) desde el celular o desde otra PC, y escribe el código;
  - el programa pregunta solo si ya se autorizó, sin que haga falta tocar nada. "Comprobar ahora" es opcional;
  - el código vence a los 30 minutos;
  - en Windows 10 y 11 también está "Usar el navegador de esta PC". En XP, solo el código, porque XP no tiene un navegador que funcione con Google;
  - Ventrik trae su propia biblioteca de TLS. Para hablar con Google, además, verifica el certificado contra una lista de autoridades que trae el programa, porque la de XP está vieja.
- **Recuperar** (Recuperar-1 a 3):
  - **Dónde aparece:** al abrir Ventrik por primera vez ("Empezar de cero" o "Recuperar desde Google Drive") y en Ajustes → Copias de seguridad.
  - **Pasos:** iniciar sesión en Google, elegir la copia, poner la contraseña. Probar una contraseña equivocada no rompe nada.
  - **Qué vuelve:** la identidad, los amigos, los grupos, los mensajes, los adjuntos, los servidores guardados y los ajustes.
  - **Lo que pasó después de la copia:** el programa se lo pide a cada amigo y grupo. A los que estén desconectados, cuando se conecten.
- **Ajustes → "Copias de seguridad"** (Ajustes-copias.png):
  - cuenta vinculada y Desvincular;
  - hora, adjuntos y semanas;
  - última copia, última completa y espacio usado en Drive;
  - "Hacer una copia ahora", "Recuperar desde Google Drive" y "Cambiar la contraseña de copias".

### Ajustes → "Amigos y privacidad"

- Tu ID, con "Copiar" y "Cambiar la parte antispam".
- Quién ve si estás conectado.
- Conexión con tus amigos: "Directa cuando se pueda (ven tu IP)" o "Siempre por el relay".
- Solicitudes de amistad: de quien tenga tu ID, o solo con código corto.
- Llamadas mientras estás en un servidor: avisarme y silenciarme ahí al aceptar, o no avisar.
- Bloqueados, con "Ver".

### Varias PCs con la misma identidad

Una identidad puede estar en varias PCs, por ejemplo la de escritorio y la netbook. Cada PC es un miembro aparte en MLS, y el historial se sincroniza entre ellas cuando están conectadas a la vez.

### Windows XP

- **Todo funciona en XP:** UDP, DHT, MLS (si se verifica la implementación) y TLS propio para Drive.
- **Se mezclan PCs clásicas y modernas:** en las llamadas y los chats directos no hay problema. La regla "modernas con modernas" es solo para los nodos de un servidor.

### Orden y pruebas

Primero, amigos, solicitudes, chat de a dos, llamadas de voz de a dos, Ajustes → Amigos y privacidad, copias en Drive y Recuperar, que no dependen de las etapas 12 y 13. Después, grupos, llamadas grupales con reparto (usa el algoritmo de la etapa 13) y pantalla en llamadas (usa la etapa 12).

- **Conexión:** dos PCs detrás de módems distintos, y una con CGNAT, que tiene que ir por un participante o por el relay; con el directorio apagado, los que ya estaban en la red se siguen encontrando.
- **Privacidad:** un extraño con tu ID no puede saber si estás conectado ni ver tu IP.
- **Mensajes:** un mensaje y un archivo a un amigo desconectado llegan cuando los dos se conectan; en un grupo, el que vuelve recibe lo que se perdió, sin duplicados.
- **Llamadas:** en una llamada grupal se van el que coordina y uno que reenvía pantalla, y la voz no se corta y la pantalla vuelve en segundos; alguien sale de un grupo y no puede leer lo nuevo.
- **Copias:** formatear y recuperar desde Drive en Windows 10 y en XP (con el código); una contraseña equivocada falla sin romper nada; un adjunto que aparece en varias copias se sube una sola vez.
- **Fuzzing:** de los mensajes entre PCs y de la red DHT.

## Bots (etapa 15)

Diseño del dueño del 2026-10-26. Es la última etapa.

| Captura (docs/pantallas) | Qué muestra |
| --- | --- |
| BotsServidor.png | Pantalla de Bots, con Rocola elegida. A la izquierda, los bots del servidor con su estado, los oficiales para agregar y "Bot de otra persona" (pegar el código VKB-…). A la derecha, "Qué puede hacer" (permisos) y "Ajustes" del bot |
| BotsServidor-guardia.png y BotsServidor-partidas.png | Los permisos y ajustes de Guardia y de Partidas |
| BotsServidor-externo.png | Un bot externo: aviso amarillo, permisos y datos (comandos, huella del bot, uso de los últimos 7 días, registro) |
| BotsEnUso.png | En el servidor: Rocola en Partida 1 con la marca BOT y su indicador de música, tarjetas con botones en el chat y la lista de comandos al escribir "/" |
| BotsEnUso-mensajes.png | Las mismas tarjetas, sin la lista de comandos |

La lista de la izquierda del servidor dice "N conectados · M bots".

### Qué es un bot

- **Una cuenta especial con identidad propia** (su propia llave). Entra a un servidor con los permisos que le da un admin.
- **Siempre lleva la marca BOT**, que no se puede sacar, y no puede hacerse pasar por una persona.
- **Puede:**
  - leer y escribir en los canales permitidos;
  - responder comandos con "/";
  - mandar mensajes con botones y menús;
  - entrar a canales de voz y pasar audio;
  - con permiso, mover gente o borrar mensajes.

### Dos tipos

- **Oficiales:**
  - vienen con el servidor de Ventrik y se agregan con un clic;
  - corren en la misma PC del servidor, cada uno en su propio proceso: si uno falla, el servidor sigue;
  - son código del proyecto, revisado, de la misma versión que el servidor;
  - funcionan también en XP.
- **Externos (de la comunidad):**
  - son programas aparte, en cualquier lenguaje, que se conectan por la API de bots desde la PC de quien los hostea;
  - el que lo hace genera un código de invitación (VKB-…). El admin lo pega en la pantalla de Bots, ve qué permisos pide y elige cuáles le da (puede darle menos);
  - la pantalla avisa que su código no lo revisa Ventrik y que lo que lee pasa por la PC de quien lo hostea.

### API de bots

- **Conexión:** WebSocket sobre TLS, al mismo servidor y puerto. El bot se identifica firmando un desafío con su llave: no hay un "token" de texto que se pueda filtrar.
- **Eventos:** mensajes, comandos, botones, alguien que entra o sale de la voz, etc. Llegan solo los que sus permisos dejan ver.
- **Acciones:**
  - mandar, editar y borrar mensajes;
  - responder comandos;
  - mandar tarjetas con botones y menús;
  - entrar a un canal de voz y mandar audio en Opus;
  - mover gente.
- **Comandos "/":** el bot los registra con nombre, descripción y parámetros. El programa los muestra al escribir "/", con el nombre del bot al lado, y en el chat queda la línea "<persona> usó /<comando>".
- **Límites de velocidad** por bot.
- **Archivos:** los que manda un bot pasan por el antivirus igual que los de cualquiera.
- **SDK:** uno chico en C, con ejemplos, y la guía "Cómo hacer tu propio bot".

### Permisos

- **Por bot y por canal:**
  - leer y escribir en tal canal;
  - leer todos los canales;
  - responder comandos;
  - entrar a voz;
  - hablar (pasar audio);
  - mover gente;
  - borrar mensajes;
  - silenciar;
  - expulsar;
  - banear;
  - dar roles;
  - mencionar a todos;
  - crear canales temporales;
  - ver quién está conectado.
- **"Leer todos los canales" es sensible:** se marca así y se pide aparte. Sin él, el bot solo ve los mensajes que lo nombran y los comandos que le escriben.
- **Permiso de admin nuevo, "Manejar bots":** solo quien lo tiene ve la pantalla de Bots, que se abre desde el menú del servidor.
- **Registro:** todo lo que hace un bot queda en el registro de seguridad del servidor, con el motivo.
- **Lo que un bot nunca ve:** los chats de amigos (etapa 14), los canales donde no tiene permiso y las IP de los usuarios.
- **Sacar un bot:** se revoca su llave.

### Bots oficiales

- **Rocola** (música en los canales de voz):
  - **De dónde saca la música:**
    - la biblioteca del servidor: archivos MP3, OGG, FLAC u Opus que suben los admins, analizados por el antivirus;
    - radios por internet: streams Icecast o SHOUTcast que agrega un admin;
    - enlaces directos a archivos de audio.
  - **YouTube y Spotify no:** sus condiciones prohíben bajar o retransmitir su contenido y entrar con bots. Por eso Google hizo cerrar a Groovy y Rythm en 2021.
  - **Comandos:** /poner, /saltar (por votación o como elija el admin), /cola, /pausa y /volumen.
  - **Audio:**
    - 48, 64 o 96 kbps;
    - "Bajar la música cuando alguien habla";
    - cada usuario le puede bajar el volumen solo para sí, como a cualquier persona;
    - la música cuenta para la capacidad del servidor igual que una voz.
  - **Tarjeta "Sonando en <canal>":** tema, origen, quién lo pidió, barra de avance y botones Pausar, Saltar (con votos) y Cola.
  - **Responsabilidad:** el dueño del servidor responde por los derechos de lo que suena.
- **Guardia** (moderación automática):
  - bienvenida y rol "Nuevo" automático;
  - anti-spam: el mismo mensaje 3 veces o menciones masivas borran el mensaje y silencian 5 minutos;
  - filtro de palabras y de enlaces (solo dominios permitidos);
  - si entran más de 10 en un minuto, pasa a pedir aprobación;
  - modo lento.

  Todo va al registro de seguridad.
- **Partidas:**
  - **/partida <día hora juego>:**
    - publica una tarjeta con Voy, Quizás y No;
    - avisa 15 minutos antes a los que dijeron "Voy";
    - crea un canal de voz para la partida, que se borra solo cuando queda vacío.
  - **/equipos <cantidad>:** reparte en equipos, al azar o parejos, a los del canal de voz de quien lo pide. Con el permiso de mover gente, ofrece "Mover a <canales>".
  - **/dados** (por ejemplo 2d6) y **/moneda**.
- **Encuestas:** /encuesta <pregunta> con opciones y voto con botones, anónima si se elige, con cierre automático.
- **Lector:**
  - lee en voz alta, para los que están en voz, los mensajes de un canal de texto elegido;
  - motor de voz eSpeak NG (GPL versión 3, compatible con Ventrik); hay que verificar que funcione en XP;
  - cada usuario lo puede silenciar solo para sí.
- **Estado de juegos:**
  - consulta servidores de juego: Minecraft con Server List Ping, y juegos de Steam con A2S_INFO;
  - publica en un canal si están en línea, cuántos jugadores hay y el mapa;
  - avisa si se caen.

**Bibliotecas:** WebSocket, FLAC y eSpeak NG no están en third_party. Se piden al dueño al empezar la etapa.

### Orden y pruebas

1. Identidad de bot, permisos, la API y los comandos "/".
2. Bots oficiales: primero Partidas, Encuestas y Guardia; después Rocola, Lector y Estado de juegos.
3. SDK en C con ejemplos y la guía.

- **Permisos:** un bot sin "Leer todos los canales" no recibe mensajes de otros canales; un bot no puede mover gente sin el permiso.
- **Bots que fallan o abusan:** si se cae un bot oficial, el servidor sigue; un bot que manda demasiado queda frenado por el límite de velocidad.
- **Bots sacados:** un bot sacado ya no puede volver a conectarse con su llave.
- **Rocola:** con 20 escuchando, la voz no se corta y la música baja cuando alguien habla.
- **Fuzzing:** de los mensajes de la API de bots.

## Rendimiento

En modo Rendimiento, el programa apunta a usar como máximo 50 MB de RAM y casi nada de procesador cuando nadie habla. Son metas propuestas, para comprobar midiendo cada versión.

| Métrica | Rendimiento | Visual |
| --- | --- | --- |
| RAM del programa | Hasta 50 MB | Hasta 150 MB |
| Procesador sin nadie hablando | Casi 0 % | Hasta 1 % |
| Procesador con 3 personas hablando | Hasta 2 % de un núcleo | Hasta 4 % |
| Demora de la voz en red local | Hasta 80 ms | Igual |
| Conexión en silencio | Menos de 1 kbps | Igual, más las imágenes al entrar |
| Conexión por persona hablando | Unos 50 kbps (de 17 a 60 con calidad dinámica) | Igual |
| Arranque | Menos de 1 segundo | Menos de 2 segundos |

Estas metas son para PCs comunes. En la netbook de referencia (HP 2133, VIA C7-M) se midió en la etapa 1, en el nivel de consumo Bajo: unos 7 % de un núcleo mientras hablás y 2,4 % por cada voz que escuchás. Ahí la meta de 2 % con 3 personas hablando no se puede cumplir; con el nivel Bajo la netbook aguanta igual (ver "Consumo de la voz" y docs/mediciones.md).

Metas propias de la netbook de referencia, en Rendimiento (decididas por el dueño con el plan de optimización, docs/optimizacion.md):

| Situación | Meta |
| --- | --- |
| Quieta, sin nadie hablando | Hasta 1 % |
| Escuchando | Hasta 3 % más la voz |
| Con el mouse encima | Hasta 10 % |
| Hablando | La voz más 5 % |
| Memoria del programa | Hasta 20 MB |

En las dos versiones, con la ventana quieta y nadie hablando, menos de 20 despertares por segundo, y nada que impida dormir a la PC si no hay una llamada en curso.

**Cómo se logra:**

- La interfaz la dibuja el propio programa con la placa de video, como un juego. Solo se redibuja cuando algo cambia; con la ventana minimizada no se dibuja nada.
- El audio tiene su propio hilo con prioridad máxima y nunca espera a otra tarea. Un tirón de la interfaz o de la red no corta la voz.
- En silencio no se envía casi nada, y a quien silenciaste no se lo decodifica.
- Las listas del chat y del árbol dibujan solo las filas visibles. La memoria temporal tiene un tope y descarta lo más viejo.
- Las imágenes se decodifican al tamaño en que se muestran y se liberan al salir de pantalla.
- Las animaciones se pausan con la ventana minimizada o sin foco.

### Uso de núcleos

Usar todos los núcleos no hace al programa más rápido por sí solo: suma coordinación entre tareas y despierta núcleos que podrían descansar. El programa usa pocos hilos con tareas fijas, y el servidor multiplica solo lo que crece con los usuarios.

**Programa** (la cantidad de hilos es fija):

| Hilo | Tarea | Prioridad |
| --- | --- | --- |
| Audio | Captar, comprimir, mezclar y reproducir | Máxima |
| Red | Enviar, recibir y cifrar | Alta |
| Interfaz | Dibujar y atender teclado y mouse | Normal |
| Tareas pesadas (varios) | Decodificar imágenes y manejar archivos | Baja |

Con un solo núcleo, como en la netbook, funciona igual: las prioridades hacen que la voz vaya siempre primero.

**Servidor** (crece con los núcleos):

| Parte | Cómo usa los núcleos |
| --- | --- |
| Red y voz | Un hilo por núcleo. Cada canal se asigna a un hilo, así sus paquetes no se coordinan con otros hilos |
| Chat, permisos y base de datos | Un hilo propio con una cola de pedidos, para que la base no se trabe |
| Tareas pesadas | Hilos de baja prioridad para recodificar imágenes y manejar archivos |

El servidor puede usar más recursos que el programa, con una condición: la voz nunca espera a otra tarea. El límite real de un servidor casero suele ser la velocidad de subida, no el procesador.

### Servidores grandes (hasta 512 personas)

Un servidor puede tener hasta 512 personas conectadas. El procesador casi no limita, porque el servidor reenvía la voz sin decodificarla; lo que limita es la velocidad de subida de quien hostea.

Cada voz se envía una vez por cada persona que la escucha. Como la cantidad de canales la decide el dueño, el cálculo usa el peor caso: todos en un mismo canal y hablando a la vez. Con el tope de voces, cada persona recibe como máximo 4 voces, así que la subida necesaria es 4 × (personas − 1) × kbps por oyente, dividido 0,85 para usar como máximo el 85 % de la subida. Con más canales hace falta menos, así que el número es seguro.

| Personas, todas hablando (tope de 4 voces) | Alta (60 kbps) | Media (30 kbps) | Mínima (17 kbps) |
| --- | --- | --- | --- |
| 64 | 17,8 Mbps | 8,9 Mbps | 5,0 Mbps |
| 128 | 35,9 Mbps | 17,9 Mbps | 10,2 Mbps |
| 256 | 72,0 Mbps | 36,0 Mbps | 20,4 Mbps |
| 512 | 144,3 Mbps | 72,1 Mbps | 40,9 Mbps |
| 64, sin tope de voces | 285 Mbps | 142 Mbps | 81 Mbps |

**Cómo se hace posible:**

- **Tope de voces por canal:** el servidor reenvía como máximo las 4 voces más fuertes de cada canal (configurable de 2 a 8). Cada paquete trae su volumen, así el servidor elige sin decodificar el audio. Sin este tope, la última fila de la tabla sería imposible.
- **Canal escenario:** solo hablan quienes tienen permiso; el resto escucha y puede pedir la palabra.
- **Modo canal grande:** en canales de más de 50 personas (valor inicial), la calidad dinámica arranca en el nivel Media: suma unos 20 ms de demora y usa un 40 % menos de subida. Si aún falta subida, sigue bajando.
- **Chat con modo lento:** en canales grandes, el admin puede limitar cada cuánto escribe cada persona.
- **Lista de personas liviana:** el programa dibuja solo las filas visibles, y el servidor manda los cambios, no la lista entera.
- **Reconexiones escalonadas:** si el servidor se reinicia, los programas vuelven a entrar con una espera al azar, para no llegar los 512 juntos.
- **La subida se reparte en un solo lugar,** por prioridades: primero la voz, después lo demás. Las pantallas compartidas (etapa 12) se suman como lo de menor prioridad.
- **Varios núcleos:** cada canal va a un hilo, así los canales llenos se reparten entre núcleos; en un canal muy grande, el envío a sus oyentes también se reparte entre varios hilos.
- **Capacidad estimada:** al hostear, el dueño hace una prueba de velocidad (por ejemplo en speedtest.net) y anota la subida. El panel calcula cuántas personas aguanta en calidad Alta, Media y Mínima: personas ≈ subida × 0,85 ÷ (tope de voces × kbps por oyente) + 1, en el peor caso de todos hablando a la vez. Con 30 Mbps y tope de 4 da 107 personas en Alta, 213 en Media y 376 en Mínima. El máximo "Automático" usa ese número. Quienes están en el servidor sin canal de voz no cuentan para esta capacidad, pero sí para el "Máximo de usuarios" (ver "Entrar a un servidor y a la voz").

Procesador y memoria (estimación, a medir): el peor caso con 512 personas y el tope de 4 voces son unos 100.000 paquetes por segundo en calidad Alta o Normal y unos 50.000 en Media, cifrados uno por uno. Es exigente pero posible para un procesador actual de varios núcleos. La memoria ronda los 100 a 200 MB para 512 conexiones.

Detrás de CGNAT, toda esa subida pasa por el relay, y el relay puede poner su propio tope. Para servidores grandes conviene IP pública, IPv6 o una PC alquilada en un centro de datos. El modo servidor limitado de la netbook sigue con su tope de 8 personas.

## Tecnología elegida

El programa y el servidor se escriben en C++17 nativo para Windows, con bibliotecas que funcionan de XP a 11, y se compilan en dos versiones: una moderna y una clásica para Windows viejos. Cada uno es un solo archivo .exe; lo único que va aparte es ClamAV, con su base de virus, en una carpeta propia del programa.

| Parte | Elección | Para qué |
| --- | --- | --- |
| Lenguaje | C++ | Se puede compilar para XP y sigue andando en Windows 11 |
| Ventana | Win32 | La base de los programas nativos de Windows |
| Dibujo y animación | Direct3D 9 + Dear ImGui, con diseño propio | Direct3D 9 existe desde XP y sigue en 11. Todo se dibuja por código, así el aspecto es 100 % propio |
| Fuentes | Barlow y Barlow Semi Condensed para la interfaz; Bungee, Pacifico, Press Start 2P, Permanent Marker y Righteous para los nombres | Van dentro del programa: nadie descarga fuentes ajenas |
| Audio | miniaudio (Vista en adelante) y un puente propio con WinMM (XP) | miniaudio usa WASAPI desde Vista. En XP, donde WASAPI no existe, miniaudio o consume mucho procesador (DirectSound: consulta cada 1 ms) o suma mucha demora (WinMM: esperas de 40 ms como mínimo), así que Ventrik habla directo con WinMM, con pedazos de 20 ms y avisos de Windows (medido en la etapa 1 con 10 ms; la clásica usa 20 para despertarse la mitad de veces) |
| Compresión de voz | Opus | El estándar actual para voz por internet |
| Limpieza de ruido | RNNoise | Quita el ruido de fondo con poco procesador |
| Cancelación de eco | SpeexDSP | Liviana, en C y funciona en XP |
| Cifrado | Mbed TLS | TLS 1.3 y DTLS 1.2 dentro del programa, sin depender del cifrado viejo de XP. En la versión clásica, los números aleatorios salen de RtlGenRandom |
| Contraseñas | Argon2 (implementación de referencia) | Argon2id para guardar y derivar contraseñas; Mbed TLS no lo trae |
| Imágenes del modo Visual | stb\_image (GIF) y libwebp | Livianas y sin dependencias. No se aceptan PNG animados, porque stb\_image solo lee su primer cuadro |
| Compartir pantalla (etapa 12) | VP8 con libvpx en todos los equipos; H.264 por la placa de video con Media Foundation, opcional (desde Windows 7) | VP8 es libre de regalías, BSD y compatible con la GPLv3, y anda en XP. libvpx entra a third_party/ recién en la etapa 12 |
| Base de datos del servidor | SQLite | Un archivo; no hace falta instalar un motor aparte |
| Abrir puertos | miniupnpc | UPnP, solo si el dueño lo activa |
| Memoria segura | Microsoft GSL | gsl::span y otras ayudas para no leer fuera de los límites |
| Pruebas | doctest | Pruebas automáticas livianas |
| Antivirus | ClamAV: la versión oficial en la moderna y la adaptación de Netfarm en la clásica | Analizar archivos. Va como programa aparte, que se abre solo para analizar |
| Textos | Archivos de idioma aparte | Español e inglés desde la primera versión |

**Por qué no Rust:** Rust evita una clase entera de errores de memoria, pero sus versiones principales exigen Windows 10. Para XP y 7 daría mucho trabajo extra. Si algún día se deja de lado XP y 7, Rust con la biblioteca Slint es la alternativa.

**El costo de C++:** no protege la memoria como Rust. Un error puede cerrar el programa o abrir un agujero, sobre todo al leer imágenes que suben otros. Se compensa con bibliotecas muy probadas, la recodificación en un proceso aparte y pruebas automáticas con archivos rotos.

El código lo va a escribir una IA, y vos no lo revisás línea por línea. Por eso las pruebas automáticas son obligatorias: son la forma de saber que algo anda.

Comprobado en la etapa 0 y 1: las partes de C++17 que se usan compilan con el compilador para XP, y miniaudio funciona en XP, pero ahí se reemplaza por el puente propio con WinMM (ver la tabla).

### Dos versiones que se entienden

El mismo código C++17 se compila dos veces, y las dos versiones se entienden porque respetan el mismo protocolo, no porque usen el mismo compilador. Zoom hace lo mismo con programas para Windows, Mac, Linux y celulares.

| Versión | Para | Compilador | Protecciones de Windows |
| --- | --- | --- | --- |
| Moderna (64 bits) | Windows 10 y 11 | Visual Studio 2022 | Todas: DEP, ASLR y CFG |
| Clásica (32 bits) | Windows XP SP3 a 8.1, y Windows 10 de 32 bits | Visual Studio 2017 con su conjunto para XP | Las que admite cada sistema; en XP, DEP y SafeSEH |

Reglas para que se entiendan:

- **Mensajes armados campo por campo**, con tamaños fijos (un número de 32 bits ocupa siempre 4 bytes) y orden de bytes definido. Si se mandaran los datos tal como están en memoria, la versión de 32 bits y la de 64 los acomodarían distinto.
- **Texto siempre en UTF-8**; Windows usa UTF-16 por dentro y la conversión se hace al entrar y al salir.
- **Mismo cifrado:** la versión clásica solo cambia de dónde saca los números aleatorios. Mbed TLS usa BCryptGenRandom, que no existe en XP, y ahí se reemplaza por RtlGenRandom ([repositorio oficial de Mbed TLS](https://github.com/Mbed-TLS/mbedtls)). Sin ese cambio, el programa no abre en XP.
- **Voz en Opus**, un estándar que cualquier decodificador entiende.
- **Al conectarse**, cada programa informa su versión del protocolo y qué funciones tiene. Si a la clásica le falta algo, el servidor se adapta, por ejemplo mandando la imagen quieta. Las funciones son bits; uno queda reservado para "pantalla" (etapa 12), todavía sin comportamiento.
- **El saludo no cambia nunca de formato:** así cualquier versión, por vieja o nueva que sea, entiende a cualquier otra lo suficiente para decirle que tiene que actualizar.
- **Mensajes con tope:** cada mensaje mide como máximo 64 KiB; lo más grande, como los archivos, viaja en pedazos. Así la memoria que puede ocupar cada conexión también tiene techo.
- **Versión mínima:** el servidor rechaza programas demasiado viejos con un aviso para actualizar, como [hace Zoom desde 2022](https://www.theregister.com/2022/10/31/zoom_minimum_client_update/).
- **Pruebas:** cada versión se prueba en las cuatro combinaciones de servidor y programa (clásica y moderna en cada lado). Si alguna falla, no se publica.

## Reportes de errores y sugerencias

El botón "Reportar" está en Inicio y en Ajustes → Acerca de. Sirve para contar un error o proponer una mejora sin salir del programa y sin tener cuenta en ningún lado.

- **Qué se envía:** si es un error o una idea, el texto y, si la persona lo deja marcado, datos técnicos: versión, compilación (moderna o clásica), Windows, procesador, memoria y las últimas 200 líneas del registro del programa. El registro se escribe sin nombres, direcciones IP, mensajes ni contraseñas, y se puede ver exactamente qué se envía antes de mandarlo. La captura de pantalla es opcional y se muestra antes de enviarla, porque puede tener chats o nombres.
- **Adónde va:** al buzón de reportes, una parte del programa del directorio que corre en la PC del creador del proyecto. Viaja cifrado con TLS 1.3. No lleva la identidad de la persona ni sus servidores; el contacto es opcional y se borra al cerrar el reporte.
- **Si el buzón está apagado:** el reporte queda guardado en la PC y se envía solo cuando vuelve. Mientras tanto, "Copiar para GitHub" deja el texto listo para pegarlo como issue en el repositorio público.
- **Contra el abuso:** límite de reportes por hora y por IP (la IP se usa solo para ese límite y no se guarda con el reporte), tamaño máximo de texto y de captura, y la captura se recodifica en un proceso aparte, como cualquier imagen.
- **Cómo los ve el creador:** el directorio los guarda con un número (por ejemplo R-0142), el tipo, la versión y un estado: nuevo, visto o resuelto. Quien reportó ve ese estado en Ajustes → Acerca de: su programa guarda un código secreto por reporte y con él consulta el buzón.
- **Nada se envía solo:** no hay reportes automáticos. Si el programa se cerró por un error, la próxima vez que abre ofrece reportarlo, con los datos a la vista.

## Viabilidad como programa gratuito

Para que Ventrik sea gratis y siga funcionando sin una empresa detrás, no puede depender de servicios caros, y lo poco que cuesta plata tiene que poder repartirse.

### Licencia: código abierto (GPL versión 3)

- El programa, el servidor, el relay y el directorio se publican como código abierto con licencia GPL versión 3. Cualquiera puede ver el código, compilarlo y mejorarlo, y quien publique una versión modificada tiene que publicar también su código. Así nadie puede cerrar una copia y cobrarla con las mejoras de otros.
- **Por qué conviene:** quien hostea puede comprobar que el servidor hace lo que dice (importa, porque el servidor ve todo); la firma de código sale gratis; y si el proyecto original se frena, otros lo pueden seguir.
- **Bibliotecas:** Opus, RNNoise y libwebp (BSD), Dear ImGui (MIT), miniaudio, stb\_image y SQLite (dominio público o equivalentes) Mbed TLS (Apache 2.0), SpeexDSP y miniupnpc (BSD), Microsoft GSL y doctest (MIT), Argon2 (CC0 o Apache 2.0) y las fuentes Barlow, Barlow Semi Condensed, Bungee, Pacifico, Press Start 2P y Righteous (OFL) y Permanent Marker (Apache 2.0) se pueden usar dentro de un programa GPLv3 ([lista de licencias de la FSF](https://www.gnu.org/licenses/license-list.html)). Cada una exige incluir su aviso de autoría, que va en "Acerca de" y en el instalador.
- **ClamAV** (GPL versión 2) va como programa aparte: Ventrik lo abre, le pasa el archivo y lee el resultado, sin mezclar su código con el nuestro. El instalador incluye su licencia y el código fuente exacto de la versión que compilamos, incluida la adaptación de Netfarm para XP.
- **El nombre no es libre:** el código sí, pero una copia modificada tiene que usar otro nombre, como pasa con Firefox. Así nadie distribuye una versión alterada como si fuera la oficial.

### Costos fijos y cómo se cubren

| Servicio | Para qué | Costo | Cómo se mantiene gratis |
| --- | --- | --- | --- |
| Descargas y actualizaciones | Instaladores y actualizaciones firmadas | Ninguno | Se publican en GitHub, gratis para proyectos públicos |
| Firma de código | Que Windows no muestre el programa como peligroso | Ninguno | [SignPath Foundation](https://signpath.org/) firma gratis proyectos de código abierto y verifica que el .exe salga del código publicado |
| Base de virus | Mantener ClamAV al día | Ninguno | Cada PC la baja de ClamAV; el servidor la comparte con sus usuarios si eso falla |
| Directorio de la lista pública | Guardar la lista, la prueba desde internet, los enlaces cortos y el buzón de reportes | Ninguno: corre en la PC del creador del proyecto | Si está apagado, nada deja de conectar; y cualquiera puede montar otro |
| Relay oficial | Servidores detrás de CGNAT o en XP y 7 | El más caro: crece con el uso | Límites, donaciones y relays de la comunidad |

**Relay oficial con límites:**

- Hasta 16 personas por servidor y calidad Normal como máximo. Con el tope de 4 voces, el peor caso son unos 3,5 Mbps de subida por servidor (4 × 15 × 50 kbps ÷ 0,85).
- Para más gente: IP pública, IPv6, una red privada virtual, un relay de la comunidad o uno propio. El relay es un programa más del proyecto; cualquiera lo puede hostear, y el programa permite agregar relays como agrega directorios.
- **Financiación:** donaciones voluntarias con cuentas públicas (por ejemplo, en Open Collective). Sin publicidad, sin funciones pagas y sin vender datos.

### Lo que queda afuera de la primera versión

- **Grabación:** cualquiera puede grabar con otro programa y eso no se puede impedir. Si algún día el programa graba, la regla ya queda fijada: todos ven un indicador mientras alguien graba con Ventrik, y el servidor puede prohibirlo.
- **Amigos, chats y llamadas directas:** llegan en la etapa 14, sin servidor y cifrados de punta a punta (ver "Amigos y llamadas directas").
- **Bots:** llegan en la etapa 15, la última, con la marca BOT siempre a la vista y los permisos que les dé un admin (ver "Bots").
- **Compartir pantalla:** llega en la etapa 12, con el programa ya completo. El protocolo, la subida del servidor, los permisos y el proceso de medios se arman desde antes para sumarla sin rehacer nada (ver "Compartir pantalla").
- **Overlay dentro de los juegos:** se mete en el dibujo de cada juego, choca con lo liviano, los anti-trampas lo pueden marcar y es muy difícil en XP. A lo sumo, más adelante y solo en la versión moderna.

## Riesgos y límites conocidos

El riesgo más importante es de confianza: quien controla el servidor puede leer y escuchar todo.

| Riesgo | Qué pasa | Cómo se reduce |
| --- | --- | --- |
| El servidor ve todo | Quien lo controla puede leer y escuchar | Hostear en una PC propia o de alguien de confianza; el código abierto permite comprobar qué hace |
| Windows XP y 7 sin parches | La PC queda expuesta a ataques del sistema, y XP no tiene ASLR ni CFG, que dificultan aprovechar errores de memoria | Usar el relay en lugar de abrir puertos; versión moderna para Windows 10 y 11 |
| CGNAT | Nadie de afuera entra directo | Relay, IP pública, IPv6 o red privada virtual |
| Velocidad de subida del host | Pone el límite real de usuarios | Tope de voces por canal, modo canal grande, calidad dinámica y capacidad estimada al hostear |
| Compartir pantalla en XP (etapa 12) | Comprimir video gasta mucho procesador, y XP no tiene ASLR para frenar un video armado para atacar | Copia VP8 de hasta 480p, el programa baja solo lo que no aguanta, el descompresor en el proceso de medios y "No mirar pantallas en esta PC" |
| Costo del relay | Si el uso crece, el relay oficial puede costar más de lo que entra por donaciones | Límite de 16 personas por servidor, relays de la comunidad y propios |
| Antivirus en XP | Depende de una adaptación no oficial de ClamAV; si deja de actualizarse, XP se queda sin base nueva cuando su rama ya no pueda bajarla (la 1.4, después de agosto de 2028) | Probarla en XP real; si se corta, en XP se aplica "Si no se puede analizar" |
| Pérdida de la clave del servidor | Las invitaciones dejan de servir y los usuarios ven el aviso de huella cambiada | Respaldo de la identidad del servidor |
| Huella del equipo falsificable | Un baneado puede volver | Barreras de entrada |
| Errores de memoria en C++ | Cierres o agujeros de seguridad | Pruebas con archivos rotos y recodificación en un proceso aparte |
| Modo Visual en PCs débiles | Todo va lento | Arrancan en Rendimiento |
| Nombres y marcas | "Ventrilo" y "Nitro" son marcas de terceros | Nombre propio, Ventrik, con búsqueda de marcas antes de publicar |
| Datos personales | La huella del equipo puede ser un dato personal | Códigos distintos por servidor y aviso previo |

Cuenta de la subida del host: tope de voces × (personas − 1) × kbps por oyente ÷ 0,85, en el peor caso de todos en un mismo canal y hablando (ver "Servidores grandes").

## Plan de trabajo

El desarrollo va en quince etapas, y la primera es una prueba chica de voz para medir demora y consumo antes de construir el resto. Las dos versiones (moderna y clásica) se compilan y prueban desde el principio.

1. **Prueba de voz:** voz entre dos PCs en red local, con tecla para hablar. Se mide demora y consumo, también en la netbook.
2. **Base:** servidor, canales de voz y de texto, chat, cifrado con TLS 1.3 y DTLS 1.2, permisos básicos verificados en el servidor y panel de diagnóstico.
3. **Voz completa:** notificaciones (las de Windows 10 y 11 en la moderna, una ventana propia en la clásica), varias personas hablando, mezcla, limpieza de ruido, cancelación de eco, volúmenes, tecla o voz y susurro (con los permisos básicos; los permisos por rol llegan en la etapa 9). Pantallas: Ajustes, una sección por vez, con General, Teclas, Audio y micrófono y Avisos y sonidos.
4. **Identidad y seguridad:** claves de identidad, contraseñas con Argon2id, DPAPI y contraseña maestra, niveles de seguridad, historial cifrado, bloqueo automático, aviso de equipo nuevo, avisos de acciones de administradores y respaldo, mudanza y cambio de clave del servidor.
5. **Archivos y antivirus:** subida, descarga, reanudación, prioridad de la voz, ClamAV en el servidor, base de virus compartida por el servidor y envío de comprimidos con contraseña.
6. **Red:** panel de red y puertos (placa, puertos, UPnP opcional y prueba desde internet), búsqueda en la red local, relay (el oficial y agregar otros) e invitaciones.
7. **Lista pública:** directorio (y agregar otros directorios), búsqueda por nombre, IP o región, su seguridad, el buzón de reportes y el botón Reportar, sin captura todavía.
8. **Servidores grandes:** tope de voces, calidad dinámica, capacidad estimada, canal escenario, modo canal grande, chat lento, reconexiones escalonadas y la subida del servidor repartida en un solo lugar, por prioridades.
9. **Moderación:** roles, permisos por rol y canal (también para susurrar, y "Compartir pantalla" apagado hasta la etapa 12), baneos y barreras de entrada.
10. **Modo Visual:** perfiles, tarjeta con fondo animado, efecto por fuera y animación al abrirse, Inicio en modo Visual, personalización del servidor (icono animado, fondo del chat y color del tema en los detalles), marcos de avatar, editor de efectos con su código para compartir, GIFs, stickers y la captura en los reportes, que usa el mismo proceso aparte para imágenes, armado como "proceso de medios".
11. **Cierre:** modo servidor limitado, idiomas, el resto de Ajustes, instaladores firmados y pruebas largas.
12. **Compartir pantalla:** VP8 en todos los equipos, incluido XP, con captura, reenvío y topes; después, la copia H.264 por la placa de video y los cuadros en capas.
13. **Varias PCs:** un servidor sostenido entre varias PCs (todavía sin diseñar en este documento).
14. **Amigos y llamadas directas:** amigos, chats y llamadas de PC a PC sin servidor, cifrados de punta a punta con MLS, y copias en Google Drive.
15. **Bots:** identidad de bot, permisos, la API de bots con comandos "/", los bots oficiales (Partidas, Encuestas, Guardia, Rocola, Lector y Estado de juegos) y el SDK en C.

**Cómo se prueba cada versión:**

- Una prueba automática con 50 usuarios simulados, y otra con 512 antes de publicar cambios en la voz o la red. Si una versión empeora las metas de rendimiento, no se publica.
- Pruebas con archivos rotos o malformados contra el procesador de imágenes.
- Pruebas en Windows XP (en una máquina virtual y, para ClamAV, en una PC real), en Windows 7 (la netbook) y en Windows 10 y 11.
- Pruebas de 8 horas conectado, para detectar si la memoria crece sin parar.
- Un panel de diagnóstico dentro del programa, con procesador, RAM, ping y pérdida de paquetes. Es local: no manda datos afuera. En los dos modos, la barra de estado muestra un gráfico chico del ping y la pérdida del último minuto, que abre el panel.

## Decisiones pendientes

Estas decisiones quedan abiertas; ninguna frena la primera etapa.

- [ ] Búsqueda de marcas para el nombre Ventrik (INPI y otros países donde se vaya a usar).
- [ ] Nombre propio para las funciones de personalización.
- [ ] Puertos predeterminados (7700, 7701 y 7702 por ahora).
- [ ] Quién opera el relay oficial y con qué cuenta de donaciones (el directorio oficial lo hostea el creador del proyecto).
- [ ] Límites definitivos del modo servidor limitado y del relay oficial, después de probar.
- [ ] Mensajes privados dentro de un servidor. Entre amigos ya están diseñados, sin servidor y cifrados de punta a punta (etapa 14, ver "Amigos y llamadas directas").
- [ ] Qué implementación de MLS usar para la etapa 14: tiene que compilar para XP.
- [ ] Si en el futuro se agregan grabación, búsqueda de GIFs, overlay o app para celular.
- [ ] Compartir el sonido junto con la pantalla (etapa 12; en XP no hay una forma simple de capturarlo).

Ya decidido: código abierto con licencia GPL versión 3 (ver "Viabilidad como programa gratuito").

## Glosario

| Término | Qué significa |
| --- | --- |
| Argon2id | Forma de guardar o derivar contraseñas que es lenta y gasta memoria a propósito, para que probar millones de contraseñas sea carísimo |
| ASLR y CFG | Protecciones de Windows que dificultan aprovechar errores de memoria. XP no las tiene |
| Ataque de degradación | Cuando un atacante obliga a usar un cifrado más débil para poder romperlo |
| Biblioteca | Código ya hecho y probado que el programa usa para una tarea, como cifrar o comprimir la voz |
| Canal de texto | Canal solo de chat: se lee y se escribe sin salir del canal de voz donde estás |
| CGNAT | Cuando el proveedor de internet comparte una IP pública entre varios clientes. Impide que te entren directo desde afuera |
| Cifrado | Convertir los datos en algo ilegible para quien no tiene la clave |
| ClamAV | Antivirus gratuito y de código abierto que trae Ventrik para analizar archivos |
| Cliente o programa | Lo que instala y usa cada persona para conectarse |
| Códec | Programa que comprime la voz para que viaje usando poca conexión. Ventrik usa Opus |
| Código abierto | Programa cuyo código es público: cualquiera puede verlo, compilarlo y mejorarlo |
| DHT | Red repartida entre los mismos programas, sin servidor central: cada uno guarda una parte. Ventrik la usa para que los amigos se encuentren (etapa 14) |
| Demora o latencia | El tiempo entre que alguien habla y vos lo escuchás |
| Direct3D 9 | La parte de Windows que dibuja con la placa de video. Existe desde XP |
| DPAPI | Función de Windows que cifra datos atados a tu usuario de Windows. Existe desde XP |
| Fuzzing | Probar el código con millones de datos rotos o al azar hasta encontrar dónde falla |
| GPL | Licencia de código abierto que obliga a quien publique una versión modificada a publicar también su código |
| Hash o código irreversible | Un código que se saca de un dato y del que no se puede volver al dato original |
| Hilo | Una tarea que el programa ejecuta en paralelo con otras |
| Huella del certificado | Un código corto que identifica al servidor, para comprobar que es el verdadero |
| Huella del equipo | El código que identifica una PC para los baneos, distinto en cada servidor |
| IP pública | La dirección con la que tu conexión se ve desde internet |
| kbps | Kilobits por segundo: cuánta conexión se usa |
| Llave de seguridad | Un dispositivo USB (como una YubiKey) que guarda una clave secreta que nunca sale de él |
| MLS | Protocolo estándar (RFC 9420) para chats y llamadas cifrados de punta a punta en grupo. La clave cambia cuando alguien entra o sale |
| Núcleo | Cada parte del procesador que puede trabajar por su cuenta |
| Overlay | Lo que dibujan algunos programas de voz encima de un juego, por ejemplo quién está hablando |
| Proceso aparte sin permisos | Una parte del servidor o del programa separada del resto y sin acceso al sistema. Si falla, no arrastra al resto. En Ventrik, el de las imágenes es el "proceso de medios", que en la etapa 12 también descomprime video |
| Protocolo | El formato exacto de cada mensaje entre el programa y el servidor |
| RAM | La memoria de trabajo de la PC |
| Relay | Un intermediario que reenvía los datos cifrados sin poder leerlos |
| Servidor | Lo que hostea el grupo: recibe la voz, el chat y los archivos y los reparte |
| Susurro | Hablarle a una persona, a un grupo o a otro canal con una tecla aparte, sin moverte del tuyo |
| TLS y DTLS | Estándares de cifrado para conexiones por internet. DTLS es la versión para paquetes de voz |
| TOTP | Código de 6 dígitos que cambia cada 30 segundos en una app del celular, usado como segundo paso para entrar |
| UPnP | Forma automática de pedirle al router que abra un puerto |
| Win32 | La base con la que se hacen los programas nativos de Windows |
| Windows Hello | Entrar con PIN, huella o cara en Windows 10 y 11 |
