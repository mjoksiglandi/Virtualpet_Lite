#pragma once
#include <Arduino.h>

namespace Melodies {

struct Note {
  uint16_t freq;   // Hz (0 = silencio)
  uint16_t durMs;  // duración en ms
};

enum class Tune : uint8_t {
  PhaseTick = 0,
  PhaseDouble,
  PhaseAlert,
  SchoolChime18,
  Boot
};

// Inicializa buzzer (LEDC)
void begin(uint8_t gpioBuzzer, uint8_t ledcChannel = 0, uint16_t baseResolutionBits = 10);

// Llamar en loop() para avanzar la reproducción sin bloquear
void tick();

// Reproduce una melodía (no bloqueante). Si ya hay una sonando, la reemplaza.
void play(Tune t);

// Para cualquier sonido ahora mismo
void stop();

// Útil para “beep” corto
void beep(uint16_t freqHz, uint16_t durMs, uint16_t gapMs = 0);

bool isPlaying();

} // namespace Melodies
