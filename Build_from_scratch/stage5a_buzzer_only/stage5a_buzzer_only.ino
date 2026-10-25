// Stage 5a - BUZZER ONLY (no display, no buttons)
// Board: NodeMCU 1.0. Only the buzzer circuit is needed:
//   D6 -> 1k -> BC547 base | emitter -> GND | buzzer + -> 3V3 (or VIN 5V), buzzer - -> collector
//   1N4148 across the buzzer (stripe/cathode to the + side)
//
// Serial @115200, send one letter:
//   d  DC test: D6 HIGH for 1 s     (active buzzer beeps here, passive only clicks)
//   b  bit-bang 2 kHz for 1 s       (pure digitalWrite, no PWM at all: proves the wiring)
//   t  tone() 2 kHz for 1 s         (Arduino core tone)
//   1  waveform 2 kHz, 6 % duty  (low)
//   2  waveform 2 kHz, 18 % duty (mid)
//   3  waveform 2 kHz, 50 % duty (high)
//   s  sweep 500 Hz -> 4 kHz        (find the loudest pitch of your buzzer)
//   c  "focus done" chime (non-blocking note player)
//   ?  help

#include <core_esp8266_waveform.h>

#define BUZZER D6

// ---- structs first (Arduino auto-prototypes) ----
struct Note { uint16_t hz; uint16_t ms; };   // hz 0 = rest, ms 0 = end

const Note CH_DONE[] = {{1047,110},{0,20},{1319,110},{0,20},{1568,110},{0,20},{2093,320},{0,0}};

const Note* mel = nullptr;
uint8_t  idx = 0;
uint32_t noteStart = 0;
uint8_t  duty = 50;

void buzzOff() { stopWaveform(BUZZER); digitalWrite(BUZZER, LOW); }

void buzzWave(uint16_t hz, uint8_t dutyPct) {
  if (!hz) { buzzOff(); return; }
  uint32_t period = 1000000UL / hz;
  uint32_t hi = period * dutyPct / 100; if (hi < 1) hi = 1;
  startWaveform(BUZZER, hi, period - hi, 0);
}

void melodyStart(const Note* m) { mel = m; idx = 0; noteStart = millis(); buzzWave(mel[0].hz, duty); }

void melodyTick() {
  if (!mel) return;
  if (millis() - noteStart < mel[idx].ms) return;
  idx++;
  if (mel[idx].ms == 0) { mel = nullptr; buzzOff(); Serial.println("chime done"); return; }
  noteStart = millis(); buzzWave(mel[idx].hz, duty);
}

void help() {
  Serial.println("\nStage 5a buzzer only: d DC, b bit-bang, t tone(), 1/2/3 duty low/mid/high, s sweep, c chime");
}

void setup() {
  pinMode(BUZZER, OUTPUT); digitalWrite(BUZZER, LOW);
  Serial.begin(115200); delay(100);
  help();
}

void loop() {
  melodyTick();
  if (!Serial.available()) { yield(); return; }
  char c = Serial.read();
  switch (c) {
    case 'd': Serial.println("DC HIGH 1 s"); digitalWrite(BUZZER, HIGH); delay(1000); digitalWrite(BUZZER, LOW); break;
    case 'b': Serial.println("bit-bang 2 kHz 1 s");
      for (int i = 0; i < 2000; i++) { digitalWrite(BUZZER, HIGH); delayMicroseconds(250); digitalWrite(BUZZER, LOW); delayMicroseconds(250); if ((i & 255) == 0) yield(); }
      break;
    case 't': Serial.println("tone() 2 kHz 1 s"); tone(BUZZER, 2000); delay(1000); noTone(BUZZER); digitalWrite(BUZZER, LOW); break;
    case '1': case '2': case '3': {
      uint8_t d = (c == '1') ? 6 : (c == '2') ? 18 : 50;
      Serial.printf("waveform 2 kHz duty %u%% 1 s\n", d);
      buzzWave(2000, d); delay(1000); buzzOff(); break;
    }
    case 's': Serial.println("sweep 500 -> 2000 Hz");
      for (int f = 500; f <= 2000; f += 100) { buzzWave(f, 50); Serial.printf("%d Hz\n", f); delay(120); }
      buzzOff(); break;
    case 'c': Serial.println("chime"); duty = 50; melodyStart(CH_DONE); break;
    case '?': help(); break;
  }
}
