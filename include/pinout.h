#pragma once
#include <Arduino.h>

// HW I2C (GPIO10/11)
static constexpr int PIN_I2C_SCL = 10;
static constexpr int PIN_I2C_SDA = 11;

// OLED SH1106 / SSD1306 típico
static constexpr uint8_t OLED_I2C_ADDR = 0x3C;

// Waveshare ESP32-S3-Touch-LCD-1.69 battery sense divider
static constexpr int PIN_BAT_ADC = 1;

// Power key and hold lines used by this board family.
static constexpr int PIN_BTN_PWR = 40;
static constexpr int PIN_PWR_HOLD = 41;

// Buzzer (según tu dato)
static constexpr int PIN_BUZZER = 255;
