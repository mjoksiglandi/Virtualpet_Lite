#include "BatteryMonitor.h"

void BatteryMonitor::begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(_adcPin, ADC_11db);
  pinMode(_adcPin, INPUT);
}

void BatteryMonitor::update(uint32_t nowMs) {
  if (_nextBatteryAt != 0 && nowMs < _nextBatteryAt) return;

  const uint16_t mv = readBatteryMillivolts();
  if (_filteredMv <= 0.0f) _filteredMv = (float)mv;
  else _filteredMv = _filteredMv * 0.75f + (float)mv * 0.25f;

  _snapshot.valid = true;
  _snapshot.raw = (uint16_t)analogRead(_adcPin);
  _snapshot.mv = (uint16_t)roundf(_filteredMv);
  _snapshot.percent = percentFromMillivolts(_snapshot.mv);
  _snapshot.usbLikely = _snapshot.mv >= 4150;
  _nextBatteryAt = nowMs + 1500;
}

uint16_t BatteryMonitor::readBatteryMillivolts() const {
  const uint32_t samples = 12;
  uint32_t acc = 0;
  for (uint32_t i = 0; i < samples; ++i) {
    acc += (uint32_t)analogRead(_adcPin);
    delay(2);
  }

  const float raw = (float)acc / (float)samples;
  const float vadc = raw * (3.3f / 4095.0f);
  const float vbatt = vadc * _scale;
  return (uint16_t)roundf(vbatt * 1000.0f);
}

uint8_t BatteryMonitor::percentFromMillivolts(uint16_t mv) {
  if (mv <= 3300) return 0;
  if (mv >= 4200) return 100;
  return (uint8_t)(((uint32_t)(mv - 3300) * 100u) / 900u);
}
