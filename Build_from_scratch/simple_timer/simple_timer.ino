// Stage 1 - Timer engine (no buttons yet, control via Serial Monitor @115200)
// Board: NodeMCU 1.0 | Libraries: Adafruit GFX, Adafruit ST7735
// Display config locked in Stage 0: BLACKTAB, rotation 0, CS D8, DC D3, RST -1, LED D4
//
// Serial commands (type one letter/number, press Enter):
//   1 / 2 / 3 / 4  -> select preset 1, 2, 25, 50 min
//   s              -> start focus (from idle)
//   p              -> pause / resume
//   c              -> cancel focus or skip break
//   f              -> fast mode ON/OFF (1 minute becomes 1 second, to test cycles quickly)
//   ?              -> print status

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  D8
#define TFT_DC  D3
#define TFT_RST -1
#define TFT_LED D4
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ---------- colours ----------
#define C_BG     ST77XX_BLACK
#define C_AMBER  0xFD20      // focus
#define C_GREEN  0x07E0      // break
#define C_IDLE   ST77XX_WHITE
#define C_DIM    0x7BEF

// ---------- timer engine (millis based, rollover safe) ----------
struct Timer {
  uint32_t durationMs = 0;
  uint32_t startMs = 0;      // millis() when the current running stretch began
  uint32_t accMs = 0;        // time accumulated before the last pause
  bool running = false;
  bool active = false;       // started and not finished/cancelled

  void start(uint32_t dur) { durationMs = dur; accMs = 0; startMs = millis(); running = true; active = true; }
  void pause()  { if (running) { accMs += millis() - startMs; running = false; } }
  void resume() { if (active && !running) { startMs = millis(); running = true; } }
  void stop()   { running = false; active = false; }
  uint32_t elapsed() const { return accMs + (running ? (millis() - startMs) : 0); }  // unsigned math = rollover safe
  uint32_t remaining() const { uint32_t e = elapsed(); return e >= durationMs ? 0 : durationMs - e; }
  bool done() const { return active && elapsed() >= durationMs; }
};

// ---------- session logic ----------
enum State { IDLE, FOCUS, BREAK };
State state = IDLE;
Timer timer;

const uint8_t PRESETS[4] = {1, 2, 25, 50};
uint8_t presetIdx = 2;        // default 25 min
bool fastMode = false;        // 1 min -> 1 s
uint8_t cycleDots = 0;        // completed 25/50 in this cycle (0..4)
uint16_t todayCount = 0;      // completed 25/50 today
uint8_t breakMin = 0;
bool longBreak = false;

uint32_t unitMs() { return fastMode ? 1000UL : 60000UL; }
bool presetCounts() { return PRESETS[presetIdx] >= 25; }

// ---------- display (partial redraw) ----------
char shownTime[6] = "";       // last drawn "MM:SS"
const int TIME_X = 4, TIME_Y = 60;   // size-4 font: 24 px per char, 5 chars = 120 px

uint16_t stateColour() { return state == FOCUS ? C_AMBER : state == BREAK ? C_GREEN : C_IDLE; }

void drawHeader() {
  tft.fillRect(0, 0, 128, 40, C_BG);
  tft.setTextSize(2);
  tft.setTextColor(stateColour(), C_BG);
  tft.setCursor(4, 4);
  if (state == IDLE)       tft.print("READY");
  else if (state == FOCUS) tft.print("FOCUS");
  else                     tft.print(longBreak ? "LONG BRK" : "BREAK");
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(4, 26);
  tft.printf("Preset %u min%s", PRESETS[presetIdx], fastMode ? " FAST" : "");
}

void drawTime(uint32_t remMs, bool force) {
  uint32_t secs = (remMs + 999) / 1000;           // round up so 00:00 shows only at the end
  char buf[6];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  tft.setTextSize(4);
  tft.setTextColor(stateColour(), C_BG);          // bg colour = overwrite without flicker
  for (int i = 0; i < 5; i++) {
    if (force || buf[i] != shownTime[i]) {        // redraw only changed characters
      tft.setCursor(TIME_X + i * 24, TIME_Y);
      tft.print(buf[i]);
    }
  }
  memcpy(shownTime, buf, 6);
}

void drawStatus(const char* msg) {
  tft.fillRect(0, 105, 128, 12, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, C_BG);
  tft.setCursor(4, 107);
  tft.print(msg);
}

void drawFooter() {
  tft.fillRect(0, 125, 128, 35, C_BG);
  for (int i = 0; i < 4; i++) {                   // cycle dots
    int x = 40 + i * 16, y = 134;
    if (i < cycleDots) tft.fillCircle(x, y, 5, C_AMBER);
    else               tft.drawCircle(x, y, 5, C_DIM);
  }
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(4, 148);
  tft.printf("Today: %u", todayCount);
}

void redrawAll() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawTime(state == IDLE ? PRESETS[presetIdx] * unitMs() : timer.remaining(), true);
  drawStatus(state == IDLE ? "s=start" : "");
  drawFooter();
}

// ---------- transitions ----------
void startFocus() {
  state = FOCUS;
  timer.start(PRESETS[presetIdx] * unitMs());
  Serial.printf("FOCUS start: %u min%s\n", PRESETS[presetIdx], fastMode ? " (fast)" : "");
  redrawAll();
}

void startBreak(uint8_t mins, bool isLong) {
  state = BREAK; breakMin = mins; longBreak = isLong;
  timer.start(mins * unitMs());
  Serial.printf("BREAK start: %u min%s\n", mins, isLong ? " (LONG)" : "");
  redrawAll();
}

void goIdle(const char* why) {
  timer.stop();
  state = IDLE; longBreak = false;
  Serial.printf("IDLE (%s)\n", why);
  redrawAll();
}

void onFocusComplete() {
  uint32_t actual = timer.elapsed();
  Serial.printf("FOCUS complete. elapsed=%lu ms, target=%lu ms\n",
                (unsigned long)actual, (unsigned long)timer.durationMs);
  if (!presetCounts()) { goIdle("test preset done, no break, not counted"); return; }
  todayCount++;
  cycleDots++;
  Serial.printf("Counted! today=%u, dots=%u/4\n", todayCount, cycleDots);
  if (cycleDots >= 4) { cycleDots = 0; startBreak(15, true); }   // dots reset when long break starts
  else startBreak(PRESETS[presetIdx] == 25 ? 5 : 10, false);
}

void onBreakComplete() {
  Serial.printf("BREAK complete. elapsed=%lu ms\n", (unsigned long)timer.elapsed());
  goIdle("break done, waiting for start");
}

// ---------- serial commands ----------
void printStatus() {
  const char* names[] = {"IDLE", "FOCUS", "BREAK"};
  Serial.printf("[status] state=%s preset=%u running=%d remaining=%lu ms dots=%u today=%u fast=%d heap=%u\n",
                names[state], PRESETS[presetIdx], timer.running, (unsigned long)timer.remaining(),
                cycleDots, todayCount, fastMode, ESP.getFreeHeap());
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    if (c >= '1' && c <= '4') {
      if (state == IDLE) { presetIdx = c - '1'; Serial.printf("Preset -> %u min\n", PRESETS[presetIdx]); redrawAll(); }
      else Serial.println("Ignored: can't change preset while running");
    } else if (c == 's') {
      if (state == IDLE) startFocus(); else Serial.println("Ignored: already running");
    } else if (c == 'p') {
      if (state == IDLE) { Serial.println("Ignored: nothing running"); continue; }
      if (timer.running) { timer.pause(); drawStatus("PAUSED  p=resume"); Serial.println("Paused"); }
      else               { timer.resume(); drawStatus(""); Serial.println("Resumed"); }
    } else if (c == 'c') {
      if (state == FOCUS)      goIdle("focus CANCELLED, no reward, dots unchanged");
      else if (state == BREAK) goIdle("break SKIPPED, no energy");
      else Serial.println("Ignored: nothing to cancel");
    } else if (c == 'f') {
      if (state == IDLE) { fastMode = !fastMode; Serial.printf("Fast mode %s\n", fastMode ? "ON" : "OFF"); redrawAll(); }
      else Serial.println("Ignored: change fast mode only when idle");
    } else if (c == '?') {
      printStatus();
    }
  }
}

// ---------- main ----------
uint32_t lastLog = 0;
bool pauseBlink = false;
uint32_t lastBlink = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  analogWriteRange(1023);
  analogWriteFreq(1000);
  analogWrite(TFT_LED, 1023);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  Serial.println("\nStage 1 - timer engine. Commands: 1-4 preset, s start, p pause, c cancel, f fast, ? status");
  redrawAll();
  printStatus();
}

void loop() {
  handleSerial();

  if (state != IDLE) {
    if (timer.done()) {
      drawTime(0, false);
      if (state == FOCUS) onFocusComplete(); else onBreakComplete();
      return;
    }
    if (timer.running) drawTime(timer.remaining(), false);   // only changed digits get drawn

    // blink PAUSED label
    if (!timer.running && millis() - lastBlink >= 500) {
      lastBlink = millis(); pauseBlink = !pauseBlink;
      drawStatus(pauseBlink ? "PAUSED  p=resume" : "");
    }
    // log every 10 s for drift check against a phone stopwatch
    if (millis() - lastLog >= 10000) { lastLog = millis(); printStatus(); }
  }
  yield();
}

