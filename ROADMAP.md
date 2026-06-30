# Roadmap

> Hoja de ruta propuesta para evolucionar `Virtualpet_Lite` desde prototipo a una mascota de escritorio mas completa y mantenible.

## Enfoque

La mejor forma de planificar `Virtualpet_Lite` es tratarlo como un producto embedded pequeno con cuatro frentes que avanzan en paralelo:

1. Base tecnica estable
2. Comportamiento de la mascota
3. Interaccion y configuracion
4. Pulido de producto

Ese enfoque sigue bien la logica de orquestacion: primero asegurar que el firmware corre de forma confiable, luego expandir personalidad y finalmente cerrar experiencia de uso.

## Vision

Construir una pet de escritorio fisica que:

- Acompane la rutina diaria con expresiones y sonidos
- Reaccione al contexto fisico del dispositivo
- Tenga una personalidad reconocible
- Pueda configurarse sin friccion
- Sea lo bastante robusta para vivir encendida en escritorio

## Estado de partida

Hoy ya existe:

- Un loop principal con UI, RTC, IMU y buzzer
- Un modelo inicial de estados emocionales por horario
- Render expresivo basico sobre OLED
- Latch de energia funcional con `PWR hold`
- Ajuste de RTC por comandos seriales
- Separacion modular razonable para seguir iterando

Hoy falta cerrar:

- Interaccion por boton o flujo de configuracion
- Documentacion de wiring, setup y uso
- Expresividad visual adicional

## Fase 1: Estabilizar base tecnica

Objetivo: dejar el firmware compilando, flasheando y comportandose de forma predecible.

Entregables:

- Corregir uso de LEDC para el core actual de Arduino-ESP32.
- Confirmar placa real, tamano de flash y disponibilidad de PSRAM.
- Verificar que OLED, RTC, IMU y buzzer conviven sin conflictos.
- Agregar logs de arranque mas claros para diagnostico.
- Documentar wiring real y variantes soportadas.

Definicion de listo:

- `platformio run` exitoso.
- Boot limpio en hardware.
- Hora, IMU, OLED y buzzer operativos en una misma sesion.

Estado:

- `Completada mayormente con evidencia tecnica`.
- Build validado con la board real `ESP32-S3 DevKitC-1 N16R8`.
- RTC validado con energia en el dominio `RTC + -`.
- Latch de energia validado con `GPIO41`.
- Queda pendiente documentar con mas precision el esquema de bateria recomendado para `BAT` y `RTC`.

Pendiente real de Fase 1:

- Confirmar y documentar una bateria segura y recomendada para el conector `RTC + -`.
- Si es posible, identificar por foto/esquema si existe forma de aislar o deshabilitar la carga del dominio RTC por hardware.

## Fase 2: Convertir el prototipo en mascota coherente

Objetivo: que la pet se sienta viva y consistente, no solo funcional.

Entregables:

- Hacer visible `bodyLeanX` en la UI.
- Afinar transiciones de animacion entre estados.
- Separar mejor "estado emocional", "estado fisico" y "animacion".
- Incorporar variantes visuales por fase del dia.

Definicion de listo:

- La mascota comunica claramente su estado aunque no haya audio.
- El loop mantiene fluidez sin pausas perceptibles.

Estado:

- `Avance real implementado`.
- `Melodies` ya reemplazo los cues bloqueantes de audio.
- `bodyLeanX` ya es visible en la UI actual.
- La interaccion IMU -> ojos fue afinada directamente en hardware real.
- Pendiente: seguir puliendo expresividad y consistencia visual entre estados.

## Fase 3: Agregar interaccion de usuario

Objetivo: permitir que la pet no dependa solo del RTC y el movimiento.

Entregables:

- Consolidar el uso del boton `PWR` como boton de sistema.
- Crear un modo de ajuste de hora simple.
- Agregar acciones manuales: despertar, silenciar, cambiar humor, etc.
- Definir feedback visual y sonoro para cada accion.
- Evaluar menu minimo o "long press" segun complejidad.

Definicion de listo:

- Un usuario puede configurar lo esencial sin recompilar.
- El dispositivo responde de forma clara a input fisico.

Estado:

- `Iniciado`.
- `Long press` de `PWR` ya implementado para apagado.
- El siguiente paso natural es agregar interaccion visible o menu simple sin depender del monitor serie.

## Fase 4: Profundizar personalidad

Objetivo: pasar de estados horarios a un personaje mas rico.

Entregables:

- Mas moods y expresiones.
- Eventos especiales por hora o contexto.
- Rutinas aleatorias suaves que no rompan la personalidad.
- Modo glitch, descanso, celebracion o aburrimiento.
- Tabla de comportamiento editable desde configuracion.
- Variantes visuales mas claras entre pupila, esclera y cejas segun estado.

Definicion de listo:

- La mascota tiene identidad propia y se siente menos deterministica.

## Fase 5: Preparar MVP de escritorio

Objetivo: dejar una version que se pueda usar y mostrar como producto.

Entregables:

- README completo y guia de montaje.
- Parametros centralizados en configuracion.
- Manejo de errores visibles cuando falla RTC o IMU.
- Consumo y estabilidad validados para uso prolongado.
- Demo reproducible con flujo de setup simple.
- Documentacion alineada con el nombre del repo `Virtualpet_Lite`.

Definicion de listo:

- Otra persona puede montar, compilar y entender el proyecto sin asistencia.

## Backlog recomendado

Temas que no urgen, pero suman mucho valor:

- Persistencia en NVS para hora y preferencias.
- Perfiles de comportamiento.
- Soporte para mas displays.
- Animaciones por frames o sprites.
- Modo nocturno mas tenue.
- Integracion futura con Wi-Fi o sincronizacion de hora.

## Riesgos principales

- Incompatibilidad entre version de core y APIs usadas.
- Suposiciones incorrectas sobre la placa real.
- Bloqueos por `delay` que degraden la sensacion de vida.
- Crecimiento de comportamiento sin una maquina de estados clara.
- Acoplamiento entre UI, audio y logica temporal.

## Orden recomendado de ejecucion

1. Resolver build y hardware real.
2. Reemplazar audio bloqueante.
3. Hacer visible toda la logica ya existente en UI.
4. Incorporar boton y configuracion de hora.
5. Expandir personalidad y polish.

## Criterio de priorizacion

Para este proyecto conviene priorizar cada siguiente tarea con estas preguntas:

- ¿Hace que la mascota sea mas confiable?
- ¿Hace que se entienda mejor su personalidad?
- ¿Reduce friccion para usarla todos los dias?
- ¿Evita deuda tecnica que luego frene el resto?

Si una tarea no mejora alguna de esas cuatro cosas, probablemente puede esperar.
