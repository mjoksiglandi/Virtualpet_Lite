#include <Arduino.h>
#include <esp_sleep.h>
#include <Preferences.h>
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
static Preferences prefs;
static bool hasSavedClockBackup = false;
static DateTime savedClockBackup{2026, 1, 1, 8, 40, 0};
enum class UiMode : uint8_t {
  Pet = 0,
  Clock = 1
};
static UiMode uiMode = UiMode::Pet;

// ---------------- BUZZER ----------------
static constexpr int BUZZER = PIN_BUZZER;
static constexpr int BUZZ_CH = 0;
static constexpr int BUZZ_RES = 8;
static constexpr uint32_t PWR_LONG_PRESS_MS = 1500;
static constexpr uint32_t PWR_CLICK_MS = 350;
static constexpr uint32_t PWR_DOUBLE_CLICK_GAP_MS = 450;
static constexpr uint32_t MENU_TILT_REPEAT_MS = 180;
static constexpr float MENU_TILT_THRESHOLD = 0.38f;

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

struct BatterySnapshot {
  bool valid = false;
  bool usbLikely = false;
  uint16_t raw = 0;
  uint16_t mv = 0;
  uint8_t percent = 0;
};

static BatterySnapshot g_battery;
static constexpr float BATTERY_ADC_SCALE = 3.16f;

static const char* i2cDeviceLabel(uint8_t addr) {
  if (addr == OLED_I2C_ADDR) return "OLED SH1106/SSD1306";
  if (addr == 0x51) return "RTC PCF85063";
  if (addr == 0x15) return "Touch CST816T";
  if (addr == 0x6A || addr == 0x6B) return "IMU QMI8658?";
  if (addr >= 0x78) return "Reserved / ghost response";
  return "Unknown";
}

static void scanI2cBus() {
  uint8_t found[16];
  uint8_t foundCount = 0;

  Serial.printf("I2C scan on SDA=%d SCL=%d\n", PIN_I2C_SDA, PIN_I2C_SCL);
  for (uint8_t addr = 1; addr < 0x7F; ++addr) {
    Wire.beginTransmission(addr);
    const uint8_t err = Wire.endTransmission();
    if (err == 0) {
      if (foundCount < (sizeof(found) / sizeof(found[0]))) {
        found[foundCount++] = addr;
      }
      Serial.printf("  - 0x%02X  %s\n", addr, i2cDeviceLabel(addr));
    }
  }

  if (foundCount == 0) {
    Serial.println("  no devices found");
    return;
  }

  Serial.print("I2C summary: ");
  for (uint8_t i = 0; i < foundCount; ++i) {
    if (i > 0) Serial.print(", ");
    Serial.printf("0x%02X", found[i]);
  }
  Serial.println();
}

static uint16_t readBatteryMillivolts() {
  const uint32_t samples = 12;
  uint32_t acc = 0;
  for (uint32_t i = 0; i < samples; ++i) {
    acc += (uint32_t)analogRead(PIN_BAT_ADC);
    delay(2);
  }

  const float raw = (float)acc / (float)samples;
  const float vadc = raw * (3.3f / 4095.0f);
  const float vbatt = vadc * BATTERY_ADC_SCALE;
  return (uint16_t)roundf(vbatt * 1000.0f);
}

static uint8_t batteryPercentFromMv(uint16_t mv) {
  if (mv <= 3300) return 0;
  if (mv >= 4200) return 100;
  return (uint8_t)(((uint32_t)(mv - 3300) * 100u) / 900u);
}

static void updateBatterySnapshot(uint32_t nowMs) {
  static uint32_t nextBatteryAt = 0;
  static float filteredMv = 0.0f;

  if (nextBatteryAt != 0 && nowMs < nextBatteryAt) {
    return;
  }

  const uint16_t mv = readBatteryMillivolts();
  if (filteredMv <= 0.0f) filteredMv = (float)mv;
  else filteredMv = filteredMv * 0.75f + (float)mv * 0.25f;

  g_battery.valid = true;
  g_battery.raw = (uint16_t)analogRead(PIN_BAT_ADC);
  g_battery.mv = (uint16_t)roundf(filteredMv);
  g_battery.percent = batteryPercentFromMv(g_battery.mv);
  g_battery.usbLikely = g_battery.mv >= 4150;
  nextBatteryAt = nowMs + 1500;
}

static void printBatterySnapshot() {
  Serial.printf(
    "BAT raw=%u mv=%u pct=%u usb_likely=%s\n",
    g_battery.raw,
    g_battery.mv,
    g_battery.percent,
    g_battery.usbLikely ? "yes" : "no"
  );
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

static void saveClockBackup(const DateTime& dt) {
  savedClockBackup = dt;
  hasSavedClockBackup = true;
  prefs.putBool("has_time", true);
  prefs.putUShort("year", dt.year);
  prefs.putUChar("month", dt.month);
  prefs.putUChar("day", dt.day);
  prefs.putUChar("hour", dt.hour);
  prefs.putUChar("minute", dt.minute);
  prefs.putUChar("second", dt.second);
}

static void saveUiMode(UiMode mode) {
  uiMode = mode;
  prefs.putUChar("ui_mode", (uint8_t)mode);
}

static void handleSerialCommand(const String& raw) {
  String cmd = raw;
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd.equalsIgnoreCase("HELP")) {
    Serial.println("Commands:");
    Serial.println("  BAT?");
    Serial.println("  I2C?");
    Serial.println("  RTC?");
    Serial.println("  SET_RTC YYYY-MM-DD HH:MM:SS");
    return;
  }

  if (cmd.equalsIgnoreCase("BAT?")) {
    printBatterySnapshot();
    return;
  }

  if (cmd.equalsIgnoreCase("I2C?")) {
    scanI2cBus();
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

    saveClockBackup(dt);
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

static void clampDateTime(DateTime& dt) {
  dt.year = constrain((int)dt.year, 2020, 2099);
  dt.month = (uint8_t)constrain((int)dt.month, 1, 12);
  dt.day = (uint8_t)constrain((int)dt.day, 1, (int)daysInMonth(dt.year, dt.month));
  dt.hour = (uint8_t)constrain((int)dt.hour, 0, 23);
  dt.minute = (uint8_t)constrain((int)dt.minute, 0, 59);
  dt.second = (uint8_t)constrain((int)dt.second, 0, 59);
}

static void applyMenuDelta(DateTime& dt, uint8_t fieldIndex, int delta) {
  if (fieldIndex == 0) dt.day = (uint8_t)constrain((int)dt.day + delta, 1, 31);
  else if (fieldIndex == 1) dt.month = (uint8_t)constrain((int)dt.month + delta, 1, 12);
  else if (fieldIndex == 2) dt.year = (uint16_t)constrain((int)dt.year + delta, 2020, 2099);
  else if (fieldIndex == 3) dt.hour = (uint8_t)constrain((int)dt.hour + delta, 0, 23);
  else if (fieldIndex == 4) dt.minute = (uint8_t)constrain((int)dt.minute + delta, 0, 59);
  clampDateTime(dt);
}

static void openClockMenu(ClockMenuState& menu, bool rtcHasTime, const DateTime& current) {
  menu.active = true;
  menu.dirty = false;
  menu.rtcValidOnEntry = rtcHasTime;
  menu.usingBackupSeed = !rtcHasTime && hasSavedClockBackup;
  menu.fieldIndex = 0;
  menu.lastInteractionMs = millis();
  menu.lastTiltStepMs = 0;
  if (rtcHasTime) menu.draft = current;
  else if (hasSavedClockBackup) menu.draft = savedClockBackup;
  else menu.draft = DateTime{2026, 1, 1, 8, 40, 0};
  menu.draft.second = 0;
  clampDateTime(menu.draft);
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

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
  pinMode(PIN_BAT_ADC, INPUT);

  // Periféricos
  Melodies::begin(BUZZER, BUZZ_CH, BUZZ_RES);

  // RTC
  const bool rtcOk = rtc.begin();
  scanI2cBus();
  prefs.begin("virtualpet", false);
  hasSavedClockBackup = prefs.getBool("has_time", false);
  if (hasSavedClockBackup) {
    savedClockBackup.year = prefs.getUShort("year", 2026);
    savedClockBackup.month = prefs.getUChar("month", 1);
    savedClockBackup.day = prefs.getUChar("day", 1);
    savedClockBackup.hour = prefs.getUChar("hour", 8);
    savedClockBackup.minute = prefs.getUChar("minute", 40);
    savedClockBackup.second = prefs.getUChar("second", 0);
    clampDateTime(savedClockBackup);
  }
  const uint8_t storedUiMode = prefs.getUChar("ui_mode", (uint8_t)UiMode::Pet);
  uiMode = (storedUiMode == (uint8_t)UiMode::Clock) ? UiMode::Clock : UiMode::Pet;

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
  static bool pwrMenuHoldHandled = false;
  static uint32_t pwrReleasedAt = 0;
  static uint8_t pwrClickCount = 0;
  static uint32_t motionAlertUntil = 0;
  static GestureOverride gestureOverride;
  static ClockMenuState clockMenu;
  static uint32_t shakeCooldownUntil = 0;
  static bool shakeArmed = true;
  static uint32_t tiltGestureCooldownUntil = 0;
  static uint32_t pitchHoldSince = 0;
  static int8_t pitchHoldDir = 0;

  const uint32_t now = millis();
  updateBatterySnapshot(now);

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
      pwrMenuHoldHandled = false;
    } else if (clockMenu.active && !pwrMenuHoldHandled && (now - pwrPressedAt) >= PWR_LONG_PRESS_MS) {
      pwrMenuHoldHandled = true;
      clockMenu.draft.second = 0;
      clampDateTime(clockMenu.draft);
      if (rtc.set(clockMenu.draft)) {
        saveClockBackup(clockMenu.draft);
        dt = clockMenu.draft;
        hasTime = true;
        currentPhase = PetPhase::Unknown;
        nextRtcAt = 0;
        clockMenu.active = false;
        clockMenu.dirty = false;
        Melodies::play(Melodies::Tune::PhaseTick);
        Serial.println("RTC updated from device menu");
      } else {
        Melodies::play(Melodies::Tune::PhaseAlert);
        Serial.println("RTC update from device menu failed");
      }
    } else if (!clockMenu.active && !pwrShutdownArmed && (now - pwrPressedAt) >= PWR_LONG_PRESS_MS) {
      pwrShutdownArmed = true;
      Serial.println("Power button long press: shutting down");
      Melodies::play(Melodies::Tune::PhaseAlert);
      delay(120);
      powerHold(false);
    }
  } else {
    if (pwrPressedAt != 0) {
      const uint32_t pressDur = now - pwrPressedAt;
      if (clockMenu.active) {
        if (!pwrMenuHoldHandled && pressDur <= PWR_CLICK_MS) {
          clockMenu.fieldIndex = (uint8_t)((clockMenu.fieldIndex + 1) % 5);
          clockMenu.lastInteractionMs = now;
        }
      } else if (!pwrShutdownArmed && pressDur <= PWR_CLICK_MS) {
        pwrClickCount++;
        pwrReleasedAt = now;
      }
    }
    pwrPressedAt = 0;
    pwrShutdownArmed = false;
    pwrMenuHoldHandled = false;
  }

  if (!clockMenu.active && pwrClickCount > 0 && (now - pwrReleasedAt) > PWR_DOUBLE_CLICK_GAP_MS) {
    if (pwrClickCount >= 2) {
      DateTime seed = hasTime ? dt : (hasSavedClockBackup ? savedClockBackup : DateTime{2026, 1, 1, 8, 40, 0});
      openClockMenu(clockMenu, hasTime && rtc.valid(), seed);
      Melodies::play(Melodies::Tune::PhaseDouble);
      Serial.println("Clock menu opened");
    } else if (pwrClickCount == 1) {
      saveUiMode(uiMode == UiMode::Pet ? UiMode::Clock : UiMode::Pet);
      Melodies::play(Melodies::Tune::PhaseTick);
      Serial.printf("UI mode: %s\n", uiMode == UiMode::Clock ? "clock" : "pet");
    }
    pwrClickCount = 0;
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

      if (!clockMenu.active && rtc.valid() && ph == PetPhase::NightSleep) {
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

    if (clockMenu.active) {
      if (fabsf(rollNorm) >= MENU_TILT_THRESHOLD && (now - clockMenu.lastTiltStepMs) >= MENU_TILT_REPEAT_MS) {
        applyMenuDelta(clockMenu.draft, clockMenu.fieldIndex, rollNorm > 0.0f ? 1 : -1);
        clockMenu.dirty = true;
        clockMenu.lastTiltStepMs = now;
        clockMenu.lastInteractionMs = now;
      } else if (fabsf(rollNorm) < (MENU_TILT_THRESHOLD * 0.55f)) {
        clockMenu.lastTiltStepMs = 0;
      }
    }

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

    const bool pitchHeld = !clockMenu.active && fabsf(pitchNorm) > 0.72f;
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
        } else {
          gestureOverride.mood = Mood::Angry;
          gestureOverride.brow = 92;
          gestureOverride.untilMs = now + 1400;
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

  if (!clockMenu.active && gestureOverride.untilMs > now) {
    pet.state().mood = gestureOverride.mood;
    pet.state().brow = gestureOverride.brow;
  }

  // render (sin hora arriba)
  ClockMenuView menuView{};
  ClockFaceView clockView{};
  if (clockMenu.active) {
    menuView.active = true;
    menuView.rtcValid = clockMenu.rtcValidOnEntry;
    menuView.dirty = clockMenu.dirty;
    menuView.fieldIndex = clockMenu.fieldIndex;
    menuView.value = clockMenu.draft;
  }
  if (!clockMenu.active && uiMode == UiMode::Clock) {
    clockView.active = true;
    clockView.rtcValid = hasTime && rtc.valid();
    clockView.batteryVisible = g_battery.valid;
    clockView.batteryUsb = g_battery.usbLikely;
    clockView.batteryPercent = g_battery.percent;
    clockView.batteryMv = g_battery.mv;
    if (hasTime) clockView.value = dt;
    else if (hasSavedClockBackup) clockView.value = savedClockBackup;
    else clockView.value = DateTime{2026, 1, 1, 8, 40, 0};
  }
  pet.render(
    hasTime ? &dt : nullptr,
    clockMenu.active ? &menuView : nullptr,
    clockView.active ? &clockView : nullptr
  );

  delay(16); // ~60fps
}
