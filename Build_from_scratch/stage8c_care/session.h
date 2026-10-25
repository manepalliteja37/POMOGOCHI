// session.h - Pomodoro engine: presets, focus, break, cycle dots (Stage 1 timer logic, locked)
// Presets 1 / 2 (tests, no break, never count) / 25 (-> 5) / 50 (-> 10).
// Every 4th counted focus -> 15-min long break. Break auto-starts (or waits for OK when
// Break auto-start is off on the web page). After a break, Setup waits for OK.
// A reboot mid-session loses that session (spec).
#pragma once
#include <Arduino.h>
#include "appdata.h"
#include "gfx.h"
#include "petrules.h"

struct Timer {                       // unchanged from Stage 1
  uint32_t durationMs = 0, startMs = 0, accMs = 0;
  bool running = false, active = false;
  void start(uint32_t dur) { durationMs = dur; accMs = 0; startMs = millis(); running = true; active = true; }
  void pause()  { if (running) { accMs += millis() - startMs; running = false; } }
  void resume() { if (active && !running) { startMs = millis(); running = true; } }
  void stop()   { running = false; active = false; }
  uint32_t elapsed() const { return accMs + (running ? (millis() - startMs) : 0); }
  uint32_t remaining() const { uint32_t e = elapsed(); return e >= durationMs ? 0 : durationMs - e; }
  bool done() const { return active && elapsed() >= durationMs; }
};
enum SessKind : uint8_t { S_NONE, S_FOCUS, S_BREAK };

const uint8_t PRESETS[4] = { 1, 2, 25, 50 };
Timer    timer;
SessKind sess = S_NONE;
bool     longBreak = false;
uint8_t  breakMins = 0;
bool     fastMode = false;           // Serial 'f': 1 minute = 1 second (testing)
bool     uiHeader = false;           // focus/break header needs a redraw (pause, prompt)
bool     confirmShown = false;       // "BACK again = cancel" visible

uint32_t unitMs()        { return fastMode ? 1000UL : 60000UL; }
uint8_t  presetMin(uint8_t i) { return PRESETS[i & 3]; }
bool     sessionActive() { return sess != S_NONE; }
bool     breakWaiting()  { return sess == S_BREAK && !timer.running && timer.elapsed() == 0; }
int      sessionLeftSec(){ return sess == S_NONE ? 0 : (int)((timer.remaining() + 999) / 1000); }
uint8_t  breakAfter(uint8_t mins) { if (mins < 25) return 0; if (settings.cycleDots >= 3) return 15; return mins >= 50 ? 10 : 5; }

void sessionEnd() {                  // back to Setup (it waits for OK)
  timer.stop(); sess = S_NONE; confirmShown = false;
  if (longBreak) { settings.cycleDots = 0; markDirty(DIRTY_SET); }   // ●●●● empties after the long break
  longBreak = false;
  if (hbWanted) soundFocusEnd();
  goScreen(SC_SETUP);
}

void focusStart(uint8_t idx) {
  if (settings.lastPreset != idx) { settings.lastPreset = idx; markDirty(DIRTY_SET); }
  sess = S_FOCUS; confirmShown = false;
  timer.start(presetMin(idx) * unitMs());
  soundFocusBegin();
  goScreen(SC_FOCUS);
  Serial.printf("[focus] start %u min%s\n", presetMin(idx), fastMode ? " (FAST)" : "");
}

void breakStart(uint8_t mins, bool isLong) {
  sess = S_BREAK; breakMins = mins; longBreak = isLong; confirmShown = false;
  timer.start(mins * unitMs());
  if (!settings.breakAuto) timer.pause();          // waits for OK
  goScreen(SC_BREAK);
  Serial.printf("[break] %u min%s%s\n", mins, isLong ? " LONG" : "", settings.breakAuto ? "" : " (waiting for OK)");
}

void focusComplete() {
  uint8_t m = presetMin(settings.lastPreset);
  soundFocusEnd(); playChime(1);                   // locked "Focus done" chime
  Serial.printf("[focus] complete %u min (elapsed %lu ms)\n", m, (unsigned long)timer.elapsed());
  if (m < 25) { petOnTestDone(m); sessionEnd(); return; }
  settings.todayCount++; settings.cycleDots++; markDirty(DIRTY_SET);
  petOnFocusDone(m);
  if (settings.cycleDots >= 4) breakStart(15, true);
  else breakStart(m >= 50 ? 10 : 5, false);
}

void breakComplete() { playChime(3); petOnBreakDone(); Serial.println("[break] done"); sessionEnd(); }

void focusPauseToggle() {
  if (timer.running) { timer.pause(); soundFocusEnd(); Serial.println("[focus] paused"); }
  else { timer.resume(); soundFocusResume(); Serial.println("[focus] resumed"); }
  uiHeader = true;
}
void breakPauseToggle() {
  if (timer.running) timer.pause(); else timer.resume();
  uiHeader = true;
}

void focusCancel() { uint32_t el = timer.elapsed(); soundFocusEnd(); petOnFocusCancel(el); Serial.println("[focus] CANCELLED"); sessionEnd(); }
void breakSkip()   { petOnBreakSkip(); Serial.println("[break] SKIPPED"); sessionEnd(); }

void sessionTick() {
  if (sess == S_NONE || !timer.done()) return;
  if (sess == S_FOCUS) focusComplete(); else breakComplete();
}
