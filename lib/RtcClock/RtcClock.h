#pragma once
#include <Arduino.h>

struct DateTime {
  uint16_t year;
  uint8_t month, day;
  uint8_t hour, minute, second;
};

class RtcClock {
public:
  bool begin();
  bool read(DateTime& out);

  // escribe fecha/hora (UTC/local da lo mismo mientras seas consistente)
  bool set(const DateTime& dt);

  // “válido” si el chip responde y ya vimos un año razonable
  bool valid() const { return _ok && _hasTime; }

  bool isRunning() const { return _ok; }

private:
  bool _ok = false;
  bool _hasTime = false;

  static constexpr uint8_t ADDR = 0x51; // PCF85063(A)

  static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
  static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }
};
