# Virtualpet_Lite

> Mascota embebida para `ESP32-S3` con ojos animados, `RTC`, `IMU`, modo reloj y medicion de bateria.

## Que es

`Virtualpet_Lite` es un firmware para una mascota de escritorio minimalista. La UI vive en una pantalla monocroma y mezcla tres fuentes de contexto:

- hora real desde `RTC PCF85063`
- movimiento fisico desde `IMU QMI8658`
- estado de bateria medido por `ADC`

Hoy el proyecto ya funciona como un dispositivo autocontenido con dos modos de UI:

- `Pet`: ojos animados y expresivos
- `Clock`: reloj en pantalla con fecha y bateria

## Estado actual

El firmware actual ya implementa:

- render de ojos con esclera, pupila, highlight, cejas y parpadeo
- seguimiento de inclinacion con `roll` y `pitch`
- rutina diaria por fases usando `RTC`
- menu local para ajustar fecha y hora sin recompilar
- persistencia en `Preferences` para hora respaldada y modo de UI
- deep sleep nocturno con wake programado a las `08:40`
- medicion de bateria por `GPIO1`
- escaneo I2C por serial para diagnostico

No esta implementado o no se usa en esta variante:

- touch
- audio funcional
- tests automatizados

## Hardware real validado

La integracion actual esta alineada a una placa `Waveshare ESP32-S3-Touch-LCD-1.69`, pero la UI usada por este repo hoy corre sobre un display `OLED I2C` externo detectado en `0x3C`.

Perifericos confirmados por escaneo o por codigo:

- `0x3C`: OLED `SH1106/SSD1306`
- `0x51`: `RTC PCF85063`
- `0x6B`: `IMU QMI8658`
- `GPIO1`: divisor de bateria
- `GPIO40`: boton `PWR`
- `GPIO41`: `PWR_HOLD`

Escaneo real observado:

```text
I2C summary: 0x3C, 0x51, 0x6B, 0x7E
```

`0x7E` se trata como respuesta reservada o ghost, no como periferico funcional.

## Pines usados

Definidos en [include/pinout.h](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/include/pinout.h):

- `GPIO10`: `I2C SCL`
- `GPIO11`: `I2C SDA`
- `GPIO1`: `BAT ADC`
- `GPIO40`: `BTN_PWR`
- `GPIO41`: `PWR_HOLD`
- `GPIO255`: buzzer deshabilitado en esta variante

## Modos de UI

### Pet

Es el modo principal. Muestra los ojos y aplica:

- estado emocional por horario
- seguimiento IMU
- micro movimiento cuando la IMU no domina la mirada
- `motion alert` visual ante movimiento brusco

### Clock

Muestra:

- hora `HH:MM:SS`
- fecha `dd-mm-yyyy`
- bateria estimada como `BAT xx% x.xxV`

La medicion de bateria se toma desde `GPIO1` con divisor `200k / 100k`, siguiendo la referencia de Waveshare para la `ESP32-S3-Touch-LCD-1.69`. La escala actual fue calibrada en hardware con factor `3.16`.

## Interaccion por boton

Con el boton `PWR`:

- `1 click`: alterna entre `Pet` y `Clock`
- `2 clicks`: abre `SET CLOCK`
- `hold largo`: guarda hora en el menu o apaga la placa fuera del menu

## Menu de reloj

El menu `SET CLOCK` permite editar:

- dia
- mes
- anio
- hora
- minuto

Comportamiento:

- inclinacion lateral: edita el campo activo
- click corto: avanza al siguiente campo
- hold largo: guarda en `RTC` y en `Preferences`

Cuando no hay hora valida en `RTC`, el menu parte desde:

1. hora actual del `RTC`, si existe
2. ultimo backup guardado en flash
3. valor por defecto `2026-01-01 08:40:00`

## Comportamiento diario

La mascota cambia segun la hora:

- `07:00-09:00`: sleepy
- `09:00-12:30`: active
- `12:30-14:00`: lunch rest
- `14:00-14:30`: post lunch nap
- `14:30-18:00`: work to angry
- `18:00-22:00`: relax
- `22:00-07:00`: night sleep

En `NightSleep`, si el `RTC` es valido y no estas en el menu de reloj, entra en deep sleep y programa wake para las `08:40`.

## Gestos IMU

La IMU se usa para dos cosas:

- movimiento continuo de ojos y leve `body lean`
- gestos temporales

Gestos implementados:

- inclinacion sostenida hacia arriba: `Sleepy`
- inclinacion sostenida hacia abajo: `Angry`
- shake fuerte: efecto `Glitch`

## Comandos seriales

Disponibles desde `HELP`:

```text
BAT?
I2C?
RTC?
SET_RTC YYYY-MM-DD HH:MM:SS
```

Ejemplos:

```text
RTC?
BAT?
I2C?
SET_RTC 2026-07-01 08:40:00
```

## Build

Compilar:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

Flashear:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload
```

Monitor serial:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor
```

## Estructura del repo

```text
src/main.cpp          Orquestacion del firmware
include/pinout.h      Pines y constantes de hardware
lib/PetUI/            Render y estado visual
lib/RtcClock/         Driver minimo del PCF85063
lib/ImuQmi8658/       Wrapper de IMU
lib/Melodies/         Audio no bloqueante, hoy deshabilitado por pinout
ROADMAP.md            Hoja de ruta del proyecto
```

## Limitaciones conocidas

- El porcentaje de bateria es una aproximacion lineal entre `3.30V` y `4.20V`.
- El valor `usb_likely` se infiere por voltaje alto, no por deteccion dedicada de carga.
- El buzzer no esta activo en esta variante de hardware.
- El touch no participa en la UI actual.
- No hay suite de tests.

## Referencias relacionadas

- Hoja de ruta: [ROADMAP.md](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/ROADMAP.md)
- Hardware y diagnostico: [docs/HARDWARE.md](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/docs/HARDWARE.md)
