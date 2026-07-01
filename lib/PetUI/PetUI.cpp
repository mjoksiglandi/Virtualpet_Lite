#include "PetUI.h"
#include <stdio.h>

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
      float y = (x < 0.5f) ? x * 2 : (1 - x) * 2;
      _s.blink = uint8_t(y * 100);
    }
  }
}

void PetUI::tickMoodAuto() {
  // El micro movimiento solo se aplica cuando la IMU no esta guiando la mirada.
  if (_s.imuActive) return;

  if (random(0, 100) < 2) {
    _s.lookX = random(-3, 4);
    _s.lookY = random(-2, 3);
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

  drawEyes(_d.getDisplayWidth(), _d.getDisplayHeight());
  if (_s.motionAlert || _s.mood == Mood::Glitch) {
    drawGlitchOverlay(_d.getDisplayWidth(), _d.getDisplayHeight());
  }
  _d.sendBuffer();
}

void PetUI::drawEyes(int w, int h) {
  // === DIMENSIONES (ajustadas a tu referencia) ===
  const int eyeW = 38;
  const int eyeH = 24;
  const int gap  = 6;

  int cx = w / 2;
  int cy = h / 2;

  const int leanX = constrain((int)_s.bodyLeanX, -8, 8);
  const int leanY = abs(leanX) / 3;

  int lx = cx - eyeW - gap / 2 + _s.eyeShiftX + leanX;
  int rx = cx + gap / 2 + _s.eyeShiftX + leanX;
  int y  = cy - eyeH / 2 + _s.eyeShiftY + leanY;

  // Blink reduce altura
  int openH = eyeH;
  if (_s.blink > 0) {
    float f = 1.0f - (_s.blink / 100.0f);
    if (f < 0.15f) f = 0.15f;
    openH = eyeH * f;
  }

  int yy = y + (eyeH - openH) / 2;

  // === OJOS (BLANCO) ===
  _d.setDrawColor(1);
  _d.drawRBox(lx, yy, eyeW, openH, 6);
  _d.drawRBox(rx, yy, eyeW, openH, 6);

  // === PUPILAS (MÁS GRANDES) ===
  const int pupilR = 7;
  const int pupilInset = -2;
  const int maxPupilX = max(0, eyeW / 2 - pupilR - pupilInset);
  const int maxPupilY = max(0, openH / 2 - pupilR - pupilInset);
  int px = constrain(_s.lookX, -maxPupilX, maxPupilX);
  int py = constrain(_s.lookY, -maxPupilY, maxPupilY);

  _d.setDrawColor(0);
  _d.drawDisc(lx + eyeW / 2 + px, yy + openH / 2 + py, pupilR);
  _d.drawDisc(rx + eyeW / 2 + px, yy + openH / 2 + py, pupilR);

  // Highlight
  _d.setDrawColor(1);
  _d.drawDisc(lx + eyeW / 2 + px - 2, yy + openH / 2 + py - 2, 2);
  _d.drawDisc(rx + eyeW / 2 + px - 2, yy + openH / 2 + py - 2, 2);

  // === CEJAS (EXPRESIVAS) ===
  int browY = yy - 6;
  int strength = _s.brow;

  int tilt = 0;
  if (_s.mood == Mood::Angry) tilt = -6;
  if (_s.mood == Mood::Sleepy) tilt = 5;

  int browInset = map(strength, 0, 100, 2, 7);
  int leanBrow = leanX / 2;

  _d.setDrawColor(1);
  _d.drawLine(lx + browInset, browY + tilt + leanBrow, lx + eyeW - browInset, browY - leanBrow / 2);
  _d.drawLine(rx + browInset, browY + leanBrow / 2, rx + eyeW - browInset, browY + tilt - leanBrow);

  if (_s.motionAlert) {
    const int accentY = yy + openH + 4;
    _d.drawLine(lx + 6, accentY, lx + eyeW - 6, accentY + 2);
    _d.drawLine(rx + 6, accentY + 2, rx + eyeW - 6, accentY);
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
    { dateX - 2, 14, 20, 17 }, // day
    { dateX + 22, 14, 20, 17 }, // month
    { dateX + 46, 14, 36, 17 }, // year
    { timeX - 2, 32, 20, 17 }, // hour
    { timeX + 28, 32, 20, 17 } // minute
  };

  const BoxDef& box = boxes[menu.fieldIndex < 5 ? menu.fieldIndex : 0];
  _d.drawFrame(box.x, box.y, box.w, box.h);
  _d.drawFrame(box.x - 1, box.y - 1, box.w + 2, box.h + 2);

  _d.setFont(u8g2_font_5x8_tf);
  _d.drawStr(2, 57, "Tilt edit  Click next");
  _d.drawStr(2, 64, menu.rtcValid ? "Hold save  RTC OK" : "Hold save  RTC empty");
}

void PetUI::drawClockFace(int w, int h, const ClockFaceView& clock) {
  char timeLine[16];
  char dateLine[20];
  char batteryLine[24];
  snprintf(timeLine, sizeof(timeLine), "%02u:%02u:%02u", clock.value.hour, clock.value.minute, clock.value.second);
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
