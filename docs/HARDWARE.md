# Hardware y Diagnostico

## Variante documentada

La documentacion del firmware esta alineada con una `Waveshare ESP32-S3-Touch-LCD-1.69`, pero el proyecto actual usa un `OLED I2C` externo para la UI activa.

## Bus I2C usado por el firmware

- `SDA`: `GPIO11`
- `SCL`: `GPIO10`

Dispositivos detectados en hardware real:

- `0x3C`: OLED `SH1106/SSD1306`
- `0x51`: `RTC PCF85063`
- `0x6B`: `IMU QMI8658`

Respuesta adicional observada:

- `0x7E`: respuesta reservada o ghost, no usada por el firmware

## Medicion de bateria

La bateria se mide por `ADC` en:

- `GPIO1`

Referencia de Waveshare:

- divisor `R1 = 200k`
- divisor `R2 = 100k`

El firmware convierte:

```text
Vadc = raw * (3.3 / 4095)
Vbatt = Vadc * BATTERY_ADC_SCALE
```

Valor actual de calibracion:

```text
BATTERY_ADC_SCALE = 3.16
```

Ese ajuste se aplico porque una medicion real mostro:

- firmware: `3.93-3.94V`
- tester: `4.15V`

## Botones y energia

- `GPIO40`: boton `PWR`
- `GPIO41`: `PWR_HOLD`

Uso actual:

- click simple: cambia modo de UI
- doble click: abre `SET CLOCK`
- hold largo: guarda hora o apaga el equipo

## Audio

El buzzer esta deshabilitado en esta variante:

- `PIN_BUZZER = 255`

Se dejo asi para evitar el pitido continuo observado al usar un pin incompatible con la placa real.

## Diagnostico por serial

Comandos disponibles:

```text
BAT?
I2C?
RTC?
SET_RTC YYYY-MM-DD HH:MM:SS
```

## Ejemplo de salida I2C

```text
I2C scan on SDA=11 SCL=10
  - 0x3C  OLED SH1106/SSD1306
  - 0x51  RTC PCF85063
  - 0x6B  IMU QMI8658?
  - 0x7E  Reserved / ghost response
I2C summary: 0x3C, 0x51, 0x6B, 0x7E
```
