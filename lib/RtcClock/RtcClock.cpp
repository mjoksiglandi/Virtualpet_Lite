#include "RtcClock.h"
#include <Wire.h>

bool RtcClock::begin() {
  Wire.beginTransmission(ADDR);
  _ok = (Wire.endTransmission() == 0);
  return _ok;
}

bool RtcClock::read(DateTime& out) {
  if (!_ok) return false;

  // PCF85063(A) time regs típicos desde 0x04:
  // 0x04 sec, 0x05 min, 0x06 hour, 0x07 day,
  // 0x08 weekday, 0x09 month, 0x0A year(00..99)
  Wire.beginTransmission(ADDR);
  Wire.write(0x04);
  if (Wire.endTransmission(false) != 0) return false;

  uint8_t n = Wire.requestFrom((int)ADDR, 7);
  if (n != 7) return false;

  uint8_t sec   = Wire.read();
  uint8_t min   = Wire.read();
  uint8_t hour  = Wire.read();
  uint8_t day   = Wire.read();
  uint8_t wday  = Wire.read(); (void)wday;
  uint8_t month = Wire.read();
  uint8_t year  = Wire.read();

  out.second = bcd2dec(sec  & 0x7F);
  out.minute = bcd2dec(min  & 0x7F);
  out.hour   = bcd2dec(hour & 0x3F);
  out.day    = bcd2dec(day  & 0x3F);
  out.month  = bcd2dec(month & 0x1F);
  out.year   = 2000 + bcd2dec(year);

  // sanity: evita “00/00/00”
  if (out.year >= 2020 && out.year <= 2099 && out.month >= 1 && out.month <= 12 && out.day >= 1 && out.day <= 31) {
    _hasTime = true;
  }

  return true;
}

bool RtcClock::readStatus(bool& oscillatorStopped) {
  oscillatorStopped = false;
  if (!_ok) return false;

  Wire.beginTransmission(ADDR);
  Wire.write(0x04);
  if (Wire.endTransmission(false) != 0) return false;

  uint8_t n = Wire.requestFrom((int)ADDR, 1);
  if (n != 1) return false;

  const uint8_t sec = Wire.read();
  oscillatorStopped = (sec & 0x80) != 0;
  return true;
}

bool RtcClock::readControl(bool& clockStopped) {
  clockStopped = false;
  if (!_ok) return false;

  Wire.beginTransmission(ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return false;

  uint8_t n = Wire.requestFrom((int)ADDR, 1);
  if (n != 1) return false;

  const uint8_t ctrl1 = Wire.read();
  clockStopped = (ctrl1 & 0x20) != 0;
  return true;
}

bool RtcClock::set(const DateTime& dt) {
  if (!_ok) return false;

  // Ensure the RTC divider chain is running and default control flags are cleared.
  Wire.beginTransmission(ADDR);
  Wire.write(0x00);
  Wire.write((uint8_t)0x00); // Control_1: STOP=0, normal 24h mode
  Wire.write((uint8_t)0x00); // Control_2: clear flags / disable interrupts
  if (Wire.endTransmission() != 0) return false;

  // escribe 7 bytes desde 0x04
  Wire.beginTransmission(ADDR);
  Wire.write(0x04);

  Wire.write(dec2bcd(dt.second) & 0x7F);
  Wire.write(dec2bcd(dt.minute) & 0x7F);
  Wire.write(dec2bcd(dt.hour) & 0x3F);
  Wire.write(dec2bcd(dt.day) & 0x3F);
  Wire.write((uint8_t)0x00); // weekday (si no lo calculamos)
  Wire.write(dec2bcd(dt.month) & 0x1F);
  Wire.write(dec2bcd((uint8_t)(dt.year % 100)));

  if (Wire.endTransmission() != 0) return false;

  _hasTime = true;
  return true;
}
