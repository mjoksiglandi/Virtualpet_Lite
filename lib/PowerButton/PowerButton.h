#pragma once

#include <Arduino.h>

enum class PowerAction : uint8_t {
  None = 0,
  ToggleUiMode,
  OpenClockMenu,
  Shutdown,
  ClockMenuNextField,
  ClockMenuSave
};

class PowerButton {
public:
  PowerButton(uint32_t longPressMs, uint32_t clickMs, uint32_t doubleClickGapMs)
    : _longPressMs(longPressMs), _clickMs(clickMs), _doubleClickGapMs(doubleClickGapMs) {}

  PowerAction update(uint32_t now, bool pressed, bool clockMenuActive);

private:
  uint32_t _longPressMs;
  uint32_t _clickMs;
  uint32_t _doubleClickGapMs;
  uint32_t _pressedAt = 0;
  bool _shutdownArmed = false;
  bool _menuHoldHandled = false;
  uint32_t _releasedAt = 0;
  uint8_t _clickCount = 0;
};
