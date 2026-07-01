#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "../RtcClock/RtcClock.h"

enum class Mood : uint8_t {
  Neutral = 0,
  Angry,
  Sleepy,
  Relax,
  Confused,
  Dazed,
  Glitch
};

enum class OverlayFx : uint8_t {
  None = 0,
  Calm,
  Doze,
  Focus,
  Dizzy,
  Confused,
  Startle,
  Glitch
};

struct PetState {
  Mood mood = Mood::Neutral;

  int8_t lookX = 0;
  int8_t lookY = 0;
  int8_t eyeShiftX = 0;
  int8_t eyeShiftY = 0;
  uint8_t blink = 0;
  uint8_t brow = 40;
  uint8_t squint = 0;
  int8_t bodyLeanX = 0;
  int8_t idleBobY = 0;
  bool imuActive = false;
  bool motionAlert = false;
  OverlayFx overlay = OverlayFx::None;
  uint8_t overlayStrength = 0;
};

struct ClockMenuView {
  bool active = false;
  bool rtcValid = false;
  bool dirty = false;
  uint8_t fieldIndex = 0;
  DateTime value{2026, 1, 1, 8, 0, 0};
};

struct ClockFaceView {
  bool active = false;
  bool rtcValid = false;
  bool batteryVisible = false;
  bool batteryUsb = false;
  uint8_t batteryPercent = 0;
  uint16_t batteryMv = 0;
  DateTime value{2026, 1, 1, 8, 0, 0};
};

class PetUI {
public:
  explicit PetUI(U8G2& d);

  void render(
    const DateTime* dtNullable,
    const ClockMenuView* menuNullable = nullptr,
    const ClockFaceView* clockNullable = nullptr
  );

  void tickBlink();
  void tickMoodAuto();

  const PetState& state() const { return _s; }
  PetState& state() { return _s; }

private:
  U8G2& _d;
  PetState _s;

  uint32_t _nextBlinkAt = 0;
  uint32_t _blinkStart = 0;
  bool _blinking = false;
  Mood _lastMood = Mood::Neutral;
  Mood _fromMood = Mood::Neutral;
  uint32_t _moodTransitionStart = 0;
  uint32_t _idleRetargetAt = 0;
  uint32_t _microAnimUntil = 0;
  int8_t _idleTargetLookX = 0;
  int8_t _idleTargetLookY = 0;
  uint8_t _microSquint = 0;

  void drawEyes(int w, int h);
  void drawGlitchOverlay(int w, int h);
  void drawAccentOverlay(int w, int h);
  void drawClockMenu(int w, int h, const ClockMenuView& menu);
  void drawClockFace(int w, int h, const ClockFaceView& clock);
};
