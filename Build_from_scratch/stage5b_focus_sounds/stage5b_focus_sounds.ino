// Stage 5b - FOCUS SOUNDS (buzzer only, no display, no buttons)
// Same wiring as Stage 5a: D6 -> 1k -> BC547, passive buzzer, 1N4148 across it.
//
// Your buzzer, measured in 5a: duty 50% = quietest, 6% = loudest; it fades above ~2 kHz.
// So volume is mapped by ear:  soft = 50%, mid = 18%, loud = 6%. All tunes stay at or below 2.1 kHz.
//
// Serial @115200:
//   1  Water drop    - one falling drip (1100 -> 600 Hz glide)
//   2  Breath bell   - two slow notes, E5 then C5
//   3  Heartbeat     - two low soft taps (lub-dub)
//   4  Wind chime    - three light random notes from a calm scale
//   5  Halfway cue   - gentle rising pair (plays once at the middle of a session)
//   6  Last-minute   - three soft drops (1 minute left)
//   7  Focus done    - your approved chime from 5a
//   v  volume soft / mid / loud (focus sounds should be soft)
//   r  focus simulation ON/OFF: the selected sound (1-4) repeats every 8-12 s at random
//      (the real timer will use 45-75 s; random so your brain stops expecting it)
//   ?  help

#include <core_esp8266_waveform.h>

#define BUZZER D6

// ---- structs first (Arduino auto-prototypes) ----
struct Note { uint16_t hz; uint16_t ms; };   // hz 0 = rest, ms 0 = end

// a glide is many 12 ms steps -> sounds like a smooth drip
const Note S_DROP[]   = {{1100,12},{1020,12},{950,12},{880,12},{820,12},{760,14},{700,16},{650,18},{600,22},{0,0}};
const Note S_BELL[]   = {{659,260},{0,140},{523,420},{0,0}};
const Note S_HEART[]  = {{392,45},{0,110},{349,60},{0,0}};
const Note S_HALF[]   = {{523,180},{0,90},{659,260},{0,0}};
const Note S_LAST[]   = {{1100,12},{950,12},{820,14},{700,18},{0,250},
                         {1100,12},{950,12},{820,14},{700,18},{0,250},
                         {1100,12},{950,12},{820,14},{700,18},{0,0}};
const Note S_DONE[]   = {{1047,110},{0,20},{1319,110},{0,20},{1568,110},{0,20},{2093,320},{0,0}};

// wind chime is built at random from a pentatonic scale (no clashing notes)
const uint16_t PENTA[] = {523, 587, 659, 784, 880, 1047};
Note windBuf[7];

struct Sound { const char* name; const Note* notes; };
const Sound SOUNDS[] = {
  {"Water drop", S_DROP}, {"Breath bell", S_BELL}, {"Heartbeat", S_HEART}, {"Wind chime", windBuf},
  {"Halfway cue", S_HALF}, {"Last-minute", S_LAST}, {"Focus done", S_DONE},
};

const uint8_t VOL_DUTY[] = {50, 18, 6};
const char*   VOL_NAME[] = {"soft", "mid", "loud"};
uint8_t vol = 0;

const Note* mel = nullptr;
uint8_t  idx = 0;
uint32_t noteStart = 0;
uint8_t  sel = 0;
bool     sim = false;
uint32_t nextSim = 0;

void buzzOff() { stopWaveform(BUZZER); digitalWrite(BUZZER, LOW); }

void buzzWave(uint16_t hz) {
  if (!hz) { buzzOff(); return; }
  uint32_t period = 1000000UL / hz;
  uint32_t hi = period * VOL_DUTY[vol] / 100; if (hi < 1) hi = 1;
  startWaveform(BUZZER, hi, period - hi, 0);
}

void makeWind() {
  for (int i = 0; i < 3; i++) {
    windBuf[i * 2]     = { PENTA[random(6)], (uint16_t)(70 + random(50)) };
    windBuf[i * 2 + 1] = { 0, (uint16_t)(120 + random(200)) };
  }
  windBuf[6] = {0, 0};
}

void playSound(uint8_t i) {
  if (i == 3) makeWind();
  Serial.printf("> %s (%s)\n", SOUNDS[i].name, VOL_NAME[vol]);
  mel = SOUNDS[i].notes; idx = 0; noteStart = millis(); buzzWave(mel[0].hz);
}

void melodyTick() {
  if (!mel) return;
  if (millis() - noteStart < mel[idx].ms) return;
  idx++;
  if (mel[idx].ms == 0) { mel = nullptr; buzzOff(); return; }
  noteStart = millis(); buzzWave(mel[idx].hz);
}

void help() {
  Serial.println("\nStage 5b focus sounds: 1 drop, 2 bell, 3 heartbeat, 4 wind, 5 halfway, 6 last-minute, 7 done, v volume, r simulation");
}

void setup() {
  pinMode(BUZZER, OUTPUT); digitalWrite(BUZZER, LOW);
  Serial.begin(115200); delay(100);
  randomSeed(ESP.getCycleCount());
  help();
}

void loop() {
  melodyTick();
  if (sim && !mel && (int32_t)(millis() - nextSim) >= 0) {
    playSound(sel);
    nextSim = millis() + 8000 + random(4000);
  }
  if (!Serial.available()) { yield(); return; }
  char c = Serial.read();
  if (c >= '1' && c <= '7') { uint8_t i = c - '1'; if (i <= 3) sel = i; playSound(i); }
  else if (c == 'v') { vol = (vol + 1) % 3; Serial.printf("Volume %s\n", VOL_NAME[vol]); }
  else if (c == 'r') {
    sim = !sim; nextSim = millis() + 2000;
    Serial.printf("Focus simulation %s with %s\n", sim ? "ON" : "OFF", SOUNDS[sel].name);
  }
  else if (c == '?') help();
}
