#include "PetUI.h"
#include <math.h>
#include <stdio.h>

namespace {
static int easeInt(int current, int target, int step) {
  if (current < target) return min(current + step, target);
  if (current > target) return max(current - step, target);
  return current;
}

static int shapeTopCut(Mood mood, bool leftEye, bool innerSide) {
  if (mood == Mood::Angry) {
    if (innerSide) return leftEye ? 6 : 2;
    return leftEye ? 2 : 6;
  }
  if (mood == Mood::Sleepy) return innerSide ? 5 : 4;
  if (mood == Mood::Relax) return innerSide ? 2 : 1;
  if (mood == Mood::Confused) {
    if (leftEye) return innerSide ? 6 : 1;
    return innerSide ? 1 : 6;
  }
  if (mood == Mood::Dazed) return innerSide ? 3 : 7;
  if (mood == Mood::Glitch) return innerSide ? 7 : 1;
  return 0;
}

static int shapeBottomCut(Mood mood, bool leftEye, bool innerSide) {
  if (mood == Mood::Sleepy) return innerSide ? 1 : 2;
  if (mood == Mood::Relax) return 0;
  if (mood == Mood::Confused) {
    if (leftEye) return innerSide ? 0 : 3;
    return innerSide ? 3 : 0;
  }
  if (mood == Mood::Dazed) return innerSide ? 3 : 2;
  if (mood == Mood::Glitch) return innerSide ? 3 : 1;
  return 0;
}

static void carveTop(U8G2& d, int x, int y, int w, int leftCut, int rightCut) {
  const int maxCut = max(leftCut, rightCut);
  for (int i = 0; i < maxCut; ++i) {
    const int insetL = map(i, 0, maxCut, leftCut, 0);
    const int insetR = map(i, 0, maxCut, rightCut, 0);
    const int lineW = w - insetL - insetR;
    if (lineW > 0) d.drawHLine(x + insetL, y + i, lineW);
  }
}

static void carveBottom(U8G2& d, int x, int y, int w, int h, int leftCut, int rightCut) {
  const int maxCut = max(leftCut, rightCut);
  for (int i = 0; i < maxCut; ++i) {
    const int insetL = map(i, 0, maxCut, leftCut, 0);
    const int insetR = map(i, 0, maxCut, rightCut, 0);
    const int lineW = w - insetL - insetR;
    if (lineW > 0) d.drawHLine(x + insetL, y + h - 1 - i, lineW);
  }
}

static void drawEyeShell(U8G2& d, int x, int y, int w, int h, Mood mood, bool leftEye) {
  d.setDrawColor(1);
  d.drawRBox(x, y, w, h, 6);
  d.setDrawColor(0);
  carveTop(d, x, y, w, shapeTopCut(mood, leftEye, false), shapeTopCut(mood, leftEye, true));
  carveBottom(d, x, y, w, h, shapeBottomCut(mood, leftEye, false), shapeBottomCut(mood, leftEye, true));
  d.setDrawColor(1);
}

static void drawSwirlPupil(U8G2& d, int cx, int cy, int radius, bool mirrored) {
  const int r1 = max(1, radius - 1);
  const int r2 = max(1, radius - 3);
  d.drawCircle(cx, cy, radius);
  if (mirrored) {
    d.drawLine(cx + r1, cy - 1, cx + 1, cy - r1);
    d.drawLine(cx + 1, cy - r1, cx - r2, cy - 1);
    d.drawLine(cx - r2, cy - 1, cx - 1, cy + r2);
  } else {
    d.drawLine(cx - r1, cy - 1, cx - 1, cy - r1);
    d.drawLine(cx - 1, cy - r1, cx + r2, cy - 1);
    d.drawLine(cx + r2, cy - 1, cx + 1, cy + r2);
  }
  d.drawPixel(cx, cy);
}
}

PetUI::PetUI(U8G2& d) : _d(d) {}

void PetUI::tickBlink() {
  uint32_t now = millis();

  if (!_blinking && _nextBlinkAt == 0) {
    _nextBlinkAt = now + 2000 + random(0, 3000);
  }

  if (!_blinking && now >= _nextBlinkAt) {
    _blinking = true;
    _blinkStart = now;
    _nextBlinkAt = 0;
  }

  if (_blinking) {
    uint32_t t = now - _blinkStart;
    if (t > 180) {
      _blinking = false;
      _s.blink = 0;
    } else {
      float x = t / 180.0f;
      float y = (x < 0.5f) ? x * 2.0f : (1.0f - x) * 2.0f;
      _s.blink = uint8_t(y * 100.0f);
    }
  }
}

void PetUI::tickMoodAuto() {
  uint32_t now = millis();

  if (_s.imuActive) {
    _idleRetargetAt = now + 800;
    _idleTargetLookX = 0;
    _idleTargetLookY = 0;
    _microAnimUntil = 0;
    _microSquint = 0;
    _s.idleBobY = 0;
    return;
  }

  int glanceRangeX = 3;
  int glanceRangeY = 2;
  int bobAmp = 1;
  if (_s.mood == Mood::Sleepy) {
    glanceRangeX = 2;
    glanceRangeY = 1;
    bobAmp = 2;
  } else if (_s.mood == Mood::Relax) {
    glanceRangeX = 4;
    glanceRangeY = 2;
    bobAmp = 2;
  } else if (_s.mood == Mood::Angry) {
    glanceRangeX = 2;
    glanceRangeY = 1;
    bobAmp = 0;
  } else if (_s.mood == Mood::Confused) {
    glanceRangeX = 5;
    glanceRangeY = 3;
    bobAmp = 1;
  } else if (_s.mood == Mood::Dazed) {
    glanceRangeX = 3;
    glanceRangeY = 2;
    bobAmp = 1;
  } else if (_s.mood == Mood::Glitch) {
    glanceRangeX = 5;
    glanceRangeY = 3;
    bobAmp = 1;
  }

  if (_idleRetargetAt == 0 || now >= _idleRetargetAt) {
    _idleRetargetAt = now + 900 + random(0, 1800);
    _idleTargetLookX = random(-glanceRangeX, glanceRangeX + 1);
    _idleTargetLookY = random(-glanceRangeY, glanceRangeY + 1);

    if (_s.mood == Mood::Sleepy && random(0, 100) < 45) {
      _microAnimUntil = now + 800 + random(0, 500);
    } else if (_s.mood == Mood::Relax && random(0, 100) < 35) {
      _microAnimUntil = now + 500 + random(0, 300);
    } else if (_s.mood == Mood::Neutral && random(0, 100) < 18) {
      _microAnimUntil = now + 240 + random(0, 220);
    } else {
      _microAnimUntil = 0;
    }
  }

  _s.lookX = (int8_t)easeInt(_s.lookX, _idleTargetLookX, 1);
  _s.lookY = (int8_t)easeInt(_s.lookY, _idleTargetLookY, 1);

  const float bobPhase = (now % 2600u) / 2600.0f;
  _s.idleBobY = (int8_t)roundf(sinf(bobPhase * 6.2831853f) * bobAmp);

  if (_microAnimUntil > now) {
    const float microPhase = 1.0f - ((float)(_microAnimUntil - now) / 900.0f);
    const float pulse = sinf(microPhase * 6.2831853f);
    if (_s.mood == Mood::Sleepy) {
      _microSquint = (uint8_t)(8 + max(0, (int)roundf((pulse + 1.0f) * 5.0f)));
      _s.lookY = min<int>(_s.lookY + 1, glanceRangeY);
    } else if (_s.mood == Mood::Confused) {
      _microSquint = (uint8_t)(2 + max(0, (int)roundf((pulse + 1.0f) * 2.0f)));
      _s.lookX = (_s.lookX >= 0) ? -2 : 2;
    } else if (_s.mood == Mood::Dazed) {
      _microSquint = (uint8_t)(5 + max(0, (int)roundf((pulse + 1.0f) * 2.0f)));
    } else if (_s.mood == Mood::Relax) {
      _microSquint = (uint8_t)(4 + max(0, (int)roundf((pulse + 1.0f) * 3.0f)));
    } else {
      _microSquint = (uint8_t)max(0, (int)roundf((pulse + 1.0f) * 2.5f));
    }
  } else {
    _microSquint = 0;
  }
}

void PetUI::render(const DateTime*, const ClockMenuView* menuNullable, const ClockFaceView* clockNullable) {
  _d.clearBuffer();
  if (menuNullable && menuNullable->active) {
    drawClockMenu(_d.getDisplayWidth(), _d.getDisplayHeight(), *menuNullable);
    _d.sendBuffer();
    return;
  }
  if (clockNullable && clockNullable->active) {
    drawClockFace(_d.getDisplayWidth(), _d.getDisplayHeight(), *clockNullable);
    _d.sendBuffer();
    return;
  }

  if (_s.mood != _lastMood) {
    _fromMood = _lastMood;
    _lastMood = _s.mood;
    _moodTransitionStart = millis();
  }

  drawEyes(_d.getDisplayWidth(), _d.getDisplayHeight());
  drawAccentOverlay(_d.getDisplayWidth(), _d.getDisplayHeight());
  if (_s.motionAlert || _s.overlay == OverlayFx::Glitch || _s.mood == Mood::Glitch) {
    drawGlitchOverlay(_d.getDisplayWidth(), _d.getDisplayHeight());
  }
  _d.sendBuffer();
}

void PetUI::drawEyes(int w, int h) {
  const int eyeW = 38;
  const int eyeH = 24;
  const int gap = 6;

  int cx = w / 2;
  int cy = h / 2;

  const int leanX = constrain((int)_s.bodyLeanX, -8, 8);
  const int leanY = abs(leanX) / 3;

  int lx = cx - eyeW - gap / 2 + _s.eyeShiftX + leanX;
  int rx = cx + gap / 2 + _s.eyeShiftX + leanX;
  int y = cy - eyeH / 2 + _s.eyeShiftY + leanY + _s.idleBobY;

  int openH = eyeH;
  const int squint = constrain((int)_s.squint + (int)_microSquint, 0, 14);
  openH = max(8, openH - squint);
  if (_s.blink > 0) {
    float f = 1.0f - (_s.blink / 100.0f);
    if (f < 0.12f) f = 0.12f;
    openH = max(4, (int)roundf(openH * f));
  }

  int yy = y + (eyeH - openH) / 2;

  drawEyeShell(_d, lx, yy, eyeW, openH, _s.mood, true);
  drawEyeShell(_d, rx, yy, eyeW, openH, _s.mood, false);

  const int pupilR = (_s.mood == Mood::Dazed) ? max(3, 5 - squint / 6) : max(4, 7 - squint / 5);
  const int pupilInset = -2;
  const int maxPupilX = max(0, eyeW / 2 - pupilR - pupilInset);
  const int maxPupilY = max(0, openH / 2 - pupilR - pupilInset);
  int pxL = constrain(_s.lookX, -maxPupilX, maxPupilX);
  int pyL = constrain(_s.lookY, -maxPupilY, maxPupilY);
  int pxR = pxL;
  int pyR = pyL;

  if (_s.mood == Mood::Confused) {
    pxL = constrain(pxL - 2, -maxPupilX, maxPupilX);
    pxR = constrain(pxR + 2, -maxPupilX, maxPupilX);
    pyL = constrain(pyL - 1, -maxPupilY, maxPupilY);
    pyR = constrain(pyR + 1, -maxPupilY, maxPupilY);
  } else if (_s.mood == Mood::Dazed) {
    pxL = constrain(pxL - 1, -maxPupilX, maxPupilX);
    pxR = constrain(pxR + 1, -maxPupilX, maxPupilX);
  }

  _d.setDrawColor(0);
  if (_s.mood == Mood::Dazed) {
    drawSwirlPupil(_d, lx + eyeW / 2 + pxL, yy + openH / 2 + pyL, pupilR, false);
    drawSwirlPupil(_d, rx + eyeW / 2 + pxR, yy + openH / 2 + pyR, pupilR, true);
  } else {
    _d.drawDisc(lx + eyeW / 2 + pxL, yy + openH / 2 + pyL, pupilR);
    _d.drawDisc(rx + eyeW / 2 + pxR, yy + openH / 2 + pyR, pupilR);
  }

  _d.setDrawColor(1);
  if (_s.mood != Mood::Dazed) {
    _d.drawDisc(lx + eyeW / 2 + pxL - 2, yy + openH / 2 + pyL - 2, 2);
    _d.drawDisc(rx + eyeW / 2 + pxR - 2, yy + openH / 2 + pyR - 2, 2);
  }

}

void PetUI::drawGlitchOverlay(int w, int h) {
  _d.setDrawColor(1);
  for (int y = 0; y < h; y += 12) {
    int len = random(20, w - 10);
    int x = random(0, w - len);
    _d.drawHLine(x, y, len);
  }
}

void PetUI::drawAccentOverlay(int w, int) {
  const int strength = constrain((int)_s.overlayStrength, 0, 100);
  if (_s.overlay == OverlayFx::None || strength == 0) return;

  const int cx = w / 2;
  const int leftEyeX = cx - 22 + _s.eyeShiftX + _s.bodyLeanX;
  const int rightEyeX = cx + 22 + _s.eyeShiftX + _s.bodyLeanX;
  const int topY = 16 + _s.eyeShiftY + _s.idleBobY;
  const int bottomY = 46 + _s.eyeShiftY + _s.idleBobY;

  _d.setDrawColor(1);
  if (_s.overlay == OverlayFx::Doze) {
    if (strength > 60) {
      _d.setFont(u8g2_font_4x6_tf);
      _d.drawStr(cx + 26, 13, "z");
      if (strength > 80) _d.drawStr(cx + 32, 9, "z");
    }
  } else if (_s.overlay == OverlayFx::Dizzy) {
    if (strength > 55) {
      _d.setFont(u8g2_font_4x6_tf);
      _d.drawStr(cx + 26, 11, "*");
      _d.drawStr(cx + 31, 16, "*");
    }
  } else if (_s.overlay == OverlayFx::Confused) {
    if (strength > 55) {
      _d.setFont(u8g2_font_4x6_tf);
      _d.drawStr(cx + 27, 12, "?");
    }
  } else if (_s.overlay == OverlayFx::Startle) {
    _d.drawFrame(4, 4, 5, 5);
    _d.drawFrame(w - 9, 4, 5, 5);
  }
}

void PetUI::drawClockMenu(int w, int h, const ClockMenuView& menu) {
  char line1[24];
  char line2[24];
  snprintf(line1, sizeof(line1), "%02u-%02u-%04u", menu.value.day, menu.value.month, menu.value.year);
  snprintf(line2, sizeof(line2), "%02u:%02u", menu.value.hour, menu.value.minute);

  _d.setDrawColor(1);
  _d.setFont(u8g2_font_6x10_tf);
  _d.drawStr(2, 10, "SET CLOCK");
  _d.drawStr(116, 10, menu.dirty ? "*" : " ");

  _d.setFont(u8g2_font_7x13B_tf);
  const int dateX = 14;
  const int dateY = 28;
  const int timeX = 38;
  const int timeY = 46;
  _d.drawStr(dateX, dateY, line1);
  _d.drawStr(timeX, timeY, line2);

  struct BoxDef { int x; int y; int w; int h; };
  const BoxDef boxes[] = {
    { dateX - 2, 14, 20, 17 },
    { dateX + 22, 14, 20, 17 },
    { dateX + 46, 14, 36, 17 },
    { timeX - 2, 32, 20, 17 },
    { timeX + 28, 32, 20, 17 }
  };

  const BoxDef& box = boxes[menu.fieldIndex < 5 ? menu.fieldIndex : 0];
  _d.drawFrame(box.x, box.y, box.w, box.h);
  _d.drawFrame(box.x - 1, box.y - 1, box.w + 2, box.h + 2);

  _d.setFont(u8g2_font_5x8_tf);
  _d.drawStr(2, 57, "Tilt edit  Click next");
  _d.drawStr(2, 64, menu.rtcValid ? "Hold save  RTC OK" : "Hold save  RTC empty");
}

void PetUI::drawClockFace(int w, int h, const ClockFaceView& clock) {
  char timeLine[8];
  char dateLine[20];
  char batteryLine[24];
  snprintf(timeLine, sizeof(timeLine), "%02u:%02u", clock.value.hour, clock.value.minute);
  snprintf(dateLine, sizeof(dateLine), "%02u-%02u-%04u", clock.value.day, clock.value.month, clock.value.year);

  _d.setDrawColor(1);
  _d.setFont(u8g2_font_logisoso20_tf);
  const int timeW = _d.getStrWidth(timeLine);
  _d.drawStr((w - timeW) / 2, 34, timeLine);

  _d.setFont(u8g2_font_7x13B_tf);
  const int dateW = _d.getStrWidth(dateLine);
  _d.drawStr((w - dateW) / 2, clock.batteryVisible ? 50 : 54, dateLine);

  if (clock.batteryVisible) {
    snprintf(
      batteryLine,
      sizeof(batteryLine),
      "BAT %u%% %u.%02uV%s",
      clock.batteryPercent,
      clock.batteryMv / 1000,
      (clock.batteryMv % 1000) / 10,
      clock.batteryUsb ? " USB" : ""
    );
    _d.setFont(u8g2_font_5x8_tf);
    const int batteryW = _d.getStrWidth(batteryLine);
    _d.drawStr((w - batteryW) / 2, 63, batteryLine);
  }
}
