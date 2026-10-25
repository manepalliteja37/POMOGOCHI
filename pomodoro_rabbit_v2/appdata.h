// appdata.h - everything the device remembers + dirty-flag saving (Stage 7)
//
// Three save files through storage.h (A/B slots, CRC, data version):
//   settings v3, pet v2 (6a layout + name at the END), todos v2 (9.3b: 12 slots, carry + focus count),
//   day v1 (today's focus log), hist v1 (index of the last 30 day sheets in /d/YYYYMMDD.json)
// Rule (6a lock): saves are dirty-flag only, never every loop. During focus they wait for the
// heartbeat's long quiet gap, so a slow LittleFS write (up to ~390 ms) can't stretch a beat.
// Pet/streak RULES (rewards, penalties, decay) live in petrules.h. Here we only store + save them.
#pragma once
#include <Arduino.h>
#include "storage.h"
#include "clock.h"
#include "buzzer.h"

// ---- all structs/enums at the top ----
struct Settings {                // same layout as 6b; v3 only changes what "volume" means
  uint8_t  lastPreset = 1;
  uint8_t  muted = 0;
  uint8_t  volume = 3;           // v3: level 1-10 (v2 files stored 0 soft / 1 mid / 2 loud)
  uint8_t  hbSel = 3;            // 264 Hz deep 50bpm
  uint16_t backlight = 1023;
  uint8_t  breakAuto = 1;
  uint8_t  onboarded = 0;
  uint8_t  todayCount = 0;
  uint8_t  cycleDots = 0;
  uint32_t todayDate = 0;        // yyyymmdd of the last day we saw
  uint32_t bootCount = 0;
  uint8_t  tortureOn = 0;        // 6a test leftover (kept for layout)
  uint32_t festivalDay = 0;      // b4 (settings v4): yyyymmdd of a no-penalty festival day (was the 6a cutsCaught leftover)
  uint8_t  offlineTest = 0;      // 6b test leftover
  uint32_t rollovers = 0;        // 6b test leftover
  uint8_t  focusDigits = 1;      // b6 (settings v5): show the mm:ss under the hourglass during focus
  // b11 (settings v6): ntfy phone alerts. Old saves are shorter, so these keep their defaults.
  char     ntfyTopic[33] = "";   // empty = alerts off. Never exported.
  uint8_t  ntfyFlags = 7;        // bit0 break over, bit1 hungry, bit2 evening summary
  uint8_t  summaryHour = 21;     // 18..23
  uint8_t  ntHungryMday = 0;     // day of month the hungry alert went out (once a day)
  uint8_t  ntSummaryMday = 0;    // day of month the summary went out
};

struct PetSave {                 // 6a layout (v1) + name at the end = v2
  uint8_t  stage = 0;
  uint16_t growth = 0;
  uint8_t  fullness = 50, energy = 60, love = 60;   // b7: same as the 8 am fresh day (FRESH_F/E/L)
  uint8_t  moodFlags = 0;        // bit0 worried, bit1 sick
  uint8_t  sickDays = 0;
  uint16_t streakCur = 0, streakBest = 0;
  uint8_t  shield = 0;
  uint8_t  last7 = 0;            // bit0 = today, bit6 = 6 days ago
  uint32_t lastStreakDay = 0;    // yyyymmdd
  uint32_t tortureWrites = 0;    // 6a test leftover
  // Stage 8b: daily counters + carrots live in the old 16 reserved bytes (same file layout, old saves load as zeros)
  uint8_t  carrots = 0;          // food items, max 9, fed from Home (care menu in 8c)
  uint8_t  rewardedToday = 0;    // sessions that paid a reward today (max 4)
  uint8_t  growthToday = 0;      // Growth earned today (cap 8)
  uint8_t  cancelsToday = 0;     // cancels after the 2-min grace today
  uint8_t  cureCount = 0;        // completed sessions while Sick
  uint8_t  pendingEvent = 0;     // 1 hatch, 2 stage up: Home plays it next time it is shown
  uint8_t  petsToday = 0;        // 8c: Pet actions used today (max 3)
  uint8_t  decayHour = 0;        // v1.1: last clock hour the daytime hunger/loneliness was checked
  uint8_t  decayInit = 0;        // v1.1: 0 = first boot on v1.1 (start counting from now, no catch-up)
  uint8_t  morningDay = 0;       // v1.1-b2: day-of-month of the last 8 am fresh start (0 = never)
  // b12 rewards shop (the old 6 reserved bytes; old saves load as 0)
  uint16_t stars = 0;            // shop currency: 25 min = 1, 50 min = 2, task done +1 (max 8 a day)
  uint16_t owned = 0;            // bit 0-5 hats, bit 8-11 scenes
  uint8_t  wear = 0;             // low nibble = hat + 1, high nibble = scene + 1 (0 = none)
  uint8_t  starsToday = 0;
  char     name[12] = "Bunny";   // NEW in v2
};

// 9.3b: up to 7 OPEN tasks; tasks done today stay (web "Done today") until midnight -> 12 slots
#define TODO_MAX      12
#define TODO_OPEN_MAX 7
struct TodoItem { char text[33]; uint8_t done; uint8_t carry; uint8_t focus; };   // carry = days carried over, focus = sessions spent
struct TodoList { uint8_t count = 0; TodoItem item[TODO_MAX]; };
struct LogE    { uint16_t minOfDay; uint8_t mins; uint8_t done; char task[33]; uint8_t pad; };   // one finished focus
#define LOG_MAX 12
struct DayLog  { uint32_t ymd = 0; uint16_t focusMin = 0; uint8_t n25 = 0, n50 = 0, nTest = 0, nLog = 0; uint8_t feeds = 0, plays = 0; LogE log[LOG_MAX]; };   // b6: feeds/plays today (were pad bytes)
struct HistE   { uint32_t ymd; uint16_t mins; uint8_t sessions, done, open, pad; };
#define HIST_MAX 30
struct HistIdx { uint8_t n = 0; uint8_t pad[3] = {0}; HistE e[HIST_MAX]; };

enum DirtyBit : uint8_t { DIRTY_SET = 1, DIRTY_PET = 2, DIRTY_TODO = 4, DIRTY_DAY = 8, DIRTY_HIST = 16 };

// ---- data ----
Settings settings;
PetSave  pet;
TodoList todos;
Blob setBlob  = { "settings", 6, &settings, sizeof(settings) };
Blob petBlob  = { "pet",      2, &pet,      sizeof(pet) };
DayLog   dayLog;
HistIdx  hist;
Blob todoBlob = { "todos",    2, &todos,    sizeof(todos) };
static_assert(sizeof(TodoList) <= STORE_MAX && sizeof(DayLog) <= STORE_MAX && sizeof(HistIdx) <= STORE_MAX, "blob too big");
Blob dayBlob  = { "day",      1, &dayLog,   sizeof(dayLog) };
Blob histBlob = { "hist",     1, &hist,     sizeof(hist) };

bool     hbWanted = false;       // a focus is running (not paused): heartbeat should loop
bool     bellPending = false;    // a chime is playing; heartbeat (re)starts after it
bool     onboardRequest = false; // web "Replay onboarding"
bool     uiPet = true;           // pet/streak changed -> Home redraws its parts
uint8_t  dirty = 0;
uint32_t dirtyMs = 0;
uint32_t rev = 1;                // bumps on every change; the web page re-renders when it moves
uint8_t  todoSel = 0;            // highlighted row on the device
bool     uiTodo = true;
uint32_t loopMaxUs = 0;

bool festivalOn(uint32_t ymd) { return ymd && settings.festivalDay == ymd; }   // b4
bool festivalToday() { return festivalOn(clockToday()); }

const char* STAGE_NAME[] = { "Egg", "Baby", "Kid", "Teen", "Adult" };

uint8_t openCount() { uint8_t n = 0; for (uint8_t i = 0; i < todos.count; i++) n += !todos.item[i].done; return n; }
uint8_t doneCount() { return todos.count - openCount(); }
int8_t  openAt(uint8_t k) { for (uint8_t i = 0; i < todos.count; i++) if (!todos.item[i].done && !k--) return i; return -1; }

void markDirty(uint8_t bits) {
  dirty |= bits; dirtyMs = millis(); rev++;
  if (bits & DIRTY_PET) uiPet = true;
  if (bits & DIRTY_TODO) { uiTodo = true; uint8_t n = openCount(); if (todoSel >= n) todoSel = n ? n - 1 : 0; }
}

uint8_t  stageOf(uint16_t g) { return g == 0 ? 0 : g < 10 ? 1 : g < 40 ? 2 : g < 100 ? 3 : 4; }
uint16_t nextStageAt(uint8_t s) { static const uint16_t N[] = { 1, 10, 40, 100, 0 }; return N[s]; }

const char* moodName() {
  if (pet.moodFlags & 2) return "Sick";
  if (pet.moodFlags & 1) return "Worried";
  if (pet.fullness < 30) return "Hungry";
  if (pet.energy < 30)   return "Tired";
  if (pet.love < 30)     return "Sad";
  return "Happy";
}

// ---- loading ----
void loadOne(Blob& b) {
  bool ok = storeLoad(b);
  if (!ok) Serial.printf("[load] %s: no save yet, defaults\n", b.name);
  else if (b.loadedVer < b.ver) Serial.printf("[load] %s v%u upgraded to v%u (old data kept)\n", b.name, b.loadedVer, b.ver);
  if (b.badSlots) Serial.printf("[load] %s: %u broken slot ignored\n", b.name, b.badSlots);
}

void dataBegin() {
  loadOne(setBlob); loadOne(petBlob); loadOne(todoBlob);
  if (todoBlob.loadedVer == 1) {                               // v1 file: 7 x {text[33], done} -> v2 (raw bytes still in storeTmp)
    TodoList nl; memset(&nl, 0, sizeof(nl));
    nl.count = min((int)storeTmp[0], 7);
    for (uint8_t i = 0; i < nl.count; i++) { memcpy(nl.item[i].text, storeTmp + 1 + i * 34, 33); nl.item[i].done = storeTmp[1 + i * 34 + 33] ? 1 : 0; }
    todos = nl; markDirty(DIRTY_TODO);
    Serial.printf("[load] todos moved to v2 (%u tasks)\n", todos.count);
  }
  loadOne(dayBlob); loadOne(histBlob);
  if (todos.count > TODO_MAX) todos.count = 0;
  for (uint8_t i = 0; i < TODO_MAX; i++) todos.item[i].text[32] = 0;
  for (uint8_t i = 0; i < LOG_MAX; i++) dayLog.log[i].task[32] = 0;
  if (dayLog.nLog > LOG_MAX) dayLog.nLog = 0;
  if (hist.n > HIST_MAX) hist.n = 0;
  pet.name[sizeof(pet.name) - 1] = 0;
  if (!pet.name[0]) strcpy(pet.name, "Bunny");
  if (setBlob.loadedVer && setBlob.loadedVer < 3) {             // old 3-step volume -> 1-10 level
    static const uint8_t OLD[] = { 1, 6, 10 };
    settings.volume = OLD[min((int)settings.volume, 2)];
    Serial.printf("[load] volume moved to level %u/10\n", settings.volume);
  }
  settings.volume = constrain(settings.volume, 1, 10);
  if (setBlob.loadedVer && setBlob.loadedVer < 4) settings.festivalDay = 0;   // old 6a test counter lived here
  if (settings.festivalDay && (settings.festivalDay < 20240101UL || settings.festivalDay > 29991231UL)) settings.festivalDay = 0;
  settings.offlineTest = 0;
  if (settings.focusDigits > 1) settings.focusDigits = 1;
  settings.ntfyTopic[32] = 0;                                   // b11
  if (setBlob.loadedVer && setBlob.loadedVer < 6) { memset(settings.ntfyTopic, 0, sizeof(settings.ntfyTopic)); settings.ntfyFlags = 7; settings.summaryHour = 21; settings.ntHungryMday = settings.ntSummaryMday = 0; }
  settings.ntfyFlags &= 7;
  if (settings.summaryHour < 18 || settings.summaryHour > 23) settings.summaryHour = 21;
  settings.bootCount++;
  markDirty(DIRTY_SET);
}

// ---- dirty-flag saving ----
bool inQuietGap() { return curMel == hbNotes && noteIdx == 3 && millis() - noteStart < 300; }   // 975 ms gap, 675+ ms left

void saveTick() {
  if (!dirty || millis() - dirtyMs < 500) return;              // coalesce fast changes (backlight slider)
  if (hbWanted && curMel != nullptr && !inQuietGap()) return;  // focus: wait for the heartbeat gap
  uint32_t t0 = micros();
  if (dirty & DIRTY_SET)  storeSave(setBlob);
  if (dirty & DIRTY_PET)  storeSave(petBlob);
  if (dirty & DIRTY_TODO) storeSave(todoBlob);
  if (dirty & DIRTY_DAY)  storeSave(dayBlob);
  if (dirty & DIRTY_HIST) storeSave(histBlob);
  Serial.printf("[save] %s%s%s%s in %lu ms%s\n", dirty & DIRTY_SET ? "settings " : "", dirty & DIRTY_PET ? "pet " : "",
                dirty & DIRTY_TODO ? "todos " : "", dirty & (DIRTY_DAY | DIRTY_HIST) ? "history " : "", (unsigned long)((micros() - t0) / 1000), hbWanted ? " (heartbeat gap)" : "");
  dirty = 0;
}

// ---- sound: bell -> heartbeat during focus (locked plan), silent break ----
void setMuted(bool m) {
  muted = m; settings.muted = m;
  if (m) stopSound();
  playChime(m ? 6 : 7);
  if (!m && hbWanted) bellPending = true;    // heartbeat comes back after the unmute beep
  markDirty(DIRTY_SET);
}
void soundFocusBegin()  { hbWanted = true; if (muted) bellPending = false; else { playChime(0); bellPending = true; } }   // mute = focus sounds only
void soundFocusResume() { hbWanted = true; bellPending = false; startHeartbeat(); }
void soundFocusEnd()    { hbWanted = false; bellPending = false; stopSound(); }
void soundTick() {
  if (bellPending && !buzzerBusy()) { bellPending = false; if (hbWanted) startHeartbeat(); }
}

void petNightly(uint32_t oldDay, int32_t days);   // petrules.h
void historyRollover(uint32_t oldDay, uint32_t newDay);   // history.h

// ---- midnight rollover (petrules.h runs streak reset, decay, penalties from here) ----
void checkRollover() {
  uint32_t today = clockToday();
  if (!today) return;
  if (settings.todayDate == 0) { settings.todayDate = today; markDirty(DIRTY_SET); return; }
  if (today > settings.todayDate) {
    if (hbWanted && curMel != nullptr && !inQuietGap()) return;   // b6: the day-sheet write waits for the heartbeat gap
    int32_t days = daysBetween(settings.todayDate, today);
    Serial.printf("[ROLLOVER] %lu -> %lu (%ld days)\n", (unsigned long)settings.todayDate, (unsigned long)today, (long)days);
    historyRollover(settings.todayDate, today);    // 9.3b: write the day sheet, drop done tasks, carry the rest
    petNightly(settings.todayDate, days);          // streak, shield, decay, Sick, daily counters
    pet.last7 = days >= 7 ? 0 : (uint8_t)((pet.last7 << days) & 0x7F);
    settings.todayCount = 0; settings.cycleDots = 0; settings.rollovers++;
    if (settings.festivalDay && settings.festivalDay < today) settings.festivalDay = 0;   // b4: the festival is over
    settings.todayDate = today; markDirty(DIRTY_SET | DIRTY_PET);
  } else if (today < settings.todayDate && clockSrc == CLK_NTP && !clockFake) {
    settings.todayDate = today; markDirty(DIRTY_SET);
  }
}
