#include <Arduino.h>
#include <esp_sleep.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <driver/gpio.h>

#include "pinout.h"

#include "PetUI.h"
#include "RtcClock.h"
#include "ImuQmi8658.h"
#include "Melodies.h"

// static constexpr int OLED_I2C_ADDR = 0x3C;

// SH1106 (tu caso, por la línea rara al lado derecho)
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// UI
static PetUI pet(u8g2);

// RTC + IMU
static RtcClock rtc;
static ImuQmi8658 imu;

// ---------------- BUZZER ----------------
static constexpr int BUZZER = PIN_BUZZER;
static constexpr int BUZZ_CH = 0;
static constexpr int BUZZ_RES = 8;
static constexpr uint32_t PWR_LONG_PRESS_MS = 1500;

// ---------------- HORARIO / FASES ----------------
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

static void playPhaseCue(PetPhase ph) {
  if (ph == PetPhase::NightSleep) Melodies::play(Melodies::Tune::PhaseAlert);
  else if (ph == PetPhase::WorkToAngry) Melodies::play(Melodies::Tune::PhaseDouble);
  else Melodies::play(Melodies::Tune::PhaseTick);
}

static void powerHold(bool enable) {
  digitalWrite(PIN_PWR_HOLD, enable ? HIGH : LOW);
}

static void printRtcSnapshot() {
  DateTime dt{};
  bool oscStopped = false;
  bool clockStopped = false;
  const bool readOk = rtc.read(dt);
  const bool statusOk = rtc.readStatus(oscStopped);
  const bool controlOk = rtc.readControl(clockStopped);

  if (!readOk) {
    Serial.println("RTC read failed");
    return;
  }

  Serial.printf(
    "RTC %04u-%02u-%02u %02u:%02u:%02u | valid=%s | osc_stopped=%s\n",
    dt.year,
    dt.month,
    dt.day,
    dt.hour,
    dt.minute,
    dt.second,
    rtc.valid() ? "yes" : "no",
    statusOk ? (oscStopped ? "yes" : "no") : "unknown"
  );
  Serial.printf("RTC ctrl_stop=%s\n", controlOk ? (clockStopped ? "yes" : "no") : "unknown");
}

static bool parseDateTime(const String& value, DateTime& out) {
  int yy, mo, dd, hh, mi, ss;
  if (sscanf(value.c_str(), "%d-%d-%d %d:%d:%d", &yy, &mo, &dd, &hh, &mi, &ss) != 6) {
    return false;
  }

  if (yy < 2020 || yy > 2099) return false;
  if (mo < 1 || mo > 12) return false;
  if (dd < 1 || dd > 31) return false;
  if (hh < 0 || hh > 23) return false;
  if (mi < 0 || mi > 59) return false;
  if (ss < 0 || ss > 59) return false;

  out.year = (uint16_t)yy;
  out.month = (uint8_t)mo;
  out.day = (uint8_t)dd;
  out.hour = (uint8_t)hh;
  out.minute = (uint8_t)mi;
  out.second = (uint8_t)ss;
  return true;
}

static void handleSerialCommand(const String& raw) {
  String cmd = raw;
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd.equalsIgnoreCase("HELP")) {
    Serial.println("Commands:");
    Serial.println("  RTC?");
    Serial.println("  SET_RTC YYYY-MM-DD HH:MM:SS");
    return;
  }

  if (cmd.equalsIgnoreCase("RTC?")) {
    printRtcSnapshot();
    return;
  }

  if (cmd.startsWith("SET_RTC ")) {
    DateTime dt{};
    const String value = cmd.substring(8);
    if (!parseDateTime(value, dt)) {
      Serial.println("Invalid format. Use: SET_RTC YYYY-MM-DD HH:MM:SS");
      return;
    }

    if (!rtc.set(dt)) {
      Serial.println("RTC set failed");
      return;
    }

    Serial.println("RTC updated");
    printRtcSnapshot();
    return;
  }

  Serial.println("Unknown command. Type HELP");
}

static PetPhase currentPhase = PetPhase::Unknown;
static uint32_t lastSchoolChimeKey = 0; // yyyy*512 + mm*32 + dd

static PetPhase phaseForTime(const DateTime& dt) {
  const int m = dt.hour * 60 + dt.minute;

  // 07:00-09:00 somnoliento
  if (m >= 7*60 && m < 9*60) return PetPhase::SleepyAM;

  // 09:00-12:30 activo
  if (m >= 9*60 && m < (12*60 + 30)) return PetPhase::ActiveAM;

  // 12:30-14:00 descanso almuerzo
  if (m >= (12*60 + 30) && m < 14*60) return PetPhase::LunchRest;

  // 14:00-14:30 sueño post almuerzo
  if (m >= 14*60 && m < (14*60 + 30)) return PetPhase::PostLunchNap;

  // 14:30 - 18:00 progresión a cansado/enojado
  if (m >= (14*60 + 30) && m < 18*60) return PetPhase::WorkToAngry;

  // 18:00 - 22:00 relajado
  if (m >= 18*60 && m < 22*60) return PetPhase::RelaxPM;

  // 22:00 - 07:00 dormir
  return PetPhase::NightSleep;
}

static void applyPhaseToPet(PetPhase ph, const DateTime& dt) {
  // base
  pet.state().mood = Mood::Neutral;
  pet.state().brow = 20;

  if (ph == PetPhase::SleepyAM) {
    pet.state().mood = Mood::Sleepy;
    pet.state().brow = 15;
  } else if (ph == PetPhase::ActiveAM) {
    pet.state().mood = Mood::Neutral;
    pet.state().brow = 20;
  } else if (ph == PetPhase::LunchRest) {
    pet.state().mood = Mood::Relax;
    pet.state().brow = 10;
  } else if (ph == PetPhase::PostLunchNap) {
    pet.state().mood = Mood::Sleepy;
    pet.state().brow = 10;
  } else if (ph == PetPhase::WorkToAngry) {
    // sube ceja con el tiempo (más “chato”)
    int m0 = (14*60 + 30);
    int m1 = (18*60);
    int m  = dt.hour*60 + dt.minute;
    int t = constrain(m - m0, 0, (m1 - m0));
    pet.state().mood = (t > (m1-m0)*3/4) ? Mood::Angry : Mood::Neutral;
    pet.state().brow = (uint8_t)constrain(20 + (t * 70) / (m1 - m0), 20, 90);
  } else if (ph == PetPhase::RelaxPM) {
    pet.state().mood = Mood::Relax;
    pet.state().brow = 12;
  } else if (ph == PetPhase::NightSleep) {
    pet.state().mood = Mood::Sleepy;
    pet.state().brow = 8;
  }
}

struct GestureOverride {
  Mood mood = Mood::Neutral;
  uint8_t brow = 20;
  uint32_t untilMs = 0;
};

static int8_t easeToZero(int8_t value) {
  if (value > 0) return value - 1;
  if (value < 0) return value + 1;
  return 0;
}

static bool isLeapYear(uint16_t year) {
  return ((year % 4u) == 0u && (year % 100u) != 0u) || ((year % 400u) == 0u);
}

static uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t DAYS[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month == 2) return isLeapYear(year) ? 29 : 28;
  if (month < 1 || month > 12) return 31;
  return DAYS[month - 1];
}

static void advanceOneDay(DateTime& dt) {
  dt.day++;
  if (dt.day > daysInMonth(dt.year, dt.month)) {
    dt.day = 1;
    dt.month++;
    if (dt.month > 12) {
      dt.month = 1;
      dt.year++;
    }
  }
}

static uint32_t secondsUntilNextWake(const DateTime& dt) {
  DateTime wake = dt;
  wake.hour = 8;
  wake.minute = 40;
  wake.second = 0;

  const bool alreadyPastWake =
    (dt.hour > 8) ||
    (dt.hour == 8 && (dt.minute > 40 || (dt.minute == 40 && dt.second > 0)));

  if (dt.hour >= 22 || alreadyPastWake) {
    advanceOneDay(wake);
  }

  uint32_t nowSec = (uint32_t)dt.hour * 3600u + (uint32_t)dt.minute * 60u + dt.second;
  uint32_t wakeSec = (uint32_t)wake.hour * 3600u + (uint32_t)wake.minute * 60u + wake.second;
  uint32_t dayOffset = 0;

  if (wake.year != dt.year || wake.month != dt.month || wake.day != dt.day) {
    dayOffset = 24u * 3600u;
  }

  return (dayOffset + wakeSec) - nowSec;
}

static void enterNightDeepSleep(const DateTime& dt) {
  const uint32_t sleepSec = max<uint32_t>(secondsUntilNextWake(dt), 5u);

  Serial.printf("Entering deep sleep for %lu seconds until next wake window\n", (unsigned long)sleepSec);
  Serial.flush();

  Melodies::stop();
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  u8g2.setPowerSave(1);

  powerHold(true);
  gpio_hold_dis((gpio_num_t)PIN_PWR_HOLD);
  gpio_hold_en((gpio_num_t)PIN_PWR_HOLD);
  gpio_deep_sleep_hold_en();

  esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);
  esp_deep_sleep_start();
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(PIN_PWR_HOLD, OUTPUT);
  gpio_hold_dis((gpio_num_t)PIN_PWR_HOLD);
  powerHold(true);

  pinMode(PIN_BTN_PWR, INPUT_PULLUP);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  // OLED
  u8g2.begin();
  u8g2.setI2CAddress(OLED_I2C_ADDR << 1);
  u8g2.setContrast(200);

  // Periféricos
  Melodies::begin(BUZZER, BUZZ_CH, BUZZ_RES);

  // RTC
  const bool rtcOk = rtc.begin();

  // IMU
  imu.begin();

  // estado inicial
  pet.state().mood = Mood::Neutral;
  pet.state().brow = 20;
  pet.state().lookX = 0;
  pet.state().lookY = 0;
  pet.state().blink = 0;
  pet.state().bodyLeanX = 0;

  Melodies::play(Melodies::Tune::Boot);

  Serial.printf("RTC begin: %s\n", rtcOk ? "OK" : "FAIL");
  Serial.println("Type HELP for RTC serial commands");
  Serial.println("Pet boot OK");
}

// ---------------- LOOP ----------------
void loop() {
  static uint32_t nextRtcAt = 0;
  static DateTime dt{};
  static bool hasTime = false;
  static String serialLine;
  static uint32_t pwrPressedAt = 0;
  static bool pwrShutdownArmed = false;
  static uint32_t motionAlertUntil = 0;
  static GestureOverride gestureOverride;
  static uint32_t shakeCooldownUntil = 0;
  static bool shakeArmed = true;
  static uint32_t tiltGestureCooldownUntil = 0;
  static uint32_t pitchHoldSince = 0;
  static int8_t pitchHoldDir = 0;

  const uint32_t now = millis();

  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handleSerialCommand(serialLine);
      serialLine = "";
      continue;
    }

    if (serialLine.length() < 96) {
      serialLine += c;
    }
  }

  const bool pwrPressed = digitalRead(PIN_BTN_PWR) == LOW;
  if (pwrPressed) {
    if (pwrPressedAt == 0) {
      pwrPressedAt = now;
      pwrShutdownArmed = false;
    } else if (!pwrShutdownArmed && (now - pwrPressedAt) >= PWR_LONG_PRESS_MS) {
      pwrShutdownArmed = true;
      Serial.println("Power button long press: shutting down");
      Melodies::play(Melodies::Tune::PhaseAlert);
      delay(120);
      powerHold(false);
    }
  } else {
    pwrPressedAt = 0;
    pwrShutdownArmed = false;
  }

  // RTC read cada 1s
  if (now >= nextRtcAt) {
    nextRtcAt = now + 1000;

    if (rtc.read(dt)) {
      hasTime = true;

      PetPhase ph = phaseForTime(dt);
      if (ph != currentPhase) {
        currentPhase = ph;

        // campana 18:00 (solo 1 vez por día)
        uint32_t dayKey = (uint32_t)(dt.year % 100) * 512u + (uint32_t)dt.month * 32u + (uint32_t)dt.day;
        if (ph == PetPhase::RelaxPM && dt.hour == 18 && dt.minute == 0 && lastSchoolChimeKey != dayKey) {
          lastSchoolChimeKey = dayKey;
          Melodies::play(Melodies::Tune::SchoolChime18);
        }

        // beep por cambio de fase
        if (!(ph == PetPhase::RelaxPM && dt.hour == 18 && dt.minute == 0)) {
          playPhaseCue(ph);
        }
      }

      applyPhaseToPet(ph, dt);

      if (rtc.valid() && ph == PetPhase::NightSleep) {
        delay(120);
        enterNightDeepSleep(dt);
      }
    }
  }

  // IMU -> mirada suave + "se cae" por inclinación sostenida (no por sacudida)
  ImuSample imuSample{};
  if (imu.readSample(imuSample)) {
    pet.state().imuActive = true;

    // roll/pitch en grados aprox
    static float rollF = 0.0f;
    static float pitchF = 0.0f;
    static float lastRollF = 0.0f;
    static float lastPitchF = 0.0f;
    // low-pass: mas responsivo para que la UI siga mejor la mano
    rollF  = rollF  * 0.70f + imuSample.angles.roll  * 0.30f;
    pitchF = pitchF * 0.70f + imuSample.angles.pitch * 0.30f;

    // mirada (más suave)
    const float rollNorm = constrain(rollF / 34.0f, -1.0f, 1.0f);
    const float pitchNorm = constrain(-pitchF / 24.0f, -1.0f, 1.0f);
    pet.state().lookX = (int8_t)roundf(rollNorm * 15.0f);
    pet.state().lookY = (int8_t)roundf(pitchNorm * 12.0f);
    pet.state().eyeShiftX = (int8_t)roundf(rollNorm * 20.0f);
    pet.state().eyeShiftY = (int8_t)roundf(pitchNorm * 15.0f);

    // inclinación sostenida -> cuerpo se desplaza a un lado
    int targetLean = (int)roundf(rollNorm * 3.0f);

    // easing del cuerpo
    int cur = pet.state().bodyLeanX;
    if (cur < targetLean) cur++;
    else if (cur > targetLean) cur--;
    pet.state().bodyLeanX = (int8_t)cur;
    
    const float motionDelta = fabsf(rollF - lastRollF) + fabsf(pitchF - lastPitchF);
    if (motionDelta > 5.5f) {
      motionAlertUntil = now + 180;
    }

    const float accelMag = sqrtf(
      imuSample.ax * imuSample.ax +
      imuSample.ay * imuSample.ay +
      imuSample.az * imuSample.az
    );
    const float accelDynamic = fabsf(accelMag - 1.0f);
    const float gyroShake = fabsf(imuSample.gx) + fabsf(imuSample.gy) + fabsf(imuSample.gz);

    if (accelDynamic < 0.18f && gyroShake < 90.0f) {
      shakeArmed = true;
    }

    const bool strongShake = (accelDynamic > 0.38f && gyroShake > 140.0f) || gyroShake > 320.0f;
    if (strongShake && shakeArmed && now >= shakeCooldownUntil) {
      shakeArmed = false;
      shakeCooldownUntil = now + 1400;
      gestureOverride.mood = Mood::Glitch;
      gestureOverride.brow = 95;
      gestureOverride.untilMs = now + 700;
      motionAlertUntil = now + 700;
    }

    const bool pitchHeld = fabsf(pitchNorm) > 0.72f;
    const int8_t pitchDir = (pitchNorm > 0.0f) ? 1 : -1;
    if (pitchHeld) {
      if (pitchHoldSince == 0 || pitchHoldDir != pitchDir) {
        pitchHoldSince = now;
        pitchHoldDir = pitchDir;
      } else if (now >= tiltGestureCooldownUntil && (now - pitchHoldSince) > 850) {
        tiltGestureCooldownUntil = now + 2200;
        pitchHoldSince = now;

        if (pitchDir > 0) {
          gestureOverride.mood = Mood::Sleepy;
          gestureOverride.brow = 6;
          gestureOverride.untilMs = now + 1400;
          Melodies::beep(920, 80, 25);
        } else {
          gestureOverride.mood = Mood::Angry;
          gestureOverride.brow = 92;
          gestureOverride.untilMs = now + 1400;
          Melodies::play(Melodies::Tune::PhaseDouble);
        }
      }
    } else {
      pitchHoldSince = 0;
      pitchHoldDir = 0;
    }

    pet.state().motionAlert = motionAlertUntil > now;
    lastRollF = rollF;
    lastPitchF = pitchF;
  } else {
    // sin IMU: vuelve al centro lentamente
    pet.state().imuActive = false;
    pet.state().motionAlert = false;
    pet.state().lookX = easeToZero(pet.state().lookX);
    pet.state().lookY = easeToZero(pet.state().lookY);
    pet.state().eyeShiftX = easeToZero(pet.state().eyeShiftX);
    pet.state().eyeShiftY = easeToZero(pet.state().eyeShiftY);
    pet.state().bodyLeanX = easeToZero(pet.state().bodyLeanX);
  }

  // vida: blink y micro anim
  pet.tickBlink();
  pet.tickMoodAuto();
  Melodies::tick();

  if (gestureOverride.untilMs > now) {
    pet.state().mood = gestureOverride.mood;
    pet.state().brow = gestureOverride.brow;
  }

  // render (sin hora arriba)
  pet.render(hasTime ? &dt : nullptr);

  delay(16); // ~60fps
}
