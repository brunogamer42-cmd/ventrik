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

Lo que no hace en la primera versión: video, compartir pantalla, app para celular, bots, búsqueda de GIFs en servicios externos, grabación de la voz ni overlay dentro de los juegos (ver "Viabilidad como programa gratuito").

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

- **Arriba:** el servidor, los botones (micrófono, audio, servidores, perfil, personalizar para los administradores, ajustes, desconectar), el selector de modo y el estado de la tecla para hablar.
- **Izquierda:** la lista de canales en dos grupos, "Canales de texto" arriba y "Canales de voz" abajo (ver "Canales de voz y de texto"). Los de voz muestran las personas adentro y su estado: habla, micrófono apagado, audio apagado, ausente o no molestar (sin sonidos de aviso), y se pliegan. La lista se desplaza con su propia barra; abajo queda fijo tu audio: medidor, cómo transmitís (tecla o voz, con su sensibilidad) y volúmenes.
- **Derecha:** el chat del canal de voz donde estás o del canal de texto que elegiste, con los archivos.
- **Abajo:** la barra de estado con la conexión, el ping, el cifrado y la verificación del servidor.

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

En Rendimiento todo eso se apaga, no se oculta: el programa no lo descarga ni lo procesa. Los dos modos tienen los mismos botones, canales, chat, tarjeta de perfil y controles de audio; solo cambia lo que se descarga y se anima. La única excepción es "Ocultar sus efectos" en la tarjeta de otro, que solo aparece en Visual porque en Rendimiento no hay efectos. En Ajustes hay un tercer modo, Automático: usa Visual, salvo en PCs débiles o con un juego en pantalla completa abierto, donde pasa solo a Rendimiento. Las PCs débiles arrancan en Rendimiento.

Aspecto, tomado de la imagen de referencia del autor: grafito azulado con acento naranja; botones con esquinas biseladas y borde oscuro; botones secundarios marcados solo por esquineros; triángulos claros en las esquinas de los paneles; rayado diagonal y cuadrícula tenue en los encabezados; avatares con una esquina cortada; medidores segmentados; selector de modo enmarcado en naranja, y las tipografías Barlow y Barlow Semi Condensed. El celeste marca a quien habla y el naranja, cuando transmitís vos. La ventana principal mide 1376 × 768 en el boceto y es la base de todas las demás pantallas. Medidas y colores exactos: docs/aspecto.md.

**Idiomas:** el programa sale en español e inglés. Todos los textos están en archivos aparte desde el principio, así sumar otro idioma no exige tocar el código y la comunidad puede traducirlo.

### Pantallas

El programa abre en **Inicio**: no hay lista de amigos, porque Ventrik se organiza por servidores.

| Pantalla | Qué tiene |
| --- | --- |
| Inicio (en los dos modos) | Tus servidores con su estado (en línea, cuántos hay, ping, tu último canal, solicitudes pendientes, tu servidor apagado), entrar con invitación, tu estado, el aviso de copia de tu identidad y el botón Reportar. En Visual suma el encabezado y el icono animados de cada servidor y tu tarjeta |
| Servidor (Rendimiento o Visual) | Canales de voz y de texto, chat, archivos, tu audio y tarjetas de perfil |
| Explorar servidores | Lista pública con búsqueda por nombre, IP o región; red local; guardados. En Visual, al elegir un servidor se ven su fondo e icono animados |
| Hostear un servidor | Configuración, seguridad, calidad de voz, archivos y antivirus, invitaciones, red y puertos, capacidad y respaldo de la identidad del servidor |
| Ajustes | General (con el idioma), teclas (hablar y susurros), apariencia, avisos y sonidos, descargas, seguridad, red y acerca de (con Reportar, la licencia y las bibliotecas) |
| Audio y micrófono | Dispositivos, cómo transmitís (tecla o voz) con medidor y prueba, limpieza de la voz, consumo de la voz, volúmenes y bajar otros programas cuando alguien habla (desde Windows 7) |
| Enviar archivo con contraseña | Pide la contraseña, analiza el comprimido con ClamAV en tu PC y lo envía; la contraseña no sale de tu PC |
| Reportar un error o sugerir | Error o idea, datos técnicos que se pueden ver antes de enviar, captura opcional y contacto opcional |
| Perfil y Personalizar servidor | Lo del modo Visual, incluida la tarjeta de perfil |
| Editor de efectos | Capas, vista previa ampliada y en tamaño real, límites y código para compartir, para el marco del avatar y el de la tarjeta |
| Notificaciones | Mensajes nuevos y conexión cortada mientras Ventrik no está al frente: las de Windows 10 y 11 en la versión moderna, una ventana propia en la clásica (ver "Notificaciones") |

### Notificaciones

Avisan de lo que pasa mientras Ventrik no está al frente o está minimizado. Nunca avisan de tus propios mensajes, y tocarlas abre Ventrik en ese canal.

- **De qué avisan:** mensajes nuevos en canales de texto, mensajes del chat de tu canal de voz y conexión cortada (el servidor se cortó o dejó de responder). En Ajustes → Avisos se apagan todas juntas o cada tipo por separado. No molestar las silencia.
- **Qué dicen:** si hay un solo mensaje, quién lo escribió y el texto ("Tano en #general: ¿Arrancamos la segunda?"). Si hay varios, se amontonan en una sola notificación por canal, que se va actualizando: "3 mensajes en #general de Tano", o "de Tano, Sofi y 2 más".
- **De dónde sale el texto:** de los canales de texto que no estás mirando, el servidor solo avisa el contador (ver "Canales de voz y de texto"). Cuando va a mostrar una notificación, el programa le pide esos mensajes nuevos al servidor como cualquier historial, con sus permisos y topes.
- **Windows 10 y 11 (versión moderna):** las notificaciones del sistema, las que salen abajo a la derecha y quedan en el Centro de actividades, con el nombre y el ícono de Ventrik. Se cargan con GetProcAddress; si no están, se usa la ventana propia.
- **Versión clásica (XP a 8.1 y Windows 10 de 32 bits):** una ventana propia, chica, abajo a la derecha, parecida a la de Windows 10 y con el aspecto de Ventrik (docs/aspecto.md). No le quita el foco a lo que estés haciendo, se cierra sola a los pocos segundos y se dibuja sin Direct3D, para no gastar en la netbook.
- **Privacidad:** Windows guarda sus notificaciones en un archivo propio, fuera de Ventrik y sin cifrar. Por eso, en Ajustes → Avisos se puede elegir que digan solo el canal y la cantidad, sin quién ni el texto.

## Voz

La voz se comprime con Opus, el estándar actual para voz por internet, y cada persona que habla usa unos 50 kbps de conexión, contando los encabezados del cifrado.

Recorrido de la voz:

1. El micrófono capta el sonido a 48 kHz, la calidad con la que trabaja Opus, así no hay que convertirlo. En los niveles de consumo Bajo y Mínimo capta a 16 u 8 kHz (ver "Consumo de la voz").
2. Se limpia el ruido de fondo y, si no usás auriculares, se cancela el eco (si Windows no informa qué tipo de salida es, queda encendida). Las dos cosas son opcionales porque son lo que más procesador gasta.
3. Se transmite solo mientras apretás la tecla para hablar, o, en el modo por voz, mientras tu voz supera la sensibilidad que marcás en el medidor. El modo se elige en el panel "Tu audio", en los dos modos de la interfaz. En silencio no se envía casi nada.
4. Opus comprime la voz en pedazos de 20 milésimas de segundo, a 24 kbps.
5. La voz se cifra y viaja al servidor.
6. El servidor la reenvía a los demás del canal tal como llegó: no la decodifica ni la mezcla.
7. Cada programa espera unas milésimas para ordenar los pedazos que llegan desordenados, los decodifica y mezcla las voces.

Ahorros automáticos:

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

- **De voz:** como siempre. Entrar te mueve, hablás ahí y tiene su chat.
- **De texto:** solo chat. No se entra: se lee y se escribe sin salir del canal de voz donde estás.

En la lista de la izquierda van en dos grupos, "Canales de texto" arriba y "Canales de voz" abajo. Cada grupo tiene su botón + para crear un canal, solo para el dueño (y los admins desde la etapa 9). Los de texto llevan #, la cantidad de mensajes nuevos y el nombre en negrita si hay nuevos.

- **Elegir un canal de texto:** se marca en la lista, el chat pasa a ese canal y el encabezado dice "Canal de texto · seguís hablando en <canal de voz>". Tu canal de voz sigue con "estás acá", y tocarlo vuelve a su chat.
- **"Solo los admins escriben":** marca opcional de cada canal de texto, por ejemplo #anuncios, con un candado en la lista. Se elige al crear el canal y el dueño la cambia después con clic derecho, junto con el nombre. El resto lo lee con el campo de mensaje bloqueado, y el servidor lo verifica en cada mensaje. Hasta la etapa 9, "admins" es solo el dueño; los permisos por rol y canal siguen en la etapa 9.
- **Mismo chat que los de voz:** mismo guardado en SQLite, mismos topes, historial y tiempo de guardado. Para escribir en un canal de texto no hace falta estar adentro, pero el pedido pasa por los permisos del servidor (permisos.cpp) como cualquier otro.
- **Quién recibe cada mensaje:** el programa le dice al servidor qué canal de texto está mirando, uno por vez. El mensaje completo va a quienes están en ese canal de voz o miran ese canal de texto. A los demás que pueden leerlo, el servidor solo les avisa qué canal tiene mensajes nuevos y su contador, nunca el texto: como mucho un aviso por segundo por persona, con todos los canales que cambiaron juntos.
- **Mensajes nuevos:** cada canal tiene un contador que solo sube con cada mensaje; borrar mensajes viejos no lo baja. Cada programa guarda en su PC hasta qué número leyó en cada canal, y los nuevos son la diferencia. La primera vez que ves un servidor, todo cuenta como leído.
- **Protocolo y base:** cada canal lleva su tipo (voz o texto), la marca de "solo admins" y su contador. Se suma antes de publicar la primera versión, así no hay que negociarlo entre versiones. El servidor rechaza usar un canal de texto como canal de voz.
- **Borrar un canal de texto** borra sus mensajes, y quien lo estaba mirando vuelve al chat de su canal de voz.
- **Un servidor nuevo** arranca solo con "General", que es de voz. Los canales de texto los crea el dueño; un "general" de texto no puede convivir con "General" de voz, porque los nombres no distinguen mayúsculas.
- **En los dos modos es igual,** porque es texto.

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

Al tocar el nombre de alguien se abre su tarjeta. En modo Visual, cada persona elige cómo se ve la suya:

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

El servidor recodifica cada imagen al recibirla: la lleva a un tamaño fijo, limita los cuadros por segundo y la duración, y guarda una versión quieta. Lo hace en un proceso aparte, sin permisos, por seguridad.

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
- **Formato de la voz, dentro de DTLS:** `u16 tipo` y el cuerpo. El programa manda `VozSubida` (número de paquete de 16 bits y hasta 400 bytes de Opus); el servidor reenvía `VozBajada` (quién habla, que lo pone el servidor, número y Opus) a los demás del mismo canal. `Ping` y `Pong` cada 5 segundos mantienen abierto el camino (NAT) y miden la conexión.
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
| Conexiones muertas | Ping cada 5 segundos; sin datos recibidos ni enviados durante 30 segundos, se cortan |
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
- **Actualizaciones firmadas:** cada actualización va firmada, y el programa trae la clave pública adentro y rechaza lo que no esté firmado. Firmar el .exe para Windows (Authenticode) suele requerir un certificado pago; como el proyecto es de código abierto, se puede pedir gratis a [SignPath Foundation](https://signpath.org/) (ver "Viabilidad como programa gratuito").
- **Bibliotecas:** versiones fijas, seguimiento de avisos de seguridad y actualización rápida. Las de imágenes ya tuvieron fallas graves: libwebp, una usada en ataques reales en 2023 ([CVE-2023-4863](https://db.gcve.eu/cve/CVE-2023-4863)), y stb\_image, varias al leer GIF ([CVE-2023-45661](https://ubuntu.com/security/CVE-2023-45661)).

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
- **Probar desde internet:** un servicio externo (el directorio o el relay) intenta conectarse a la IP pública y al puerto, y dice si responde. Es la única forma segura de saber que funciona. Si el directorio y el relay están apagados, la prueba no está disponible, pero el servidor anda igual.
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

Límite honesto: la huella se puede falsificar con un programa modificado. Frena al usuario común, no al que se lo propone.

Nota legal, que no reemplaza a un abogado: los identificadores de equipo pueden considerarse datos personales (Ley 25.326 en Argentina, RGPD en Europa). Los códigos distintos por servidor reducen ese riesgo, pero cada servidor debería explicarlo en su política.

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
- **Varios núcleos:** cada canal va a un hilo, así los canales llenos se reparten entre núcleos; en un canal muy grande, el envío a sus oyentes también se reparte entre varios hilos.
- **Capacidad estimada:** al hostear, el dueño hace una prueba de velocidad (por ejemplo en speedtest.net) y anota la subida. El panel calcula cuántas personas aguanta en calidad Alta, Media y Mínima: personas ≈ subida × 0,85 ÷ (tope de voces × kbps por oyente) + 1, en el peor caso de todos hablando a la vez. Con 30 Mbps y tope de 4 da 107 personas en Alta, 213 en Media y 376 en Mínima. El máximo "Automático" usa ese número.

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
| Audio | miniaudio (Vista en adelante) y un puente propio con WinMM (XP) | miniaudio usa WASAPI desde Vista. En XP, donde WASAPI no existe, miniaudio o consume mucho procesador (DirectSound: consulta cada 1 ms) o suma mucha demora (WinMM: esperas de 40 ms como mínimo), así que Ventrik habla directo con WinMM, con esperas de 10 ms y avisos de Windows (medido en la etapa 1) |
| Compresión de voz | Opus | El estándar actual para voz por internet |
| Limpieza de ruido | RNNoise | Quita el ruido de fondo con poco procesador |
| Cancelación de eco | SpeexDSP | Liviana, en C y funciona en XP |
| Cifrado | Mbed TLS | TLS 1.3 y DTLS 1.2 dentro del programa, sin depender del cifrado viejo de XP. En la versión clásica, los números aleatorios salen de RtlGenRandom |
| Contraseñas | Argon2 (implementación de referencia) | Argon2id para guardar y derivar contraseñas; Mbed TLS no lo trae |
| Imágenes del modo Visual | stb\_image (GIF) y libwebp | Livianas y sin dependencias. No se aceptan PNG animados, porque stb\_image solo lee su primer cuadro |
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
- **Al conectarse**, cada programa informa su versión del protocolo y qué funciones tiene. Si a la clásica le falta algo, el servidor se adapta, por ejemplo mandando la imagen quieta.
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
- **Overlay dentro de los juegos:** se mete en el dibujo de cada juego, choca con lo liviano, los anti-trampas lo pueden marcar y es muy difícil en XP. A lo sumo, más adelante y solo en la versión moderna.

## Riesgos y límites conocidos

El riesgo más importante es de confianza: quien controla el servidor puede leer y escuchar todo.

| Riesgo | Qué pasa | Cómo se reduce |
| --- | --- | --- |
| El servidor ve todo | Quien lo controla puede leer y escuchar | Hostear en una PC propia o de alguien de confianza; el código abierto permite comprobar qué hace |
| Windows XP y 7 sin parches | La PC queda expuesta a ataques del sistema, y XP no tiene ASLR ni CFG, que dificultan aprovechar errores de memoria | Usar el relay en lugar de abrir puertos; versión moderna para Windows 10 y 11 |
| CGNAT | Nadie de afuera entra directo | Relay, IP pública, IPv6 o red privada virtual |
| Velocidad de subida del host | Pone el límite real de usuarios | Tope de voces por canal, modo canal grande, calidad dinámica y capacidad estimada al hostear |
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

El desarrollo va en once etapas, y la primera es una prueba chica de voz para medir demora y consumo antes de construir el resto. Las dos versiones (moderna y clásica) se compilan y prueban desde el principio.

1. **Prueba de voz:** voz entre dos PCs en red local, con tecla para hablar. Se mide demora y consumo, también en la netbook.
2. **Base:** servidor, canales de voz y de texto, chat, cifrado con TLS 1.3 y DTLS 1.2, permisos básicos verificados en el servidor y panel de diagnóstico.
3. **Voz completa:** notificaciones (las de Windows 10 y 11 en la moderna, una ventana propia en la clásica), varias personas hablando, mezcla, limpieza de ruido, cancelación de eco, volúmenes, tecla o voz y susurro (con los permisos básicos; los permisos por rol llegan en la etapa 9). Pantallas: Audio y micrófono, Ajustes → Teclas y Ajustes → Avisos.
4. **Identidad y seguridad:** claves de identidad, contraseñas con Argon2id, DPAPI y contraseña maestra, niveles de seguridad, historial cifrado, bloqueo automático, aviso de equipo nuevo, avisos de acciones de administradores y respaldo, mudanza y cambio de clave del servidor.
5. **Archivos y antivirus:** subida, descarga, reanudación, prioridad de la voz, ClamAV en el servidor, base de virus compartida por el servidor y envío de comprimidos con contraseña.
6. **Red:** panel de red y puertos (placa, puertos, UPnP opcional y prueba desde internet), búsqueda en la red local, relay (el oficial y agregar otros) e invitaciones.
7. **Lista pública:** directorio (y agregar otros directorios), búsqueda por nombre, IP o región, su seguridad, el buzón de reportes y el botón Reportar, sin captura todavía.
8. **Servidores grandes:** tope de voces, calidad dinámica, capacidad estimada, canal escenario, modo canal grande, chat lento y reconexiones escalonadas.
9. **Moderación:** roles, permisos por rol y canal (también para susurrar), baneos y barreras de entrada.
10. **Modo Visual:** perfiles, tarjeta con fondo animado, efecto por fuera y animación al abrirse, Inicio en modo Visual, personalización del servidor (icono animado, fondo del chat y color del tema en los detalles), marcos de avatar, editor de efectos con su código para compartir, GIFs, stickers y la captura en los reportes, que usa el mismo proceso aparte para imágenes.
11. **Cierre:** modo servidor limitado, idiomas, el resto de Ajustes, instaladores firmados y pruebas largas.

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
- [ ] Mensajes privados entre usuarios (todavía no diseñados) y, si se suman, cifrado de punta a punta opcional para que el servidor no los pueda leer.
- [ ] Si en el futuro se agregan grabación, búsqueda de GIFs, overlay o app para celular.

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
| Núcleo | Cada parte del procesador que puede trabajar por su cuenta |
| Overlay | Lo que dibujan algunos programas de voz encima de un juego, por ejemplo quién está hablando |
| Proceso aparte sin permisos | Una parte del servidor o del programa separada del resto y sin acceso al sistema. Si falla, no arrastra al resto |
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
