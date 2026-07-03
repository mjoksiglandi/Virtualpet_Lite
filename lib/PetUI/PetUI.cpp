#include "PetUI.h"
#include <math.h>
#include <stdio.h>

namespace {
static const char* monthShort(uint8_t month) {
  static const char* MONTHS[] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
  };
  if (month < 1 || month > 12) return "???";
  return MONTHS[month - 1];
}

static const char* menuFieldLabel(uint8_t fieldIndex) {
  static const char* LABELS[] = {"DAY", "MONTH", "YEAR", "HOUR", "MIN"};
  return LABELS[fieldIndex < 5 ? fieldIndex : 0];
}

static void drawBadge(U8G2& d, int x, int y, const char* text, bool inverted = false) {
  d.setFont(u8g2_font_5x8_tf);
  const int padX = 3;
  const int textW = d.getStrWidth(text);
  const int boxW = textW + padX * 2;
  const int boxH = 10;
  if (inverted) {
    d.drawBox(x, y - 8, boxW, boxH);
    d.setDrawColor(0);
    d.drawStr(x + padX, y, text);
    d.setDrawColor(1);
  } else {
    d.drawRFrame(x, y - 8, boxW, boxH, 2);
    d.drawStr(x + padX, y, text);
  }
}

static void drawBatteryIcon(U8G2& d, int x, int y, uint8_t percent, bool usb) {
  const int bodyW = 16;
  const int bodyH = 8;
  d.drawFrame(x, y, bodyW, bodyH);
  d.drawBox(x + bodyW, y + 2, 2, 4);
  const int fillW = map(constrain((int)percent, 0, 100), 0, 100, 0, bodyW - 4);
  if (fillW > 0) d.drawBox(x + 2, y + 2, fillW, bodyH - 4);
  if (usb) {
    d.drawPixel(x - 2, y + 3);
    d.drawPixel(x - 1, y + 4);
    d.drawPixel(x - 2, y + 5);
  }
}

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
    uint32_t baseDelay = 2600;
    uint32_t jitter = 2600;
    if (_s.mood == Mood::Sleepy) {
      baseDelay = 1400;
      jitter = 1800;
    } else if (_s.mood == Mood::Relax) {
      baseDelay = 2200;
      jitter = 2200;
    } else if (_s.mood == Mood::Angry) {
      baseDelay = 3400;
      jitter = 3200;
    } else if (_s.mood == Mood::Confused) {
      baseDelay = 2100;
      jitter = 2000;
    } else if (_s.mood == Mood::Dazed) {
      baseDelay = 1800;
      jitter = 1700;
    } else if (_s.mood == Mood::Glitch) {
      baseDelay = 900;
      jitter = 900;
    }
    _nextBlinkAt = now + baseDelay + random(0, (long)jitter);
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
      _blinkLeft = 100;
      _blinkRight = 100;
      if (_doubleBlinkQueued) {
        _doubleBlinkQueued = false;
        _nextBlinkAt = now + 90 + random(0, 80);
      }
    } else {
      float x = t / 180.0f;
      float y = (x < 0.5f) ? x * 2.0f : (1.0f - x) * 2.0f;
      _s.blink = uint8_t(y * 100.0f);
      const int asym =
        (_s.mood == Mood::Glitch) ? 0 :
        (_s.mood == Mood::Confused || _s.mood == Mood::Dazed) ? 10 :
        (_s.mood == Mood::Sleepy ? 7 : 4);
      _blinkLeft = (uint8_t)constrain((int)_s.blink + asym, 0, 100);
      _blinkRight = (uint8_t)constrain((int)_s.blink - asym, 0, 100);
    }
  } else if (
    !_doubleBlinkQueued &&
    (_s.mood == Mood::Sleepy || _s.mood == Mood::Relax || _s.mood == Mood::Glitch) &&
    random(0, 1000) < (_s.mood == Mood::Glitch ? 10 : 2)
  ) {
    _doubleBlinkQueued = true;
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
    _microBiasLX = 0;
    _microBiasRX = 0;
    _microBiasLY = 0;
    _microBiasRY = 0;
    _microLeftSquint = 0;
    _microRightSquint = 0;
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
    glanceRangeX = 6;
    glanceRangeY = 3;
    bobAmp = 0;
  }

  if (_idleRetargetAt == 0 || now >= _idleRetargetAt) {
    _idleRetargetAt = now + 900 + random(0, 1800);
    _idleTargetLookX = random(-glanceRangeX, glanceRangeX + 1);
    _idleTargetLookY = random(-glanceRangeY, glanceRangeY + 1);

    if (_s.mood == Mood::Sleepy && random(0, 100) < 50) {
      _microAnimUntil = now + 800 + random(0, 500);
    } else if (_s.mood == Mood::Relax && random(0, 100) < 38) {
      _microAnimUntil = now + 620 + random(0, 340);
    } else if (_s.mood == Mood::Neutral && random(0, 100) < 22) {
      _microAnimUntil = now + 180 + random(0, 160);
    } else if (_s.mood == Mood::Angry && random(0, 100) < 16) {
      _microAnimUntil = now + 180 + random(0, 100);
    } else if (_s.mood == Mood::Confused && random(0, 100) < 34) {
      _microAnimUntil = now + 420 + random(0, 260);
    } else if (_s.mood == Mood::Dazed && random(0, 100) < 30) {
      _microAnimUntil = now + 520 + random(0, 240);
    } else if (_s.mood == Mood::Glitch && random(0, 100) < 55) {
      _microAnimUntil = now + 120 + random(0, 110);
    } else {
      _microAnimUntil = 0;
    }
  }

  _s.lookX = (int8_t)easeInt(_s.lookX, _idleTargetLookX, 1);
  _s.lookY = (int8_t)easeInt(_s.lookY, _idleTargetLookY, 1);

  const float bobPhase = (now % 2600u) / 2600.0f;
  _s.idleBobY = (int8_t)roundf(sinf(bobPhase * 6.2831853f) * bobAmp);

  _microBiasLX = 0;
  _microBiasRX = 0;
  _microBiasLY = 0;
  _microBiasRY = 0;
  _microLeftSquint = 0;
  _microRightSquint = 0;

  if (_microAnimUntil > now) {
    const float microPhase = 1.0f - ((float)(_microAnimUntil - now) / 900.0f);
    const float pulse = sinf(microPhase * 6.2831853f);
    if (_s.mood == Mood::Sleepy) {
      _microSquint = (uint8_t)(8 + max(0, (int)roundf((pulse + 1.0f) * 5.0f)));
      _s.lookY = min<int>(_s.lookY + 1, glanceRangeY);
      _microLeftSquint = 2;
      _microRightSquint = 4;
      _microBiasLY = 1;
      _microBiasRY = 1;
    } else if (_s.mood == Mood::Neutral) {
      _microSquint = (uint8_t)max(0, (int)roundf((pulse + 1.0f) * 1.0f));
      const int dartDir = (pulse > 0.22f) ? 1 : ((pulse < -0.22f) ? -1 : 0);
      _microBiasLX = dartDir;
      _microBiasRX = dartDir;
      _microLeftSquint = (pulse > 0.75f) ? 1 : 0;
    } else if (_s.mood == Mood::Confused) {
      _microSquint = (uint8_t)(2 + max(0, (int)roundf((pulse + 1.0f) * 2.0f)));
      _s.lookX = (_s.lookX >= 0) ? -2 : 2;
      _microBiasLX = -2;
      _microBiasRX = 2;
      _microBiasLY = -1;
      _microBiasRY = 1;
      _microLeftSquint = 1;
      _microRightSquint = 3;
    } else if (_s.mood == Mood::Dazed) {
      _microSquint = (uint8_t)(5 + max(0, (int)roundf((pulse + 1.0f) * 2.0f)));
      _microBiasLX = (int8_t)roundf(pulse * 2.0f);
      _microBiasRX = (int8_t)roundf(-pulse * 2.0f);
      _microBiasLY = 1;
      _microBiasRY = -1;
    } else if (_s.mood == Mood::Relax) {
      _microSquint = (uint8_t)(3 + max(0, (int)roundf((pulse + 1.0f) * 2.0f)));
      _microBiasLX = (pulse > 0.18f) ? 1 : 0;
      _microBiasRX = (pulse < -0.18f) ? -1 : 0;
      _microLeftSquint = 1;
      _microRightSquint = 1;
      _microBiasLY = (pulse > 0.0f) ? 1 : 0;
    } else if (_s.mood == Mood::Angry) {
      _microSquint = (uint8_t)(1 + max(0, (int)roundf((pulse + 1.0f) * 1.5f)));
      _microBiasLX = 1;
      _microBiasRX = -1;
      _microLeftSquint = 2;
      _microRightSquint = 0;
      _microBiasLY = -1;
      _microBiasRY = -1;
    } else if (_s.mood == Mood::Glitch) {
      _microSquint = (uint8_t)(2 + max(0, (int)roundf((pulse + 1.0f) * 2.0f)));
      const int dartX = (pulse > 0.45f) ? 3 : ((pulse < -0.45f) ? -3 : 0);
      const int dartY = (pulse > 0.10f && pulse < 0.75f) ? -1 : ((pulse < -0.10f && pulse > -0.75f) ? 1 : 0);
      _microBiasLX = dartX;
      _microBiasRX = dartX;
      _microBiasLY = dartY;
      _microBiasRY = dartY;
      _microLeftSquint = (pulse > 0.7f || pulse < -0.7f) ? 2 : 0;
      _microRightSquint = _microLeftSquint;
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
  if (_s.overlay == OverlayFx::Glitch || _s.mood == Mood::Glitch) {
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

  const int baseSquint = constrain((int)_s.squint + (int)_microSquint, 0, 14);
  int openHL = max(8, eyeH - baseSquint - (int)_microLeftSquint);
  int openHR = max(8, eyeH - baseSquint - (int)_microRightSquint);
  if (_s.blink > 0) {
    float fL = 1.0f - (_blinkLeft / 100.0f);
    float fR = 1.0f - (_blinkRight / 100.0f);
    if (fL < 0.12f) fL = 0.12f;
    if (fR < 0.12f) fR = 0.12f;
    openHL = max(4, (int)roundf(openHL * fL));
    openHR = max(4, (int)roundf(openHR * fR));
  }

  int yyL = y + (eyeH - openHL) / 2;
  int yyR = y + (eyeH - openHR) / 2;

  drawEyeShell(_d, lx, yyL, eyeW, openHL, _s.mood, true);
  drawEyeShell(_d, rx, yyR, eyeW, openHR, _s.mood, false);

  const int pupilR = (_s.mood == Mood::Dazed) ? max(3, 5 - baseSquint / 6) : max(4, 7 - baseSquint / 5);
  const int pupilInset = -2;
  const int maxPupilXL = max(0, eyeW / 2 - pupilR - pupilInset);
  const int maxPupilYL = max(0, openHL / 2 - pupilR - pupilInset);
  const int maxPupilXR = max(0, eyeW / 2 - pupilR - pupilInset);
  const int maxPupilYR = max(0, openHR / 2 - pupilR - pupilInset);
  int pxL = constrain((int)_s.lookX + (int)_microBiasLX, -maxPupilXL, maxPupilXL);
  int pyL = constrain((int)_s.lookY + (int)_microBiasLY, -maxPupilYL, maxPupilYL);
  int pxR = constrain((int)_s.lookX + (int)_microBiasRX, -maxPupilXR, maxPupilXR);
  int pyR = constrain((int)_s.lookY + (int)_microBiasRY, -maxPupilYR, maxPupilYR);

  if (_s.mood == Mood::Confused) {
    pxL = constrain(pxL - 2, -maxPupilXL, maxPupilXL);
    pxR = constrain(pxR + 2, -maxPupilXR, maxPupilXR);
    pyL = constrain(pyL - 1, -maxPupilYL, maxPupilYL);
    pyR = constrain(pyR + 1, -maxPupilYR, maxPupilYR);
  } else if (_s.mood == Mood::Dazed) {
    pxL = constrain(pxL - 1, -maxPupilXL, maxPupilXL);
    pxR = constrain(pxR + 1, -maxPupilXR, maxPupilXR);
  } else if (_s.mood == Mood::Glitch) {
    pxL = pxR;
    pyL = pyR;
  }

  _d.setDrawColor(0);
  if (_s.mood == Mood::Dazed) {
    drawSwirlPupil(_d, lx + eyeW / 2 + pxL, yyL + openHL / 2 + pyL, pupilR, false);
    drawSwirlPupil(_d, rx + eyeW / 2 + pxR, yyR + openHR / 2 + pyR, pupilR, true);
  } else {
    _d.drawDisc(lx + eyeW / 2 + pxL, yyL + openHL / 2 + pyL, pupilR);
    _d.drawDisc(rx + eyeW / 2 + pxR, yyR + openHR / 2 + pyR, pupilR);
  }

  _d.setDrawColor(1);
  if (_s.mood != Mood::Dazed) {
    _d.drawDisc(lx + eyeW / 2 + pxL - 2, yyL + openHL / 2 + pyL - 2, 2);
    _d.drawDisc(rx + eyeW / 2 + pxR - 2, yyR + openHR / 2 + pyR - 2, 2);
  }

}

void PetUI::drawGlitchOverlay(int w, int h) {
  _d.setDrawColor(1);
  for (int y = 10; y < h - 8; y += 20) {
    int len = random(8, w / 3);
    int x = random(6, w - len - 6);
    _d.drawHLine(x, y, len);
  }
}

void PetUI::drawAccentOverlay(int w, int) {
  const int strength = constrain((int)_s.overlayStrength, 0, 100);
  if (_s.overlay == OverlayFx::None || strength == 0) return;

  const int cx = w / 2;
  const int topY = 16 + _s.eyeShiftY + _s.idleBobY;
  const int leftEyeX = cx - 22 + _s.eyeShiftX + _s.bodyLeanX;
  const int rightEyeX = cx + 22 + _s.eyeShiftX + _s.bodyLeanX;

  _d.setDrawColor(1);
  if (_s.overlay == OverlayFx::Calm) {
    if (strength > 35) {
      _d.drawLine(leftEyeX - 8, topY + 2, leftEyeX - 2, topY - 1);
      _d.drawLine(rightEyeX + 2, topY - 1, rightEyeX + 8, topY + 2);
    }
  } else if (_s.overlay == OverlayFx::Doze) {
    if (strength > 60) {
      _d.setFont(u8g2_font_4x6_tf);
      _d.drawStr(cx + 26, 13, "z");
      if (strength > 80) _d.drawStr(cx + 32, 9, "z");
    }
  } else if (_s.overlay == OverlayFx::Focus) {
    if (strength > 45) {
      _d.drawLine(leftEyeX - 9, topY + 7, leftEyeX - 3, topY + 5);
      _d.drawLine(rightEyeX + 3, topY + 5, rightEyeX + 9, topY + 7);
      _d.drawHLine(cx - 4, topY - 3, 8);
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
  char dateParts[3][8];
  char timeParts[2][8];
  snprintf(dateParts[0], sizeof(dateParts[0]), "%02u", menu.value.day);
  snprintf(dateParts[1], sizeof(dateParts[1]), "%02u", menu.value.month);
  snprintf(dateParts[2], sizeof(dateParts[2]), "%04u", menu.value.year);
  snprintf(timeParts[0], sizeof(timeParts[0]), "%02u", menu.value.hour);
  snprintf(timeParts[1], sizeof(timeParts[1]), "%02u", menu.value.minute);

  _d.setDrawColor(1);
  _d.setFont(u8g2_font_6x10_tf);
  _d.drawStr(2, 9, "SET CLOCK");
  drawBadge(_d, 78, 9, menu.dirty ? "EDITING" : "READY", menu.dirty);

  _d.setFont(u8g2_font_5x8_tf);
  const char* sourceLabel = menu.rtcValid ? "RTC OK" : (menu.usingBackupSeed ? "BACKUP" : "RTC EMPTY");
  const int sourceW = _d.getStrWidth(sourceLabel);
  _d.drawStr(w - sourceW - 2, 20, sourceLabel);

  const char* fieldLabel = menuFieldLabel(menu.fieldIndex);
  const int fieldW = _d.getStrWidth(fieldLabel);
  _d.drawStr((w - fieldW) / 2, 20, fieldLabel);

  const int dateY = 36;
  const int timeY = 52;
  const int partW = 18;
  const int yearW = 30;
  const int partH = 14;
  const int dateStartX = 14;
  const int timeStartX = 37;

  struct SegmentDef {
    int x;
    int y;
    int w;
    const char* text;
  };
  const SegmentDef segments[] = {
    {dateStartX, dateY, partW, dateParts[0]},
    {dateStartX + 24, dateY, partW, dateParts[1]},
    {dateStartX + 48, dateY, yearW, dateParts[2]},
    {timeStartX, timeY, partW, timeParts[0]},
    {timeStartX + 30, timeY, partW, timeParts[1]}
  };

  _d.setFont(u8g2_font_7x13B_tf);
  for (uint8_t i = 0; i < 5; ++i) {
    const SegmentDef& seg = segments[i];
    const bool active = (menu.fieldIndex == i);
    if (active) {
      _d.drawRBox(seg.x - 3, seg.y - 11, seg.w + 6, partH, 3);
      _d.setDrawColor(0);
    }
    const int textW = _d.getStrWidth(seg.text);
    _d.drawStr(seg.x + (seg.w - textW) / 2, seg.y, seg.text);
    if (active) _d.setDrawColor(1);
  }
  _d.drawStr(dateStartX + 19, dateY, "-");
  _d.drawStr(dateStartX + 43, dateY, "-");
  _d.drawStr(timeStartX + 22, timeY, ":");

  _d.setFont(u8g2_font_5x8_tf);
  _d.drawStr(2, h - 1, "Tilt +/-   Click next   Hold save");
}

void PetUI::drawClockFace(int w, int h, const ClockFaceView& clock) {
  char timeLine[8];
  char dateLine[20];
  snprintf(timeLine, sizeof(timeLine), "%02u:%02u", clock.value.hour, clock.value.minute);
  snprintf(dateLine, sizeof(dateLine), "%02u %s %04u", clock.value.day, monthShort(clock.value.month), clock.value.year);

  _d.setDrawColor(1);
  const char* statusLabel = clock.rtcValid ? "RTC OK" : (clock.usingBackupValue ? "BACKUP" : "RTC EMPTY");
  drawBadge(_d, 2, 9, statusLabel, false);
  if (!clock.rtcValid) {
    _d.drawTriangle(58, 10, 62, 4, 66, 10);
    _d.drawPixel(62, 8);
  }

  if (clock.batteryVisible) {
    drawBatteryIcon(_d, w - 20, 2, clock.batteryPercent, clock.batteryUsb);
  }

  _d.setFont(clock.rtcValid ? u8g2_font_logisoso20_tf : u8g2_font_logisoso18_tf);
  const int timeW = _d.getStrWidth(timeLine);
  _d.drawStr((w - timeW) / 2, 36, timeLine);

  _d.setFont(u8g2_font_7x13B_tf);
  const int dateW = _d.getStrWidth(dateLine);
  _d.drawStr((w - dateW) / 2, 54, dateLine);

  if (!clock.rtcValid) {
    _d.setFont(u8g2_font_5x8_tf);
    const char* hint = clock.usingBackupValue ? "stored fallback time" : "set clock to confirm";
    const int hintW = _d.getStrWidth(hint);
    _d.drawStr((w - hintW) / 2, 63, hint);
  }
}
