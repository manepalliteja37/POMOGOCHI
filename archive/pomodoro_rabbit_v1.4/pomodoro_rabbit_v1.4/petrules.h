// petrules.h - Stage 8b: the pet rules from the spec (all numbers in one place)
//
// Completed 25/50 : 25 = +1 carrot, 50 = +2 (max 9), +1/+2 Growth (cap 8/day), streak day marked
// Full break      : +20 Energy           (rewards only for the first 4 sessions a day)
// Care menu (Home OK): Feed = 1 carrot -> +10 Fullness +10 Energy (v1.1); Pet = +5 Love (3 a day);
//                     Play = +5 Love for 10 Energy (b4, was +10; not while Sick, not under 20 Energy)
// Cancel focus    : first 2 min free; later -5 Love; 3 such cancels in a day -> Worried until midnight
// Skip break      : no Energy, -5 Energy
// Fresh day (b2)  : at 8 am Fullness 40, Love 40, Energy 60 (device off at 8? it happens at the next boot that day)
// Daytime (b2)    : Fullness drops every hour (9..23), Love -5 every 2 h (10,12..22). Hours the device was off catch up.
//                   b4 hunger tiers: -5/h, then -4/-3/-2 after 1/2/3 rewarded focuses, none after 4.
// Midnight        : streak check only (the old -10/-10 nightly decay is gone: 8 am resets anyway)
// Day with no 25/50: streak resets (a held shield saves it once) + b4: Growth -1 (never below the
//                   start of the current stage, so nothing un-hatches or shrinks a stage)
// Festival day (b4): set on the web page. No hourly drops, no cancel/skip penalty, no streak break,
//                   no Growth -1, no Sick count. The 8 am fresh start still happens. Focus still counts.
// Fullness 0 for 2 nights -> Sick (grey). 2 completed sessions cure it.
// Streak shield   : earned every 7 streak days, max 1 held. Milestones 3, 7, 30, 100.
// The Egg gets no decay or penalties: nothing can go wrong before it hatches.
#pragma once
#include <Arduino.h>
#include "appdata.h"

const uint8_t  REWARD_SESSIONS = 4, GROWTH_CAP = 8, CARROT_MAX = 9;
const uint8_t  FEED_FULL = 10, FEED_ENERGY = 10, BREAK_ENERGY = 20;
const uint8_t  DAY_START_H = 8, DAY_FULL_EVERY = 1, DAY_LOVE = 5, DAY_LOVE_EVERY = 2;
const uint8_t  HUNGER_TIER[5] = { 5, 4, 3, 2, 0 };   // b4: Fullness lost per hour by rewarded focuses today (0..4+)
const uint8_t  MISS_GROWTH = 1;                       // b4: Growth lost for a day with no 25/50 focus
const uint8_t  FRESH_F = 40, FRESH_L = 40, FRESH_E = 60;
const uint8_t  CANCEL_LOVE = 5, WORRY_CANCELS = 3, SKIP_ENERGY = 5;
const uint32_t CANCEL_GRACE_MS = 120000UL;
// (b2: NIGHT_FULL/NIGHT_ENERGY/MISS_FULL removed - the 8 am fresh start replaces them)
const uint8_t  SICK_NIGHTS = 2, CURE_SESSIONS = 2;
const uint8_t  PET_LOVE = 5, PET_MAX = 3, PLAY_LOVE = 5, PLAY_ENERGY = 10, PLAY_MIN_ENERGY = 20;
const uint8_t  MOOD_WORRIED = 1, MOOD_SICK = 2;

char     speechMsg[22] = "";
uint32_t speechUntil = 0;
bool     breakRewarded = false;      // this break follows a rewarded session

void say(const char* s, uint32_t ms) { strlcpy(speechMsg, s, sizeof(speechMsg)); speechUntil = millis() + ms; uiPet = true; }
void sub(uint8_t& v, uint8_t d) { v = v > d ? v - d : 0; }
void add(uint8_t& v, uint8_t d) { v = v + d > 100 ? 100 : v + d; }

// first completed 25/50 of the day
void markStreakDay() {
  uint32_t today = clockToday();
  pet.last7 |= 1;
  if (!today || pet.lastStreakDay == today) return;
  pet.lastStreakDay = today;
  pet.streakCur++;
  if (pet.streakCur > pet.streakBest) pet.streakBest = pet.streakCur;
  Serial.printf("[pet] streak day -> %u (best %u)\n", pet.streakCur, pet.streakBest);
  char b[22];
  if (pet.streakCur % 7 == 0 && !pet.shield) { pet.shield = 1; Serial.println("[pet] shield earned"); }
  uint16_t s = pet.streakCur;
  if (s == 3 || s == 7 || s == 30 || s == 100) {
    snprintf(b, sizeof(b), s == 7 ? "7 days! Shield won!" : "Day %u streak!", s);
    say(b, 10000);
  }
}

void petOnFocusDone(uint8_t mins) {
  bool rewarded = pet.rewardedToday < REWARD_SESSIONS;
  breakRewarded = rewarded;
  uint8_t gotC = 0;
  if (rewarded) { pet.rewardedToday++; gotC = mins >= 50 ? 2 : 1; pet.carrots = min(CARROT_MAX, (uint8_t)(pet.carrots + gotC)); }   // 9.3b: 25 = 1 carrot, 50 = 2
  uint8_t oldSt = stageOf(pet.growth);
  uint8_t g = mins >= 50 ? 2 : 1, room = pet.growthToday < GROWTH_CAP ? GROWTH_CAP - pet.growthToday : 0;
  if (g > room) g = room;
  pet.growth += g; pet.growthToday += g;
  uint8_t newSt = stageOf(pet.growth);
  pet.stage = newSt;
  if (newSt != oldSt) pet.pendingEvent = oldSt == 0 ? 1 : 2;
  say(!rewarded ? "Great focus!" : gotC == 2 ? "Great focus! +2 carrots" : "Great focus! +carrot", 8000);
  markStreakDay();
  if (pet.moodFlags & MOOD_SICK) {
    if (++pet.cureCount >= CURE_SESSIONS) { pet.moodFlags &= ~MOOD_SICK; pet.sickDays = 0; pet.cureCount = 0; say("I feel better!", 8000); Serial.println("[pet] cured"); }
  }
  Serial.printf("[pet] focus %u: %s, carrots %u, Growth +%u -> %u (today %u/%u)%s\n", mins, rewarded ? "rewarded" : "no reward (4/day done)",
                pet.carrots, g, pet.growth, pet.growthToday, GROWTH_CAP, newSt != oldSt ? "  STAGE UP" : "");
  markDirty(DIRTY_PET);
}

void petOnTestDone(uint8_t mins) { Serial.printf("[pet] %u-min test done (never counts)\n", mins); }

void petOnFocusCancel(uint32_t elMs) {
  if (elMs < CANCEL_GRACE_MS) { Serial.printf("[pet] cancel after %lu s: grace, no penalty\n", (unsigned long)(elMs / 1000)); return; }
  if (festivalToday()) { say("Festival - no worries", 5000); Serial.println("[pet] cancel on a festival day: no penalty"); return; }
  sub(pet.love, CANCEL_LOVE);
  if (pet.cancelsToday < 255) pet.cancelsToday++;
  if (pet.cancelsToday >= WORRY_CANCELS) pet.moodFlags |= MOOD_WORRIED;
  say(pet.moodFlags & MOOD_WORRIED ? "I'm worried..." : "Aww...", 6000);
  Serial.printf("[pet] cancel after %lu s: Love -%u -> %u, cancels today %u%s\n", (unsigned long)(elMs / 1000), CANCEL_LOVE, pet.love,
                pet.cancelsToday, pet.moodFlags & MOOD_WORRIED ? " -> WORRIED" : "");
  markDirty(DIRTY_PET);
}

void petOnBreakDone() {
  if (breakRewarded) { add(pet.energy, BREAK_ENERGY); say("I feel rested!", 6000); }
  Serial.printf("[pet] full break: Energy %s -> %u\n", breakRewarded ? "+20" : "+0 (4/day done)", pet.energy);
  breakRewarded = false;
  markDirty(DIRTY_PET);
}

void petOnBreakSkip() {
  breakRewarded = false;
  if (festivalToday()) { Serial.println("[pet] break skipped on a festival day: no penalty"); return; }
  sub(pet.energy, SKIP_ENERGY);
  say("I'm tired...", 6000);
  Serial.printf("[pet] break skipped: Energy -%u -> %u\n", SKIP_ENERGY, pet.energy);
  markDirty(DIRTY_PET);
}

// ---- care menu actions (true = it happened, play the animation) ----
bool petFeed() {
  if (stageOf(pet.growth) == 0) { say("Hatch me first!", 3000); return false; }
  if (!pet.carrots)             { say("No carrots yet", 3000); return false; }
  if (pet.fullness >= 100 && pet.energy >= 100) { say("I'm full!", 3000); return false; }
  pet.carrots--; add(pet.fullness, FEED_FULL); add(pet.energy, FEED_ENERGY);
  say("Yum! Thank you!", 4000);
  Serial.printf("[pet] fed: Fullness -> %u, Energy -> %u, carrots left %u\n", pet.fullness, pet.energy, pet.carrots);
  markDirty(DIRTY_PET);
  return true;
}

bool petPet() {
  if (pet.love >= 100)          { say("I'm so loved!", 3000); return false; }   // b2: no pet used at full Love
  if (pet.petsToday >= PET_MAX) { say("Pets again tomorrow", 3000); return false; }
  pet.petsToday++; add(pet.love, PET_LOVE);
  say("Hehe, I love you!", 4000);
  Serial.printf("[pet] petted: Love -> %u (%u/%u today)\n", pet.love, pet.petsToday, PET_MAX);
  markDirty(DIRTY_PET);
  return true;
}

bool petPlay() {
  if (pet.moodFlags & MOOD_SICK)       { say("Too sick to play", 3000); return false; }
  if (pet.energy < PLAY_MIN_ENERGY)    { say("Too tired to play", 3000); return false; }
  sub(pet.energy, PLAY_ENERGY); add(pet.love, PLAY_LOVE);
  say("Wheee! So fun!", 4000);
  Serial.printf("[pet] played: Love -> %u, Energy -> %u\n", pet.love, pet.energy);
  markDirty(DIRTY_PET);
  return true;
}

// Runs once per midnight that passed (from checkRollover). oldDay = the last day the device saw.
void petNightly(uint32_t oldDay, int32_t days) {
  bool egg = pet.growth == 0;
  int32_t n = days > 7 ? 7 : days;               // after a week everything is already at its floor
  for (int32_t k = 0; k < n && !egg; k++) {
    uint32_t day = ymdAdd(oldDay, k);
    bool had = (k == 0) && pet.lastStreakDay == oldDay;
    if (festivalOn(day)) { Serial.printf("[night] %lu was a festival day: no penalties\n", (unsigned long)day); continue; }
    if (!had) {
      if (pet.streakCur) {
        if (pet.shield) { pet.shield = 0; Serial.println("[night] missed day - SHIELD used, streak kept"); }
        else { Serial.printf("[night] missed day - streak %u reset (best %u)\n", pet.streakCur, pet.streakBest); pet.streakCur = 0; }
      }
      uint16_t floorG = nextStageAt(stageOf(pet.growth) - 1);      // start of the current stage (Baby 1, Kid 10, Teen 40, Adult 100)
      uint16_t g0 = pet.growth;
      pet.growth = pet.growth > floorG + MISS_GROWTH ? pet.growth - MISS_GROWTH : floorG;
      if (pet.growth < g0) Serial.printf("[night] missed day - Growth %u -> %u\n", g0, pet.growth);
    }
    if (pet.fullness == 0) {
      if (pet.sickDays < 255) pet.sickDays++;
      if (pet.sickDays >= SICK_NIGHTS && !(pet.moodFlags & MOOD_SICK)) { pet.moodFlags |= MOOD_SICK; pet.cureCount = 0; Serial.println("[night] -> SICK"); }
    } else pet.sickDays = 0;
  }
  pet.rewardedToday = 0; pet.growthToday = 0; pet.cancelsToday = 0; pet.petsToday = 0;
  pet.decayHour = 0; pet.decayInit = 1;          // the new day's daytime slots start fresh
  pet.moodFlags &= ~MOOD_WORRIED;
  Serial.printf("[night] %ld night(s): F%u E%u L%u streak %u shield %u sickDays %u %s\n", (long)days, pet.fullness, pet.energy, pet.love,
                pet.streakCur, pet.shield, pet.sickDays, egg ? "(egg: no decay)" : "");
  markDirty(DIRTY_PET);
}

// v1.1 daytime decay. Checked once a minute; catches up on hours the device was off (same day only).
void petDaytimeTick() {
  static uint32_t last = 0;
  if (millis() - last < 60000UL && last) return;
  last = millis();
  if (!clockValid()) return;
  time_t t = time(nullptr); tm lt; localtime_r(&t, &lt);
  uint8_t h = lt.tm_hour;
  if (!pet.decayInit) { pet.decayInit = 1; pet.decayHour = h; markDirty(DIRTY_PET); Serial.printf("[day] decay starts counting from %02u:00\n", h); return; }
  if (!pet.morningDay) { pet.morningDay = lt.tm_mday; markDirty(DIRTY_PET); }      // first b2 boot: fresh start begins tomorrow
  if (h >= DAY_START_H && pet.morningDay != lt.tm_mday) {
    pet.morningDay = lt.tm_mday;
    pet.fullness = FRESH_F; pet.love = FRESH_L; pet.energy = FRESH_E;
    if (pet.decayHour < DAY_START_H) pet.decayHour = DAY_START_H;
    say("Good morning!", 6000);
    Serial.printf("[day] fresh day: F%u L%u E%u\n", FRESH_F, FRESH_L, FRESH_E);
    markDirty(DIRTY_PET);
  }
  if (pet.decayHour >= h) return;
  bool egg = pet.growth == 0 || festivalToday(), ch = false;     // b4: festival day = no hourly drops
  while (pet.decayHour < h) {
    uint8_t x = ++pet.decayHour;
    if (egg || x <= DAY_START_H) continue;
    uint8_t since = x - DAY_START_H;
    uint8_t hf = HUNGER_TIER[min((int)pet.rewardedToday, 4)];   // b4 tiers: 5/4/3/2/0
    if (since % DAY_FULL_EVERY == 0 && hf) { sub(pet.fullness, hf); ch = true; }
    if (since % DAY_LOVE_EVERY == 0) { sub(pet.love, DAY_LOVE); ch = true; }
  }
  if (ch) Serial.printf("[day] %02u:00 F%u L%u E%u\n", h, pet.fullness, pet.love, pet.energy);
  markDirty(DIRTY_PET);
}
