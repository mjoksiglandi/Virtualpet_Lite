#include "PetUI.h"

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
  // micro movimiento de pupilas (friendly)
  if (random(0, 100) < 2) {
    _s.lookX = random(-3, 4);
    _s.lookY = random(-2, 3);
  }
}

void PetUI::render(const DateTime*) {
  _d.clearBuffer();
  drawEyes(_d.getDisplayWidth(), _d.getDisplayHeight());
  if (_s.mood == Mood::Glitch) {
    drawGlitchOverlay(_d.getDisplayWidth(), _d.getDisplayHeight());
  }
  _d.sendBuffer();
}

void PetUI::drawEyes(int w, int h) {
  // === DIMENSIONES (ajustadas a tu referencia) ===
  const int eyeW = 36;
  const int eyeH = 24;
  const int gap  = 10;

  int cx = w / 2;
  int cy = h / 2;

  int lx = cx - eyeW - gap / 2 + _s.eyeShiftX;
  int rx = cx + gap / 2 + _s.eyeShiftX;
  int y  = cy - eyeH / 2 + _s.eyeShiftY;

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
  int px = constrain(_s.lookX, -10, 10);
  int py = constrain(_s.lookY, -8, 8);

  const int pupilR = 7;

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

  _d.setDrawColor(1);
  _d.drawLine(lx + 6, browY + tilt, lx + eyeW - 6, browY);
  _d.drawLine(rx + 6, browY, rx + eyeW - 6, browY + tilt);
}

void PetUI::drawGlitchOverlay(int w, int h) {
  _d.setDrawColor(1);
  for (int y = 0; y < h; y += 12) {
    int len = random(20, w - 10);
    int x = random(0, w - len);
    _d.drawHLine(x, y, len);
  }
}
