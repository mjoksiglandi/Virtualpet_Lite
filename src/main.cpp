#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#include "pinout.h"

#include "PetUI.h"
#include "RtcClock.h"
#include "ImuQmi8658.h"

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

static void buzzerInit() {
  ledcSetup(BUZZ_CH, 2000, BUZZ_RES);
  ledcAttachPin(BUZZER, BUZZ_CH);
  ledcWriteTone(BUZZ_CH, 0);
}

static void beepPattern(uint8_t id) {
  // patrones cortos (bloqueantes, pero breves)
  switch (id) {
    case 1: // tick
      ledcWriteTone(BUZZ_CH, 2200); delay(35);
      ledcWriteTone(BUZZ_CH, 0);    delay(25);
      break;

    case 2: // doble
      ledcWriteTone(BUZZ_CH, 1800); delay(40);
      ledcWriteTone(BUZZ_CH, 0);    delay(35);
      ledcWriteTone(BUZZ_CH, 2000); delay(45);
      ledcWriteTone(BUZZ_CH, 0);    delay(25);
      break;

    case 3: // “alerta” suave
      ledcWriteTone(BUZZ_CH, 1200); delay(70);
      ledcWriteTone(BUZZ_CH, 0);    delay(35);
      ledcWriteTone(BUZZ_CH, 900);  delay(85);
      ledcWriteTone(BUZZ_CH, 0);    delay(35);
      break;

    default:
      ledcWriteTone(BUZZ_CH, 0);
      break;
  }
  ledcWriteTone(BUZZ_CH, 0);
}

// mini “campana de salida” (tipo escuela). Se ejecuta SOLO una vez al día.
static void melodySchool() {
  const int notes[] = { 988, 784, 659, 784, 988, 0, 988, 1047 };
  const int dur[]   = { 180, 180, 180, 180, 280, 120, 180, 420 };

  for (size_t i = 0; i < sizeof(notes)/sizeof(notes[0]); i++) {
    if (notes[i] > 0) {
      ledcWriteTone(BUZZ_CH, notes[i]);
    } else {
      ledcWriteTone(BUZZ_CH, 0);
    }
    delay(dur[i]);
  }
  ledcWriteTone(BUZZ_CH, 0);
}

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

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  // OLED
  u8g2.begin();
  u8g2.setI2CAddress(OLED_I2C_ADDR << 1);
  u8g2.setContrast(200);

  // Periféricos
  buzzerInit();

  // RTC
  rtc.begin();

  // IMU
  imu.begin();

  // estado inicial
  pet.state().mood = Mood::Neutral;
  pet.state().brow = 20;
  pet.state().lookX = 0;
  pet.state().lookY = 0;
  pet.state().blink = 0;
  pet.state().bodyLeanX = 0;

  Serial.println("Pet boot OK");
}

// ---------------- LOOP ----------------
void loop() {
  static uint32_t nextRtcAt = 0;
  static DateTime dt{};
  static bool hasTime = false;

  const uint32_t now = millis();

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
          melodySchool();
        }

        // beep por cambio de fase
        if (ph == PetPhase::NightSleep) beepPattern(3);
        else if (ph == PetPhase::WorkToAngry) beepPattern(2);
        else beepPattern(1);
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

  // render (sin hora arriba)
  pet.render(hasTime ? &dt : nullptr);

  delay(16); // ~60fps
}
