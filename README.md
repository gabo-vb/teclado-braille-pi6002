# Dispositivo interactivo para el aprendizaje de Braille

Teclado físico de bajo costo con teclas de relieve Braille, retroalimentación auditiva (texto a voz) y conexión Bluetooth como teclado de computadora. Proyecto del curso PI6002 Proyectos Interdisciplinarios II, UTEC, Grupo 5. Alineado con las metas 4.5 y 4.a del ODS 4.

**Versión:** v1.0

## Hardware

- ESP32-WROOM (placa ESP32 Dev Module)
- Matriz de 3 × 10 pulsadores (30 teclas: A–Z, Ñ, coma, punto y espacio)
- Amplificador I2S MAX98357A y parlante de 8 Ω y 2 W
- Pulsador de función
- Alimentación por Micro USB (adaptador de 5 V)

## Conexiones

| Señal | GPIO del ESP32 |
| --- | --- |
| Filas F1, F2, F3 | 27, 26, 25 |
| Columnas C1 a C10 | 23, 22, 21, 19, 18, 17, 16, 15, 14, 13 |
| Pulsador de función | 32 |
| I2S BCLK | 33 |
| I2S LRC (WS) | 4 |
| I2S DIN | 5 |

## Modos de uso

- **LETRAS:** cada tecla se pronuncia en voz alta.
- **PALABRAS:** las teclas acumulan letras; la tecla espacio confirma y dice la palabra.
- **WINDOWS:** el dispositivo envía las teclas por Bluetooth como un teclado normal.
- **OFF:** apagado lógico.

Pulsación corta del botón de función: cambia entre LETRAS, PALABRAS y WINDOWS.
Pulsación de 1 s o más: pasa a OFF.

## Compilación

1. Instala el Arduino IDE 2.3.11 y el soporte para placas ESP32.
2. En el Gestor de bibliotecas o desde GitHub, instala NimBLE-Arduino, arduino-audio-tools y arduino-espeak-ng.
3. Abre `CODIGOTECLADOSP/CODIGOTECLADOSP.ino`.
4. Selecciona la placa **ESP32 Dev Module** y el puerto correspondiente.
5. Compila y carga. El monitor serial funciona a 115200 baudios.

## Uso

1. Alimenta el dispositivo; al iniciar dice "Hola, esta es una prueba de voz".
2. En la computadora, activa el Bluetooth y empareja el dispositivo "Braille Keyboard".
3. Cambia de modo con el botón de función.

## Limitaciones conocidas

- Tras desconectarse, el dispositivo no vuelve a anunciarse por Bluetooth; hay que reiniciarlo.
- El firmware no activa automáticamente el narrador de Windows.
- Las constantes de debounce (`debounceDelay`) están declaradas pero no se aplican en el barrido de la matriz.

## Autores

Nahuel Adonai Magallanes Loredo, Sebastian Andre Moreno Apolitano, Thierry Henrry Gomez Hilario, Leonardo José Olivera Herrera y Gabriel Villaseca Balletta.
