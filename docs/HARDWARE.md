# Hardware y Diagnostico

## Variante actual

La documentacion del repo esta alineada con una placa de la familia `Waveshare ESP32-S3-Touch-LCD-1.69`, pero la UI activa del proyecto corre hoy sobre un `OLED I2C` externo detectado en `0x3C`.

En otras palabras:

- la base MCU es `ESP32-S3`
- la pantalla usada por este firmware hoy no es la LCD integrada
- el flujo visual activo se prueba sobre `OLED SH1106/SSD1306`

## Pines usados

Definidos en [`include/pinout.h`](C:/Users/juan.cornejo/Documents/PlatformIO/Projects/gif/include/pinout.h):

- `GPIO10`: `I2C SCL`
- `GPIO11`: `I2C SDA`
- `GPIO1`: `BAT ADC`
- `GPIO40`: `BTN_PWR`
- `GPIO41`: `PWR_HOLD`
- `GPIO255`: buzzer deshabilitado en esta variante

## Bus I2C

Dispositivos observados en hardware real:

- `0x3C`: OLED `SH1106/SSD1306`
- `0x51`: `RTC PCF85063`
- `0x6B`: `IMU QMI8658`

Respuesta adicional:

- `0x7E`: respuesta reservada o ghost, no usada por el firmware

Ejemplo de salida:

```text
I2C scan on SDA=11 SCL=10
  - 0x3C  OLED SH1106/SSD1306
  - 0x51  RTC PCF85063
  - 0x6B  IMU QMI8658?
  - 0x7E  Reserved / ghost response
I2C summary: 0x3C, 0x51, 0x6B, 0x7E
```

## RTC

Chip esperado:

- `PCF85063`

Uso actual:

- entrega hora local al firmware
- define fases horarias de la pet
- permite deep sleep nocturno con wake programado

Si el `RTC` no es valido:

- el menu de reloj puede restaurar la hora manualmente
- el sleep por bateria baja usa un fallback temporal

## IMU

Chip esperado:

- `QMI8658`

Uso actual:

- seguimiento continuo de `roll` y `pitch`
- gestos para editar reloj
- reacciones temporales como `Confused`, `Dazed` y `Glitch`

## Medicion de bateria

La bateria se mide por `ADC` en:

- `GPIO1`

Modelo actual:

```text
Vadc = raw * (3.3 / 4095)
Vbatt = Vadc * BATTERY_ADC_SCALE
```

Valor de calibracion actual:

```text
BATTERY_ADC_SCALE = 3.16
```

Notas importantes:

- el porcentaje es lineal entre `3.30V` y `4.20V`
- `usbLikely` se infiere por voltaje alto, no por una linea dedicada
- el firmware entra en deep sleep por proteccion cuando la bateria cae a `30%`

## Energia

Lineas usadas:

- `GPIO40`: boton `PWR`
- `GPIO41`: `PWR_HOLD`

Comportamiento:

- click simple: cambia modo de UI
- doble click: abre `SET CLOCK`
- hold largo fuera del menu: apaga la placa
- hold largo dentro del menu: guarda fecha y hora

## Audio

El buzzer esta deshabilitado en esta variante:

```text
PIN_BUZZER = 255
```

Se mantiene asi para evitar conflictos con la placa real usada durante las pruebas.

## Diagnostico por serial

Comandos disponibles:

```text
BAT?
I2C?
RTC?
SET_RTC YYYY-MM-DD HH:MM:SS
```

Sugerencia de chequeo rapido:

1. correr `I2C?`
2. confirmar `0x3C`, `0x51` y `0x6B`
3. correr `RTC?`
4. correr `BAT?`

## Riesgos de hardware aun abiertos

- la calibracion de bateria sigue siendo aproximada
- el estado `usbLikely` puede fallar cerca de bateria llena
- la variante real puede no coincidir exactamente con la documentacion original de Waveshare
