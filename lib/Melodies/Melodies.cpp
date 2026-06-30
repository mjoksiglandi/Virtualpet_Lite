#include "Melodies.h"

namespace Melodies {

static uint8_t  g_gpio = 255;
static uint8_t  g_ch   = 0;
static uint8_t  g_res  = 10;

static const Note* g_seq = nullptr;
static uint16_t g_len = 0;
static uint16_t g_idx = 0;

static uint32_t g_nextMs = 0;
static bool g_playing = false;

static const uint16_t T = 200;   // 200 ms “corto”
static const uint16_t L = 400;   // 2T “largo”

static const uint16_t G4 = 392;
static const uint16_t F4 = 349;
static const uint16_t E4 = 330;
static const uint16_t B3 = 247;

// ---- melodías editables acá (sin tocar tu UI) ----

// cambio suave
static const Note SEQ_PHASE_TICK[] = {
  { 2200, 35 }, { 0, 25 }
};

// cambio marcado
static const Note SEQ_PHASE_DOUBLE[] = {
  { 1800, 40 }, { 0, 35 }, { 2000, 45 }, { 0, 25 }
};

// entrada a noche / alerta suave
static const Note SEQ_PHASE_ALERT[] = {
  { 1200, 70 }, { 0, 35 }, { 900, 85 }, { 0, 35 }
};

// “boot” corto
static const Note SEQ_BOOT[] = {
  { 523, 70 }, { 659, 70 }, { 784, 90 }, { 0, 40 }, { 988, 120 }
};

// “sonidito tipo salida del cole” (inspiración chime, no 1:1 para evitar líos)
// Ajusta libre: frecuencias y tiempos.
static const Note SEQ_SCHOOL_18[] = {
  // change 2
  { E4, T }, { G4, T }, { F4, T }, { B3, L },
  // change 3
  { E4, T }, { F4, T }, { G4, T }, { E4, L },
  // change 4
  { G4, T }, { E4, T }, { F4, T }, { B3, L },
  // change 5
  { B3, T }, { F4, T }, { G4, T }, { E4, L },
};

static void ledcTone(uint16_t freq) {
  if (g_gpio == 255) return;
  if (freq == 0) {
    ledcWriteTone(g_gpio, 0);
    ledcWrite(g_gpio, 0);
  } else {
    ledcWriteTone(g_gpio, freq);
    // duty medio (depende del buzzer, ajustable)
    ledcWrite(g_gpio, (1 << g_res) / 2);
  }
}

void begin(uint8_t gpioBuzzer, uint8_t ledcChannel, uint16_t baseResolutionBits) {
  g_gpio = gpioBuzzer;
  g_ch   = ledcChannel;
  g_res  = (uint8_t)baseResolutionBits;

  ledcAttachChannel(g_gpio, 2000, g_res, g_ch);
  stop();
}

void stop() {
  g_seq = nullptr;
  g_len = 0;
  g_idx = 0;
  g_playing = false;
  ledcTone(0);
}

bool isPlaying() { return g_playing; }

void play(Tune t) {
  switch (t) {
    case Tune::PhaseTick: g_seq = SEQ_PHASE_TICK; g_len = sizeof(SEQ_PHASE_TICK)/sizeof(SEQ_PHASE_TICK[0]); break;
    case Tune::PhaseDouble: g_seq = SEQ_PHASE_DOUBLE; g_len = sizeof(SEQ_PHASE_DOUBLE)/sizeof(SEQ_PHASE_DOUBLE[0]); break;
    case Tune::PhaseAlert: g_seq = SEQ_PHASE_ALERT; g_len = sizeof(SEQ_PHASE_ALERT)/sizeof(SEQ_PHASE_ALERT[0]); break;
    case Tune::SchoolChime18: g_seq = SEQ_SCHOOL_18; g_len = sizeof(SEQ_SCHOOL_18)/sizeof(SEQ_SCHOOL_18[0]); break;
    case Tune::Boot: g_seq = SEQ_BOOT; g_len = sizeof(SEQ_BOOT)/sizeof(SEQ_BOOT[0]); break;
    default: g_seq = nullptr; g_len = 0; break;
  }

  g_idx = 0;
  g_playing = (g_seq && g_len);
  g_nextMs = 0; // fuerza disparo inmediato en tick()
}

void beep(uint16_t freqHz, uint16_t durMs, uint16_t gapMs) {
  static Note tmp[2];
  tmp[0] = { freqHz, durMs };
  tmp[1] = { 0, gapMs };
  g_seq = tmp;
  g_len = 2;
  g_idx = 0;
  g_playing = true;
  g_nextMs = 0;
}

void tick() {
  if (!g_playing || !g_seq || g_len == 0) return;

  uint32_t now = millis();
  if (g_nextMs != 0 && (int32_t)(now - g_nextMs) < 0) return;

  if (g_idx >= g_len) {
    stop();
    return;
  }

  const Note& n = g_seq[g_idx++];
  ledcTone(n.freq);
  g_nextMs = now + n.durMs;
}

} // namespace Melodies
