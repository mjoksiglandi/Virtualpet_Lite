#include "ImuMotion.h"

#include <math.h>
#include "ClockLogic.h"

bool ImuMotion::update(
  uint32_t now,
  const ImuSample& sample,
  bool clockMenuActive,
  ClockMenuState& clockMenu,
  VisualState& visual,
  GestureOverride& gestureOverride
) {
  visual.imuActive = true;

  _rollF = _rollF * 0.70f + sample.angles.roll * 0.30f;
  _pitchF = _pitchF * 0.70f + sample.angles.pitch * 0.30f;

  const float rollNorm = constrain(_rollF / 34.0f, -1.0f, 1.0f);
  const float pitchNorm = constrain(-_pitchF / 24.0f, -1.0f, 1.0f);

  if (clockMenuActive) {
    if (fabsf(rollNorm) >= _menuTiltThreshold && (now - clockMenu.lastTiltStepMs) >= _menuTiltRepeatMs) {
      ClockLogic::applyMenuDelta(clockMenu.draft, clockMenu.fieldIndex, rollNorm > 0.0f ? 1 : -1);
      clockMenu.dirty = true;
      clockMenu.lastTiltStepMs = now;
      clockMenu.lastInteractionMs = now;
    } else if (fabsf(rollNorm) < (_menuTiltThreshold * 0.55f)) {
      clockMenu.lastTiltStepMs = 0;
    }
  }

  visual.lookX = (int8_t)roundf(rollNorm * 15.0f);
  visual.lookY = (int8_t)roundf(pitchNorm * 12.0f);
  visual.eyeShiftX = (int8_t)roundf(rollNorm * 20.0f);
  visual.eyeShiftY = (int8_t)roundf(pitchNorm * 15.0f);

  const int targetLean = (int)roundf(rollNorm * 3.0f);
  int curLean = visual.bodyLeanX;
  if (curLean < targetLean) curLean++;
  else if (curLean > targetLean) curLean--;
  visual.bodyLeanX = (int8_t)curLean;

  const float motionDelta = fabsf(_rollF - _lastRollF) + fabsf(_pitchF - _lastPitchF);
  if (motionDelta > 5.5f) _motionAlertUntil = now + 220;

  const float accelMag = sqrtf(
    sample.ax * sample.ax +
    sample.ay * sample.ay +
    sample.az * sample.az
  );
  const float accelDynamic = fabsf(accelMag - 1.0f);
  const float gyroShake = fabsf(sample.gx) + fabsf(sample.gy) + fabsf(sample.gz);
  const bool heavyMotionNow = (accelDynamic > 0.24f && gyroShake > 105.0f) || gyroShake > 190.0f;

  if (heavyMotionNow) {
    if (_heavyMotionSince == 0) _heavyMotionSince = now;
  } else {
    _heavyMotionSince = 0;
  }

  if (accelDynamic < 0.18f && gyroShake < 90.0f) _shakeArmed = true;

  const bool dazedShake = (accelDynamic > 0.22f && gyroShake > 95.0f) || gyroShake > 155.0f;
  const bool glitchShake = (accelDynamic > 0.48f && gyroShake > 230.0f) || gyroShake > 380.0f;
  const bool sustainedDazed = _heavyMotionSince != 0 && (now - _heavyMotionSince) >= 700;

  if ((dazedShake || sustainedDazed) && now >= _shakeCooldownUntil) {
    _shakeArmed = false;
    _shakeCooldownUntil = now + (sustainedDazed ? 2200 : 1600);
    gestureOverride.active = true;
    if (glitchShake) {
      gestureOverride.mood = Mood::Glitch;
      gestureOverride.brow = 95;
      gestureOverride.squint = 6;
      gestureOverride.overlay = OverlayFx::Glitch;
      gestureOverride.overlayStrength = 100;
      gestureOverride.untilMs = now + 1200;
      _motionAlertUntil = now + 950;
    } else {
      gestureOverride.mood = Mood::Dazed;
      gestureOverride.brow = 38;
      gestureOverride.squint = 8;
      gestureOverride.overlay = OverlayFx::Dizzy;
      gestureOverride.overlayStrength = 100;
      gestureOverride.untilMs = now + (sustainedDazed ? 4800 : 3600);
      _motionAlertUntil = now + 2400;
    }
    _heavyMotionSince = 0;
  }

  const bool pitchHeld = !clockMenuActive && fabsf(pitchNorm) > 0.72f;
  const int8_t pitchDir = (pitchNorm > 0.0f) ? 1 : -1;
  if (pitchHeld) {
    if (_pitchHoldSince == 0 || _pitchHoldDir != pitchDir) {
      _pitchHoldSince = now;
      _pitchHoldDir = pitchDir;
    } else if (now >= _tiltGestureCooldownUntil && (now - _pitchHoldSince) > 850) {
      _tiltGestureCooldownUntil = now + 2200;
      _pitchHoldSince = now;
      gestureOverride.active = true;

      if (pitchDir > 0) {
        gestureOverride.mood = Mood::Sleepy;
        gestureOverride.brow = 6;
        gestureOverride.squint = 9;
        gestureOverride.overlay = OverlayFx::Doze;
        gestureOverride.overlayStrength = 78;
        gestureOverride.untilMs = now + 1900;
      } else {
        gestureOverride.mood = Mood::Angry;
        gestureOverride.brow = 92;
        gestureOverride.squint = 4;
        gestureOverride.overlay = OverlayFx::Focus;
        gestureOverride.overlayStrength = 84;
        gestureOverride.untilMs = now + 1900;
      }
    }
  } else {
    _pitchHoldSince = 0;
    _pitchHoldDir = 0;
  }

  visual.motionAlert = _motionAlertUntil > now;
  _lastRollF = _rollF;
  _lastPitchF = _pitchF;
  return visual.motionAlert;
}

void ImuMotion::updateNoSample(VisualState& visual) const {
  visual.imuActive = false;
  visual.lookX = easeToZero(visual.lookX);
  visual.lookY = easeToZero(visual.lookY);
  visual.eyeShiftX = easeToZero(visual.eyeShiftX);
  visual.eyeShiftY = easeToZero(visual.eyeShiftY);
  visual.bodyLeanX = easeToZero(visual.bodyLeanX);
}

int8_t ImuMotion::easeToZero(int8_t value) {
  if (value > 0) return value - 1;
  if (value < 0) return value + 1;
  return 0;
}
