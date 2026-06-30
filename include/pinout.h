#pragma once
#include <Arduino.h>

// HW I2C (GPIO10/11)
static constexpr int PIN_I2C_SCL = 10;
static constexpr int PIN_I2C_SDA = 11;

// OLED SH1106 / SSD1306 típico
static constexpr uint8_t OLED_I2C_ADDR = 0x3C;

// Key2 (según lo que venías usando)
static constexpr int PIN_BTN_USER = 40;

// Buzzer (según tu dato)
static constexpr int PIN_BUZZER = 42;
