#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "pinout.h"

#include "BatteryMonitor.h"
#include "ClockLogic.h"
#include "FirmwareTypes.h"
#include "ImuQmi8658.h"
#include "ImuMotion.h"
#include "Melodies.h"
#include "PetBehavior.h"
#include "PetUI.h"
#include "PowerButton.h"
#include "RtcClock.h"

static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
static PetUI pet(u8g2);
static RtcClock rtc;
static ImuQmi8658 imu;
static Preferences prefs;
static BatteryMonitor batteryMonitor(PIN_BAT_ADC, 3.16f);
static PowerButton powerButton(1500, 350, 450);
static ImuMotion imuMotion(0.38f, 180);

static bool hasSavedClockBackup = false;
static DateTime savedClockBackup{2026, 1, 1, 8, 40, 0};
static UiMode uiMode = UiMode::Pet;
static PetPhase currentPhase = PetPhase::Unknown;
static uint32_t lastSchoolChimeKey = 0;

static constexpr int BUZZER = PIN_BUZZER;
static constexpr int BUZZ_CH = 0;
static constexpr int BUZZ_RES = 8;
static constexpr uint8_t LOW_BATTERY_SLEEP_PERCENT = 30;
static constexpr uint32_t LOW_BATTERY_FALLBACK_SLEEP_SEC = 30u * 60u;

static void powerHold(bool enable) {
  digitalWrite(PIN_PWR_HOLD, enable ? HIGH : LOW);
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

static void playPhaseCue(PetPhase ph) {
  if (ph == PetPhase::NightSleep) Melodies::play(Melodies::Tune::PhaseAlert);
  else if (ph == PetPhase::WorkToAngry) Melodies::play(Melodies::Tune::PhaseDouble);
  else Melodies::play(Melodies::Tune::PhaseTick);
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

static void printBatterySnapshot() {
  const BatterySnapshot& battery = batteryMonitor.snapshot();
  Serial.printf(
    "BAT raw=%u mv=%u pct=%u usb_likely=%s\n",
    battery.raw,
    battery.mv,
    battery.percent,
    battery.usbLikely ? "yes" : "no"
  );
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
    if (!ClockLogic::parseDateTime(value, dt)) {
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

static void enterDeepSleepForSeconds(uint32_t sleepSec, const char* reason) {
  const uint32_t clampedSleepSec = max<uint32_t>(sleepSec, 5u);

  Serial.printf("Entering deep sleep (%s) for %lu seconds\n", reason, (unsigned long)clampedSleepSec);
  Serial.flush();

  Melodies::stop();
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  u8g2.setPowerSave(1);

  powerHold(true);
  gpio_hold_dis((gpio_num_t)PIN_PWR_HOLD);
  gpio_hold_en((gpio_num_t)PIN_PWR_HOLD);
  gpio_deep_sleep_hold_en();

  esp_sleep_enable_timer_wakeup((uint64_t)clampedSleepSec * 1000000ULL);
  esp_deep_sleep_start();
}

static void enterNightDeepSleep(const DateTime& dt) {
  enterDeepSleepForSeconds(ClockLogic::secondsUntilNextWake(dt), "night schedule");
}

static void enterLowBatteryDeepSleep(bool hasTime, const DateTime& dt) {
  if (hasTime && rtc.valid()) {
    enterDeepSleepForSeconds(ClockLogic::secondsUntilNextWake(dt), "low battery until next wake");
    return;
  }

  enterDeepSleepForSeconds(LOW_BATTERY_FALLBACK_SLEEP_SEC, "low battery fallback");
}

static void applyPhaseFromClock(const DateTime& dt) {
  const PetPhase ph = ClockLogic::phaseForTime(dt);
  if (ph != currentPhase) {
    currentPhase = ph;
    const uint32_t dayKey = (uint32_t)(dt.year % 100) * 512u + (uint32_t)dt.month * 32u + (uint32_t)dt.day;
    if (ph == PetPhase::RelaxPM && dt.hour == 18 && dt.minute == 0 && lastSchoolChimeKey != dayKey) {
      lastSchoolChimeKey = dayKey;
      Melodies::play(Melodies::Tune::SchoolChime18);
    }
    if (!(ph == PetPhase::RelaxPM && dt.hour == 18 && dt.minute == 0)) {
      playPhaseCue(ph);
    }
  }
}

static void updateClockView(ClockFaceView& clockView, bool hasTime) {
  const BatterySnapshot& battery = batteryMonitor.snapshot();
  clockView.active = true;
  clockView.rtcValid = hasTime && rtc.valid();
  clockView.batteryVisible = battery.valid;
  clockView.batteryUsb = battery.usbLikely;
  clockView.batteryPercent = battery.percent;
  clockView.batteryMv = battery.mv;
}

void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(PIN_PWR_HOLD, OUTPUT);
  gpio_hold_dis((gpio_num_t)PIN_PWR_HOLD);
  powerHold(true);
  pinMode(PIN_BTN_PWR, INPUT_PULLUP);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  u8g2.begin();
  u8g2.setI2CAddress(OLED_I2C_ADDR << 1);
  u8g2.setContrast(200);

  batteryMonitor.begin();
  Melodies::begin(BUZZER, BUZZ_CH, BUZZ_RES);

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
    ClockLogic::clampDateTime(savedClockBackup);
  }

  const uint8_t storedUiMode = prefs.getUChar("ui_mode", (uint8_t)UiMode::Pet);
  uiMode = (storedUiMode == (uint8_t)UiMode::Clock) ? UiMode::Clock : UiMode::Pet;

  imu.begin();

  pet.state().mood = Mood::Neutral;
  pet.state().brow = 20;
  pet.state().squint = 0;
  pet.state().lookX = 0;
  pet.state().lookY = 0;
  pet.state().blink = 0;
  pet.state().bodyLeanX = 0;
  pet.state().overlay = OverlayFx::None;
  pet.state().overlayStrength = 0;

  Melodies::play(Melodies::Tune::Boot);

  Serial.printf("RTC begin: %s\n", rtcOk ? "OK" : "FAIL");
  Serial.println("Type HELP for RTC serial commands");
  Serial.println("Pet boot OK");
}

void loop() {
  static uint32_t nextRtcAt = 0;
  static DateTime dt{};
  static bool hasTime = false;
  static String serialLine;
  static GestureOverride gestureOverride;
  static ClockMenuState clockMenu;
  static MoodIntent phaseIntent;
  static VisualState visual;

  const uint32_t now = millis();
  batteryMonitor.update(now);
  const BatterySnapshot& battery = batteryMonitor.snapshot();

  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handleSerialCommand(serialLine);
      serialLine = "";
      continue;
    }
    if (serialLine.length() < 96) serialLine += c;
  }

  switch (powerButton.update(now, digitalRead(PIN_BTN_PWR) == LOW, clockMenu.active)) {
    case PowerAction::Shutdown:
      Serial.println("Power button long press: shutting down");
      Melodies::play(Melodies::Tune::PhaseAlert);
      delay(120);
      powerHold(false);
      break;
    case PowerAction::ClockMenuSave:
      clockMenu.draft.second = 0;
      ClockLogic::clampDateTime(clockMenu.draft);
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
      break;
    case PowerAction::ClockMenuNextField:
      clockMenu.fieldIndex = (uint8_t)((clockMenu.fieldIndex + 1) % 5);
      clockMenu.lastInteractionMs = now;
      break;
    case PowerAction::OpenClockMenu: {
      const DateTime seed = hasTime ? dt : (hasSavedClockBackup ? savedClockBackup : DateTime{2026, 1, 1, 8, 40, 0});
      ClockLogic::openClockMenu(clockMenu, hasTime && rtc.valid(), hasSavedClockBackup, seed, savedClockBackup);
      Melodies::play(Melodies::Tune::PhaseDouble);
      Serial.println("Clock menu opened");
      break;
    }
    case PowerAction::ToggleUiMode:
      saveUiMode(uiMode == UiMode::Pet ? UiMode::Clock : UiMode::Pet);
      Melodies::play(Melodies::Tune::PhaseTick);
      Serial.printf("UI mode: %s\n", uiMode == UiMode::Clock ? "clock" : "pet");
      break;
    case PowerAction::None:
    default:
      break;
  }

  if (!clockMenu.active && battery.valid && !battery.usbLikely && battery.percent <= LOW_BATTERY_SLEEP_PERCENT) {
    Serial.printf(
      "Low battery detected: %u%% (%umV). Entering protective deep sleep.\n",
      battery.percent,
      battery.mv
    );
    delay(120);
    enterLowBatteryDeepSleep(hasTime, dt);
  }

  if (now >= nextRtcAt) {
    nextRtcAt = now + 1000;
    if (rtc.read(dt)) {
      hasTime = true;
      applyPhaseFromClock(dt);
      phaseIntent = ClockLogic::phaseIntentFor(currentPhase, dt);

      if (!clockMenu.active && rtc.valid() && currentPhase == PetPhase::NightSleep) {
        delay(120);
        enterNightDeepSleep(dt);
      }
    }
  }

  if (gestureOverride.active && now >= gestureOverride.untilMs) {
    gestureOverride.active = false;
  }

  ImuSample imuSample{};
  if (imu.readSample(imuSample)) {
    imuMotion.update(now, imuSample, clockMenu.active, clockMenu, visual, gestureOverride);
  } else {
    imuMotion.updateNoSample(visual);
  }

  PetBehavior::applyVisualState(pet.state(), visual);
  const MoodIntent visualIntent = PetBehavior::composeVisualIntent(
    phaseIntent,
    gestureOverride,
    clockMenu.active,
    visual.motionAlert
  );
  PetBehavior::applyMoodIntent(pet.state(), visualIntent);

  pet.tickBlink();
  pet.tickMoodAuto();
  Melodies::tick();

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
    updateClockView(clockView, hasTime);
    if (hasTime) clockView.value = dt;
    else if (hasSavedClockBackup) clockView.value = savedClockBackup;
    else clockView.value = DateTime{2026, 1, 1, 8, 40, 0};
  }

  pet.render(
    hasTime ? &dt : nullptr,
    clockMenu.active ? &menuView : nullptr,
    clockView.active ? &clockView : nullptr
  );

  delay(16);
}
