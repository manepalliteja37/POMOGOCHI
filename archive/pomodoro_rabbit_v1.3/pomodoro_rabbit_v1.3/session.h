// session.h - Pomodoro engine: presets, focus, break, cycle dots (Stage 1 timer logic, locked)
// Presets 1 / 2 (tests, no break, never count) / 25 (-> 5) / 50 (-> 10).
// Every 4th counted focus -> 15-min long break. Break auto-starts (or waits for OK when
// Break auto-start is off on the web page). After a break, Setup waits for OK.
// 9.3b: a focus can be linked to a to-do; when it ends the device asks "Task done?" before the break.
// 9.3b: resume after reset - the running session is mirrored to RTC memory every 0.5 s. RTC memory
//       survives a reset (laptop USB wake, crash, watchdog) but not a power cut, so only resets resume.
#pragma once
#include <Arduino.h>
#include "appdata.h"
#include "gfx.h"
#include "petrules.h"
#include "history.h"

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
struct RtcSess {                     // 56 bytes, a multiple of 4 for rtcUserMemory
  uint32_t magic; uint8_t kind, preset, breakMins, flags; uint32_t durMs, elMs;
  char link[33]; uint8_t pad[3]; uint32_t crc;
};
#define RTC_MAGIC 0x50524142UL       // "PRAB"
#define RTC_SLOT  32                 // rtcUserMemory block (4-byte blocks)

const uint8_t PRESETS[4] = { 1, 2, 25, 50 };
Timer    timer;
SessKind sess = S_NONE;
bool     longBreak = false;
uint8_t  breakMins = 0;
bool     fastMode = false;           // Serial 'f': 1 minute = 1 second (testing)
bool     uiHeader = false;           // focus/break header needs a redraw (pause, prompt)
bool     confirmShown = false;       // "BACK again = cancel" visible
char     linkText[33] = "";          // 9.3b: the to-do this focus is for ("" = none)
int8_t   askIdx = -1;                // "Task done?" is asking about this to-do
uint8_t  askBreak = 0; bool askLong = false; int8_t askLog = -1; uint32_t askAt = 0;

uint32_t unitMs()        { return fastMode ? 1000UL : 60000UL; }
uint8_t  presetMin(uint8_t i) { return PRESETS[i & 3]; }
bool     sessionActive() { return sess != S_NONE; }
bool     breakWaiting()  { return sess == S_BREAK && !timer.running && timer.elapsed() == 0; }
int      sessionLeftSec(){ return sess == S_NONE ? 0 : (int)((timer.remaining() + 999) / 1000); }
uint8_t  breakAfter(uint8_t mins) { if (mins < 25) return 0; if (settings.cycleDots >= 3) return 15; return mins >= 50 ? 10 : 5; }

int8_t findLinked() {                // the open to-do this focus belongs to (found by text: web edits can move rows)
  if (!linkText[0]) return -1;
  for (uint8_t i = 0; i < todos.count; i++) if (!todos.item[i].done && !strcmp(todos.item[i].text, linkText)) return i;
  return -1;
}

void sessionEnd() {                  // back to Setup (it waits for OK)
  timer.stop(); sess = S_NONE; confirmShown = false; linkText[0] = 0;
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
  Serial.printf("[focus] start %u min%s%s%s\n", presetMin(idx), fastMode ? " (FAST)" : "", linkText[0] ? " for: " : "", linkText);
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
  int8_t ti = findLinked();
  int8_t lr = logFocus(m, ti >= 0 ? todos.item[ti].text : nullptr);
  if (ti >= 0) { if (todos.item[ti].focus < 255) todos.item[ti].focus++; markDirty(DIRTY_TODO); }
  uint8_t bm = 0; bool lg = false;
  if (m < 25) petOnTestDone(m);
  else {
    settings.todayCount++; settings.cycleDots++; markDirty(DIRTY_SET);
    petOnFocusDone(m);
    if (settings.cycleDots >= 4) { bm = 15; lg = true; } else bm = m >= 50 ? 10 : 5;
  }
  if (ti >= 0) {                                   // ask first, the break starts after the answer
    timer.stop(); sess = S_NONE; confirmShown = false;
    askIdx = ti; askBreak = bm; askLong = lg; askLog = lr; askAt = millis();
    goScreen(SC_ASK);
    return;
  }
  if (bm) breakStart(bm, lg); else sessionEnd();
}

void askAnswer(bool yes) {                         // "Task done?" OK = yes, BACK / 2 min = not yet
  if (askIdx >= 0 && askIdx < todos.count && yes) {
    todos.item[askIdx].done = 1; markDirty(DIRTY_TODO);
    if (askLog >= 0) { dayLog.log[askLog].done = 1; markDirty(DIRTY_DAY); }
    playChime(10);
    Serial.printf("[todo] done: %s\n", todos.item[askIdx].text);
  } else Serial.println("[todo] not yet");
  askIdx = -1; linkText[0] = 0;
  if (askBreak) breakStart(askBreak, askLong);
  else goScreen(SC_TODO);                          // test preset: straight back to the list
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

// ---- resume after reset (RTC memory) ----
void rtcSave() {
  RtcSess r; memset(&r, 0, sizeof(r));
  r.magic = RTC_MAGIC; r.kind = sess; r.preset = settings.lastPreset; r.breakMins = breakMins;
  r.flags = (longBreak ? 1 : 0) | (timer.running ? 2 : 0) | (fastMode ? 4 : 0) | (breakRewarded ? 8 : 0);
  r.durMs = timer.durationMs; r.elMs = timer.elapsed();
  strlcpy(r.link, linkText, sizeof(r.link));
  r.crc = storeCrc((const uint8_t*)&r, offsetof(RtcSess, crc));
  ESP.rtcUserMemoryWrite(RTC_SLOT, (uint32_t*)&r, sizeof(r));
}
void rtcTick() {
  static uint32_t last = 0; static bool had = false;
  if (sess == S_NONE) { if (had) { had = false; rtcSave(); } return; }    // writes kind 0 once = nothing to resume
  if (millis() - last < 500) return;
  last = millis(); had = true; rtcSave();
}
bool rtcResume() {                                 // call once in setup(), after the screen + data are ready
  RtcSess r;
  if (!ESP.rtcUserMemoryRead(RTC_SLOT, (uint32_t*)&r, sizeof(r))) return false;
  if (r.magic != RTC_MAGIC || r.crc != storeCrc((const uint8_t*)&r, offsetof(RtcSess, crc))) return false;
  if (r.kind != S_FOCUS && r.kind != S_BREAK) return false;
  if (!settings.onboarded || r.durMs == 0 || r.elMs > r.durMs + 60000UL) return false;
  bool running = r.flags & 2;
  r.link[32] = 0; strlcpy(linkText, r.link, sizeof(linkText));
  settings.lastPreset = r.preset & 3; fastMode = r.flags & 4;
  sess = (SessKind)r.kind; breakMins = r.breakMins; longBreak = r.flags & 1; breakRewarded = r.flags & 8; confirmShown = false;
  timer.durationMs = r.durMs; timer.active = true; timer.startMs = millis();
  timer.accMs = r.elMs + (running ? millis() : 0);  // the reboot time counts as focus time
  timer.running = running;
  if (sess == S_FOCUS && running) soundFocusResume();
  goScreen(sess == S_FOCUS ? SC_FOCUS : SC_BREAK);
  Serial.printf("[resume] %s resumed after reset: %lu s left%s\n", sess == S_FOCUS ? "focus" : "break",
                (unsigned long)(timer.remaining() / 1000), running ? "" : " (paused)");
  return true;
}

void sessionTick() {
  rtcTick();
  if (sess == S_NONE || !timer.done()) return;
  if (sess == S_FOCUS) focusComplete(); else breakComplete();
}
