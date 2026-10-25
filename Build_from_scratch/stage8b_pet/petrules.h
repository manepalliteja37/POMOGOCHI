// petrules.h - Stage 8b: the pet rules from the spec (all numbers in one place)
//
// Completed 25/50 : +1 carrot (max 9), +1/+2 Growth (cap 8/day), streak day marked
// Full break      : +20 Energy           (rewards only for the first 4 sessions a day)
// Feed (Home OK)  : 1 carrot -> +20 Fullness   (moves into the care menu in 8c)
// Cancel focus    : first 2 min free; later -5 Love; 3 such cancels in a day -> Worried until midnight
// Skip break      : no Energy, -5 Energy
// Every midnight  : Fullness -10, Energy -10 (daily decay)
// Day with no 25/50: streak resets (a held shield saves it once), Fullness -25
// Fullness 0 for 2 nights -> Sick (grey). 2 completed sessions cure it.
// Streak shield   : earned every 7 streak days, max 1 held. Milestones 3, 7, 30, 100.
// The Egg gets no decay or penalties: nothing can go wrong before it hatches.
#pragma once
#include <Arduino.h>
#include "appdata.h"

const uint8_t  REWARD_SESSIONS = 4, GROWTH_CAP = 8, CARROT_MAX = 9;
const uint8_t  FEED_FULL = 20, BREAK_ENERGY = 20;
const uint8_t  CANCEL_LOVE = 5, WORRY_CANCELS = 3, SKIP_ENERGY = 5;
const uint32_t CANCEL_GRACE_MS = 120000UL;
const uint8_t  NIGHT_FULL = 10, NIGHT_ENERGY = 10, MISS_FULL = 25;
const uint8_t  SICK_NIGHTS = 2, CURE_SESSIONS = 2;
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
  if (rewarded) { pet.rewardedToday++; if (pet.carrots < CARROT_MAX) pet.carrots++; }
  uint8_t oldSt = stageOf(pet.growth);
  uint8_t g = mins >= 50 ? 2 : 1, room = pet.growthToday < GROWTH_CAP ? GROWTH_CAP - pet.growthToday : 0;
  if (g > room) g = room;
  pet.growth += g; pet.growthToday += g;
  uint8_t newSt = stageOf(pet.growth);
  pet.stage = newSt;
  if (newSt != oldSt) pet.pendingEvent = oldSt == 0 ? 1 : 2;
  say(rewarded ? "Great focus! +carrot" : "Great focus!", 8000);
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
  sub(pet.energy, SKIP_ENERGY); breakRewarded = false;
  say("I'm tired...", 6000);
  Serial.printf("[pet] break skipped: Energy -%u -> %u\n", SKIP_ENERGY, pet.energy);
  markDirty(DIRTY_PET);
}

// Feed one carrot (Home OK for now). Returns the frame to show: 0 = no feeding.
bool petFeed() {
  if (stageOf(pet.growth) == 0) { say("Hatch me first!", 3000); return false; }
  if (!pet.carrots)             { say("No carrots yet", 3000); return false; }
  if (pet.fullness >= 100)      { say("I'm full!", 3000); return false; }
  pet.carrots--; add(pet.fullness, FEED_FULL);
  say("Yum! Thank you!", 4000);
  Serial.printf("[pet] fed: Fullness -> %u, carrots left %u\n", pet.fullness, pet.carrots);
  markDirty(DIRTY_PET);
  return true;
}

// Runs once per midnight that passed (from checkRollover). oldDay = the last day the device saw.
void petNightly(uint32_t oldDay, int32_t days) {
  bool egg = pet.growth == 0;
  int32_t n = days > 7 ? 7 : days;               // after a week everything is already at its floor
  for (int32_t k = 0; k < n && !egg; k++) {
    bool had = (k == 0) && pet.lastStreakDay == oldDay;
    sub(pet.fullness, NIGHT_FULL); sub(pet.energy, NIGHT_ENERGY);
    if (!had) {
      sub(pet.fullness, MISS_FULL);
      if (pet.streakCur) {
        if (pet.shield) { pet.shield = 0; Serial.println("[night] missed day - SHIELD used, streak kept"); }
        else { Serial.printf("[night] missed day - streak %u reset (best %u)\n", pet.streakCur, pet.streakBest); pet.streakCur = 0; }
      }
    }
    if (pet.fullness == 0) {
      if (pet.sickDays < 255) pet.sickDays++;
      if (pet.sickDays >= SICK_NIGHTS && !(pet.moodFlags & MOOD_SICK)) { pet.moodFlags |= MOOD_SICK; pet.cureCount = 0; Serial.println("[night] -> SICK"); }
    } else pet.sickDays = 0;
  }
  pet.rewardedToday = 0; pet.growthToday = 0; pet.cancelsToday = 0;
  pet.moodFlags &= ~MOOD_WORRIED;
  Serial.printf("[night] %ld night(s): F%u E%u L%u streak %u shield %u sickDays %u %s\n", (long)days, pet.fullness, pet.energy, pet.love,
                pet.streakCur, pet.shield, pet.sickDays, egg ? "(egg: no decay)" : "");
  markDirty(DIRTY_PET);
}
