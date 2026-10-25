// Stage 6b - Wi-Fi setup portal + NTP (IST) + saved epoch + midnight rollover (Serial only)
// Uses the locked storage.h from 6a. Same IDE settings as 6a (4MB FS:2MB, Erase: Only Sketch).
//
// Serial (115200, Newline):
//   ?  status                       W  forget Wi-Fi -> setup portal
//   y  force NTP resync             o  offline test ON/OFF (reboots, Wi-Fi off)
//   j  fake clock to 23:59:50       J  fake clock +3 days (missed-days test)
//   s  save epoch now               r  reboot
// Browser: when online, open http://<IP> shown in Serial -> status page.

#include <Arduino.h>
#include <LittleFS.h>
#include "storage.h"
#include "netsetup.h"
#include "clock.h"

// ---- all structs/enums at the top (Arduino auto-prototypes) ----
struct Settings {                // 6a layout + new field at the END -> version 2 upgrades old saves
  uint8_t  lastPreset = 1;
  uint8_t  muted = 0;
  uint8_t  volume = 0;
  uint8_t  hbSel = 3;
  uint16_t backlight = 1023;
  uint8_t  breakAuto = 1;
  uint8_t  onboarded = 0;
  uint8_t  todayCount = 0;
  uint8_t  cycleDots = 0;
  uint32_t todayDate = 0;        // yyyymmdd of the last day we saw
  uint32_t bootCount = 0;
  uint8_t  tortureOn = 0;
  uint32_t cutsCaught = 0;
  uint8_t  offlineTest = 0;      // NEW in v2
  uint32_t rollovers = 0;        // NEW in v2 (test counter)
};

Settings settings;
Blob setBlob = { "settings", 2, &settings, sizeof(settings) };
uint32_t lastMinuteMs = 0;

// ---- midnight rollover (in the final firmware: pet decay + streak check go here) ----
void onNewDay(uint32_t oldDay, uint32_t newDay, int32_t days) {
  settings.rollovers++;
  Serial.printf("\n[ROLLOVER] %lu -> %lu  (%ld day%s later, %ld missed day%s)\n", (unsigned long)oldDay, (unsigned long)newDay,
                (long)days, days == 1 ? "" : "s", (long)(days - 1), days - 1 == 1 ? "" : "s");
  Serial.printf("           today's count %u -> 0, cycle dots reset, streak check would run here\n\n", settings.todayCount);
  settings.todayCount = 0; settings.cycleDots = 0;
}

void checkRollover() {
  uint32_t today = clockToday();
  if (!today) return;
  if (settings.todayDate == 0) { settings.todayDate = today; storeSave(setBlob); Serial.printf("[day] first day set: %lu\n", (unsigned long)today); return; }
  if (today > settings.todayDate) {
    onNewDay(settings.todayDate, today, daysBetween(settings.todayDate, today));
    settings.todayDate = today; storeSave(setBlob);
  } else if (today < settings.todayDate && clockSrc == CLK_NTP && !clockFake) {
    Serial.printf("[day] saved day %lu is ahead of real time - corrected to %lu\n", (unsigned long)settings.todayDate, (unsigned long)today);
    settings.todayDate = today; storeSave(setBlob);
  }
}

void homePage() {                // placeholder for the Stage 7 page
  String h = FPSTR(PAGE_HEAD);
  h += "<p>Time: <b>" + clockFull() + "</b> IST<br><small>source: " + clockSrcName() + "</small></p>";
  h += "<p>Wi-Fi: " + htmlEsc(wifiCfg.ssid) + " (" + String(WiFi.RSSI()) + " dBm)<br>Day: " + String(settings.todayDate) +
       "<br>Rollovers: " + String(settings.rollovers) + "<br>Uptime: " + String(millis() / 1000) + " s<br>Heap: " +
       String(ESP.getFreeHeap()) + "</p><small>Stage 7 page goes here</small></body></html>";
  server.send(200, "text/html", h);
}

void printStatus() {
  Serial.printf("\nTime   : %s IST  [%s]%s  top bar %s\n", clockFull().c_str(), clockSrcName(), clockFake ? " FAKE" : "", clockHHMM().c_str());
  Serial.printf("NTP    : %lu syncs, epoch saved seq %lu (every 10 min)\n", (unsigned long)ntpSyncs, (unsigned long)clockBlob.seq);
  Serial.printf("Wi-Fi  : %s  ssid '%s'  ip %s  rssi %d\n", netStateName(), wifiCfg.ssid,
                portalOn ? "192.168.4.1" : WiFi.localIP().toString().c_str(), WiFi.RSSI());
  Serial.printf("Day    : %lu  rollovers %lu  boots %lu  offline test %s\n", (unsigned long)settings.todayDate,
                (unsigned long)settings.rollovers, (unsigned long)settings.bootCount, settings.offlineTest ? "ON" : "off");
  Serial.printf("Heap   : %u\n\n", ESP.getFreeHeap());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n\nStage 6b - Wi-Fi + NTP + rollover");
  storeBegin();
  bool ok = storeLoad(setBlob);
  if (ok && setBlob.loadedVer < setBlob.ver) Serial.printf("[load] settings v%u upgraded to v%u (old data kept)\n", setBlob.loadedVer, setBlob.ver);
  settings.bootCount++; storeSave(setBlob);

  clockBegin();
  netHomePage = homePage;
  netBegin(settings.offlineTest);
  checkRollover();
  printStatus();
}

void loop() {
  netTick();
  clockTick();
  checkRollover();

  if (millis() - lastMinuteMs >= 60000) {
    lastMinuteMs = millis();
    Serial.printf("[min] %s  %s  wifi %s\n", clockHHMM().c_str(), clockSrcName(), netStateName());
  }

  if (!Serial.available()) return;
  char c = Serial.read();
  if (c == '\n' || c == '\r') return;
  switch (c) {
    case '?': printStatus(); break;
    case 'W': netForget(); break;
    case 'y': Serial.println("[time] NTP resync requested"); clockResync(); break;
    case 's': clockSaveNow(); Serial.printf("[time] epoch saved, seq %lu\n", (unsigned long)clockBlob.seq); break;
    case 'o': settings.offlineTest ^= 1; storeSave(setBlob);
              Serial.printf("Offline test %s - rebooting\n", settings.offlineTest ? "ON" : "OFF"); delay(100); ESP.restart(); break;
    case 'j': case 'J': {
      if (!clockValid()) { Serial.println("Clock not set yet"); break; }
      time_t t = time(nullptr);
      if (c == 'j') { tm lt; localtime_r(&t, &lt); lt.tm_hour = 23; lt.tm_min = 59; lt.tm_sec = 50; t = mktime(&lt); }
      else t += 3L * 86400L;
      clockFake = true; clockSet(t);
      Serial.printf("[time] FAKE clock -> %s (press y to get real time back)\n", clockFull().c_str());
    } break;
    case 'r': Serial.println("Rebooting"); delay(100); ESP.restart(); break;
    default: Serial.printf("Unknown '%c' - press ?\n", c);
  }
}
