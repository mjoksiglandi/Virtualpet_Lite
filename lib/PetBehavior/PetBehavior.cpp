#include "PetBehavior.h"

namespace PetBehavior {
uint8_t easeU8(uint8_t current, uint8_t target, uint8_t step) {
  if (current < target) return (uint8_t)min((int)current + (int)step, (int)target);
  if (current > target) return (uint8_t)max((int)current - (int)step, (int)target);
  return current;
}

static void applyOverlayIntent(MoodIntent& intent, OverlayState overlays) {
  if (hasOverlay(overlays, OverlayState::LowBattery)) {
    intent.mood = Mood::Sleepy;
    intent.brow = min<uint8_t>(intent.brow, 12);
    intent.squint = max<uint8_t>(intent.squint, 8);
    intent.overlay = OverlayFx::Doze;
    intent.overlayStrength = max<uint8_t>(intent.overlayStrength, 72);
  }

  if (hasOverlay(overlays, OverlayState::RtcError)) {
    intent.mood = Mood::Confused;
    intent.brow = 26;
    intent.squint = max<uint8_t>(intent.squint, 4);
    intent.overlay = OverlayFx::Confused;
    intent.overlayStrength = max<uint8_t>(intent.overlayStrength, 70);
  }

  if (hasOverlay(overlays, OverlayState::ImuError)) {
    intent.mood = Mood::Confused;
    intent.brow = max<uint8_t>(intent.brow, 36);
    intent.squint = max<uint8_t>(intent.squint, 3);
    intent.overlay = OverlayFx::Confused;
    intent.overlayStrength = max<uint8_t>(intent.overlayStrength, 82);
  }

  if (hasOverlay(overlays, OverlayState::Charging)) {
    if (intent.mood != Mood::Confused) intent.mood = Mood::Relax;
    intent.brow = min<uint8_t>(intent.brow, 18);
    intent.squint = min<uint8_t>(intent.squint, 4);
    if (intent.overlay == OverlayFx::None || intent.overlay == OverlayFx::Focus) {
      intent.overlay = OverlayFx::Calm;
      intent.overlayStrength = max<uint8_t>(intent.overlayStrength, 52);
    }
  }

  if (hasOverlay(overlays, OverlayState::Focus) && !hasOverlay(overlays, OverlayState::LowBattery)) {
    if (intent.mood == Mood::Neutral || intent.mood == Mood::Relax) intent.mood = Mood::Neutral;
    intent.brow = max<uint8_t>(intent.brow, 34);
    intent.squint = min<uint8_t>(intent.squint, 2);
    if (intent.overlay == OverlayFx::None || intent.overlay == OverlayFx::Calm) {
      intent.overlay = OverlayFx::Focus;
      intent.overlayStrength = max<uint8_t>(intent.overlayStrength, 44);
    }
  }

  if (hasOverlay(overlays, OverlayState::Interaction) && intent.overlay == OverlayFx::None) {
    intent.overlay = OverlayFx::Startle;
    intent.overlayStrength = max<uint8_t>(intent.overlayStrength, 28);
  }
}

MoodIntent composeVisualIntent(
  const MoodIntent& phaseIntent,
  OverlayState overlays,
  const GestureOverride& gestureOverride,
  bool clockMenuActive,
  bool motionAlert
) {
  MoodIntent visualIntent = phaseIntent;
  applyOverlayIntent(visualIntent, overlays);
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
