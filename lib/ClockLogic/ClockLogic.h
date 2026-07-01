#pragma once

#include <Arduino.h>
#include "FirmwareTypes.h"

namespace ClockLogic {
bool parseDateTime(const String& value, DateTime& out);
bool isLeapYear(uint16_t year);
uint8_t daysInMonth(uint16_t year, uint8_t month);
void advanceOneDay(DateTime& dt);
void clampDateTime(DateTime& dt);
void applyMenuDelta(DateTime& dt, uint8_t fieldIndex, int delta);
void openClockMenu(ClockMenuState& menu, bool rtcHasTime, bool hasSavedClockBackup, const DateTime& current, const DateTime& savedClockBackup);
PetPhase phaseForTime(const DateTime& dt);
MoodIntent phaseIntentFor(PetPhase ph, const DateTime& dt);
uint32_t secondsUntilNextWake(const DateTime& dt);
}
