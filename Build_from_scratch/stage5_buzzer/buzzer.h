// buzzer.h - non-blocking sound module (Stage 5, moves into the final firmware as-is)
// Passive buzzer on D6 via BC547. Volume by ear on this buzzer: duty 50% soft / 18% mid / 6% loud.
// Sound plan: OK -> Focus start bell -> soft heartbeat loop -> Focus done chime.
//
// IMPORTANT: D6 = GPIO12 = hardware SPI MISO. Call buzzerBegin() AFTER tft.initR()/SPI.begin(),
// otherwise SPI keeps the pin and the buzzer stays silent. The display never reads, so MISO is not needed.
//
// Use: buzzerBegin(); call buzzerTick() every loop(); playChime(i); startHeartbeat(); stopSound(); state: muted, volume (0-2), hbSel
#pragma once
#include <Arduino.h>
#include <core_esp8266_waveform.h>

#define BUZZER D6

struct Note  { uint16_t hz; uint16_t ms; };          // hz 0 = rest, ms 0 = end
struct Chime { const char* name; const Note* notes; bool always; };   // always = plays even when muted
struct HbVar { const char* name; uint16_t lub; uint16_t dub; uint16_t gapMs; };

const Note CH_START[]  = {{659,260},{0,140},{523,420}, {0,0}};                                // Breath bell (5b, liked)
const Note CH_DONE[]   = {{1047,110},{0,20},{1319,110},{0,20},{1568,110},{0,20},{2093,320}, {0,0}};   // locked
const Note CH_BREAK[]  = {{1319,160},{0,40},{1047,260}, {0,0}};
const Note CH_WAKE[]   = {{784,120},{0,30},{1047,120},{0,30},{1319,120},{0,30},{1568,120},{0,60},{1568,90},{0,40},{2093,260}, {0,0}};
const Note CH_HATCH[]  = {{1568,60},{0,40},{1568,60},{0,160},{1047,90},{1319,90},{1568,90},{2093,90},{1568,90},{2093,350}, {0,0}};
const Note CH_STAGE[]  = {{784,120},{1047,120},{1319,120},{1568,240},{0,60},{1319,120},{1568,420}, {0,0}};
const Note CH_MUTE[]   = {{1047,80},{0,30},{523,140}, {0,0}};
const Note CH_UNMUTE[] = {{523,80},{0,30},{1047,140}, {0,0}};

const Chime CHIMES[] = {
  {"Focus start", CH_START,  false},
  {"Focus done",  CH_DONE,   false},
  {"Break start", CH_BREAK,  false},
  {"Break done",  CH_WAKE,   false},
  {"Hatch",       CH_HATCH,  false},
  {"Stage up",    CH_STAGE,  false},
  {"Mute",        CH_MUTE,   true},
  {"Unmute",      CH_UNMUTE, true},
};
const uint8_t N_CHIMES = sizeof(CHIMES) / sizeof(CHIMES[0]);

// Heartbeat variants: deeper = further from the buzzer's loud zone = softer + warmer
const HbVar HB[] = {
  {"432 Hz 50bpm",       432, 384,  975},
  {"216 Hz deep 50bpm",  216, 192,  975},   // 432 one octave down
  {"528 Hz 50bpm",       528, 470,  975},
  {"264 Hz deep 50bpm",  264, 235,  975},   // 528 one octave down
  {"216 Hz deep 42bpm",  216, 192, 1250},   // slower, calmer
};
const uint8_t N_HB = sizeof(HB) / sizeof(HB[0]);
uint8_t hbSel = 0;
Note hbNotes[5];

const uint8_t VOL_DUTY[] = { 50, 18, 6 };     // soft / mid / loud (inverted on this buzzer)
const char*   VOL_NAME[] = { "soft", "mid", "loud" };
const uint8_t HB_DUTY = 50;                   // heartbeat always soft
bool    muted = false;
uint8_t volume = 0;

const Note* curMel = nullptr;
bool     curLoop = false;
uint8_t  curDuty = 50;
uint8_t  noteIdx = 0;
uint32_t noteStart = 0;
uint32_t noteLateMax = 0;

void buzzerBegin() { pinMode(BUZZER, OUTPUT); digitalWrite(BUZZER, LOW); }

void buzzerOff() { stopWaveform(BUZZER); digitalWrite(BUZZER, LOW); }

void buzzerTone(uint16_t hz, uint8_t duty) {
  if (!hz) { buzzerOff(); return; }
  uint32_t period = 1000000UL / hz;
  uint32_t hi = period * duty / 100; if (hi < 1) hi = 1;
  startWaveform(BUZZER, hi, period - hi, 0);
}

void startNote() { noteStart = millis(); buzzerTone(curMel[noteIdx].hz, curDuty); }

void playMel(const Note* m, bool loop, uint8_t duty) { curMel = m; curLoop = loop; curDuty = duty; noteIdx = 0; startNote(); }

void playChime(uint8_t i) {
  if (muted && !CHIMES[i].always) { Serial.printf("(muted) %s skipped\n", CHIMES[i].name); return; }
  playMel(CHIMES[i].notes, false, VOL_DUTY[volume]);
}

void buildHeartbeat() {
  const HbVar& v = HB[hbSel];
  hbNotes[0] = {v.lub, 45}; hbNotes[1] = {0, 120}; hbNotes[2] = {v.dub, 60}; hbNotes[3] = {0, v.gapMs}; hbNotes[4] = {0, 0};
}
void startHeartbeat() { buildHeartbeat(); if (!muted) playMel(hbNotes, true, HB_DUTY); }
void stopSound() { curMel = nullptr; buzzerOff(); }

bool buzzerBusy() { return curMel != nullptr && !curLoop; }

void buzzerTick() {
  if (!curMel) return;
  uint32_t el = millis() - noteStart;
  if (el < curMel[noteIdx].ms) return;
  uint32_t late = el - curMel[noteIdx].ms;
  if (late > noteLateMax) noteLateMax = late;
  noteIdx++;
  if (curMel[noteIdx].ms == 0) {
    if (curLoop) noteIdx = 0;
    else { curMel = nullptr; buzzerOff(); return; }
  }
  startNote();
}
// =====================================================================

