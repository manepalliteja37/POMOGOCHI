// Stage 4 - Buttons (input layer) + sprite speed fix
// Board: NodeMCU 1.0 | Libraries: Adafruit GFX, Adafruit ST7735
// Display (Stage 0 lock): BLACKTAB, rotation 0, CS D8, DC D3, RST -1, LED D4 PWM
//
// Wiring: every button goes between its pin and GND.
//   MODE D1 (internal pull-up)   OK D2 (internal pull-up)   BACK D0 (EXTERNAL 10k to 3V3 - GPIO16 has no pull-up)
//
// Rules (spec v3): poll 5 ms, debounce = 6 equal reads (30 ms), boot lockout 500 ms,
//   SHORT < 700 ms, LONG >= 1000 ms (fires once while held, release ignored),
//   BACK DOUBLE = 2nd SHORT within 2 s, gap >= 120 ms, first stable press wins when 2 are held.
//
// Serial @115200:
//   w  raw mode on/off (prints pin levels + raw edge count per press = bounce)
//   c  clear counters      ?  counters + status
//   x  sprite speed test: adult + sleep frames at 8 / 20 / 27 / 40 MHz SPI
//   s  set SPI speed for the rest of the run (cycles 8 -> 20 -> 27 -> 40 MHz)
//
// Art: limengdu/reTerminal_Sticky_Bunny (MIT License, (c) 2026 limengdu) - see LICENSE_bunny_art.txt

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "pet_frames.h"

#define TFT_CS  D8
#define TFT_DC  D3
#define TFT_RST -1
#define TFT_LED D4
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

#define C_NAVY 0x10A8
#define C_GREY 0x4208
#define C_AMBER 0xFD20

// =====================================================================
// INPUT LAYER  (this block is what moves into the final firmware)
// =====================================================================
enum BtnId : uint8_t { B_MODE, B_OK, B_BACK, B_COUNT };
enum EvType : uint8_t { EV_NONE, EV_SHORT, EV_LONG, EV_DOUBLE, EV_CONFIRM_TIMEOUT };
struct Event { EvType type; BtnId btn; };

const uint8_t  BTN_PIN[B_COUNT]  = { D1, D2, D0 };
const char*    BTN_NAME[B_COUNT] = { "MODE", "OK", "BACK" };

const uint16_t POLL_MS        = 5;
const uint8_t  STABLE_READS   = 6;      // 6 x 5 ms = 30 ms
const uint16_t BOOT_LOCK_MS   = 500;
const uint16_t SHORT_MAX_MS   = 700;
const uint16_t LONG_MS        = 1000;
const uint16_t DOUBLE_GAP_MS  = 120;
const uint16_t DOUBLE_WIN_MS  = 2000;

struct Btn {
  bool     stable;      // true = pressed (debounced)
  bool     lastRaw;
  uint8_t  same;        // equal raw reads in a row
  bool     armed;       // seen released after boot lockout
  bool     owner;       // this press got accepted (first-press-wins)
  bool     longFired;
  uint32_t downAt, upAt;
  uint16_t rawEdges;    // bounce counter for the current press
};
Btn btn[B_COUNT];

// confirm window for BACK double
bool     confirmOpen = false;
uint32_t confirmFirstUp = 0;

// event queue (tiny ring)
Event    evq[8]; uint8_t evHead = 0, evTail = 0;
void pushEv(EvType t, BtnId b) { uint8_t n = (evHead + 1) & 7; if (n != evTail) { evq[evHead] = {t, b}; evHead = n; } }
bool popEv(Event& e) { if (evTail == evHead) return false; e = evq[evTail]; evTail = (evTail + 1) & 7; return true; }

// test counters
struct Cnt { uint16_t shorts, longs, doubles, midDrops, rejected, bounceGaps, maxEdges; } cnt[B_COUNT];
bool rawMode = false;

bool readPressed(uint8_t i) { return digitalRead(BTN_PIN[i]) == LOW; }

void inputBegin() {
  pinMode(D1, INPUT_PULLUP);
  pinMode(D2, INPUT_PULLUP);
  pinMode(D0, INPUT);                       // external 10k pull-up
  for (uint8_t i = 0; i < B_COUNT; i++) {
    bool p = readPressed(i);
    btn[i] = { p, p, 0, false, false, false, 0, 0, 0 };
  }
}

bool anotherOwner(uint8_t me) {
  for (uint8_t i = 0; i < B_COUNT; i++) if (i != me && btn[i].stable && btn[i].owner) return true;
  return false;
}

void onStablePress(uint8_t i, uint32_t now) {
  Btn& b = btn[i];
  b.downAt = now; b.longFired = false;
  if (!b.armed || anotherOwner(i)) { b.owner = false; if (b.armed) cnt[i].rejected++; return; }
  b.owner = true;
}

void onStableRelease(uint8_t i, uint32_t now) {
  Btn& b = btn[i];
  b.upAt = now;
  if (b.rawEdges > cnt[i].maxEdges) cnt[i].maxEdges = b.rawEdges;
  if (rawMode) Serial.printf("  %s released, raw edges this press: %u\n", BTN_NAME[i], b.rawEdges);
  b.rawEdges = 0;
  if (!b.armed) { if (now >= BOOT_LOCK_MS) b.armed = true; return; }   // held through boot: just arm
  if (!b.owner) return;
  b.owner = false;
  if (b.longFired) return;                               // LONG already sent, release ignored
  uint32_t held = now - b.downAt;
  if (held >= SHORT_MAX_MS) { cnt[i].midDrops++; return; } // 700..999 ms: neither short nor long

  if (i == B_BACK) {
    if (confirmOpen && now - confirmFirstUp <= DOUBLE_WIN_MS) {
      uint32_t gap = b.downAt - confirmFirstUp;
      if (gap < DOUBLE_GAP_MS) { cnt[i].bounceGaps++; return; }   // too fast = bounce, ignore
      confirmOpen = false; cnt[i].doubles++; pushEv(EV_DOUBLE, B_BACK); return;
    }
    confirmOpen = true; confirmFirstUp = now;
  }
  cnt[i].shorts++; pushEv(EV_SHORT, (BtnId)i);
}

void inputPoll() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < POLL_MS) return;
  last = now;
  for (uint8_t i = 0; i < B_COUNT; i++) {
    Btn& b = btn[i];
    bool r = readPressed(i);
    if (r != b.lastRaw) { b.lastRaw = r; b.same = 1; if (b.rawEdges < 999) b.rawEdges++; continue; }
    if (b.same < STABLE_READS) { b.same++; if (b.same < STABLE_READS) continue; }
    if (r != b.stable) {
      b.stable = r;
      if (r) onStablePress(i, now); else onStableRelease(i, now);
    } else if (!r && !b.armed && now >= BOOT_LOCK_MS) {
      b.armed = true;                                    // released & lockout over
    }
    if (b.stable && b.owner && !b.longFired && now - b.downAt >= LONG_MS) {
      b.longFired = true; cnt[i].longs++; pushEv(EV_LONG, (BtnId)i);
    }
  }
  if (confirmOpen && now - confirmFirstUp > DOUBLE_WIN_MS) { confirmOpen = false; pushEv(EV_CONFIRM_TIMEOUT, B_BACK); }
}
// =====================================================================

// ---------------- fast sprite draw (byte-wise, for the speed test) ----
uint16_t rowBuf[128];
uint32_t drawSprite(int x, int y, const Sprite& s, uint16_t bg) {
  uint32_t t0 = micros();
  int rb = (s.w + 7) / 8;
  tft.startWrite();
  tft.setAddrWindow(x, y, s.w, s.h);
  for (int r = 0; r < s.h; r++) {
    const uint8_t* ip = s.ink + r * rb;
    const uint8_t* mp = s.mask ? s.mask + r * rb : nullptr;
    int c = 0;
    for (int bx = 0; bx < rb; bx++) {
      uint8_t ib = pgm_read_byte(ip + bx);
      uint8_t mb = mp ? pgm_read_byte(mp + bx) : 0xFF;
      for (uint8_t bit = 0x80; bit && c < s.w; bit >>= 1, c++)
        rowBuf[c] = (ib & bit) ? ST77XX_BLACK : ((mb & bit) ? ST77XX_WHITE : bg);
    }
    tft.writePixels(rowBuf, s.w);
  }
  tft.endWrite();
  return micros() - t0;
}

const uint32_t SPI_SPEEDS[] = { 8000000, 20000000, 27000000, 40000000 };
uint8_t spiIdx = 0;
void setSpi(uint8_t i) { spiIdx = i; tft.setSPISpeed(SPI_SPEEDS[i]); }

// ---------------- test screen ----------------
const char* lastEvText = "-";
BtnId lastEvBtn = B_MODE;
bool screenDirty = true;

void drawScreen() {
  tft.fillScreen(C_NAVY);
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE, C_NAVY);
  tft.setCursor(2, 3); tft.print("STAGE 4  BUTTONS");
  tft.setCursor(2, 100); tft.print("       S  L  D dr rj");
}

void drawButtons() {
  static int8_t shown[B_COUNT] = { -1, -1, -1 };
  for (uint8_t i = 0; i < B_COUNT; i++) {
    int8_t st = btn[i].stable ? (btn[i].owner ? 2 : 1) : 0;   // 0 up, 1 pressed-ignored, 2 pressed
    if (st == shown[i]) continue;
    shown[i] = st;
    uint16_t col = st == 2 ? ST77XX_GREEN : st == 1 ? ST77XX_RED : C_GREY;
    int x = 4 + i * 41;
    tft.fillRoundRect(x, 18, 38, 30, 4, col);
    tft.setTextColor(ST77XX_WHITE, col);
    tft.setCursor(x + (38 - strlen(BTN_NAME[i]) * 6) / 2, 30); tft.print(BTN_NAME[i]);
  }
}

void drawEvent() {
  tft.fillRect(0, 58, 128, 34, C_NAVY);
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE, C_NAVY); tft.setCursor(2, 60);
  tft.printf("Last event: %s", BTN_NAME[lastEvBtn]);
  tft.setTextSize(2); tft.setTextColor(C_AMBER, C_NAVY); tft.setCursor(2, 74);
  tft.print(lastEvText);
  tft.setTextSize(1);
}

void drawCounters() {
  tft.setTextColor(ST77XX_WHITE, C_NAVY);
  for (uint8_t i = 0; i < B_COUNT; i++) {
    tft.setCursor(2, 112 + i * 10);
    tft.printf("%-4s %3u %2u %2u %2u %2u", BTN_NAME[i], cnt[i].shorts, cnt[i].longs, cnt[i].doubles, cnt[i].midDrops, cnt[i].rejected);
  }
}

void drawConfirm() {
  static int lastW = -1;
  int w = 0;
  if (confirmOpen) w = 124 - (int)((millis() - confirmFirstUp) * 124 / DOUBLE_WIN_MS);
  if (w < 0) w = 0;
  if (w == lastW) return;
  if ((w == 0) != (lastW <= 0)) {           // prompt appears / disappears
    tft.fillRect(0, 144, 128, 16, C_NAVY);
    if (w > 0) { tft.setTextColor(ST77XX_YELLOW, C_NAVY); tft.setCursor(2, 144); tft.print("BACK again = cancel"); }
  }
  if (w > 0) { tft.fillRect(2, 155, w, 3, ST77XX_YELLOW); tft.fillRect(2 + w, 155, 124 - w, 3, C_NAVY); }
  lastW = w;
}

void printCounters() {
  Serial.println("[counters]  short long double drop(700-999ms) rejected(2nd button) bounceGap(<120ms) maxRawEdges");
  for (uint8_t i = 0; i < B_COUNT; i++)
    Serial.printf("  %-4s %4u %4u %4u %4u %4u %4u %4u\n", BTN_NAME[i], cnt[i].shorts, cnt[i].longs, cnt[i].doubles,
                  cnt[i].midDrops, cnt[i].rejected, cnt[i].bounceGaps, cnt[i].maxEdges);
}

uint32_t maxLoopUs = 0;
void printStatus() {
  printCounters();
  Serial.printf("[status] SPI=%lu MHz maxLoop=%lu us heap=%u\n", (unsigned long)(SPI_SPEEDS[spiIdx] / 1000000),
                (unsigned long)maxLoopUs, ESP.getFreeHeap());
  maxLoopUs = 0;
}

void speedTest() {
  Serial.println("[speed test] sprite draw time by SPI clock (look at the screen for glitches at each speed)");
  uint8_t keep = spiIdx;
  for (uint8_t s = 0; s < 4; s++) {
    setSpi(s);
    tft.fillScreen(C_NAVY);
    uint32_t a = 0, sl = 0;
    for (int k = 0; k < 10; k++) a += drawSprite((128 - ADULT[0].w) / 2, 30, ADULT[k % ADULT_N], C_NAVY);
    delay(600);
    for (int k = 0; k < 10; k++) sl += drawSprite(0, 30, SLEEP[k & 1], C_NAVY);
    Serial.printf("  %2lu MHz: adult %lu us, sleep %lu us\n", (unsigned long)(SPI_SPEEDS[s] / 1000000),
                  (unsigned long)(a / 10), (unsigned long)(sl / 10));
    delay(600);
    yield();
  }
  setSpi(keep);
  Serial.println("Tell me the fastest speed where the picture still looked clean.");
  screenDirty = true;
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'w': rawMode = !rawMode; Serial.printf("Raw mode %s\n", rawMode ? "ON" : "OFF"); break;
      case 'c': memset(cnt, 0, sizeof(cnt)); Serial.println("Counters cleared"); drawCounters(); break;
      case '?': printStatus(); break;
      case 'x': speedTest(); break;
      case 's': setSpi((spiIdx + 1) % 4); Serial.printf("SPI now %lu MHz\n", (unsigned long)(SPI_SPEEDS[spiIdx] / 1000000)); break;
    }
  }
}

void rawPrint() {
  static uint32_t last = 0; static uint8_t lastMask = 0xFF;
  if (!rawMode || millis() - last < 20) return;
  last = millis();
  uint8_t m = 0;
  for (uint8_t i = 0; i < B_COUNT; i++) if (readPressed(i)) m |= 1 << i;
  if (m != lastMask) {
    lastMask = m;
    Serial.printf("raw  D1(MODE)=%d  D2(OK)=%d  D0(BACK)=%d   (1 = pressed)\n", m & 1, (m >> 1) & 1, (m >> 2) & 1);
  }
}

void setup() {
  inputBegin();                                   // first, so the boot lockout clock is honest
  Serial.begin(115200);
  delay(50);
  analogWriteRange(1023); analogWriteFreq(1000); analogWrite(TFT_LED, 1023);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  setSpi(0);
  Serial.println("\nStage 4 - buttons. w raw, c clear, ? counters, x SPI speed test, s set SPI");
  Serial.printf("Boot pin levels: D1=%d D2=%d D0=%d (expect 1 1 1 when nothing is pressed)\n",
                digitalRead(D1), digitalRead(D2), digitalRead(D0));
}

void loop() {
  uint32_t t0 = micros();
  inputPoll();
  handleSerial();
  rawPrint();

  if (screenDirty) { screenDirty = false; drawScreen(); drawCounters(); drawEvent(); }
  Event e;
  while (popEv(e)) {
    lastEvBtn = e.btn;
    lastEvText = e.type == EV_SHORT ? "SHORT" : e.type == EV_LONG ? "LONG" : e.type == EV_DOUBLE ? "DOUBLE" : "timeout";
    Serial.printf("%8lu ms  %-4s %s\n", (unsigned long)millis(), BTN_NAME[e.btn], lastEvText);
    if (e.type == EV_LONG && e.btn == B_MODE) Serial.println("          (MODE LONG = mute toggle in the final firmware)");
    if (e.type == EV_DOUBLE) Serial.println("          (BACK DOUBLE = cancel confirmed)");
    drawEvent(); drawCounters();
  }
  drawButtons();
  drawConfirm();

  uint32_t dt = micros() - t0;
  if (dt > maxLoopUs) maxLoopUs = dt;
  yield();
}
