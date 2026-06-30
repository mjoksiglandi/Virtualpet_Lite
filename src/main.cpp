#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

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

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(PIN_PWR_HOLD, OUTPUT);
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
    }
  }

  // IMU -> mirada suave + "se cae" por inclinación sostenida (no por sacudida)
  ImuAngles ang{};
  if (imu.readAngles(ang)) {
    // roll/pitch en grados aprox
    static float rollF = 0.0f;
    static float pitchF = 0.0f;
    // low-pass (suave)
    rollF  = rollF  * 0.92f + ang.roll  * 0.08f;
    pitchF = pitchF * 0.92f + ang.pitch * 0.08f;

    // mirada (más suave)
    int lx = (int)(rollF / 3.0f);    // 1 px cada ~3°
    int ly = (int)(-pitchF / 4.0f);  // 1 px cada ~4°
    pet.state().lookX = (int8_t)constrain(lx, -8, 8);
    pet.state().lookY = (int8_t)constrain(ly, -4, 4);

    // inclinación sostenida -> cuerpo se desplaza a un lado
    static uint32_t tiltSince = 0;
    const float tiltTh = 18.0f; // grados para "caerse"
    if (fabsf(rollF) > tiltTh) {
      if (tiltSince == 0) tiltSince = millis();
    } else {
      tiltSince = 0;
    }

    int targetLean = 0;
    if (tiltSince && (millis() - tiltSince) > 700) {
      targetLean = (rollF > 0) ? 6 : -6;
    }

    // easing del cuerpo
    int cur = pet.state().bodyLeanX;
    if (cur < targetLean) cur++;
    else if (cur > targetLean) cur--;
    pet.state().bodyLeanX = (int8_t)cur;
  } else {
    // sin IMU: vuelve al centro lentamente
    if (pet.state().bodyLeanX > 0) pet.state().bodyLeanX--;
    else if (pet.state().bodyLeanX < 0) pet.state().bodyLeanX++;
  }

  // vida: blink y micro anim
  pet.tickBlink();
  pet.tickMoodAuto();
  Melodies::tick();

  // render (sin hora arriba)
  pet.render(hasTime ? &dt : nullptr);

  delay(16); // ~60fps
}
