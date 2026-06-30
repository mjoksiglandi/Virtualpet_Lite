#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "../RtcClock/RtcClock.h"

enum class Mood : uint8_t {
  Neutral = 0,
  Angry,
  Sleepy,
  Relax,
  Glitch
};

struct PetState {
  Mood mood = Mood::Neutral;

  // Pupilas
  int8_t lookX = 0;
  int8_t lookY = 0;

  // Desplazamiento completo de los ojos (IMU / caída)
  int8_t eyeShiftX = 0;
  int8_t eyeShiftY = 0;

  // Blink 0–100
  uint8_t blink = 0;

  // Fuerza de cejas 0–100
  uint8_t brow = 40;

  int8_t bodyLeanX = 0;
  bool imuActive = false;
  bool motionAlert = false;
};

class PetUI {
public:
  explicit PetUI(U8G2& d);

  void render(const DateTime* dtNullable);

  // usados por main.cpp
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

  void drawEyes(int w, int h);
  void drawGlitchOverlay(int w, int h);
};
