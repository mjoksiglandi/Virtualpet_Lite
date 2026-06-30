# gif

> Pet de escritorio embebida para ESP32-S3 con OLED, RTC, IMU y buzzer.

## Resumen

Este proyecto implementa una mascota de escritorio minimalista sobre un ESP32-S3. La mascota vive en una pantalla OLED monocroma, cambia de expresion segun la hora del dia, reacciona a la inclinacion del dispositivo mediante una IMU y emite sonidos simples por buzzer en transiciones relevantes.

Hoy el firmware ya tiene una arquitectura base funcional:

- Render de ojos, pupilas, cejas y parpadeo en OLED SH1106.
- Estados de animo basados en horario usando RTC externo.
- Lectura de inclinacion con QMI8658 para mover la mirada.
- Patrones de sonido no bloqueantes por cambio de fase y campana diaria.
- Latch de energia por boton `PWR` y apagado por `long press`.
- Modulos separados para UI, reloj e IMU.

## Idea del proyecto

La intencion del repo es construir una "pet de escritorio" fisica: una pequena presencia animada que acompana la jornada, expresa estados de animo y responde a su contexto sin necesidad de una interfaz compleja.

La logica actual ya modela una rutina diaria:

- `07:00-09:00`: somnolienta
- `09:00-12:30`: activa
- `12:30-14:00`: descanso
- `14:00-14:30`: sueno post almuerzo
- `14:30-18:00`: cansancio / enojo progresivo
- `18:00-22:00`: relajada
- `22:00-07:00`: durmiendo

## Hardware confirmado

La placa conectada y validada en esta etapa corresponde a una:

- `ESP32-S3 DevKitC-1 N16R8`
- `16 MB` flash
- `8 MB` PSRAM embebida
- USB nativo `USB-Serial/JTAG` de Espressif

El firmware esta escrito alrededor de estos componentes:

- ESP32-S3
- OLED I2C tipo SH1106, direccion `0x3C`
- RTC PCF85063A por I2C
- IMU QMI8658 por I2C
- Buzzer en GPIO `42`
- Boton `PWR` en GPIO `40`
- Linea de hold de energia en GPIO `41`

Pines definidos en [include/pinout.h](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/include/pinout.h:1):

- `SCL`: GPIO `10`
- `SDA`: GPIO `11`
- `BTN_PWR`: GPIO `40`
- `PWR_HOLD`: GPIO `41`
- `BUZZER`: GPIO `42`

## Alimentacion de la placa

Esta placa expone dos conectores distintos de bateria:

- `BAT + -`: bateria principal de la placa
- `RTC + -`: alimentacion dedicada para el dominio del RTC

Hallazgos practicos confirmados durante la integracion:

- El RTC no se comporto de forma estable hasta energizar el conector `RTC + -`.
- El circuito de `BAT` y el de `RTC` no deben asumirse como el mismo dominio.
- Conectar alimentacion simultanea en `BAT` y `RTC` genero calentamiento y no debe considerarse una configuracion segura sin validar antes el hardware.

Recomendacion actual:

- Usa `BAT` para la bateria principal de la placa.
- Usa `RTC` solo para pruebas controladas del respaldo del reloj y solo con una bateria compatible con ese dominio.

## Estructura del proyecto

```text
src/main.cpp              Flujo principal del firmware
include/pinout.h          Pines y direccion I2C
include/config.h          Configuracion global reservada
lib/PetUI/                Render y estado visual de la mascota
lib/RtcClock/             Abstraccion minima del RTC
lib/ImuQmi8658/           Abstraccion minima de la IMU
lib/Melodies/             Reproduccion no bloqueante de tonos integrada en main
```

## Arquitectura actual

### `main.cpp`

Coordina todo el comportamiento del dispositivo:

- Inicializa I2C, display, RTC, IMU y buzzer.
- Activa el `power hold` para que la placa permanezca encendida tras soltar `PWR`.
- Consulta la hora una vez por segundo.
- Calcula la fase del dia y ajusta `mood` y `brow`.
- Reproduce un patron sonoro no bloqueante al cambiar de fase.
- Dispara una campana diaria al entrar a la fase de tarde relajada.
- Lee la inclinacion y la traduce en desplazamiento de mirada.
- Atiende comandos seriales para consultar y ajustar el RTC.
- Permite apagado intencional mediante `long press` del boton `PWR`.
- Refresca la UI a aproximadamente 60 FPS.

### `PetUI`

La UI actual dibuja una cara simple y expresiva:

- Ojos redondeados
- Pupilas con micro movimiento
- Parpadeo periodico
- Cejas segun estado emocional
- Overlay glitch reservado para un modo especial

### `RtcClock`

Implementa una capa directa sobre `Wire` para leer y escribir registros del PCF85063A. El proyecto usa esta hora como fuente principal para la rutina diaria de la mascota.

### `ImuQmi8658`

Lee acelerometro y giroscopio desde la libreria `QMI8658` y calcula `roll` y `pitch` a partir de aceleracion para obtener una respuesta estable.

### `Melodies`

La libreria `Melodies` ya esta integrada en el firmware principal y se usa para:

- sonido de arranque
- cues por cambio de fase
- campana programada de las 18:00

La reproduccion es no bloqueante y se actualiza desde el loop principal.

## Dependencias

Definidas en [platformio.ini](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/platformio.ini:1):

- `olikraus/U8g2`
- `lahavg/QMI8658`
- `solderedelectronics/Soldered PCF85063A RTC Library`

## Build y entorno

Comando de compilacion:

```powershell
& "C:\Users\juan.cornejo\.platformio\penv\Scripts\platformio.exe" run
```

Monitor serial:

```powershell
& "C:\Users\juan.cornejo\.platformio\penv\Scripts\platformio.exe" device monitor
```

## Comandos seriales

El firmware expone comandos utiles por puerto serie para mantenimiento del RTC:

```text
HELP
RTC?
SET_RTC YYYY-MM-DD HH:MM:SS
```

Ejemplo:

```text
SET_RTC 2026-06-30 15:00:00
```

Esto permite:

- consultar si el RTC tiene hora valida
- ver el estado de `osc_stopped`
- ajustar la fecha y hora sin recompilar

## Estado actual

Estado funcional general: **prototipo vertical avanzado**.

Fase 1 actual: **tecnicamente cerrada**, con una nota pendiente de recomendacion final para la bateria del dominio `RTC + -`.

Lo que ya esta implementado:

- Mascota renderizada y animada.
- Modelo de estados por horario.
- Lectura de RTC y de IMU.
- Audio no bloqueante por buzzer.
- Latch de energia funcional.
- Apagado por `long press` del boton `PWR`.
- Ajuste de RTC por comandos seriales.
- Separacion modular suficiente para seguir iterando.

Lo que todavia esta incompleto o en transicion:

- No hay tests automatizados.
- No hay interfaz local en pantalla para configurar hora.
- `config.h` esta reservado pero vacio.

## Hallazgos de build documentados

Durante la revision tecnica del `2026-06-30` se detectaron y resolvieron dos puntos base:

- El firmware usaba una API LEDC antigua (`ledcSetup` y `ledcAttachPin`).
- PlatformIO estaba resolviendo una variante generica `N8` que no representaba el hardware real conectado.

Estado actual luego del ajuste:

- El buzzer ya usa la API LEDC compatible con el core actual de Arduino-ESP32 3.x.
- `platformio.ini` ahora apunta a `esp32-s3-devkitc1-n16r8`.
- El build actual resuelve correctamente `16 MB Flash` y `8 MB PSRAM`.
- La compilacion completa termina en `SUCCESS`.
- La placa permanece encendida tras soltar `PWR` gracias al hold en `GPIO41`.
- El apagado intencional se ejecuta por `long press` en `GPIO40`.
- El RTC fue validado en funcionamiento real con el dominio `RTC + -` energizado.

## Limitaciones conocidas

- La UX actual sigue siendo principalmente visual y sonora.
- No hay persistencia de configuracion ni preferencias.
- No hay documentacion de ensamblaje fisico o wiring mas alla de los pines en codigo.
- El dominio `RTC + -` requiere validacion adicional de bateria compatible antes de recomendarlo como configuracion final de campo.

## Documentos relacionados

- Roadmap: [ROADMAP.md](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/ROADMAP.md)
