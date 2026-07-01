# Roadmap

> Hoja de ruta realista para evolucionar `Virtualpet_Lite` desde el prototipo actual a un dispositivo mas pulido y mantenible.

## Estado de partida

Hoy ya existe una base funcional sobre hardware real:

- modo `Pet`
- modo `Clock`
- menu local para ajustar `RTC`
- deep sleep nocturno con wake a `08:40`
- IMU integrada a la UI
- medicion de bateria por `ADC` en `GPIO1`
- escaneo I2C por serial

El proyecto ya no esta en etapa de bring-up puro. El foco ahora es consolidar experiencia de uso, robustez y calibracion.

## Prioridades vigentes

1. Confiabilidad en hardware real
2. Claridad de la experiencia de usuario
3. Calibracion de sensores y energia
4. Mantenibilidad del firmware

## Fase 1: Cerrar base de hardware

Objetivo: dejar estabilizados los comportamientos dependientes de placa real.

Ya completado:

- board `esp32-s3-devkitc1-n16r8` confirmada en `PlatformIO`
- `RTC`, `IMU` y `OLED` validados
- latch de energia operativo
- audio deshabilitado para evitar conflicto de pinout
- medicion inicial de bateria integrada

Pendiente:

- terminar calibracion de bateria con 2 o 3 puntos reales
- decidir si `usb_likely` queda como heuristica o se elimina
- documentar mejor las conexiones fisicas usadas en esta variante

## Fase 2: Mejorar UX local

Objetivo: que el dispositivo se pueda usar sin depender de serial.

Pendiente:

- refinar layout del `Clock Mode`
- hacer mas clara la UI del menu de reloj
- agregar feedback visual cuando se guarda la hora
- evaluar un pequeno indicador de estado para deep sleep, RTC invalido o bateria baja

## Fase 3: Afinar comportamiento de la mascota

Objetivo: que la pet se sienta mas viva y consistente.

Pendiente:

- pulir transiciones entre moods
- expandir repertorio visual sin romper fluidez
- definir mejor la relacion entre mood horario, gesto IMU y overlays temporales
- agregar micro animaciones contextuales en reposo

## Fase 4: Energia y autonomia

Objetivo: hacer el dispositivo mas util fuera de USB.

Pendiente:

- calibrar correctamente la curva de bateria
- definir umbrales para low battery
- evaluar bajar brillo, FPS o actividad visual con bateria baja
- revisar si el wake de `08:40` debe ser configurable desde menu

## Fase 5: Preparar MVP reproducible

Objetivo: que otra persona pueda replicar el proyecto con baja friccion.

Pendiente:

- documentar montaje y cableado real
- agregar capturas o fotos de referencia
- dejar parametros clave mas centralizados
- revisar mensajes seriales y troubleshooting
- definir una version demostrable como release

## Backlog tecnico

- separar mejor configuracion de hardware por variante
- extraer bateria y diagnosticos a un modulo propio
- revisar si conviene reemplazar algunos `delay` pequeños por scheduling no bloqueante
- agregar tests de utilidades puras como fecha, clamps y conversiones

## Riesgos actuales

- que el hardware real no coincida con la variante documentada por Waveshare
- que la medicion ADC de bateria dependa demasiado de tolerancias del divisor
- que cambios de pinout vuelvan a romper pantalla o audio
- que crezca la logica en `main.cpp` sin extraer responsabilidades

## Siguiente trabajo recomendado

1. Terminar calibracion de bateria.
2. Mostrar estado de bateria de forma confiable en `Clock Mode`.
3. Mejorar la UX del menu local.
4. Seguir profundizando personalidad visual.
