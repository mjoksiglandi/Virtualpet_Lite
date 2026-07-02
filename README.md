# Virtualpet Lite

> Mascota embebida para `ESP32-S3` con ojos expresivos, `RTC`, `IMU`, modo reloj y proteccion de bateria.

## Por que existe

Este proyecto explora una mascota de escritorio minimalista que vive en una pantalla monocroma y reacciona al contexto real del dispositivo:

- la hora define su ritmo diario
- la IMU define hacia donde mira y como reacciona al movimiento
- la bateria define cuando debe proteger su autonomia

La idea no es solo mostrar ojos animados. La meta es que la pet se sienta consistente, con personalidad y con comportamiento util en hardware real.

## Que hace hoy

El firmware actual ya implementa:

- modo `Pet` con ojos deformables, pupilas, parpadeo y microanimaciones
- modo `Clock` con hora, fecha y bateria
- rutina diaria por fases usando `RTC PCF85063`
- reaccion a inclinacion, sacudidas y movimiento sostenido usando `QMI8658`
- menu local para ajustar fecha y hora sin recompilar
- persistencia en `Preferences` para backup de hora y modo de UI
- modo nocturno con pantalla apagada
- deep sleep por bateria baja cuando cae a `30%`
- medicion de bateria por `ADC` en `GPIO1`
- diagnostico por serial para `BAT`, `I2C` y `RTC`

## Quick start

### Requisitos

- `PlatformIO`
- una `ESP32-S3`
- un `OLED I2C` en `0x3C`
- `RTC PCF85063`
- `IMU QMI8658`

### Compilar

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

### Flashear

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload
```

### Monitor serial

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor
```

## Interaccion del dispositivo

### Boton `PWR`

- `1 click`: alterna entre `Pet` y `Clock`
- `2 clicks`: abre `SET CLOCK`
- `hold largo`: guarda hora en el menu o apaga la placa fuera del menu

### Menu `SET CLOCK`

Permite editar:

- dia
- mes
- anio
- hora
- minuto

Controles:

- inclinacion lateral: modifica el campo activo
- click corto: avanza al siguiente campo
- hold largo: guarda en `RTC` y en `Preferences`

Si el `RTC` no tiene una hora valida, el menu inicializa desde:

1. hora leida desde `RTC`, si existe
2. ultimo backup persistido en flash
3. `2026-01-01 08:40:00`

## Comportamiento de la pet

### Fases horarias

- `07:00-09:00`: `SleepyAM`
- `09:00-12:30`: `ActiveAM`
- `12:30-14:00`: `LunchRest`
- `14:00-14:30`: `PostLunchNap`
- `14:30-18:00`: `WorkToAngry`
- `18:00-22:00`: `RelaxPM`
- `22:00-07:00`: `NightSleep`

### Gestos y reacciones IMU

La IMU se usa para dos capas:

- movimiento continuo: mirada, desplazamiento del ojo y leve inclinacion del cuerpo
- reacciones temporales: overrides de mood y overlays

Estados y reacciones actuales:

- inclinacion sostenida hacia arriba: gesto `Sleepy`
- inclinacion sostenida hacia abajo: gesto `Angry`
- movimiento brusco medio: `Confused`
- movimiento fuerte o sostenido cerca de `2 s`: `Dazed`
- sacudida extrema: `Glitch`

Visualmente, la expresividad vive principalmente en:

- deformacion de la forma del ojo
- apertura y squint
- posicion de pupilas
- overlays contextuales como `Doze`, `Confused`, `Dizzy` y `Glitch`

## Bateria y energia

La bateria se mide desde `GPIO1` usando el divisor de la variante Waveshare.

Comportamiento energetico actual:

- actualizacion periodica con filtro para estabilizar lectura
- porcentaje lineal aproximado entre `3.30V` y `4.20V`
- heuristica `usbLikely` cuando el voltaje es alto
- pantalla apagada durante la noche
- deep sleep defensivo si la bateria baja a `30%` y no parece estar en USB

En esta variante el `deep sleep` del `ESP32-S3` queda deshabilitado por defecto porque el hardware actual no vuelve de forma confiable y el `RTC` externo no esta cableado como fuente de wake.
Durante `NightSleep` el firmware deja la pantalla en ahorro de energia y sigue corriendo con el reloj externo.
Si la bateria cae a `30%`, el equipo se apaga para proteger la celda.

## Arquitectura del firmware

El proyecto ya no depende de un `main.cpp` gigante. La logica se separa en modulos:

- [`src/main.cpp`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/src/main.cpp): orquestacion del loop
- [`lib/PetUI`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/PetUI): render visual
- [`lib/ImuMotion`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/ImuMotion): lectura IMU, tilt, shake y estados derivados
- [`lib/PetBehavior`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/PetBehavior): composicion de intencion visual
- [`lib/ClockLogic`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/ClockLogic): fechas, fases, menu y wake scheduling
- [`lib/BatteryMonitor`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/BatteryMonitor): medicion y filtrado de bateria
- [`lib/PowerButton`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/PowerButton): clicks, doble click y hold
- [`lib/FirmwareTypes`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/FirmwareTypes): tipos compartidos

Hay una explicacion mas detallada en [docs/ARCHITECTURE.md](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/docs/ARCHITECTURE.md).

## Diagnostico por serial

Comandos disponibles:

```text
BAT?
I2C?
RTC?
SET_RTC YYYY-MM-DD HH:MM:SS
```

Ejemplo:

```text
SET_RTC 2026-07-01 08:40:00
BAT?
RTC?
```

## Hardware validado

La variante documentada del proyecto se apoya en una `Waveshare ESP32-S3-Touch-LCD-1.69`, pero la UI activa de este repo hoy corre en un `OLED I2C` externo.

Perifericos confirmados por codigo o por escaneo:

- `0x3C`: OLED `SH1106/SSD1306`
- `0x51`: `RTC PCF85063`
- `0x6B`: `IMU QMI8658`
- `GPIO1`: medicion de bateria
- `GPIO40`: boton `PWR`
- `GPIO41`: `PWR_HOLD`

Mas detalle en [docs/HARDWARE.md](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/docs/HARDWARE.md).

## Limitaciones conocidas

- el porcentaje de bateria sigue siendo una aproximacion lineal
- `usbLikely` es una heuristica, no una deteccion dedicada de carga
- el buzzer sigue deshabilitado en esta variante de hardware
- el touch no participa en la UI
- no hay tests automatizados
- para recuperar wake automatico real hace falta mantener operativo el dominio RTC del `ESP32-S3` o cablear la salida `INT` del `PCF85063`

## Documentacion relacionada

- [docs/ARCHITECTURE.md](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/docs/ARCHITECTURE.md)
- [docs/HARDWARE.md](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/docs/HARDWARE.md)
- [ROADMAP.md](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/ROADMAP.md)
