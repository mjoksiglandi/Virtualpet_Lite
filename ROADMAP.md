# Roadmap

> Hoja de ruta realista para seguir puliendo `Virtualpet Lite` sobre hardware real.

## Estado actual

Hoy el proyecto ya tiene una base funcional y mas mantenible que al inicio:

- modos `Pet` y `Clock`
- `RTC`, `IMU` y `OLED` validados
- menu local para ajustar hora
- rutina diaria por fases
- deep sleep nocturno
- deep sleep por bateria baja
- medicion de bateria integrada
- logica refactorizada en modulos

El foco ya no es bring-up. El foco es calidad de experiencia, robustez energetica y personalidad visual.

## Prioridades vigentes

1. Confiabilidad en hardware real
2. Claridad de la UX local
3. Consistencia de la personalidad visual
4. Mantenibilidad del firmware

## Fase 1: Cerrar base de hardware

Objetivo: reducir incertidumbre de la placa real y de la medicion energetica.

Pendiente:

- calibrar bateria con 2 o 3 puntos medidos
- decidir si `usbLikely` se mantiene o se reemplaza
- documentar el cableado fisico real de la variante usada
- validar el umbral de sleep por bateria baja en uso prolongado

## Fase 2: Mejorar UX local

Objetivo: que el dispositivo sea entendible sin depender de serial.

Pendiente:

- mejorar la legibilidad del `Clock Mode`
- agregar feedback visual al guardar hora
- hacer mas evidente el estado de `RTC` invalido
- evaluar un indicador visual para bateria baja antes del sleep

## Fase 3: Profundizar personalidad visual

Objetivo: que la pet se sienta viva, legible y consistente.

Pendiente:

- seguir refinando las deformaciones de ojo por mood
- exagerar lo necesario para que `Confused` y `Dazed` sean legibles en OLED pequeno
- pulir la persistencia y salida de gestos temporales
- sumar pequenas variaciones contextuales en reposo sin ruido visual

## Fase 4: Energia y autonomia

Objetivo: aumentar utilidad fuera de USB.

Pendiente:

- revisar si conviene bajar contraste o actividad visual con bateria baja
- validar el comportamiento del wake programado cuando entra por low battery
- evaluar si el wake de `08:40` debe volverse configurable

## Fase 5: Preparar una version demostrable

Objetivo: dejar el proyecto listo para que otra persona lo replique y entienda.

Pendiente:

- agregar capturas y fotos de referencia
- documentar flujo de armado y pruebas
- centralizar parametros tunables de comportamiento
- dejar una release demostrable con changelog

## Backlog tecnico

- extraer el diagnostico serial a un modulo propio
- agregar tests para utilidades puras de fecha y clamps
- revisar si conviene eliminar pequenos `delay` residuales
- evaluar `FreeRTOS` solo si aparece una necesidad real de concurrencia

## Riesgos actuales

- que la bateria medida no represente bien el estado real de carga
- que la legibilidad de algunos moods siga siendo insuficiente en OLED pequeno
- que cambios de pinout rompan perifericos en variantes de hardware cercanas
- que la personalidad visual crezca mas rapido que la capacidad de testearla

## Siguiente trabajo recomendado

1. Calibrar bateria con mediciones reales.
2. Afinar la legibilidad de `Dazed` y `Confused` en hardware.
3. Mejorar feedback visual del menu de reloj.
4. Seguir compactando la configuracion tunable del firmware.
