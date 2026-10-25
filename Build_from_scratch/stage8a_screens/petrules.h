// petrules.h - hooks the session calls when something happens to the pet (Stage 8a = stubs)
// Stage 8b fills these with the spec rules: food/Growth/Energy rewards, daily cap 4,
// cancel grace 2 min, -5 Love, Worried at 3 cancels, skip-break -5 Energy, streak + shield,
// hatch + stage-up animations. In 8a they only log and show a speech line, so the flow can be tested.
#pragma once
#include <Arduino.h>
#include "appdata.h"

char     speechMsg[22] = "";
uint32_t speechUntil = 0;

void say(const char* s, uint32_t ms) {        // event line on Home, e.g. "Great focus!"
  strlcpy(speechMsg, s, sizeof(speechMsg)); speechUntil = millis() + ms; uiPet = true;
}

void petOnFocusDone(uint8_t mins) {
  Serial.printf("[pet] focus %u min done -> 8b: +1 food, +%u Growth, streak day\n", mins, mins >= 50 ? 2 : 1);
  say("Great focus!", 8000);
}
void petOnTestDone(uint8_t mins)        { Serial.printf("[pet] %u-min test done (never counts)\n", mins); }
void petOnFocusCancel(uint32_t elMs)    {
  Serial.printf("[pet] focus cancelled after %lu s -> 8b: %s\n", (unsigned long)(elMs / 1000), elMs < 120000UL ? "grace, no penalty" : "-5 Love, 'Aww...'");
  if (elMs >= 120000UL) say("Aww...", 6000);
}
void petOnBreakDone()                   { Serial.println("[pet] full break -> 8b: +1 Energy"); say("I feel rested!", 6000); }
void petOnBreakSkip()                   { Serial.println("[pet] break skipped -> 8b: -5 Energy"); say("I'm tired...", 6000); }
