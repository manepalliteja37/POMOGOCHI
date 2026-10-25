// screens.h - every screen + what each button does on it (Stage 8a, 8b pet events, 8c care menu)
//
// Home -> MODE -> Setup -> (MODE cycles all 4 presets, one more MODE) -> To-do -> MODE -> Home
// Setup: OK start, BACK Home.   Focus: OK pause/resume, BACK x2 cancel.   Break: OK pause, BACK x2 skip.
// Home BACK opens Info (Wi-Fi name + web address): OK replays the intro, BACK/MODE go Home.
// Home OK opens the care menu: MODE next item, OK do it, BACK close (closes itself after 10 s).
// To-do (open tasks only): OK = focus on it (Setup), OK hold = done now, BACK next row, MODE Home.
// Task done? (after a linked focus): OK yes, BACK not yet (2 min = not yet), then the break starts.
// MODE hold = mute focus sounds, everywhere.
// b4: 1 h with no button (and no session) -> Sleeping screen (dim, Wi-Fi off). Any button wakes -> Setup.
// Onboarding: OK next, BACK previous (4 pages).   Wi-Fi help: shown when the setup hotspot opens.
#pragma once
#include "gfx.h"
#include "session.h"
#include "hourglass.h"
#include "petrules.h"

#define C_NEST 0x8A22

// ---- screen state ----
uint8_t  obPage = 0;
uint8_t  setupIdx = 2, setupBrowse = 0;
String   shownHHMM;
NetState shownNet = NET_OFF;
bool     shownMute = false;
uint16_t shownStreak = 0xFFFF;
char     shownSpeech[32] = "";
uint8_t  shownStage = 0xFF;
// Home idle animation
const uint8_t SEQ_WOBBLE[] = { 1, 2, 1, 2, 0 };
const uint8_t SEQ_BLINK[]  = { 1, 0 };
const uint8_t SEQ_TWITCH[] = { 2, 0 };
const uint8_t SEQ_SIGN[]   = { 1, 0 };
const uint8_t* seq = nullptr; uint8_t seqLen = 0, seqPos = 0; uint16_t seqStepMs = 0;
uint32_t seqNext = 0, nextIdleAt = 0;
// break + pause visuals
uint8_t  sleepF = 0; uint32_t sleepNext = 0;
bool     blinkOn = false; uint32_t blinkNext = 0;
int      shownBarW = -1;
// care menu + Play hop
bool     careOpen = false; uint8_t careSel = 0; uint32_t careIdle = 0;
uint8_t  hopStep = 0; uint32_t hopNext = 0;
// to-do scrolling
uint8_t  scrollOff = 0; uint32_t scrollNext = 0;

// =================== HOME ===================
const Sprite* stageSet(uint8_t st) { switch (st) { case 0: return EGG; case 1: return BABY; case 2: return KID; case 3: return TEEN; default: return ADULT; } }
uint8_t baseFrame(uint8_t st) {
  if (st == 1 || st == 2) { if (pet.fullness < 30) return 3; if (pet.energy < 30) return 4; }
  return 0;
}
#define C_SICK 0xAD55   // grey tint while Sick
int8_t petDY = 0;           // Play hop offset
void drawSpriteStage(const Sprite& s) {
  drawSprite((128 - s.w) / 2, 30 + (96 - s.h) / 2 + petDY, s, C_NAVY, (pet.moodFlags & 2) ? C_SICK : C_WHITE);
}
void drawPetFrame(uint8_t f) {
  if (pet.pendingEvent == 1) { drawSpriteStage(EGG[0]); return; }     // hatch not played yet
  uint8_t st = stageOf(pet.growth);
  drawSpriteStage(stageSet(st)[f ? f : baseFrame(st)]);
}
// hatch (egg cracks open) and stage-up, played when Home is shown
const uint8_t HATCH_SEQ[] = { 1, 2, 1, 2, 3, 3, 4, 4, 5, 5 };
uint8_t  evStep = 0; uint32_t evNext = 0;
void startPendingEvent() {
  if (pet.pendingEvent == 1) { evStep = 1; evNext = millis(); Serial.println("[ui] hatching!"); return; }
  uint8_t st = stageOf(pet.growth);
  pet.pendingEvent = 0; markDirty(DIRTY_PET);
  fastFill(0, 30, 128, 96, C_NAVY); shownStage = st; drawPetFrame(0);
  playChime(5);
  char b[22]; snprintf(b, sizeof(b), "I'm a%s %s now!", st == 4 ? "n" : "", STAGE_NAME[st]); say(b, 10000);
  Serial.printf("[ui] stage up -> %s\n", STAGE_NAME[st]);
}
bool hatchTick() {                                   // true while the hatch is playing
  if (!evStep) return false;
  if (millis() < evNext) return true;
  if (evStep <= sizeof(HATCH_SEQ)) { drawSpriteStage(EGG[HATCH_SEQ[evStep - 1]]); evStep++; evNext = millis() + 260; return true; }
  evStep = 0; pet.pendingEvent = 0; markDirty(DIRTY_PET);
  fastFill(0, 30, 128, 96, C_NAVY); shownStage = stageOf(pet.growth); drawPetFrame(0);
  playChime(4);
  char b[22]; snprintf(b, sizeof(b), "Hi! I'm %s!", pet.name); say(b, 10000);
  nextIdleAt = millis() + 4000;
  return false;
}

const char* homeSpeech() {
  static char b[22];
  if (millis() < speechUntil) return speechMsg;
  if (stageOf(pet.growth) == 0) return "Focus to hatch me!";
  if (pet.moodFlags & 2) return "I feel sick...";
  if (pet.moodFlags & 1) return "I'm worried...";
  if (pet.fullness < 30) return pet.carrots ? "Hungry! OK = feed" : "I'm Hungry.\nPls feed with task";   // b4: two lines
  if (pet.energy < 30)   return "I'm sleepy";
  if (pet.love < 30)     return "Pet me?";
  if (festivalToday()) return "Happy festival!";                        // b4
  if (pet.streakCur >= 2) { snprintf(b, sizeof(b), "Day %u streak!", pet.streakCur); return b; }
  return "Ready to focus?";
}
void drawSpeech(bool force) {
  const char* s = homeSpeech();
  if (!force && !strcmp(s, shownSpeech)) return;
  strlcpy(shownSpeech, s, sizeof(shownSpeech));
  fastFill(0, 14, 128, 16, C_NAVY);
  char* nl = strchr(shownSpeech, '\n');
  if (!nl) { txtC(18, shownSpeech, C_WHITE, C_NAVY); return; }
  *nl = 0; txtC(15, shownSpeech, C_WHITE, C_NAVY); txtC(23, nl + 1, C_WHITE, C_NAVY); *nl = '\n';   // b4: two-line speech
}

void iconCarrot(int y) { tft.fillTriangle(2, y + 3, 9, y + 3, 5, y + 9, C_ORANGE); tft.drawLine(5, y, 5, y + 2, C_GREEN); tft.drawLine(4, y + 1, 7, y, C_GREEN); }
void iconHeart(int y)  { tft.fillCircle(4, y + 3, 2, C_RED); tft.fillCircle(8, y + 3, 2, C_RED); tft.fillTriangle(2, y + 4, 10, y + 4, 6, y + 9, C_RED); }
void iconBolt(int y)   { tft.fillTriangle(8, y, 3, y + 5, 6, y + 5, C_YELLOW); tft.fillTriangle(5, y + 4, 9, y + 4, 4, y + 9, C_YELLOW); }
void drawBar(int y, uint8_t v, uint8_t icon) {
  if (v > 100) v = 100;
  fastFill(0, y, 16, 10, C_NAVY);
  if (icon == 0) iconCarrot(y); else if (icon == 1) iconHeart(y); else iconBolt(y);
  int w = 96 * v / 100;
  uint16_t c = v > 60 ? C_GREEN : v >= 30 ? C_AMBER : C_RED;
  fastFill(16, y + 1, w, 8, c); fastFill(16 + w, y + 1, 96 - w, 8, C_BARBG);
}
void drawBars() {
  drawBar(127, pet.fullness, 0); drawBar(138, pet.love, 1); drawBar(149, pet.energy, 2);
  fastFill(112, 127, 16, 33, C_NAVY);
  char c[4]; snprintf(c, sizeof(c), "x%u", pet.carrots); txt(115, 128, c, C_ORANGE, C_NAVY);   // carrots held
  if (pet.shield) { tft.drawRect(117, 140, 7, 8, C_GREEN); tft.drawLine(120, 142, 120, 145, C_GREEN); }   // shield held
}

void drawHome() {
  fastFill(0, 14, 128, 146, C_NAVY);
  drawTopBar(); drawSpeech(true);
  shownStage = stageOf(pet.growth); drawPetFrame(0); drawBars();
  seq = nullptr; evStep = 0; hopStep = 0; careOpen = false; nextIdleAt = millis() + random(3000, 8000);
  uiPet = false;
  if (pet.pendingEvent) startPendingEvent();
}

const uint8_t SEQ_FEED_S[] = { 5, 0 };   // baby/kid feed frame
const uint8_t SEQ_FEED_B[] = { 2, 0 };   // teen/adult feed frame
const uint8_t SEQ_PETTED[] = { 1, 0, 1, 0 };
// ---- care menu (drawn over the bars) ----
const char* CARE_NAME[] = { "Feed", "Pet", "Play" };
void drawCareItem(uint8_t i) {
  int x = 2 + i * 42;
  bool on = i == careSel;
  uint16_t bg = on ? C_AMBER : C_BARBG, fg = on ? C_BLACK : C_WHITE;
  fastFill(x, 129, 40, 29, bg);
  char d[8];
  if (i == 0) snprintf(d, sizeof(d), "x%u", pet.carrots);
  else if (i == 1) snprintf(d, sizeof(d), "%u left", pet.petsToday < PET_MAX ? PET_MAX - pet.petsToday : 0);
  else strcpy(d, (pet.moodFlags & MOOD_SICK) ? "sick" : pet.energy < PLAY_MIN_ENERGY ? "tired" : "-10 E");
  txt(x + (40 - (int)strlen(CARE_NAME[i]) * 6) / 2, 133, CARE_NAME[i], fg, bg);
  txt(x + (40 - (int)strlen(d) * 6) / 2, 146, d, fg, bg);
}
void drawCare() { fastFill(0, 127, 128, 33, C_NAVY); for (uint8_t i = 0; i < 3; i++) drawCareItem(i); }
void careOpenMenu() {
  if (evStep) return;
  if (stageOf(pet.growth) == 0) { say("Hatch me first!", 3000); return; }
  careOpen = true; careSel = 0; careIdle = millis(); drawCare();
  Serial.println("[care] menu open");
}
void careClose() { careOpen = false; fastFill(0, 127, 128, 33, C_NAVY); drawBars();   // wipe the whole strip: bars leave 1-px gaps
  Serial.println("[care] menu closed"); }
void careDo() {
  careIdle = millis();
  uint8_t st = stageOf(pet.growth);
  bool ok = careSel == 0 ? petFeed() : careSel == 1 ? petPet() : petPlay();
  if (ok) {
    if (careSel == 0)      { seq = st <= 2 ? SEQ_FEED_S : SEQ_FEED_B; seqLen = 2; seqStepMs = 1300; }
    else if (careSel == 1) { seq = SEQ_PETTED; seqLen = sizeof(SEQ_PETTED); seqStepMs = 220; }
    else                   { hopStep = 1; hopNext = millis(); }
    seqPos = 0; seqNext = millis();
    playChime(8 + careSel);                          // b2: eat / love / play sound after the press
  }
  drawCare();
}
void drawLoveHeart() { tft.fillCircle(58, 34, 3, C_RED); tft.fillCircle(64, 34, 3, C_RED); tft.fillTriangle(55, 35, 67, 35, 61, 42, C_RED); }
void hopTick() {                                     // Play: three little hops
  static const int8_t H[] = { -6, 0, -8, 0, -6, 0 };
  if (!hopStep || millis() < hopNext) return;
  fastFill(0, 30, 128, 96, C_NAVY);
  petDY = H[hopStep - 1]; drawPetFrame(0); petDY = 0;
  hopNext = millis() + 170;
  if (++hopStep > sizeof(H)) { hopStep = 0; nextIdleAt = millis() + 4000; }
}

void homeTick() {
  uint32_t now = millis();
  if (hatchTick()) { if (uiPet) { uiPet = false; drawBars(); drawSpeech(false); } return; }
  if (pet.pendingEvent && !seq && !hopStep) { if (careOpen) careOpen = false; startPendingEvent(); return; }
  if (careOpen && now - careIdle > 10000) careClose();
  hopTick();
  if (uiPet) {
    uiPet = false;
    uint8_t st = stageOf(pet.growth);
    if (st != shownStage) { shownStage = st; fastFill(0, 30, 128, 96, C_NAVY); seq = nullptr; }
    if (!seq && !hopStep) drawPetFrame(0);
    if (careOpen) drawCare(); else drawBars();
    drawSpeech(false);
  }
  if (hopStep) return;
  if (!seq && now >= nextIdleAt) {                 // start an idle move every 3-8 s
    uint8_t st = stageOf(pet.growth);
    if (st == 0)      { seq = SEQ_WOBBLE; seqLen = sizeof(SEQ_WOBBLE); seqStepMs = 160; }
    else if (st <= 2) { bool b = random(2); seq = b ? SEQ_BLINK : SEQ_TWITCH; seqLen = 2; seqStepMs = b ? 150 : 300; }
    else              { seq = SEQ_SIGN; seqLen = 2; seqStepMs = 700; }
    seqPos = 0; seqNext = now;
  }
  if (seq && now >= seqNext) {
    drawPetFrame(seq[seqPos]); if (seq == SEQ_PETTED) drawLoveHeart(); seqNext = now + seqStepMs;
    if (++seqPos >= seqLen) { seq = nullptr; nextIdleAt = now + random(3000, 8000); fastFill(50, 30, 24, 14, C_NAVY); drawPetFrame(0); }
  }
}

// =================== SETUP ===================
void drawSetupPreset() {
  uint8_t m = presetMin(setupIdx);
  fastFill(0, 30, 128, 60, C_NAVY);
  char n[4]; snprintf(n, sizeof(n), "%u", m);
  int len = strlen(n), x0 = (128 - (len * 24 + 48)) / 2;
  txt(x0, 34, n, C_AMBER, C_NAVY, 4);
  txt(x0 + len * 24, 48, " min", C_WHITE, C_NAVY, 2);
  char b[24];
  if (m < 25) strcpy(b, "Test - no break");
  else snprintf(b, sizeof(b), "Then %u min break", breakAfter(m));
  txtC(76, b, C_DIM, C_NAVY);
}
void drawSetup() {
  fastFill(0, 14, 128, 146, C_NAVY);
  drawTopBar();
  if (linkText[0]) { char t[22]; strlcpy(t, linkText, sizeof(t)); txtC(19, t, C_AMBER, C_NAVY); }   // 9.3b: focus for this task
  else txtC(19, "POMODORO", C_AMBER, C_NAVY);
  drawSetupPreset();
  drawDots(64, 98, settings.cycleDots, 5, C_NAVY);
  char b[24]; snprintf(b, sizeof(b), "Today: %u session%s", settings.todayCount, settings.todayCount == 1 ? "" : "s");
  txtC(114, b, C_WHITE, C_NAVY);
  fastFill(0, 138, 128, 22, C_GREY);
  txt(2, 141, linkText[0] ? "MODE 25/50 OK start" : "MODE next  OK start", C_WHITE, C_GREY);
  txt(2, 151, linkText[0] ? "BACK to-do" : "BACK home", C_WHITE, C_GREY);
}
void openSetup() { linkText[0] = 0; setupIdx = settings.lastPreset & 3; setupBrowse = 0; goScreen(SC_SETUP); }
void openSetupFor(int8_t ti) {
  if (ti < 0) return;
  strlcpy(linkText, todos.item[ti].text, sizeof(linkText));
  setupIdx = (settings.lastPreset & 3) >= 2 ? (settings.lastPreset & 3) : 2;   // b4: a task gets 25 or 50 only, never a test preset
  setupBrowse = 0; goScreen(SC_SETUP);
  Serial.printf("[todo] focus on: %s\n", linkText);
}

// =================== FOCUS ===================
void drawFocusHeader() {
  fastFill(0, 0, 86, 13, C_BLACK);
  char b[16];
  if (confirmShown) txt(2, 3, "BACK=cancel?", C_RED, C_BLACK);
  else if (!timer.running && blinkOn) txt(2, 3, "PAUSED", C_WHITE, C_BLACK);
  else { snprintf(b, sizeof(b), "FOCUS %um%s", presetMin(settings.lastPreset), fastMode ? " F" : ""); txt(2, 3, b, C_AMBER, C_BLACK); }
  drawDots(106, 6, settings.cycleDots, 3, C_BLACK);
}
void drawFocus() {
  fastFill(0, 0, 128, 160, C_BLACK);
  streamDrawnTo = -1; blinkOn = false;
  drawFocusHeader(); drawGlassOutline(); updateSand(true);
  drawTime(34, 143, timer.remaining(), C_BLACK, true);
}
void focusTick() {
  if (uiHeader) { uiHeader = false; drawFocusHeader(); }
  if (timer.running) { updateSand(false); drawTime(34, 143, timer.remaining(), C_BLACK, false); }
  else if (millis() >= blinkNext) { blinkNext = millis() + 500; blinkOn = !blinkOn; drawFocusHeader(); }
  animateStream();
}

// =================== BREAK ===================
void drawBreakHeader() {
  fastFill(0, 0, 86, 13, C_BLACK);
  char b[16];
  if (confirmShown) txt(2, 3, "BACK=skip?", C_RED, C_BLACK);
  else if (breakWaiting()) txt(2, 3, "BREAK READY", C_GREEN, C_BLACK);
  else if (!timer.running && blinkOn) txt(2, 3, "PAUSED", C_WHITE, C_BLACK);
  else { if (longBreak) strcpy(b, "LONG BREAK"); else snprintf(b, sizeof(b), "BREAK %um", breakMins); txt(2, 3, b, C_GREEN, C_BLACK); }
  drawDots(106, 6, settings.cycleDots, 3, C_BLACK);
  fastFill(0, 148, 128, 12, C_BLACK);
  txtC(150, breakWaiting() ? "OK = start break" : timer.running ? "OK pause  BACKx2 skip" : "OK = resume", C_DIM, C_BLACK);
}
void drawBreakBar(bool force) {
  uint32_t dur = timer.durationMs ? timer.durationMs : 1;
  int w = (int)((uint64_t)124 * timer.remaining() / dur);
  if (!force && w == shownBarW) return;
  shownBarW = w;
  fastFill(2, 118, w, 5, C_GREEN); fastFill(2 + w, 118, 124 - w, 5, C_BARBG);
}
void drawBreak() {
  fastFill(0, 0, 128, 160, C_BLACK);
  blinkOn = false; sleepF = 0; sleepNext = millis() + 2000;
  drawBreakHeader();
  drawSprite(0, 15, SLEEP[0], C_BLACK);
  drawBreakBar(true);
  drawTime(34, 128, timer.remaining(), C_BLACK, true);
}
void breakTick() {
  uint32_t now = millis();
  if (uiHeader) { uiHeader = false; drawBreakHeader(); }
  if (timer.running) {
    drawBreakBar(false); drawTime(34, 128, timer.remaining(), C_BLACK, false);
    if (now >= sleepNext) { sleepNext = now + 2000; sleepF ^= 1; drawSprite(0, 15, SLEEP[sleepF], C_BLACK); }
  } else if (!breakWaiting() && now >= blinkNext) { blinkNext = now + 500; blinkOn = !blinkOn; drawBreakHeader(); }
}

// =================== TO-DO ===================
void drawTodoText(uint8_t r) {
  int y = 14 + r * 18;
  bool sel = r == todoSel;
  uint16_t bg = sel ? C_SEL : C_NAVY;
  int8_t ix = openAt(r); if (ix < 0) return;
  const TodoItem& it = todos.item[ix];
  fastFill(20, y, 108, 18, bg);
  char line[19]; size_t n = strlen(it.text), off = sel ? scrollOff : 0;
  if (off > n) off = 0;
  strlcpy(line, it.text + off, sizeof(line));
  if (!sel && n > 18) line[17] = '~';
  txt(20, y + 5, line, it.done ? C_DONE : C_WHITE, bg);
}
void drawTodoRow(uint8_t r) {
  int y = 14 + r * 18;
  uint8_t n = openCount();
  bool sel = (r == todoSel && r < n);
  uint16_t bg = sel ? C_SEL : C_NAVY;
  fastFill(0, y, 20, 18, bg);
  if (r >= n) { fastFill(20, y, 108, 18, C_NAVY); return; }
  if (sel) txt(1, y + 5, ">", C_AMBER, bg);
  tft.drawRect(8, y + 5, 8, 8, todos.item[openAt(r)].carry ? C_AMBER : C_WHITE);   // amber box = carried over
  drawTodoText(r);
}
void drawTodo() {
  uint8_t done = doneCount(), open = openCount();
  fastFill(0, 0, 128, 14, C_GREY);
  txt(2, 3, "TO-DO", C_WHITE, C_GREY);
  char b[16]; snprintf(b, sizeof(b), "%u left %u done", open, done);
  txt(126 - strlen(b) * 6, 3, b, C_WHITE, C_GREY);
  for (uint8_t r = 0; r < 7; r++) drawTodoRow(r);
  if (!open && done) {
    txtC(46, "All done today!", C_GREEN, C_NAVY);
    txtC(62, "Add more on the", C_WHITE, C_NAVY);
    txtC(74, "web page", C_WHITE, C_NAVY);
  } else if (!open) {
    txt(4, 40, "No tasks.", C_WHITE, C_NAVY);
    txt(4, 58, "Add them at", C_WHITE, C_NAVY);
    if (netState == NET_ONLINE) { String u = "http://" + WiFi.localIP().toString(); txt(4, 72, u.c_str(), C_AMBER, C_NAVY); }
    else txt(4, 72, "the web page", C_AMBER, C_NAVY);
  }
  fastFill(0, 140, 128, 20, C_GREY);
  txt(2, 142, "OK focus  BACK down", C_WHITE, C_GREY);
  txt(2, 151, "hold OK=done MODE>", C_WHITE, C_GREY);
  scrollOff = 0; scrollNext = millis() + 1200;
}
void todoTick() {
  if (uiTodo) { uiTodo = false; drawTodo(); return; }
  int8_t ix = openAt(todoSel);
  if (ix < 0 || millis() < scrollNext) return;
  size_t n = strlen(todos.item[ix].text);
  if (n <= 18) return;
  if (scrollOff >= n - 18) { scrollOff = 0; scrollNext = millis() + 1200; }        // back to start, pause
  else { scrollOff++; scrollNext = millis() + (scrollOff >= n - 18 ? 1200 : 280); }  // pause at the end
  drawTodoText(todoSel);
}

// =================== TASK DONE? (9.3b) ===================
void drawAsk() {
  fastFill(0, 0, 128, 160, C_NAVY);
  fastFill(0, 0, 128, 14, C_AMBER); txt(2, 3, "FOCUS DONE", C_BLACK, C_AMBER);
  txtC(26, "Task done?", C_WHITE, C_NAVY, 2);
  const char* t = (askIdx >= 0 && askIdx < todos.count) ? todos.item[askIdx].text : "";
  size_t n = strlen(t);
  for (uint8_t l = 0; l < 2 && l * 18 < n; l++) { char b[19]; strlcpy(b, t + l * 18, sizeof(b)); txtC(56 + l * 12, b, C_AMBER, C_NAVY); }
  char c[24]; snprintf(c, sizeof(c), "Focus sessions: %u", (askIdx >= 0 && askIdx < todos.count) ? todos.item[askIdx].focus : 0);
  txtC(96, c, C_DIM, C_NAVY);
  if (askBreak) { snprintf(c, sizeof(c), "Then %u min break", askBreak); txtC(112, c, C_DIM, C_NAVY); }
  fastFill(0, 140, 128, 20, C_GREY);
  txt(2, 142, "OK  = yes, done!", C_WHITE, C_GREY);
  txt(2, 151, "BACK = not yet", C_WHITE, C_GREY);
}
void askTick() { if (millis() - askAt > 120000UL) askAnswer(false); }

// =================== WI-FI HELP (setup hotspot is on) ===================
void drawWifiHelp() {
  fastFill(0, 0, 128, 160, C_NAVY);
  fastFill(0, 0, 128, 14, C_AMBER);
  txt(2, 3, "WI-FI SETUP", C_BLACK, C_AMBER);
  static const char* const L[] = { "1. On your phone open", "   Wi-Fi settings", "2. Join (no password)", "*  Pomodoro-Setup",
                                   "3. Page opens itself", "*  or 192.168.4.1", "4. Pick your Wi-Fi /", "   hotspot + password" };
  for (uint8_t i = 0; i < 8; i++) { bool hi = L[i][0] == '*'; txt(2, 22 + i * 13, hi ? L[i] + 1 : L[i], hi ? C_AMBER : C_WHITE, C_NAVY); }
  fastFill(0, 140, 128, 20, C_GREY);
  txt(2, 146, "Any button = Home", C_WHITE, C_GREY);
}

// =================== ONBOARDING ===================
void drawOnboard() {
  fastFill(0, 0, 128, 160, C_NAVY);
  static const char* const TITLE[] = { "Welcome!", "Buttons", "Rewards", "Wi-Fi" };
  txtC(8, TITLE[obPage], C_AMBER, C_NAVY, 2);
  if (obPage == 0) {
    tft.fillRoundRect(36, 82, 56, 10, 5, C_NEST);
    drawSprite(51, 36, EGG[0], C_NAVY);
    txtC(104, "Focus to grow", C_WHITE, C_NAVY);
    txtC(116, "your rabbit.", C_WHITE, C_NAVY);
  } else if (obPage == 1) {
    static const char* const L[] = { "OK   = select/start", "MODE = next screen", "MODE hold = mute", "BACK = back", "BACK x2 = cancel" };
    for (uint8_t i = 0; i < 5; i++) txt(4, 36 + i * 18, L[i], C_WHITE, C_NAVY);
  } else if (obPage == 2) {
    static const char* const L[] = { "25/50 min focus", "  = carrots+growth", "Full break", "  = energy", "Cancel / skip", "  = sad rabbit" };
    for (uint8_t i = 0; i < 6; i++) txt(4, 34 + i * 16, L[i], i % 2 ? C_AMBER : C_WHITE, C_NAVY);
  } else {
    if (netState == NET_ONLINE) {
      txt(4, 36, "Connected to", C_WHITE, C_NAVY);
      char s[22]; strlcpy(s, wifiCfg.ssid, sizeof(s)); txt(4, 50, s, C_AMBER, C_NAVY);
      txt(4, 74, "Edit to-dos at", C_WHITE, C_NAVY);
      String u = "http://" + WiFi.localIP().toString(); txt(4, 88, u.c_str(), C_AMBER, C_NAVY);
    } else if (netState == NET_PORTAL) {
      txt(4, 36, "On your phone join", C_WHITE, C_NAVY);
      txt(4, 50, "Pomodoro-Setup", C_AMBER, C_NAVY);
      txt(4, 64, "then open", C_WHITE, C_NAVY);
      txt(4, 78, "192.168.4.1", C_AMBER, C_NAVY);
      txt(4, 98, "(or skip: OK)", C_DIM, C_NAVY);
    } else {
      txt(4, 36, "Connecting...", C_WHITE, C_NAVY);
      char s[22]; strlcpy(s, wifiCfg.ssid, sizeof(s)); txt(4, 50, s, C_AMBER, C_NAVY);
    }
  }
  fastFill(0, 140, 128, 20, C_GREY);
  if (obPage) txt(2, 146, "BACK", C_WHITE, C_GREY);
  char p[6]; snprintf(p, sizeof(p), "%u/4", obPage + 1); txtC(146, p, C_DIM, C_GREY);
  const char* r = obPage < 3 ? "OK next" : "OK start";
  txt(126 - strlen(r) * 6, 146, r, C_WHITE, C_GREY);
}
void finishOnboarding() {
  settings.onboarded = 1; markDirty(DIRTY_SET);
  goScreen(SC_HOME);
  Serial.println("[ui] onboarding done -> Home");
}

// =================== dispatch ===================
// =================== INFO (Home BACK) ===================
void drawInfo() {
  fastFill(0, 0, 128, 160, C_NAVY);
  fastFill(0, 0, 128, 14, C_GREY); txt(2, 3, "INFO", C_WHITE, C_GREY);
  txt(4, 22, "Wi-Fi", C_DIM, C_NAVY);
  if (netState == NET_ONLINE) {
    String ss = WiFi.SSID(); if (ss.length() > 20) ss = ss.substring(0, 20);
    txt(4, 33, ss.c_str(), C_WHITE, C_NAVY);
    txt(4, 44, "connected", C_GREEN, C_NAVY);
    txt(4, 62, "Web page", C_DIM, C_NAVY);
    String ip = WiFi.localIP().toString();
    txt(4, 73, "http://", C_AMBER, C_NAVY); txt(4, 84, ip.c_str(), C_AMBER, C_NAVY, ip.length() <= 10 ? 2 : 1);
    txt(4, 104, "or http://", C_WHITE, C_NAVY); txt(4, 115, "pomodoro.local", C_WHITE, C_NAVY);
  } else if (netState == NET_PORTAL) {
    txt(4, 33, "setup hotspot on", C_AMBER, C_NAVY);
    txt(4, 44, "Pomodoro-Setup", C_WHITE, C_NAVY);
    txt(4, 62, "Web page", C_DIM, C_NAVY); txt(4, 73, "http://192.168.4.1", C_AMBER, C_NAVY);
  } else {
    txt(4, 33, "not connected", C_RED, C_NAVY);
    txt(4, 44, "trying again...", C_WHITE, C_NAVY);
  }
  txt(4, 128, FW_VER_SHORT, C_DIM, C_NAVY);
  fastFill(0, 140, 128, 20, C_GREY);
  txt(2, 142, "OK replay intro", C_WHITE, C_GREY);
  txt(2, 151, "BACK home", C_WHITE, C_GREY);
}

// =================== SLEEPING (b4) ===================
const uint32_t IDLE_SLEEP_MS = 60UL * 60UL * 1000UL;   // 1 hour with no button
const uint16_t SLEEP_BL = 40;                          // dim backlight while asleep (30..1023)
#define C_ZZZ 0x9DFF
uint8_t  zStep = 0; uint32_t zNext = 0; String sleepHM;
void drawZzz() {
  fastFill(64, 0, 64, 22, C_BLACK);
  if (zStep >= 1) txt(78, 13, "z", C_ZZZ, C_BLACK, 1);
  if (zStep >= 2) txt(90, 6, "z", C_ZZZ, C_BLACK, 2);
  if (zStep >= 3) txt(108, 0, "Z", C_ZZZ, C_BLACK, 2);
}
void drawSleepClock() {
  sleepHM = clockHHMM();
  fastFill(0, 126, 128, 18, C_BLACK);
  txtC(128, sleepHM.c_str(), C_DIM, C_BLACK, 2);
}
void drawSleep() {
  fastFill(0, 0, 128, 160, C_BLACK);
  sleepF = 0; sleepNext = millis() + 2000; zStep = 0; zNext = millis();
  drawSprite(0, 22, SLEEP[0], C_BLACK);
  drawSleepClock();
  txtC(150, "any button = wake", C_GREY, C_BLACK);
}
void sleepScrTick() {
  uint32_t now = millis();
  if (now >= zNext) { zNext = now + 700; zStep = (zStep + 1) & 3; drawZzz(); }
  if (now >= sleepNext) { sleepNext = now + 2000; sleepF ^= 1; drawSprite(0, 22, SLEEP[sleepF], C_BLACK); }
  if (clockHHMM() != sleepHM) drawSleepClock();
}
bool canSleep() {
  return !sessionActive() && !portalOn && !evStep && !hopStep && !pet.pendingEvent &&
         (scr == SC_HOME || scr == SC_SETUP || scr == SC_TODO || scr == SC_INFO);
}
void enterSleep() {
  careOpen = false; linkText[0] = 0;
  netSleep();
  goScreen(SC_SLEEP);
  analogWrite(TFT_LED, SLEEP_BL);
  Serial.println("[sleep] 1 h idle -> sleeping");
}
void wakeUp() {
  applyBacklight();
  netWake();
  openSetup();
  Serial.println("[sleep] button -> awake (Setup)");
}

void drawFull() {
  switch (scr) {
    case SC_ONBOARD: drawOnboard(); break;
    case SC_HOME:    drawHome(); break;
    case SC_SETUP:   drawSetup(); break;
    case SC_FOCUS:   drawFocus(); break;
    case SC_BREAK:   drawBreak(); break;
    case SC_TODO:    uiTodo = false; drawTodo(); break;
    case SC_WIFI:    drawWifiHelp(); break;
    case SC_INFO:    drawInfo(); break;
    case SC_ASK:     drawAsk(); break;
    case SC_SLEEP:   drawSleep(); break;
  }
}

void screenTick() {
  static uint32_t chk = 0;
  uint32_t now = millis();
  if (now - chk >= 250) {
    chk = now;
    String hm = clockHHMM();
    bool netChanged = netState != shownNet;
    bool topCh = hm != shownHHMM || netChanged || muted != shownMute || pet.streakCur != shownStreak;
    shownHHMM = hm; shownNet = netState; shownMute = muted; shownStreak = pet.streakCur;
    if (netChanged) {
      if (netState == NET_PORTAL && !sessionActive() && scr != SC_ONBOARD && scr != SC_ASK) goScreen(SC_WIFI);
      else if (scr == SC_WIFI && netState != NET_PORTAL) goScreen(SC_HOME);
      else if (scr == SC_ONBOARD && obPage == 3) uiFull = true;
      else if (scr == SC_TODO && !openCount()) uiTodo = true;
      else if (scr == SC_INFO) uiFull = true;
    }
    if (topCh && !uiFull && (scr == SC_HOME || scr == SC_SETUP)) drawTopBar();
    if (scr == SC_HOME && !uiFull) drawSpeech(false);
  }
  if (scr != SC_SLEEP && millis() - lastActMs >= IDLE_SLEEP_MS && canSleep()) enterSleep();
  if (uiFull) { uiFull = false; uiHeader = false; drawFull(); return; }
  switch (scr) {
    case SC_HOME:  homeTick(); break;
    case SC_FOCUS: focusTick(); break;
    case SC_BREAK: breakTick(); break;
    case SC_TODO:  todoTick(); break;
    case SC_ASK:   askTick(); break;
    case SC_SLEEP: sleepScrTick(); break;
    default: break;
  }
}

void uiEvent(const Event& e) {
  if (e.type == EV_CONFIRM_TIMEOUT && scr == SC_SLEEP) return;
  lastActMs = millis();
  if (scr == SC_SLEEP) { wakeUp(); return; }        // b4: the waking press does nothing else
  if (e.type == EV_LONG && e.btn == B_MODE) { setMuted(!muted); Serial.printf("[btn] mute %s\n", muted ? "ON" : "OFF"); return; }
  bool sh = e.type == EV_SHORT, dbl = e.type == EV_DOUBLE, to = e.type == EV_CONFIRM_TIMEOUT;
  switch (scr) {
    case SC_ONBOARD:
      if (sh && e.btn == B_OK) { if (obPage < 3) { obPage++; uiFull = true; } else finishOnboarding(); }
      else if ((sh || dbl) && e.btn == B_BACK && obPage > 0) { obPage--; uiFull = true; }
      break;
    case SC_WIFI: if (sh) goScreen(SC_HOME); break;
    case SC_INFO:
      if (sh && e.btn == B_OK) { obPage = 0; goScreen(SC_ONBOARD); }
      else if ((sh || dbl) && (e.btn == B_BACK || e.btn == B_MODE)) goScreen(SC_HOME);
      break;
    case SC_HOME:
      if (careOpen) {
        if (!sh && !dbl) break;
        if (e.btn == B_MODE)      { careSel = (careSel + 1) % 3; careIdle = millis(); drawCare(); }
        else if (e.btn == B_OK)   careDo();
        else if (e.btn == B_BACK) careClose();
        break;
      }
      if (sh && e.btn == B_MODE) openSetup();
      else if (sh && e.btn == B_OK) careOpenMenu();
      else if (sh && e.btn == B_BACK && !evStep) goScreen(SC_INFO);
      break;
    case SC_SETUP:
      if (!sh) break;
      if (e.btn == B_MODE) {
        if (linkText[0]) { setupIdx = setupIdx == 2 ? 3 : 2; drawSetupPreset(); }   // b4: task focus toggles 25 <-> 50 (BACK = To-do)
        else if (++setupBrowse >= 4) { linkText[0] = 0; goScreen(SC_TODO); }  // saw all 4 presets -> next screen
        else { setupIdx = (setupIdx + 1) & 3; drawSetupPreset(); }
      } else if (e.btn == B_OK) focusStart(setupIdx);
      else if (e.btn == B_BACK) { bool l = linkText[0]; linkText[0] = 0; goScreen(l ? SC_TODO : SC_HOME); }
      break;
    case SC_TODO: {
      uint8_t n = openCount();
      if (sh && e.btn == B_MODE) goScreen(SC_HOME);
      else if (sh && e.btn == B_OK && n) openSetupFor(openAt(todoSel));
      else if (e.type == EV_LONG && e.btn == B_OK && n) {
        int8_t ix = openAt(todoSel);
        todos.item[ix].done = 1; markDirty(DIRTY_TODO); playChime(10);
        Serial.printf("[btn] done: %s\n", todos.item[ix].text);
      } else if ((sh || dbl) && e.btn == B_BACK && n) { todoSel = (todoSel + 1) % n; uiTodo = true; }
      } break;
    case SC_ASK:
      if (sh && e.btn == B_OK) askAnswer(true);
      else if ((sh || dbl) && e.btn == B_BACK) askAnswer(false);
      break;
    case SC_FOCUS:
      if (sh && e.btn == B_OK) { confirmShown = false; focusPauseToggle(); }
      else if (sh && e.btn == B_BACK) { confirmShown = true; uiHeader = true; }
      else if (dbl && e.btn == B_BACK) focusCancel();
      else if (to && confirmShown) { confirmShown = false; uiHeader = true; }
      break;
    case SC_BREAK:
      if (sh && e.btn == B_OK) { confirmShown = false; breakPauseToggle(); }
      else if (sh && e.btn == B_BACK) { confirmShown = true; uiHeader = true; }
      else if (dbl && e.btn == B_BACK) breakSkip();
      else if (to && confirmShown) { confirmShown = false; uiHeader = true; }
      break;
  }
}
