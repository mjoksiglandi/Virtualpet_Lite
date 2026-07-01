# Arquitectura del Firmware

## Vision general

El firmware sigue un modelo de loop simple y determinista:

1. leer entradas y eventos
2. actualizar estado derivado
3. componer la intencion visual
4. renderizar

No usa tareas propias de `FreeRTOS` para la logica de aplicacion. En el estado actual, la prioridad es mantener trazabilidad y comportamiento predecible en hardware real.

## Flujo principal

El loop en [`src/main.cpp`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/src/main.cpp) coordina:

- bateria
- comandos seriales
- boton de energia
- lectura RTC
- lectura IMU
- aplicacion del comportamiento visual
- render y audio no bloqueante

La idea es que `main.cpp` describa el flujo del dispositivo y que la logica especializada viva en modulos dedicados.

## Modulos

### `BatteryMonitor`

Responsabilidad:

- leer `ADC`
- filtrar medicion
- calcular `mv`, `percent` y `usbLikely`

Archivos:

- [`lib/BatteryMonitor/BatteryMonitor.h`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/BatteryMonitor/BatteryMonitor.h)
- [`lib/BatteryMonitor/BatteryMonitor.cpp`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/BatteryMonitor/BatteryMonitor.cpp)

### `ClockLogic`

Responsabilidad:

- parseo y clamp de fecha/hora
- reglas de calendario
- logica de fases horarias
- intencion visual base segun hora
- calculo del siguiente wake

Archivos:

- [`lib/ClockLogic/ClockLogic.h`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/ClockLogic/ClockLogic.h)
- [`lib/ClockLogic/ClockLogic.cpp`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/ClockLogic/ClockLogic.cpp)

### `PowerButton`

Responsabilidad:

- detectar click simple
- detectar doble click
- detectar hold largo
- traducir input fisico a acciones de alto nivel

Archivos:

- [`lib/PowerButton/PowerButton.h`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/PowerButton/PowerButton.h)
- [`lib/PowerButton/PowerButton.cpp`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/PowerButton/PowerButton.cpp)

### `ImuMotion`

Responsabilidad:

- suavizar `roll` y `pitch`
- convertir orientacion en mirada y desplazamiento ocular
- editar el menu de reloj por inclinacion
- detectar shakes, movimiento sostenido y gestos verticales

Archivos:

- [`lib/ImuMotion/ImuMotion.h`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/ImuMotion/ImuMotion.h)
- [`lib/ImuMotion/ImuMotion.cpp`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/ImuMotion/ImuMotion.cpp)

### `PetBehavior`

Responsabilidad:

- unir `phaseIntent` con `gestureOverride`
- aplicar easing a ceja, squint y overlay
- copiar el estado visual derivado al `PetUI`

Archivos:

- [`lib/PetBehavior/PetBehavior.h`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/PetBehavior/PetBehavior.h)
- [`lib/PetBehavior/PetBehavior.cpp`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/PetBehavior/PetBehavior.cpp)

### `PetUI`

Responsabilidad:

- render del modo `Pet`
- render del modo `Clock`
- render del menu de reloj
- animaciones de parpadeo y reposo

La expresividad actual depende mas de la forma del ojo que de cejas externas:

- apertura
- squint
- deformacion superior e inferior de la esclera
- posicion y estilo de pupila

Archivos:

- [`lib/PetUI/PetUI.h`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/PetUI/PetUI.h)
- [`lib/PetUI/PetUI.cpp`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/PetUI/PetUI.cpp)

### `RtcClock`, `ImuQmi8658`, `Melodies`

Responsabilidad:

- encapsular acceso a perifericos y audio

Archivos:

- [`lib/RtcClock`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/RtcClock)
- [`lib/ImuQmi8658`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/ImuQmi8658)
- [`lib/Melodies`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/lib/Melodies)

## Estados principales

### `Mood`

Estados visuales base:

- `Neutral`
- `Angry`
- `Sleepy`
- `Relax`
- `Confused`
- `Dazed`
- `Glitch`

### `OverlayFx`

Acentos visuales temporales:

- `None`
- `Calm`
- `Doze`
- `Focus`
- `Dizzy`
- `Confused`
- `Startle`
- `Glitch`

## Politicas importantes

### Sleep nocturno

Si el `RTC` es valido y la fase horaria entra en `NightSleep`, el equipo duerme hasta la siguiente ventana programada.

### Sleep por bateria baja

Si la bateria baja a `30%` y no parece estar en USB:

- si el `RTC` es valido, el equipo duerme hasta la siguiente ventana de wake
- si el `RTC` no es valido, usa un fallback de `30 minutos`

### Persistencia

Se guarda en `Preferences`:

- backup de fecha/hora
- modo de UI actual

## Criterio para futuro refactor

Antes de mover la app a tareas separadas, conviene agotar esta ruta:

- extraer diagnostico serial a modulo propio
- centralizar configuracion tunable
- agregar tests para utilidades puras

`FreeRTOS` solo deberia entrar cuando haya una necesidad concreta de concurrencia, como audio real, conectividad o distintos ritmos de update que no convenga multiplexar en un loop unico.
