#pragma once

#include <Arduino.h>
#include "PetUI.h"
#include "RtcClock.h"

enum class UiMode : uint8_t {
  Pet = 0,
  Clock = 1,
  Status = 2
};

enum class PetPhase : uint8_t {
  Unknown = 0,
  SleepyAM,
  ActiveAM,
  LunchRest,
  PostLunchNap,
  WorkToAngry,
  RelaxPM,
  NightSleep
};

enum class OverlayState : uint8_t {
  None = 0,
  Charging = 1 << 0,
  LowBattery = 1 << 1,
  RtcError = 1 << 2,
  ImuError = 1 << 3,
  Interaction = 1 << 4,
  Focus = 1 << 5
};

inline OverlayState operator|(OverlayState a, OverlayState b) {
  return (OverlayState)((uint8_t)a | (uint8_t)b);
}

inline OverlayState& operator|=(OverlayState& a, OverlayState b) {
  a = a | b;
  return a;
}

inline bool hasOverlay(OverlayState value, OverlayState flag) {
  return (((uint8_t)value) & ((uint8_t)flag)) != 0;
}

struct BatterySnapshot {
  bool valid = false;
  bool usbLikely = false;
  uint16_t raw = 0;
  uint16_t mv = 0;
  uint8_t percent = 0;
};

struct MoodIntent {
  Mood mood = Mood::Neutral;
  uint8_t brow = 20;
  uint8_t squint = 0;
  OverlayFx overlay = OverlayFx::None;
  uint8_t overlayStrength = 0;
};

struct GestureOverride {
  bool active = false;
  Mood mood = Mood::Neutral;
  uint8_t brow = 20;
  uint8_t squint = 0;
  OverlayFx overlay = OverlayFx::None;
  uint8_t overlayStrength = 0;
  uint32_t untilMs = 0;
};

struct ClockMenuState {
  bool active = false;
  bool dirty = false;
  bool rtcValidOnEntry = false;
  bool usingBackupSeed = false;
  uint8_t fieldIndex = 0;
  uint32_t lastInteractionMs = 0;
  uint32_t lastTiltStepMs = 0;
  DateTime draft{2026, 1, 1, 8, 40, 0};
};

struct VisualState {
  bool imuActive = false;
  bool motionAlert = false;
  int8_t lookX = 0;
  int8_t lookY = 0;
  int8_t eyeShiftX = 0;
  int8_t eyeShiftY = 0;
  int8_t bodyLeanX = 0;
};
