# Aspecto de Ventrik

La referencia es docs/pantallas/Rendimiento.png. Todas las demás pantallas usan las mismas piezas. Las medidas son en píxeles al 100 % de escala de Windows y se multiplican por la escala del sistema (125 %, 150 %).

Un resultado se da por bueno cuando, puesto al lado de la captura, no se distingue la diferencia en colores, esquinas ni tamaños a simple vista.

## Colores

| Nombre | Hex | Uso |
| --- | --- | --- |
| fondo | #161B1E | Fondo de la ventana y de la zona del chat |
| pozo | #1A1F22 | Campos de texto, selectores, medidores y botones contorneados |
| panel | #2A3235 | Barra superior, lateral, tarjetas y bloques de ajustes |
| tarjeta | #2B3437 | Tarjeta de perfil y filas destacadas |
| elevado | #3A4448 | Fila del canal donde estás, etiquetas y botón presionado |
| línea | #3B4549 | Bordes de paneles y separadores |
| línea 2 | #4B565B | Bordes al pasar el mouse y bordes de teclas |
| borde oscuro | #111517 | Contorno de los botones biselados y línea bajo la barra superior |
| texto | #DDE6E6 | Texto principal |
| texto 2 | #B3BDBF | Texto secundario |
| apagado | #8B9598 | Ayudas, horas y contadores |
| claro | #D3DEDE | Botón principal y opción elegida en selectores |
| tinta | #14191B | Texto sobre claro |
| naranja | #F28B30 | Transmitiendo, tu medidor y marco del selector de modo |
| tinta naranja | #2A1405 | Texto sobre naranja |
| celeste | #5CC2D6 | Quien habla, enlaces y foco del teclado |
| verde | #6FCB7E | Conectado y ping bueno |
| amarillo | #E7B850 | Avisos y favoritos |
| rojo | #E0705F | Errores y desconectar |
| triángulo | #C3D0CF | Triángulos de las esquinas |

## Tipografía

- Interfaz: Barlow, a 13 px en botones y listas y 14 px en campos de texto. Peso 400; 600 para los nombres y la opción elegida.
- Títulos y nombre del servidor: Barlow Semi Condensed, peso 600. El nombre del servidor va a 21 px y los títulos de bloque, a 15 px.
- Teclas y contadores van a 12 px; el texto de "Transmitiendo" va a 12,5 px en negrita.
- Las dos fuentes son OFL y van dentro del programa. En modo Visual se suman las de los nombres (Bungee, Pacifico, Press Start 2P, Permanent Marker y Righteous).

## Piezas

| Pieza | Forma | Medidas | Colores |
| --- | --- | --- | --- |
| Botón biselado | Octógono: las 4 esquinas cortadas a 45° | Alto 34; bisel 6; relleno a 1 px del borde con bisel 5; margen lateral 10; ícono 15 con 7 de separación | Contorno #111517; relleno degradado vertical #323C40 → #293236; brillo de 1 px arriba, blanco al 6 %; al pasar el mouse #3A464A → #30393D |
| Botón de esquineros | Sin borde: 4 escuadras en las esquinas | Alto 38; escuadras de 8 de largo y 1,5 de grosor; margen lateral 14 | Escuadras #9AA6A8 (texto al pasar el mouse); fondo blanco al 1,5 %; elegido: escuadras #D3DEDE y fondo claro al 10 % |
| Botón contorneado | Rectángulo, radio 2 | Alto 32; margen lateral 14 | Fondo #1A1F22; borde #5A666A; al pasar el mouse, borde #B3BDBF |
| Botón principal | Rectángulo con las esquinas superior izquierda e inferior derecha cortadas | Alto 38; corte 7; margen lateral 18 | Fondo #D3DEDE; texto #14191B en peso 600 |
| Etiqueta "Transmitiendo" | Esquinas superior derecha e inferior izquierda cortadas | Alto 28; corte 8; cuadrado de 9 a la izquierda | Fondo naranja; texto y cuadrado #2A1405 |
| Selector de opciones | Pozo con opciones en fila | Borde 1; margen interno 3; opciones de alto 30 | Pozo con borde #3B4549; la opción elegida va en claro con texto tinta |
| Selector de modo | El selector de opciones dentro de un marco naranja | Escuadras naranjas de 12 × 2 en las 4 esquinas, más un tramo arriba (34 % del ancho, centrado al 58 %) y otro abajo (30 %, al 72 %) | Naranja; borde interno #262E31 |
| Triángulos de esquina | Triángulo rectángulo pegado a la esquina | Catetos de 9 | #C3D0CF. Paneles: arriba a la izquierda y abajo a la derecha; tarjeta de perfil: en las 4 esquinas |
| Rayado | Rayas diagonales a 135° | Bloque de 30 × 7; rayas de 2 cada 5 | #56636A. En la tarjeta de perfil y en el encabezado del chat |
| Encabezado del chat | Marco de escuadras con cuadrícula tenue | Escuadras de 8 × 1,5; cuadrícula de 14; se desvanece hacia la izquierda | Escuadras #5E6A6E; cuadrícula de texto al 4,5 % |
| Avatar | Cuadrado con la esquina inferior derecha cortada | 26 en la lista y 28 en el chat; corte 7; radio 3 en las otras esquinas | Color propio de cada persona; aro interior celeste si habla, naranja si sos vos |
| Tecla | Rectángulo, radio 2 | Alto 26; margen lateral 9; texto a 12 px en negrita | Fondo pozo; borde #4B565B |
| Campo de texto | Rectángulo, radio 2 | Alto 36; margen lateral 12 | Fondo pozo; borde #3B4549; #4B565B al pasar el mouse |
| Medidor | Segmentos sueltos | Segmentos de 7 × 9 con 2 de separación | Encendidos en naranja (tu micrófono) o celeste (otros); apagados #3B4549 |

## Ventana principal

- El tamaño de referencia es 1376 × 768.
- Barra superior: margen de 8 arriba y abajo alrededor del emblema de 42 (unos 59 de alto), fondo panel y línea inferior de 1 en #111517. Lleva, de izquierda a derecha:
  - el emblema de 42 y el nombre del servidor;
  - 7 botones biselados con 4 de separación;
  - el selector de modo, "Hablar con" y la tecla;
  - "Transmitiendo".
- Lateral: ancho fijo de 290 y filas de 31 de alto. La fila del canal donde estás va en elevado. Abajo va el bloque "Tu audio", con su medidor y el selector Tecla para hablar / Por voz.
- Chat: encabezado con marco y cuadrícula. Los mensajes llevan el avatar a la izquierda. Los archivos van en cajas de 376 de ancho con un botón de esquineros. El campo de mensaje va abajo junto al botón principal de enviar.
- Barra de estado: alto 24. Muestra conectado (punto verde), dirección, ping, cifrado y calidad de voz, y a la derecha la huella verificada. Junto al ping va un gráfico de 64 × 14 con el último minuto (línea verde, marca roja donde se perdieron paquetes) y el porcentaje perdido; hacer clic abre el panel de diagnóstico. Va en los dos modos.
- La tarjeta de perfil abre al lado de la persona que tocaste, encima del chat, con 258 de ancho en Rendimiento y 330 en Visual (lleva imagen de fondo).

## Modo Visual

- Usa las mismas piezas y medidas.
- Cambia el fondo de la barra superior (#101417 con el dibujo animado del servidor y una línea inferior de 2 en el color del tema).
- Suma fuentes y efectos en los nombres, el fondo de la tarjeta y los efectos por fuera, y el GIF y los stickers en el chat.
- Color del tema del servidor en los acentos: franja de 3 a la izquierda de la fila del canal donde estás, barras de volumen, y contorno de botones y bordes al pasar el mouse. Nunca reemplaza al celeste (quien habla) ni al naranja (vos transmitiendo).
- Fondo del chat del servidor: va detrás de los mensajes, con una capa de #161B1E al 80 % encima para que el texto se lea; si es animado, se mueve lento. El campo de mensaje lleva su propio fondo. Se apaga en Ajustes → Apariencia.
- Marco del avatar: lo dibuja el programa por fuera de la imagen (sobra un 19 % de cada lado), con los dos colores de la tarjeta de esa persona. Catálogo: Circuito (trazos que recorren el borde), Esquinas (escuadras que laten), Órbita (dos puntos que giran) y Brasas (chispas que suben). Se ve en la lista de canales y en el chat, nunca tapa el aro celeste de "está hablando".
- Editor de efectos (docs/pantallas/EditorEfectos.png): los marcos del catálogo y los que arma cada persona son la misma cosa, una lista de hasta 4 capas que dibuja el programa con ImDrawList. Ninguna capa puede destellar más de 3 veces por segundo.
- El icono del servidor puede ser animado (GIF o WebP); en el boceto, la estrella de la luna titila.
- "Transmitiendo" late (el cuadrado se achica y la etiqueta se aclara un poco) cada 1,1 s mientras transmitís.
- Lo que mandan otros se dibuja solo si el servidor lo permite y si no lo ocultaste.

## Cómo se hace en Dear ImGui

- Las formas biseladas se dibujan con ImDrawList: AddConvexPolyFilled para el contorno y otra vez para el relleno 1 px más adentro. El degradado va con AddRectFilledMultiColor dentro de un clip con la misma forma o, más simple, como dos tonos partidos al medio.
- Las escuadras y los triángulos son líneas y triángulos sueltos con AddLine y AddTriangleFilled; no hacen falta imágenes.
- Los colores van en un único archivo de tema (src/client/ui/theme.h) con los nombres de la tabla, para no repetir valores sueltos por el código.
- El modo Rendimiento no usa animaciones ni transparencias por píxel más allá de las de esta guía, para que ande bien en la netbook con XP.
