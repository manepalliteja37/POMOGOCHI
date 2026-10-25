// Stage 6a - LittleFS storage test (Serial only, no display needed)
// Tests: save/load, survives reset + USB unplug + re-flash, 2-slot crash safety, power-cut torture.
//
// Arduino IDE settings (pick ONCE and never change, or the data area is wiped):
//   Board: NodeMCU 1.0 (ESP-12E)   Flash Size: 4MB (FS:2MB OTA:~1019KB)
//   Erase Flash: Only Sketch       Serial Monitor: 115200, Newline
//
// Serial commands:
//   ?  status of every save file          l  list files on flash
//   p  change pet (Growth+1, Love-1)      m  toggle mute in settings
//   t  add a test to-do (max 7)           d  tick/untick to-do #1
//   x  clear all to-dos                   w  power-cut torture ON/OFF (resumes after power-up)
//   r  reboot                             F  format flash (asks y to confirm)

#include <Arduino.h>
#include <LittleFS.h>
#include "storage.h"

// ---- all structs/enums at the top (Arduino auto-prototypes) ----
// Pet fields are placeholders until the pet behaviour plan; version handles changes later.
struct Settings {
  uint8_t  lastPreset = 1;      // 0=1min 1=25 2=50
  uint8_t  muted = 0;
  uint8_t  volume = 0;          // 0 soft 1 mid 2 loud
  uint8_t  hbSel = 3;           // 264 Hz deep 50bpm
  uint16_t backlight = 1023;
  uint8_t  breakAuto = 1;
  uint8_t  onboarded = 0;
  uint8_t  todayCount = 0;
  uint8_t  cycleDots = 0;
  uint32_t todayDate = 0;       // yyyymmdd, used by Stage 6b rollover
  uint32_t bootCount = 0;       // test: proves the file survives
  uint8_t  tortureOn = 0;       // test
  uint32_t cutsCaught = 0;      // test: boots that found a half-written slot
};

struct PetSave {
  uint8_t  stage = 0;           // 0 egg .. 5 adult
  uint16_t growth = 0;
  uint8_t  fullness = 80, energy = 80, love = 80;
  uint8_t  moodFlags = 0;       // worried / sick
  uint8_t  sickDays = 0;
  uint16_t streakCur = 0, streakBest = 0;
  uint8_t  shield = 0;
  uint8_t  last7 = 0;           // bit per day
  uint32_t lastStreakDay = 0;   // yyyymmdd
  uint32_t tortureWrites = 0;   // test
  uint8_t  reserved[16] = {0};
};

struct TodoItem { char text[33]; uint8_t done; };
struct TodoList { uint8_t count = 0; TodoItem item[7]; };

Settings settings;
PetSave  pet;
TodoList todos;

Blob setBlob  = { "settings", 1, &settings, sizeof(settings) };
Blob petBlob  = { "pet",      1, &pet,      sizeof(pet) };
Blob todoBlob = { "todos",    1, &todos,    sizeof(todos) };

bool     formatAsk = false;
uint32_t lastTortureMs = 0, slowestSaveUs = 0;

void loadOne(Blob& b) {
  bool ok = storeLoad(b);
  Serial.printf("[load] %-9s %s", b.name, ok ? "OK" : "none (defaults)");
  if (ok) Serial.printf(" slot %c seq %lu", 'a' + b.slot, (unsigned long)b.seq);
  if (b.badSlots) Serial.printf("  << %u broken slot ignored (power cut caught)", b.badSlots);
  Serial.println();
}

void printStatus() {
  FSInfo fi; LittleFS.info(fi);
  Serial.printf("\nFlash FS: %u / %u bytes used\n", fi.usedBytes, fi.totalBytes);
  storeInfo(setBlob); storeInfo(petBlob); storeInfo(todoBlob);
  Serial.printf("Settings: boots %lu, mute %u, preset %u, hb %u, torture %s, cuts caught %lu\n",
    (unsigned long)settings.bootCount, settings.muted, settings.lastPreset, settings.hbSel,
    settings.tortureOn ? "ON" : "off", (unsigned long)settings.cutsCaught);
  Serial.printf("Pet: growth %u, love %u, torture writes %lu, slowest save %lu us\n",
    pet.growth, pet.love, (unsigned long)pet.tortureWrites, (unsigned long)slowestSaveUs);
  Serial.printf("To-dos (%u/7):\n", todos.count);
  for (uint8_t i = 0; i < todos.count; i++) Serial.printf("  %u [%c] %s\n", i + 1, todos.item[i].done ? 'x' : ' ', todos.item[i].text);
  Serial.printf("Free heap: %u\n\n", ESP.getFreeHeap());
}

void listFiles() {
  Dir d = LittleFS.openDir("/");
  Serial.println("Files:");
  while (d.next()) Serial.printf("  %-14s %u B\n", d.fileName().c_str(), (unsigned)d.fileSize());
}

void saveAndReport(Blob& b) {
  bool ok = storeSave(b);
  if (b.lastSaveUs > slowestSaveUs) slowestSaveUs = b.lastSaveUs;
  Serial.printf("[save] %s %s -> slot %c seq %lu (%lu us)\n", b.name, ok ? "OK" : "FAILED",
                'a' + b.slot, (unsigned long)b.seq, (unsigned long)b.lastSaveUs);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n\nStage 6a - LittleFS storage");
  uint32_t t0 = millis();
  if (!storeBegin()) { Serial.println("LittleFS FAILED - check Flash Size has an FS area"); return; }
  Serial.printf("Mount: %lu ms\n", (unsigned long)(millis() - t0));

  loadOne(setBlob); loadOne(petBlob); loadOne(todoBlob);
  if (setBlob.badSlots + petBlob.badSlots + todoBlob.badSlots) settings.cutsCaught++;
  settings.bootCount++;
  saveAndReport(setBlob);
  printStatus();
  if (settings.tortureOn) Serial.println(">> Torture resumes. Pull USB any time, plug back, check the [load] lines.");
}

void loop() {
  if (settings.tortureOn && millis() - lastTortureMs >= 20) {
    lastTortureMs = millis();
    pet.tortureWrites++; pet.growth++;
    storeSave(petBlob);
    if (petBlob.lastSaveUs > slowestSaveUs) slowestSaveUs = petBlob.lastSaveUs;
    if (pet.tortureWrites % 100 == 0)
      Serial.printf("torture %lu writes, seq %lu, last %lu us, slowest %lu us\n", (unsigned long)pet.tortureWrites,
                    (unsigned long)petBlob.seq, (unsigned long)petBlob.lastSaveUs, (unsigned long)slowestSaveUs);
    if (pet.tortureWrites >= 20000) { settings.tortureOn = 0; saveAndReport(setBlob); Serial.println("Torture auto-stopped at 20000"); }
  }

  if (!Serial.available()) return;
  char c = Serial.read();
  if (c == '\n' || c == '\r') return;
  if (formatAsk) {
    formatAsk = false;
    if (c == 'y') { LittleFS.format(); Serial.println("Formatted. Rebooting."); delay(200); ESP.restart(); }
    Serial.println("Format cancelled"); return;
  }
  switch (c) {
    case '?': printStatus(); break;
    case 'l': listFiles(); break;
    case 'p': pet.growth++; if (pet.love) pet.love--; saveAndReport(petBlob); break;
    case 'm': settings.muted ^= 1; saveAndReport(setBlob); break;
    case 't':
      if (todos.count >= 7) { Serial.println("List full, 7/7"); break; }
      snprintf(todos.item[todos.count].text, 33, "Test task %u", todos.count + 1);
      todos.item[todos.count].done = 0; todos.count++; saveAndReport(todoBlob); break;
    case 'd': if (todos.count) { todos.item[0].done ^= 1; saveAndReport(todoBlob); } break;
    case 'x': todos.count = 0; saveAndReport(todoBlob); break;
    case 'w': settings.tortureOn ^= 1; saveAndReport(setBlob);
              if (!settings.tortureOn) saveAndReport(petBlob);
              Serial.printf("Torture %s\n", settings.tortureOn ? "ON - now pull USB randomly, 10 times" : "OFF"); break;
    case 'r': Serial.println("Rebooting"); delay(100); ESP.restart(); break;
    case 'F': formatAsk = true; Serial.println("Format ALL saved data? press y"); break;
    default: Serial.printf("Unknown '%c' - press ?\n", c);
  }
}
