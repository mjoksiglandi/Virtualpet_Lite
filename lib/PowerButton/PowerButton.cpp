#include "PowerButton.h"

PowerAction PowerButton::update(uint32_t now, bool pressed, bool clockMenuActive) {
  if (pressed) {
    if (_pressedAt == 0) {
      _pressedAt = now;
      _shutdownArmed = false;
      _menuHoldHandled = false;
    } else if (clockMenuActive && !_menuHoldHandled && (now - _pressedAt) >= _longPressMs) {
      _menuHoldHandled = true;
      return PowerAction::ClockMenuSave;
    } else if (!clockMenuActive && !_shutdownArmed && (now - _pressedAt) >= _longPressMs) {
      _shutdownArmed = true;
      return PowerAction::Shutdown;
    }
    return PowerAction::None;
  }

  if (_pressedAt != 0) {
    const uint32_t pressDur = now - _pressedAt;
    if (clockMenuActive) {
      if (!_menuHoldHandled && pressDur <= _clickMs) {
        _pressedAt = 0;
        _shutdownArmed = false;
        _menuHoldHandled = false;
        return PowerAction::ClockMenuNextField;
      }
    } else if (!_shutdownArmed && pressDur <= _clickMs) {
      _clickCount++;
      _releasedAt = now;
    }
  }

  _pressedAt = 0;
  _shutdownArmed = false;
  _menuHoldHandled = false;

  if (!clockMenuActive && _clickCount > 0 && (now - _releasedAt) > _doubleClickGapMs) {
    const uint8_t clicks = _clickCount;
    _clickCount = 0;
    return clicks >= 2 ? PowerAction::OpenClockMenu : PowerAction::ToggleUiMode;
  }

  return PowerAction::None;
}
