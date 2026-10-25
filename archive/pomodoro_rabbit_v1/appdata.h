// appdata.h - everything the device remembers + dirty-flag saving (Stage 7)
//
// Three save files through storage.h (A/B slots, CRC, data version):
//   settings v2 (same layout as 6b), pet v2 (6a layout + name at the END), todos v1 (6a layout)
// Rule (6a lock): saves are dirty-flag only, never every loop. During focus they wait for the
// heartbeat's long quiet gap, so a slow LittleFS write (up to ~390 ms) can't stretch a beat.
// Pet/streak RULES (rewards, penalties, decay) live in petrules.h. Here we only store + save them.
#pragma once
#include <Arduino.h>
#include "storage.h"
#include "clock.h"
#include "buzzer.h"

// ---- all structs/enums at the top ----
struct Settings {                // identical to 6b (v2) so your saved file loads as-is
  uint8_t  lastPreset = 1;
  uint8_t  muted = 0;
  uint8_t  volume = 0;           // 0 soft 1 mid 2 loud
  uint8_t  hbSel = 3;            // 264 Hz deep 50bpm
  uint16_t backlight = 1023;
  uint8_t  breakAuto = 1;
  uint8_t  onboarded = 0;
  uint8_t  todayCount = 0;
  uint8_t  cycleDots = 0;
  uint32_t todayDate = 0;        // yyyymmdd of the last day we saw
  uint32_t bootCount = 0;
  uint8_t  tortureOn = 0;        // 6a test leftover (kept for layout)
  uint32_t cutsCaught = 0;       // 6a test leftover
  uint8_t  offlineTest = 0;      // 6b test leftover
  uint32_t rollovers = 0;        // 6b test leftover
};

struct PetSave {                 // 6a layout (v1) + name at the end = v2
  uint8_t  stage = 0;
  uint16_t growth = 0;
  uint8_t  fullness = 80, energy = 80, love = 80;
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
  uint8_t  reserved[9] = {0};
  char     name[12] = "Bunny";   // NEW in v2
};

struct TodoItem { char text[33]; uint8_t done; };
struct TodoList { uint8_t count = 0; TodoItem item[7]; };

enum DirtyBit : uint8_t { DIRTY_SET = 1, DIRTY_PET = 2, DIRTY_TODO = 4 };

// ---- data ----
Settings settings;
PetSave  pet;
TodoList todos;
Blob setBlob  = { "settings", 2, &settings, sizeof(settings) };
Blob petBlob  = { "pet",      2, &pet,      sizeof(pet) };
Blob todoBlob = { "todos",    1, &todos,    sizeof(todos) };

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

const char* STAGE_NAME[] = { "Egg", "Baby", "Kid", "Teen", "Adult" };

void markDirty(uint8_t bits) {
  dirty |= bits; dirtyMs = millis(); rev++;
  if (bits & DIRTY_PET) uiPet = true;
  if (bits & DIRTY_TODO) { uiTodo = true; if (todoSel >= todos.count) todoSel = todos.count ? todos.count - 1 : 0; }
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
  if (todos.count > 7) todos.count = 0;
  for (uint8_t i = 0; i < 7; i++) todos.item[i].text[32] = 0;
  pet.name[sizeof(pet.name) - 1] = 0;
  if (!pet.name[0]) strcpy(pet.name, "Bunny");
  settings.offlineTest = 0;
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
  Serial.printf("[save] %s%s%s in %lu ms%s\n", dirty & DIRTY_SET ? "settings " : "", dirty & DIRTY_PET ? "pet " : "",
                dirty & DIRTY_TODO ? "todos " : "", (unsigned long)((micros() - t0) / 1000), hbWanted ? " (heartbeat gap)" : "");
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
void soundFocusBegin()  { hbWanted = true; if (muted) bellPending = false; else { playChime(0); bellPending = true; } }
void soundFocusResume() { hbWanted = true; bellPending = false; startHeartbeat(); }
void soundFocusEnd()    { hbWanted = false; bellPending = false; stopSound(); }
void soundTick() {
  if (bellPending && !buzzerBusy()) { bellPending = false; if (hbWanted) startHeartbeat(); }
}

void petNightly(uint32_t oldDay, int32_t days);   // petrules.h

// ---- midnight rollover (petrules.h runs streak reset, decay, penalties from here) ----
void checkRollover() {
  uint32_t today = clockToday();
  if (!today) return;
  if (settings.todayDate == 0) { settings.todayDate = today; markDirty(DIRTY_SET); return; }
  if (today > settings.todayDate) {
    int32_t days = daysBetween(settings.todayDate, today);
    Serial.printf("[ROLLOVER] %lu -> %lu (%ld days)\n", (unsigned long)settings.todayDate, (unsigned long)today, (long)days);
    petNightly(settings.todayDate, days);          // streak, shield, decay, Sick, daily counters
    pet.last7 = days >= 7 ? 0 : (uint8_t)((pet.last7 << days) & 0x7F);
    settings.todayCount = 0; settings.cycleDots = 0; settings.rollovers++;
    settings.todayDate = today; markDirty(DIRTY_SET | DIRTY_PET);
  } else if (today < settings.todayDate && clockSrc == CLK_NTP && !clockFake) {
    settings.todayDate = today; markDirty(DIRTY_SET);
  }
}
