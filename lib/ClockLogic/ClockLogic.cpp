#include "ClockLogic.h"

namespace ClockLogic {
bool parseDateTime(const String& value, DateTime& out) {
  int yy, mo, dd, hh, mi, ss;
  if (sscanf(value.c_str(), "%d-%d-%d %d:%d:%d", &yy, &mo, &dd, &hh, &mi, &ss) != 6) return false;
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

bool isLeapYear(uint16_t year) {
  return ((year % 4u) == 0u && (year % 100u) != 0u) || ((year % 400u) == 0u);
}

uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2) return isLeapYear(year) ? 29 : 28;
  if (month < 1 || month > 12) return 31;
  return DAYS[month - 1];
}

void advanceOneDay(DateTime& dt) {
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

void clampDateTime(DateTime& dt) {
  dt.year = constrain((int)dt.year, 2020, 2099);
  dt.month = (uint8_t)constrain((int)dt.month, 1, 12);
  dt.day = (uint8_t)constrain((int)dt.day, 1, (int)daysInMonth(dt.year, dt.month));
  dt.hour = (uint8_t)constrain((int)dt.hour, 0, 23);
  dt.minute = (uint8_t)constrain((int)dt.minute, 0, 59);
  dt.second = (uint8_t)constrain((int)dt.second, 0, 59);
}

void applyMenuDelta(DateTime& dt, uint8_t fieldIndex, int delta) {
  if (fieldIndex == 0) dt.day = (uint8_t)constrain((int)dt.day + delta, 1, 31);
  else if (fieldIndex == 1) dt.month = (uint8_t)constrain((int)dt.month + delta, 1, 12);
  else if (fieldIndex == 2) dt.year = (uint16_t)constrain((int)dt.year + delta, 2020, 2099);
  else if (fieldIndex == 3) dt.hour = (uint8_t)constrain((int)dt.hour + delta, 0, 23);
  else if (fieldIndex == 4) dt.minute = (uint8_t)constrain((int)dt.minute + delta, 0, 59);
  clampDateTime(dt);
}

void openClockMenu(ClockMenuState& menu, bool rtcHasTime, bool hasSavedClockBackup, const DateTime& current, const DateTime& savedClockBackup) {
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

uint8_t dayOfWeek(const DateTime& dt) {
  static const uint8_t OFFSETS[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  uint16_t year = dt.year;
  if (dt.month < 3) year--;
  return (uint8_t)((year + year / 4u - year / 100u + year / 400u + OFFSETS[dt.month - 1] + dt.day) % 7u);
}

bool isWorkdayExitTime(const DateTime& dt) {
  const uint8_t dow = dayOfWeek(dt);
  if (dow >= 1 && dow <= 4) {
    return dt.hour == 18 && dt.minute == 0;
  }
  if (dow == 5) {
    return dt.hour == 15 && dt.minute == 0;
  }
  return false;
}

bool nextWorkdayExit(const DateTime& dt, DateTime& out) {
  DateTime candidate = dt;
  candidate.second = 0;

  for (uint8_t i = 0; i < 8; ++i) {
    const uint8_t dow = dayOfWeek(candidate);
    bool isWorkday = false;
    uint8_t targetHour = 0;

    if (dow >= 1 && dow <= 4) {
      isWorkday = true;
      targetHour = 18;
    } else if (dow == 5) {
      isWorkday = true;
      targetHour = 15;
    }

    if (isWorkday) {
      candidate.hour = targetHour;
      candidate.minute = 0;
      candidate.second = 0;

      const bool sameDayFuture =
        i == 0 &&
        (dt.hour < targetHour || (dt.hour == targetHour && dt.minute == 0 && dt.second == 0));

      if (i > 0 || sameDayFuture) {
        out = candidate;
        return true;
      }
    }

    candidate.hour = 0;
    candidate.minute = 0;
    candidate.second = 0;
    advanceOneDay(candidate);
  }

  return false;
}

PetPhase phaseForTime(const DateTime& dt) {
  const int m = dt.hour * 60 + dt.minute;
  if (m >= 7 * 60 && m < 9 * 60) return PetPhase::SleepyAM;
  if (m >= 9 * 60 && m < (12 * 60 + 30)) return PetPhase::ActiveAM;
  if (m >= (12 * 60 + 30) && m < 14 * 60) return PetPhase::LunchRest;
  if (m >= 14 * 60 && m < (14 * 60 + 30)) return PetPhase::PostLunchNap;
  if (m >= (14 * 60 + 30) && m < 18 * 60) return PetPhase::WorkToAngry;
  if (m >= 18 * 60 && m < 22 * 60) return PetPhase::RelaxPM;
  return PetPhase::NightSleep;
}

MoodIntent phaseIntentFor(PetPhase ph, const DateTime& dt) {
  MoodIntent intent{};

  if (ph == PetPhase::SleepyAM) {
    intent.mood = Mood::Sleepy;
    intent.brow = 16;
    intent.squint = 5;
    intent.overlay = OverlayFx::Doze;
    intent.overlayStrength = 38;
  } else if (ph == PetPhase::ActiveAM) {
    intent.mood = Mood::Neutral;
    intent.brow = 24;
  } else if (ph == PetPhase::LunchRest) {
    intent.mood = Mood::Relax;
    intent.brow = 12;
    intent.squint = 4;
    intent.overlay = OverlayFx::Calm;
    intent.overlayStrength = 42;
  } else if (ph == PetPhase::PostLunchNap) {
    intent.mood = Mood::Sleepy;
    intent.brow = 10;
    intent.squint = 8;
    intent.overlay = OverlayFx::Doze;
    intent.overlayStrength = 72;
  } else if (ph == PetPhase::WorkToAngry) {
    const int m0 = (14 * 60 + 30);
    const int m1 = (18 * 60);
    const int m = dt.hour * 60 + dt.minute;
    const int t = constrain(m - m0, 0, (m1 - m0));
    intent.mood = (t > (m1 - m0) * 3 / 4) ? Mood::Angry : Mood::Neutral;
    intent.brow = (uint8_t)constrain(22 + (t * 68) / (m1 - m0), 22, 90);
    intent.squint = (uint8_t)constrain((t * 8) / (m1 - m0), 0, 8);
    intent.overlay = (t > (m1 - m0) / 2) ? OverlayFx::Focus : OverlayFx::None;
    intent.overlayStrength = (uint8_t)constrain((t * 100) / (m1 - m0), 0, 88);
  } else if (ph == PetPhase::RelaxPM) {
    intent.mood = Mood::Relax;
    intent.brow = 12;
    intent.squint = 3;
    intent.overlay = OverlayFx::Calm;
    intent.overlayStrength = 58;
  } else if (ph == PetPhase::NightSleep) {
    intent.mood = Mood::Sleepy;
    intent.brow = 8;
    intent.squint = 10;
    intent.overlay = OverlayFx::Doze;
    intent.overlayStrength = 84;
  }

  return intent;
}

uint32_t secondsUntilNextWake(const DateTime& dt) {
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

  const uint32_t nowSec = (uint32_t)dt.hour * 3600u + (uint32_t)dt.minute * 60u + dt.second;
  const uint32_t wakeSec = (uint32_t)wake.hour * 3600u + (uint32_t)wake.minute * 60u + wake.second;
  uint32_t dayOffset = 0;

  if (wake.year != dt.year || wake.month != dt.month || wake.day != dt.day) {
    dayOffset = 24u * 3600u;
  }

  return (dayOffset + wakeSec) - nowSec;
}
}
