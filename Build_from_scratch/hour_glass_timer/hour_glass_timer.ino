// Stage 2 - Hourglass rendering on top of the locked Stage 1 timer engine
// Board: NodeMCU 1.0 | Libraries: Adafruit GFX, Adafruit ST7735
// Display (Stage 0 lock): BLACKTAB, rotation 0, CS D8, DC D3, RST -1, LED D4 PWM
//
// Serial @115200 (same as Stage 1):
//   1/2/3/4 preset (1,2,25,50 min) | s start | p pause/resume | c cancel/skip
//   f fast mode (1 min = 1 s) | r force full redraw | ? status (+ max loop time)

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  D8
#define TFT_DC  D3
#define TFT_RST -1
#define TFT_LED D4
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ---------- colours (RGB565) ----------
#define C_BG     ST77XX_BLACK
#define C_AMBER  0xFD20   // focus sand
#define C_GREEN  0x07E0   // break sand
#define C_IDLE   0xFD20   // idle = full amber glass
#define C_GLASS  0x9CF3   // light grey outline
#define C_WOOD   0x8A22   // brown caps
#define C_DIM    0x7BEF
#define C_TEXT   ST77XX_WHITE

// =====================================================================
// Timer engine - unchanged from locked Stage 1
// =====================================================================
struct Timer {
  uint32_t durationMs = 0, startMs = 0, accMs = 0;
  bool running = false, active = false;
  void start(uint32_t dur) { durationMs = dur; accMs = 0; startMs = millis(); running = true; active = true; }
  void pause()  { if (running) { accMs += millis() - startMs; running = false; } }
  void resume() { if (active && !running) { startMs = millis(); running = true; } }
  void stop()   { running = false; active = false; }
  uint32_t elapsed() const { return accMs + (running ? (millis() - startMs) : 0); }
  uint32_t remaining() const { uint32_t e = elapsed(); return e >= durationMs ? 0 : durationMs - e; }
  bool done() const { return active && elapsed() >= durationMs; }
};

enum State { IDLE, FOCUS, BREAK };
State state = IDLE;
Timer timer;
const uint8_t PRESETS[4] = {1, 2, 25, 50};
uint8_t presetIdx = 2;
bool fastMode = false;
uint8_t cycleDots = 0;
uint16_t todayCount = 0;
bool longBreak = false;

uint32_t unitMs() { return fastMode ? 1000UL : 60000UL; }
bool presetCounts() { return PRESETS[presetIdx] >= 25; }
uint16_t sandColour() { return state == BREAK ? C_GREEN : state == FOCUS ? C_AMBER : C_IDLE; }

// =====================================================================
// Hourglass geometry
//   Neck at NECK_Y. Each bulb has H rows. Row d = distance from the neck.
//   Top bulb row d  -> y = NECK_Y - 1 - d   (sand rests on the neck side, d = 0 first)
//   Bottom row d    -> y = NECK_Y + 1 + d   (pile grows from the cap, d = H-1 first)
//   Sand is counted in PIXELS, so the amount moved is volume-correct
//   (wide rows drain slower, like a real hourglass).
// =====================================================================
const int CX = 64, NECK_Y = 80, H = 54;
uint8_t  hw[H];          // half width of the inside of each row
uint16_t rw[H];          // row width = 2*hw + 1
uint16_t tcum[H + 1];    // top bulb: pixels below row d (neck side)
uint16_t bcum[H + 1];    // bottom bulb: pixels from the cap up to row
uint16_t P;              // total sand pixels (one bulb)

uint16_t lastTop = 0xFFFF, lastBot = 0xFFFF;

void buildGlass() {
  const uint8_t shoulder[5] = {12, 7, 4, 2, 1};   // rounds the corners near the caps
  for (int d = 0; d < H; d++) {
    float t = (d + 1) / (float)H;
    float v = 2 + 38 * sin(t * PI / 2);
    int s = H - 1 - d;
    if (s < 5) v -= shoulder[s];
    hw[d] = (uint8_t)(v + 0.5f);
    rw[d] = 2 * hw[d] + 1;
  }
  tcum[0] = 0; for (int d = 0; d < H; d++) tcum[d + 1] = tcum[d] + rw[d];
  bcum[0] = 0; for (int e = 0; e < H; e++) bcum[e + 1] = bcum[e] + rw[H - 1 - e];
  P = tcum[H];
}

inline void hl(int x, int y, int w, uint16_t c) { if (w > 0) tft.drawFastHLine(x, y, w, c); }

int clampFill(int v, int w) { return v < 0 ? 0 : (v > w ? w : v); }
int topFill(int d, uint16_t pix) { return clampFill((int)pix - tcum[d], rw[d]); }
int botFill(int d, uint16_t pix) { int e = H - 1 - d; return clampFill((int)pix - bcum[e], rw[d]); }

// Top surface: sand drains from the centre (funnel), so the partial row has a centre gap
void drawTopRow(int d, uint16_t pix, uint16_t col) {
  int y = NECK_Y - 1 - d, L = CX - hw[d], w = rw[d], f = topFill(d, pix);
  int a = f / 2, b = f - a;
  hl(L, y, a, col);
  hl(L + a, y, w - a - b, C_BG);
  hl(L + w - b, y, b, col);
}

// Bottom surface: pile builds up in the centre (mound)
void drawBotRow(int d, uint16_t pix, uint16_t col) {
  int y = NECK_Y + 1 + d, L = CX - hw[d], w = rw[d], k = botFill(d, pix);
  int side = (w - k) / 2;
  hl(L, y, side, C_BG);
  hl(L + side, y, k, col);
  hl(L + side + k, y, w - side - k, C_BG);
}

int topIdx(uint16_t p) { for (int d = 0; d < H; d++) if (p < tcum[d + 1]) return d; return H - 1; }
int botIdx(uint16_t p) { for (int e = 0; e < H; e++) if (p < bcum[e + 1]) return H - 1 - e; return 0; }

void drawGlassOutline() {
  // wooden caps
  int topY = NECK_Y - 1 - (H - 1), botY = NECK_Y + 1 + (H - 1);
  tft.fillRect(CX - 46, topY - 5, 93, 4, C_WOOD);
  tft.fillRect(CX - 46, botY + 2, 93, 4, C_WOOD);
  hl(CX - hw[H - 1] - 1, topY - 1, rw[H - 1] + 2, C_GLASS);
  hl(CX - hw[H - 1] - 1, botY + 1, rw[H - 1] + 2, C_GLASS);
  // sides, joined row to row with lines so there are no gaps
  int px = 1;  // neck: inside is 1 px (the stream), outline at CX +/- 1
  tft.drawPixel(CX - 1, NECK_Y, C_GLASS); tft.drawPixel(CX + 1, NECK_Y, C_GLASS);
  for (int d = 0; d < H; d++) {
    int x = hw[d] + 1;
    int yt = NECK_Y - 1 - d, yb = NECK_Y + 1 + d;
    tft.drawLine(CX - px, yt + 1, CX - x, yt, C_GLASS);
    tft.drawLine(CX + px, yt + 1, CX + x, yt, C_GLASS);
    tft.drawLine(CX - px, yb - 1, CX - x, yb, C_GLASS);
    tft.drawLine(CX + px, yb - 1, CX + x, yb, C_GLASS);
    px = x;
  }
}

uint16_t sandTopFromTimer() {
  if (state == IDLE) return P;
  uint32_t dur = timer.durationMs ? timer.durationMs : 1;
  return (uint16_t)(((uint64_t)P * timer.remaining()) / dur);
}

// Redraw only the rows whose fill changed
void updateSand(bool force) {
  uint16_t top = sandTopFromTimer();
  uint16_t bot = P - top;
  uint16_t col = sandColour();
  if (force) {
    for (int d = 0; d < H; d++) { drawTopRow(d, top, col); drawBotRow(d, bot, col); }
  } else {
    if (top != lastTop) {
      int a = topIdx(top), b = topIdx(lastTop);
      for (int d = min(a, b); d <= max(a, b); d++) drawTopRow(d, top, col);
    }
    if (bot != lastBot) {
      int a = botIdx(bot), b = botIdx(lastBot);
      for (int d = min(a, b); d <= max(a, b); d++) drawBotRow(d, bot, col);
    }
  }
  lastTop = top; lastBot = bot;
}

// ---------- falling stream (neck -> pile), ~80 ms per frame ----------
uint8_t streamPhase = 0;
int streamDrawnTo = -1;   // rows of the stream currently drawn (-1 = none)
uint32_t lastStream = 0;

int streamEnd() { int d = 0; while (d < H && botFill(d, lastBot) == 0) d++; return d; }

void clearStream() {
  if (streamDrawnTo < 0) return;
  tft.drawPixel(CX, NECK_Y, C_BG);
  for (int d = 0; d < streamDrawnTo && d < H; d++)
    if (botFill(d, lastBot) == 0) tft.drawPixel(CX, NECK_Y + 1 + d, C_BG);
  streamDrawnTo = -1;
}

void animateStream() {
  bool flowing = state != IDLE && timer.running && lastTop > 0;
  if (!flowing) { clearStream(); return; }
  if (millis() - lastStream < 80) return;
  lastStream = millis();
  streamPhase = (streamPhase + 1) % 3;
  uint16_t col = sandColour();
  int end = streamEnd();
  tft.drawPixel(CX, NECK_Y, col);
  for (int d = 0; d < end; d++)
    tft.drawPixel(CX, NECK_Y + 1 + d, ((d + streamPhase) % 3) ? col : C_BG);
  streamDrawnTo = end;
}

// =====================================================================
// Text: header + small MM:SS under the glass
// =====================================================================
char shownTime[6] = "";
const int TIME_X = 34, TIME_Y = 143;   // size 2: 12 px per char, 5 chars = 60 px

void drawHeaderText(const char* txt, uint16_t col) {
  tft.fillRect(0, 0, 88, 12, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(col, C_BG);
  tft.setCursor(2, 3);
  tft.print(txt);
}

void drawHeader() {
  char buf[20];
  if (state == IDLE)       snprintf(buf, sizeof(buf), "READY %um%s", PRESETS[presetIdx], fastMode ? " F" : "");
  else if (state == FOCUS) snprintf(buf, sizeof(buf), "FOCUS %um", PRESETS[presetIdx]);
  else                     snprintf(buf, sizeof(buf), longBreak ? "LONG BREAK" : "BREAK");
  drawHeaderText(buf, state == BREAK ? C_GREEN : state == FOCUS ? C_AMBER : C_TEXT);
  for (int i = 0; i < 4; i++) {          // cycle dots, top right
    int x = 94 + i * 10, y = 6;
    tft.fillCircle(x, y, 3, C_BG);
    if (i < cycleDots) tft.fillCircle(x, y, 3, C_AMBER); else tft.drawCircle(x, y, 3, C_DIM);
  }
}

void drawTime(uint32_t remMs, bool force) {
  uint32_t secs = (remMs + 999) / 1000;
  char buf[6];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  tft.setTextSize(2);
  tft.setTextColor(C_TEXT, C_BG);
  for (int i = 0; i < 5; i++) {
    if (force || buf[i] != shownTime[i]) { tft.setCursor(TIME_X + i * 12, TIME_Y); tft.print(buf[i]); }
  }
  memcpy(shownTime, buf, 6);
}

uint32_t lastFullDrawUs = 0;
void redrawAll() {
  uint32_t t0 = micros();
  tft.fillScreen(C_BG);
  streamDrawnTo = -1;
  drawHeader();
  drawGlassOutline();
  updateSand(true);
  drawTime(state == IDLE ? PRESETS[presetIdx] * unitMs() : timer.remaining(), true);
  lastFullDrawUs = micros() - t0;
}

// =====================================================================
// Transitions (Stage 1 logic)
// =====================================================================
void startFocus() {
  state = FOCUS; timer.start(PRESETS[presetIdx] * unitMs());
  Serial.printf("FOCUS start: %u min%s\n", PRESETS[presetIdx], fastMode ? " (fast)" : "");
  redrawAll();
}
void startBreak(uint8_t mins, bool isLong) {
  state = BREAK; longBreak = isLong; timer.start(mins * unitMs());
  Serial.printf("BREAK start: %u min%s\n", mins, isLong ? " (LONG)" : "");
  redrawAll();
}
void goIdle(const char* why) {
  timer.stop(); state = IDLE; longBreak = false;
  Serial.printf("IDLE (%s)\n", why);
  redrawAll();
}
void onFocusComplete() {
  Serial.printf("FOCUS complete. elapsed=%lu ms target=%lu ms\n",
                (unsigned long)timer.elapsed(), (unsigned long)timer.durationMs);
  if (!presetCounts()) { goIdle("test preset, not counted"); return; }
  todayCount++; cycleDots++;
  Serial.printf("Counted! today=%u dots=%u/4\n", todayCount, cycleDots);
  if (cycleDots >= 4) { cycleDots = 0; startBreak(15, true); }
  else startBreak(PRESETS[presetIdx] == 25 ? 5 : 10, false);
}
void onBreakComplete() {
  Serial.printf("BREAK complete. elapsed=%lu ms\n", (unsigned long)timer.elapsed());
  goIdle("break done, waiting for start");
}

// =====================================================================
// Serial + loop
// =====================================================================
uint32_t maxLoopUs = 0;

void printStatus() {
  const char* names[] = {"IDLE", "FOCUS", "BREAK"};
  Serial.printf("[status] %s preset=%u run=%d rem=%lu ms sandTop=%u/%u dots=%u today=%u fast=%d maxLoop=%lu us fullDraw=%lu us heap=%u\n",
                names[state], PRESETS[presetIdx], timer.running, (unsigned long)timer.remaining(),
                lastTop, P, cycleDots, todayCount, fastMode,
                (unsigned long)maxLoopUs, (unsigned long)lastFullDrawUs, ESP.getFreeHeap());
  maxLoopUs = 0;
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    if (c >= '1' && c <= '4') {
      if (state == IDLE) { presetIdx = c - '1'; Serial.printf("Preset -> %u min\n", PRESETS[presetIdx]); redrawAll(); }
      else Serial.println("Ignored: running");
    } else if (c == 's') {
      if (state == IDLE) startFocus(); else Serial.println("Ignored: already running");
    } else if (c == 'p') {
      if (state == IDLE) { Serial.println("Ignored: nothing running"); continue; }
      if (timer.running) { timer.pause(); Serial.println("Paused"); }
      else { timer.resume(); drawHeader(); Serial.println("Resumed"); }
    } else if (c == 'c') {
      if (state == FOCUS) goIdle("focus CANCELLED");
      else if (state == BREAK) goIdle("break SKIPPED");
      else Serial.println("Ignored: nothing to cancel");
    } else if (c == 'f') {
      if (state == IDLE) { fastMode = !fastMode; Serial.printf("Fast mode %s\n", fastMode ? "ON" : "OFF"); redrawAll(); }
      else Serial.println("Ignored: only when idle");
    } else if (c == 'r') {
      redrawAll(); Serial.printf("Full redraw took %lu us\n", (unsigned long)lastFullDrawUs);
    } else if (c == '?') {
      printStatus();
    }
  }
}

uint32_t lastLog = 0, lastBlink = 0;
bool blinkOn = false;

void setup() {
  Serial.begin(115200);
  delay(200);
  analogWriteRange(1023);
  analogWriteFreq(1000);
  analogWrite(TFT_LED, 1023);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  buildGlass();
  Serial.printf("\nStage 2 - hourglass. Sand pixels per bulb: %u\n", P);
  Serial.println("Commands: 1-4 preset, s start, p pause, c cancel, f fast, r redraw, ? status");
  redrawAll();
  printStatus();
}

void loop() {
  uint32_t t0 = micros();
  handleSerial();

  if (state != IDLE) {
    if (timer.done()) {
      updateSand(false); drawTime(0, false);
      if (state == FOCUS) onFocusComplete(); else onBreakComplete();
      return;
    }
    if (timer.running) {
      updateSand(false);                 // only changed rows
      drawTime(timer.remaining(), false); // only changed digits
    } else if (millis() - lastBlink >= 500) {
      lastBlink = millis(); blinkOn = !blinkOn;
      if (blinkOn) drawHeaderText("PAUSED", C_TEXT); else drawHeader();
    }
    animateStream();
    if (millis() - lastLog >= 10000) { lastLog = millis(); printStatus(); }
  }

  uint32_t dt = micros() - t0;
  if (dt > maxLoopUs) maxLoopUs = dt;
  yield();
}
