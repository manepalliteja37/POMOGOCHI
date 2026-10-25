// Stage 5c - AMBIENT FOCUS SOUND (buzzer only, no display, no buttons)
// Same wiring as 5a/5b. Volume by ear on your buzzer: duty 50% = soft, 18% = mid.
// Each pattern LOOPS until you send 's' (the real timer plays it for the whole focus session).
//
// Serial @115200:
//   1  Breath bell loop  - 528 Hz then 432 Hz, once every ~6 s (slow breathing pace)
//   2  Heartbeat loop    - soft lub-dub at ~50 bpm (resting heart rate)
//   3  Breathing tone    - steady tone that swells 4 s in / 4 s out (volume, not pitch)
//   4  Steady drone      - plain continuous tone (just to hear it; can get tiring)
//   p  base pitch 432 / 528 Hz   (used by 2, 3, 4)
//   v  volume soft / mid
//   7  Focus done chime (locked)
//   s  stop
//
// Note: a buzzer makes a square wave, not a pure sine, so 432/528 here have
// extra overtones. They still set the "feel", but it is not studio-pure.

#include <core_esp8266_waveform.h>

#define BUZZER D6

// ---- structs first (Arduino auto-prototypes) ----
struct Note { float mul; uint16_t ms; uint8_t duty; };   // mul x basePitch (0 = rest); duty 0 = current volume; ms 0 = end

const Note P_BELL[] = { {528.0f / 432.0f, 300, 0}, {0, 160, 0}, {1.0f, 520, 0}, {0, 5000, 0}, {0, 0, 0} };   // fixed 528 -> 432
const Note P_HEART[] = { {1.0f, 45, 0}, {0, 120, 0}, {0.89f, 60, 0}, {0, 975, 0}, {0, 0, 0} };              // 1.2 s = 50 bpm
Note P_BREATH[33];                                                                                         // built in setup
const Note P_DRONE[] = { {1.0f, 1000, 0}, {0, 0, 0} };
const Note P_DONE[] = { {1047.0f, 110, 0}, {0, 20, 0}, {1319.0f, 110, 0}, {0, 20, 0}, {1568.0f, 110, 0}, {0, 20, 0}, {2093.0f, 320, 0}, {0, 0, 0} };

struct Pattern { const char* name; const Note* notes; bool loops; bool absHz; };
const Pattern PATS[] = {
  {"Breath bell 528>432", P_BELL,   true,  false},
  {"Heartbeat",           P_HEART,  true,  false},
  {"Breathing tone",      P_BREATH, true,  false},
  {"Steady drone",        P_DRONE,  true,  false},
  {"Focus done",          P_DONE,   false, true},
};

const uint8_t VOL_DUTY[] = {50, 18};
const char*   VOL_NAME[] = {"soft", "mid"};
uint8_t vol = 0;
float   basePitch = 432.0f;

const Pattern* cur = nullptr;
uint8_t  idx = 0;
uint32_t noteStart = 0;

void buzzOff() { stopWaveform(BUZZER); digitalWrite(BUZZER, LOW); }

void buzzWave(float hz, uint8_t duty) {
  if (hz < 1) { buzzOff(); return; }
  uint32_t period = (uint32_t)(1000000.0f / hz + 0.5f);
  uint32_t hi = period * duty / 100; if (hi < 1) hi = 1;
  startWaveform(BUZZER, hi, period - hi, 0);
}

void playNote() {
  const Note& n = cur->notes[idx];
  float base = cur->absHz ? 1.0f : (cur->notes == P_BELL ? 432.0f : basePitch);
  buzzWave(n.mul * base, n.duty ? n.duty : VOL_DUTY[vol]);
  noteStart = millis();
}

void start(uint8_t i) { cur = &PATS[i]; idx = 0; playNote(); Serial.printf("> %s  pitch=%.0f Hz  vol=%s\n", cur->name, basePitch, VOL_NAME[vol]); }
void stopAll() { cur = nullptr; buzzOff(); Serial.println("stopped"); }

void tick() {
  if (!cur) return;
  if (millis() - noteStart < cur->notes[idx].ms) return;
  idx++;
  if (cur->notes[idx].ms == 0) {
    if (!cur->loops) { stopAll(); return; }
    idx = 0;
  }
  playNote();
}

// breathing: 16 steps up (soft -> a bit louder), 16 steps down, 250 ms each = 8 s per breath
void buildBreath() {
  for (int i = 0; i < 16; i++) {
    uint8_t d = 50 - (i * 25) / 15;          // 50% (soft) -> 25%
    P_BREATH[i]      = {1.0f, 250, d};
    P_BREATH[31 - i] = {1.0f, 250, d};
  }
  P_BREATH[32] = {0, 0, 0};
}

void help() {
  Serial.println("\nStage 5c ambient: 1 breath bell, 2 heartbeat, 3 breathing tone, 4 drone, p pitch 432/528, v volume, 7 done, s stop");
}

void setup() {
  pinMode(BUZZER, OUTPUT); digitalWrite(BUZZER, LOW);
  Serial.begin(115200); delay(100);
  buildBreath();
  help();
}

void loop() {
  tick();
  if (!Serial.available()) { yield(); return; }
  char c = Serial.read();
  switch (c) {
    case '1': start(0); break;
    case '2': start(1); break;
    case '3': start(2); break;
    case '4': start(3); break;
    case '7': start(4); break;
    case 'p': basePitch = (basePitch == 432.0f) ? 528.0f : 432.0f; Serial.printf("Base pitch %.0f Hz\n", basePitch);
              if (cur && cur->loops) { idx = 0; playNote(); } break;
    case 'v': vol = (vol + 1) % 2; Serial.printf("Volume %s\n", VOL_NAME[vol]); break;
    case 's': stopAll(); break;
    case '?': help(); break;
  }
}
