#include "PetBehavior.h"

namespace PetBehavior {
uint8_t easeU8(uint8_t current, uint8_t target, uint8_t step) {
  if (current < target) return (uint8_t)min((int)current + (int)step, (int)target);
  if (current > target) return (uint8_t)max((int)current - (int)step, (int)target);
  return current;
}

MoodIntent composeVisualIntent(const MoodIntent& phaseIntent, const GestureOverride& gestureOverride, bool clockMenuActive, bool motionAlert) {
  MoodIntent visualIntent = phaseIntent;
  if (!clockMenuActive && gestureOverride.active) {
    visualIntent.mood = gestureOverride.mood;
    visualIntent.brow = gestureOverride.brow;
    visualIntent.squint = gestureOverride.squint;
    visualIntent.overlay = gestureOverride.overlay;
    visualIntent.overlayStrength = gestureOverride.overlayStrength;
  } else if (motionAlert && visualIntent.overlay == OverlayFx::None) {
    visualIntent.overlay = OverlayFx::Startle;
    visualIntent.overlayStrength = 48;
  }
  return visualIntent;
}

void applyVisualState(PetState& petState, const VisualState& visual) {
  petState.imuActive = visual.imuActive;
  petState.motionAlert = visual.motionAlert;
  petState.lookX = visual.lookX;
  petState.lookY = visual.lookY;
  petState.eyeShiftX = visual.eyeShiftX;
  petState.eyeShiftY = visual.eyeShiftY;
  petState.bodyLeanX = visual.bodyLeanX;
}

void applyMoodIntent(PetState& petState, const MoodIntent& intent) {
  petState.mood = intent.mood;
  petState.brow = easeU8(petState.brow, intent.brow, 2);
  petState.squint = easeU8(petState.squint, intent.squint, 1);
  petState.overlay = intent.overlay;
  petState.overlayStrength = easeU8(petState.overlayStrength, intent.overlayStrength, 4);
}
}
