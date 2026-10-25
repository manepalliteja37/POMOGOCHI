// Stage 7 v2 - setup help on screen + epoch saved before every restart
// Stage 7 - Web page: to-dos, streak, pet, settings, export/import (+ device screen mirror)
//
// Modules (one header each, included below):
//   storage.h  netsetup.h  clock.h   - LOCKED in 6a/6b, unchanged
//   buzzer.h   input.h               - LOCKED in 5/4, unchanged
//   appdata.h  - saved data + dirty-flag saves + 1-min focus test   (new)
//   screen.h   - Stage 7 test screen                                (new)
//   webui.h    - the page + API                                     (new)
//
// Board: NodeMCU 1.0 | Flash 4MB (FS:2MB OTA:~1019KB) | Erase Flash: Only Sketch
// Libraries: Adafruit GFX, Adafruit ST7735 (no new libraries)
//
// Phone: open http://<IP shown on the screen>
// Device buttons:  OK = tick highlighted task   BACK = next task
//                  MODE = start/stop 1-min test focus (bell -> heartbeat -> done)
//                  MODE long = mute
// Serial 115200 (Newline):
//   ?  status (+ resets the slowest-loop counter)   g  pretend a 25-min focus finished (growth, streak)
//   P  reset pet + streak to defaults                f  start/stop the 1-min focus test
//   W  forget Wi-Fi                                  r  reboot

#include <Arduino.h>
#include <LittleFS.h>
#include "storage.h"
#include "netsetup.h"
#include "clock.h"
#include "buzzer.h"
#include "input.h"
#include "appdata.h"
#include "screen.h"
#include "webui.h"

uint32_t lastLoopUs = 0;
bool     epochSavedForRestart = false;

void handleEvent(const Event& e) {
  if (e.type == EV_LONG && e.btn == B_MODE) { setMuted(!muted); Serial.printf("[btn] mute %s\n", muted ? "ON" : "OFF"); return; }
  if (e.type != EV_SHORT && e.type != EV_DOUBLE) return;
  switch (e.btn) {
    case B_MODE: if (e.type == EV_SHORT) { if (ft.run) focusStop(); else focusStart(); } break;
    case B_OK:
      if (netState == NET_PORTAL) break;           // list is hidden behind the setup help
      if (e.type == EV_SHORT && todos.count) {
        todos.item[todoSel].done ^= 1; markDirty(DIRTY_TODO);
        Serial.printf("[btn] tick %u '%s' -> %s\n", todoSel + 1, todos.item[todoSel].text, todos.item[todoSel].done ? "done" : "open");
      }
      break;
    case B_BACK:                                   // a quick 2nd BACK arrives as DOUBLE: still one step down
      if (netState != NET_PORTAL && todos.count) { todoSel = (todoSel + 1) % todos.count; uiTodo = true; }
      break;
    default: break;
  }
}

void printStatus() {
  Serial.printf("\nTime   : %s IST [%s]\n", clockFull().c_str(), clockSrcName());
  Serial.printf("Wi-Fi  : %s  '%s'  ip %s  rssi %d\n", netStateName(), wifiCfg.ssid,
                portalOn ? "192.168.4.1" : WiFi.localIP().toString().c_str(), WiFi.RSSI());
  Serial.printf("To-dos : %u/7   Pet: %s %s growth %u  F%u L%u E%u  %s\n", todos.count, pet.name,
                STAGE_NAME[stageOf(pet.growth)], pet.growth, pet.fullness, pet.love, pet.energy, moodName());
  Serial.printf("Streak : %u (best %u) shield %u  today %u  last7 0x%02X\n", pet.streakCur, pet.streakBest, pet.shield,
                settings.todayCount, pet.last7);
  Serial.printf("Focus  : %s  skipped seconds %u  heartbeat late max %lu ms\n", ft.run ? "running" : "idle", ft.skips,
                (unsigned long)noteLateMax);
  storeInfo(setBlob); storeInfo(petBlob); storeInfo(todoBlob);
  Serial.printf("Loop   : slowest %lu us since last '?'\n", (unsigned long)loopMaxUs);
  Serial.printf("Heap   : %u (pass: >= 20000)\n\n", ESP.getFreeHeap());
  loopMaxUs = 0; noteLateMax = 0;
}

void handleSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case '\n': case '\r': break;
    case '?': printStatus(); break;
    case 'g': fakeSession(); Serial.printf("[test] session done: growth %u, streak %u, today %u\n", pet.growth, pet.streakCur, settings.todayCount); break;
    case 'P': { char nm[12]; strcpy(nm, pet.name); pet = PetSave(); strcpy(pet.name, nm);
                settings.todayCount = 0; markDirty(DIRTY_PET | DIRTY_SET); Serial.println("[test] pet + streak reset"); } break;
    case 'f': if (ft.run) focusStop(); else focusStart(); break;
    case 'W': clockSaveNow(); netForget(); break;
    case 'r': clockSaveNow(); Serial.println("Rebooting"); delay(100); ESP.restart(); break;
    default: Serial.printf("Unknown '%c' - press ?\n", c);
  }
}

void setup() {
  inputBegin();
  Serial.begin(115200);
  delay(50);
  Serial.println("\n\nStage 7 - web page");
  screenBegin();
  buzzerBegin();               // MUST be after tft.initR(): SPI.begin() takes D6 (GPIO12 = MISO)
  storeBegin();
  dataBegin();
  applySettings();
  clockBegin();
  webBegin();                  // routes first, then netBegin() starts the server
  netBegin(false);
  checkRollover();
  printStatus();
}

void loop() {
  uint32_t now = micros();
  if (lastLoopUs) { uint32_t gap = now - lastLoopUs; if (gap > loopMaxUs) loopMaxUs = gap; }
  lastLoopUs = now;

  inputPoll();
  buzzerTick();
  netTick();                   // web requests are answered here
  if (restartPending && !epochSavedForRestart) {   // Change Wi-Fi / setup Save: keep the clock right across the restart
    epochSavedForRestart = true; clockSaveNow();
    Serial.printf("[time] epoch saved before restart: %s\n", clockFull().c_str());
  }
  clockTick();
  checkRollover();
  focusTick();
  saveTick();
  Event e;
  while (popEv(e)) handleEvent(e);
  screenTick();
  handleSerial();
}
