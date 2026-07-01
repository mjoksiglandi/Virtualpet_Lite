#pragma once

#include <Arduino.h>
#include "FirmwareTypes.h"

class BatteryMonitor {
public:
  BatteryMonitor(int adcPin, float scale) : _adcPin(adcPin), _scale(scale) {}

  void begin();
  void update(uint32_t nowMs);
  const BatterySnapshot& snapshot() const { return _snapshot; }

private:
  int _adcPin;
  float _scale;
  BatterySnapshot _snapshot;
  uint32_t _nextBatteryAt = 0;
  float _filteredMv = 0.0f;

  uint16_t readBatteryMillivolts() const;
  static uint8_t percentFromMillivolts(uint16_t mv);
};
