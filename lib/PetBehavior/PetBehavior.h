#pragma once

#include <Arduino.h>
#include "FirmwareTypes.h"
#include "PetUI.h"

namespace PetBehavior {
uint8_t easeU8(uint8_t current, uint8_t target, uint8_t step);
MoodIntent composeVisualIntent(const MoodIntent& phaseIntent, const GestureOverride& gestureOverride, bool clockMenuActive, bool motionAlert);
void applyVisualState(PetState& petState, const VisualState& visual);
void applyMoodIntent(PetState& petState, const MoodIntent& intent);
}
