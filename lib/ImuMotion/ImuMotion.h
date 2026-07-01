#pragma once

#include <Arduino.h>
#include "FirmwareTypes.h"
#include "ImuQmi8658.h"

class ImuMotion {
public:
  ImuMotion(float menuTiltThreshold, uint32_t menuTiltRepeatMs)
    : _menuTiltThreshold(menuTiltThreshold), _menuTiltRepeatMs(menuTiltRepeatMs) {}

  bool update(
    uint32_t now,
    const ImuSample& sample,
    bool clockMenuActive,
    ClockMenuState& clockMenu,
    VisualState& visual,
    GestureOverride& gestureOverride
  );

  void updateNoSample(VisualState& visual) const;

private:
  float _menuTiltThreshold;
  uint32_t _menuTiltRepeatMs;
  float _rollF = 0.0f;
  float _pitchF = 0.0f;
  float _lastRollF = 0.0f;
  float _lastPitchF = 0.0f;
  uint32_t _motionAlertUntil = 0;
  uint32_t _shakeCooldownUntil = 0;
  bool _shakeArmed = true;
  uint32_t _heavyMotionSince = 0;
  uint32_t _tiltGestureCooldownUntil = 0;
  uint32_t _pitchHoldSince = 0;
  int8_t _pitchHoldDir = 0;

  static int8_t easeToZero(int8_t value);
};
