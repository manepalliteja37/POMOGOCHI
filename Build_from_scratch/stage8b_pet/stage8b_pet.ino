// Stage 8b - pet rules on top of the locked 8a screens (care menu comes in 8c)
//
// Modules:
//   storage.h netsetup.h clock.h buzzer.h input.h   LOCKED, unchanged
//   appdata.h webui.h                               Stage 7 (small hooks for real sessions)
//   gfx.h       display basics, top bar, sprites, dots
//   session.h   timer engine, presets, focus/break, cycle dots
//   hourglass.h Stage 2 hourglass
//   petrules.h  pet rules: rewards, carrots, Growth cap, penalties, streak + shield, decay, Sick
//   screens.h   every screen + button map
// Art: limengdu/reTerminal_Sticky_Bunny (MIT, (c) 2026 limengdu) - see LICENSE_bunny_art.txt
//
// Board: NodeMCU 1.0 | Flash 4MB (FS:2MB) | Erase Flash: Only Sketch
//
// Serial 115200 (Newline):
//   ?  status          f  fast mode ON/OFF (1 min = 1 s; only when no session runs)
//   e  end the running focus/break now        o  replay onboarding
//   g  pretend a 25-min focus finished (real rules: carrot, Growth, streak)
//   n  pretend a night passed (today counted)      N  pretend a night + one MISSED day
//   0  set Fullness to 0 (to test Sick)            c  +1 carrot
//   P  reset pet + streak (egg)    W  forget Wi-Fi    r  reboot

#include <Arduino.h>
#include <LittleFS.h>
#include "storage.h"
#include "netsetup.h"
#include "clock.h"
#include "buzzer.h"
#include "input.h"
#include "appdata.h"
#include "gfx.h"
#include "petrules.h"
#include "session.h"
#include "hourglass.h"
#include "screens.h"
#include "webui.h"

uint32_t lastLoopUs = 0;
bool     epochSavedForRestart = false;

void printStatus() {
  Serial.printf("\nTime   : %s IST [%s]   Wi-Fi %s  ip %s\n", clockFull().c_str(), clockSrcName(), netStateName(),
                portalOn ? "192.168.4.1" : WiFi.localIP().toString().c_str());
  Serial.printf("Screen : %s   session %s%s  left %d s  fast %s\n", SCREEN_NAME[scr],
                sess == S_FOCUS ? "FOCUS" : sess == S_BREAK ? (longBreak ? "LONG BREAK" : "BREAK") : "none",
                sess && !timer.running ? " (paused)" : "", sessionLeftSec(), fastMode ? "ON" : "off");
  Serial.printf("Pomo   : preset %u min  today %u  dots %u/4  break auto %s  muted %d\n", presetMin(settings.lastPreset),
                settings.todayCount, settings.cycleDots, settings.breakAuto ? "on" : "off", muted);
  Serial.printf("Pet    : %s %s growth %u (today %u/8)  F%u L%u E%u  %s  carrots %u\n", pet.name, STAGE_NAME[stageOf(pet.growth)], pet.growth,
                pet.growthToday, pet.fullness, pet.love, pet.energy, moodName(), pet.carrots);
  Serial.printf("Rules  : rewarded today %u/4  cancels %u  sickDays %u  cure %u/2   streak %u best %u shield %u  last7 0x%02X\n",
                pet.rewardedToday, pet.cancelsToday, pet.sickDays, pet.cureCount, pet.streakCur, pet.streakBest, pet.shield, pet.last7);
  Serial.printf("To-dos : %u/7   heartbeat late max %lu ms   slowest loop %lu us   heap %u\n",
                todos.count, (unsigned long)noteLateMax, (unsigned long)loopMaxUs, ESP.getFreeHeap());
  loopMaxUs = 0; noteLateMax = 0;
}

void handleSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case '\n': case '\r': break;
    case '?': printStatus(); break;
    case 'f':
      if (sessionActive()) { Serial.println("Fast mode: stop the session first"); break; }
      fastMode = !fastMode; Serial.printf("Fast mode %s (1 min = %s)\n", fastMode ? "ON" : "OFF", fastMode ? "1 s" : "60 s"); break;
    case 'e':
      if (!sessionActive()) { Serial.println("No session running"); break; }
      if (!timer.running) timer.resume();
      timer.accMs += timer.remaining(); Serial.println("[test] session ends now"); break;
    case 'o': if (sessionActive()) { Serial.println("Stop the session first"); break; } obPage = 0; goScreen(SC_ONBOARD); break;
    case 'g': settings.todayCount++; markDirty(DIRTY_SET); petOnFocusDone(25); break;
    case 'n': case 'N': {                       // shift "the last day seen" back so the next rollover runs
      uint32_t today = clockToday(); if (!today) { Serial.println("No clock yet"); break; }
      time_t now = time(nullptr);
      uint32_t back = dayKey(now - (c == 'N' ? 2 : 1) * 86400L);
      if (pet.lastStreakDay == today) pet.lastStreakDay = back;   // today's sessions count for that day
      settings.todayDate = back;
      Serial.printf("[test] night%s...\n", c == 'N' ? " + missed day" : "");
      checkRollover(); } break;
    case '0': pet.fullness = 0; markDirty(DIRTY_PET); Serial.println("[test] Fullness 0"); break;
    case 'c': if (pet.carrots < CARROT_MAX) pet.carrots++; markDirty(DIRTY_PET); Serial.printf("[test] carrots %u\n", pet.carrots); break;
    case 'P': { char nm[12]; strcpy(nm, pet.name); pet = PetSave(); strcpy(pet.name, nm);
                markDirty(DIRTY_PET); Serial.println("[test] pet + streak reset (egg)"); } break;
    case 'W': clockSaveNow(); netForget(); break;
    case 'r': clockSaveNow(); Serial.println("Rebooting"); delay(100); ESP.restart(); break;
    default: Serial.printf("Unknown '%c' - press ?\n", c);
  }
}

void setup() {
  inputBegin();
  Serial.begin(115200);
  delay(50);
  Serial.println("\n\nStage 8b - pet rules");
  screenBegin();
  buzzerBegin();               // MUST be after tft.initR(): SPI.begin() takes D6 (GPIO12 = MISO)
  storeBegin();
  dataBegin();
  applySettings();
  buildGlass();
  randomSeed(ESP.getCycleCount());
  clockBegin();
  webBegin();
  netBegin(false);
  checkRollover();
  obPage = 0;
  goScreen(settings.onboarded ? SC_HOME : SC_ONBOARD);
  printStatus();
}

void loop() {
  uint32_t now = micros();
  if (lastLoopUs) { uint32_t gap = now - lastLoopUs; if (gap > loopMaxUs) loopMaxUs = gap; }
  lastLoopUs = now;

  inputPoll();
  buzzerTick();
  soundTick();
  netTick();
  if (restartPending && !epochSavedForRestart) { epochSavedForRestart = true; clockSaveNow(); }
  clockTick();
  checkRollover();
  sessionTick();
  saveTick();
  if (onboardRequest && !sessionActive()) { onboardRequest = false; obPage = 0; goScreen(SC_ONBOARD); }
  Event e;
  while (popEv(e)) uiEvent(e);
  screenTick();
  handleSerial();
}
