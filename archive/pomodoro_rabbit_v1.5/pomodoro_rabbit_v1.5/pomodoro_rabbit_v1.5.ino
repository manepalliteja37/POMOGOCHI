// Pomodoro Rabbit - firmware v1.1-b5
// v1.1-b5: power-cut resume (flash snapshot of the running focus/break every 60 s + on start/pause/end).
// v1.1-b4: festival day (web), 1 h idle sleep (Zzz, dim, Wi-Fi off; any button -> Setup), power-on display
//          re-init, missed day = Growth -1 (stage floor), hunger tiers 5/4/3/2/0, Play +5 Love,
//          two-line hungry text, task focus = 25/50 only, epoch save every 30 min and never mid-heartbeat.
// v1.1-b3 (9.3b): to-do-linked focus + "Task done?", Done today on the web, day log, 30-day history,
//          JSON day sheet download, carry-over at midnight, resume a session after a reset (RTC memory),z
//          25 = 1 carrot / 50 = 2 carrots, hunger stops after 4 rewarded focuses, volume slider removed.
// v1.1-b2: real volume (pulse width), mute = focus sounds only, eat/love/play sounds, 8 am fresh day
//          F40 L40 E60, hourly hunger, 2-hourly loneliness, no Pet at full Love, 3-letter day names.
// v1.1-b1: web volume slider 1-10, pet balance (start F40 L40 E60, daytime hunger/loneliness,
//          carrot = +10 Fullness +10 Energy), reset reason + slowest-loop culprit on the web page.
// v1.0-rc2: Home BACK = Info screen, http://pomodoro.local
// was v1.0-rc1 (Stage 9: all locked modules merged)
//
// Modules (all hardware-tested and locked):
//   storage.h   crash-safe A/B save files (6a)        netsetup.h  Wi-Fi + setup hotspot (6b)
//   clock.h     NTP time + saved fallback (6b)         buzzer.h    chimes + focus heartbeat (5)
//   input.h     3-button layer (4)                     appdata.h   settings, pet, to-dos, dirty saves (7)
//   webui.h     web page + API (7)                     gfx.h       display basics (8a)
//   session.h   focus/break engine (1, 8a)             hourglass.h hourglass (2)
//   petrules.h  pet rules, all numbers in one place (8b, 8c)
//   screens.h   every screen + button map + care menu (8a-8c)
//   history.h   day log, day sheets, 30-day history, carry-over (9.3b)
// Art: limengdu/reTerminal_Sticky_Bunny (MIT, (c) 2026 limengdu) - see LICENSE_bunny_art.txt
//
// Board: NodeMCU 1.0 | Flash 4MB (FS:2MB) | Erase Flash: Only Sketch (keeps your pet + to-dos)
//
// Serial 115200 (Newline):  ?  status (uptime, reset reason, lowest heap)
// Test keys are OFF in this build so stray characters can't change the pet.
// Set TEST_KEYS to 1 to bring back: f e o g n N 0 c k l P W r (see handleSerial).
#define FW_VERSION "v1.1-b5"
#define TEST_KEYS 0
#define FW_VER_SHORT "fw " FW_VERSION

#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266mDNS.h>
#include "storage.h"
#include "netsetup.h"
#include "clock.h"
#include "buzzer.h"
#include "input.h"
#include "appdata.h"
#include "gfx.h"
#include "petrules.h"
#include "history.h"
#include "session.h"
#include "hourglass.h"
#include "screens.h"
#include "webui.h"

uint32_t lastLoopUs = 0;
const char* loopMaxWhat = "-";   // which step of loop() was slowest in the worst loop
uint32_t secUs[12];
uint32_t heapMin = 0xFFFFFFFF;
String   bootReason;
bool     epochSavedForRestart = false;
uint32_t tftReinitAt = 0;        // b4: power-on boots re-init the display once, ~1.5 s in

String upText() {
  uint32_t t = millis() / 1000;
  char b[24]; snprintf(b, sizeof(b), "%lud %02luh %02lum", (unsigned long)(t / 86400), (unsigned long)(t / 3600 % 24), (unsigned long)(t / 60 % 60));
  return b;
}

// http://pomodoro.local - restarted every time Wi-Fi comes (back) online
void mdnsTick() {
  static NetState last = NET_OFF; static bool on = false;
  if (netState != last) {
    last = netState;
    if (on) { MDNS.end(); on = false; }
    if (netState == NET_ONLINE && MDNS.begin("pomodoro")) { MDNS.addService("http", "tcp", 80); on = true; Serial.println("[net] http://pomodoro.local ready"); }
  }
  if (on) MDNS.update();
}

void printStatus() {
  Serial.printf("\n%s   up %s   last reset: %s   lowest heap %lu\n", FW_VERSION, upText().c_str(), bootReason.c_str(), (unsigned long)heapMin);
  Serial.printf("\nTime   : %s IST [%s]   Wi-Fi %s  ip %s\n", clockFull().c_str(), clockSrcName(), netStateName(),
                portalOn ? "192.168.4.1" : WiFi.localIP().toString().c_str());
  Serial.printf("Screen : %s   session %s%s  left %d s  fast %s\n", SCREEN_NAME[scr],
                sess == S_FOCUS ? "FOCUS" : sess == S_BREAK ? (longBreak ? "LONG BREAK" : "BREAK") : "none",
                sess && !timer.running ? " (paused)" : "", sessionLeftSec(), fastMode ? "ON" : "off");
  Serial.printf("Pomo   : preset %u min  today %u  dots %u/4  break auto %s  muted %d\n", presetMin(settings.lastPreset),
                settings.todayCount, settings.cycleDots, settings.breakAuto ? "on" : "off", muted);
  Serial.printf("Pet    : %s %s growth %u (today %u/8)  F%u L%u E%u  %s  carrots %u\n", pet.name, STAGE_NAME[stageOf(pet.growth)], pet.growth,
                pet.growthToday, pet.fullness, pet.love, pet.energy, moodName(), pet.carrots);
  Serial.printf("Rules  : rewarded today %u/4  cancels %u  pets %u/3  sickDays %u  cure %u/2   streak %u best %u shield %u  last7 0x%02X\n",
                pet.rewardedToday, pet.cancelsToday, pet.petsToday, pet.sickDays, pet.cureCount, pet.streakCur, pet.streakBest, pet.shield, pet.last7);
  Serial.printf("To-dos : %u open %u done   day: %u focus %u min   history %u days\n", openCount(), doneCount(),
                dayLog.n25 + dayLog.n50, dayLog.focusMin, hist.n);
  Serial.printf("Health : heartbeat late max %lu ms   slowest loop %lu us (in %s)   heap %u\n",
                (unsigned long)noteLateMax, (unsigned long)loopMaxUs, loopMaxWhat, ESP.getFreeHeap());
  loopMaxUs = 0; noteLateMax = 0; loopMaxWhat = "-";
}

void handleSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case '\n': case '\r': break;
    case '?': printStatus(); break;
#if TEST_KEYS
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
    case 'k': pet.energy = 15; markDirty(DIRTY_PET); Serial.println("[test] Energy 15"); break;
    case 'l': pet.love = 20; markDirty(DIRTY_PET); Serial.println("[test] Love 20"); break;
    case '0': pet.fullness = 0; markDirty(DIRTY_PET); Serial.println("[test] Fullness 0"); break;
    case 'c': if (pet.carrots < CARROT_MAX) pet.carrots++; markDirty(DIRTY_PET); Serial.printf("[test] carrots %u\n", pet.carrots); break;
    case 'P': { char nm[12]; strcpy(nm, pet.name); pet = PetSave(); strcpy(pet.name, nm);
                markDirty(DIRTY_PET); Serial.println("[test] pet + streak reset (egg)"); } break;
    case 'W': clockSaveNow(); netForget(); break;
    case 'r': clockSaveNow(); Serial.println("Rebooting"); delay(100); ESP.restart(); break;
#endif
    default: Serial.printf("Unknown '%c' - press ?\n", c);
  }
}

void setup() {
  inputBegin();
  Serial.begin(115200);
  delay(50);
  bootReason = ESP.getResetReason();
  Serial.printf("\n\nPomodoro Rabbit %s   reset reason: %s\n", FW_VERSION, bootReason.c_str());
  bool coldBoot = ESP.getResetInfoPtr()->reason == REASON_DEFAULT_RST;   // power plugged in
  if (coldBoot) delay(300);    // b4: let the display's supply settle before its init
  screenBegin();
  buzzerBegin();               // MUST be after tft.initR(): SPI.begin() takes D6 (GPIO12 = MISO)
  storeBegin();
  dataBegin();
  applySettings();
  buildGlass();
  randomSeed(ESP.getCycleCount());
  clockBegin();
  clockSaveGate = []() { return !hbWanted || inQuietGap(); };   // b4: the slow epoch save never stretches a heartbeat
  webBegin();
  netBegin(false);
  checkRollover();
  obPage = 0;
  goScreen(settings.onboarded ? SC_HOME : SC_ONBOARD);
  rtcResume();                 // 9.3b: a focus/break that was running before a reset carries on
  lastActMs = millis();
  if (coldBoot) tftReinitAt = millis() + 1500;
  printStatus();
}

// every step of loop() is timed, so the worst loop also says which step caused it
#define STEP(i, call) { uint32_t _t = micros(); call; secUs[i] = micros() - _t; }
const char* STEP_NAME[12] = { "input", "buzzer", "net", "clock", "rollover", "mdns", "session", "save", "buttons", "screen", "serial", "pet" };

void loop() {
  uint32_t now = micros();
  if (lastLoopUs) {
    uint32_t gap = now - lastLoopUs;
    if (gap > loopMaxUs) {
      loopMaxUs = gap; uint8_t w = 0;
      for (uint8_t i = 1; i < 12; i++) if (secUs[i] > secUs[w]) w = i;
      loopMaxWhat = STEP_NAME[w];
    }
  }
  lastLoopUs = now;
  static bool settled = false;
  if (!settled && millis() > 10000) { settled = true; loopMaxUs = 0; loopMaxWhat = "-"; }

  STEP(0, inputPoll());
  STEP(1, { buzzerTick(); soundTick(); });
  STEP(2, netTick());
  if (restartPending && !epochSavedForRestart) { epochSavedForRestart = true; clockSaveNow(); }
  STEP(3, clockTick());
  if (netWoke && netState == NET_ONLINE) { netWoke = false; clockResync(); Serial.println("[time] NTP resync after wake"); }
  if (tftReinitAt && (int32_t)(millis() - tftReinitAt) >= 0) { tftReinitAt = 0; screenReinit(); }
  STEP(4, checkRollover());
  STEP(5, mdnsTick());
  STEP(6, sessionTick());
  STEP(7, saveTick());
  if (onboardRequest && !sessionActive() && scr != SC_ASK) { onboardRequest = false; obPage = 0; goScreen(SC_ONBOARD); }
  Event e;
  STEP(8, { while (popEv(e)) uiEvent(e); });
  STEP(9, screenTick());
  STEP(10, handleSerial());
  STEP(11, petDaytimeTick());
  uint32_t h = ESP.getFreeHeap(); if (h < heapMin) heapMin = h;
}
