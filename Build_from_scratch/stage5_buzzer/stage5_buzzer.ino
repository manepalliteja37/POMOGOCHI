// Stage 5 v3 - Buzzer with display + buttons, all LOCKED sounds (sound code in buzzer.h)
// v3 fix: D6 is GPIO12 = hardware SPI MISO. tft.initR() -> SPI.begin() switched it to SPI mode,
//         so the buzzer pin was silent. buzzerBegin() now runs after the display init.
// Board: NodeMCU 1.0 | Libraries: Adafruit GFX, Adafruit ST7735
// Display (locked): BLACKTAB, rotation 0, CS D8, DC D3, RST -1, LED D4, SPI 40 MHz
// Buttons (Stage 4 locked): MODE D1, OK D2, BACK D0 (ext 10k pull-up)
// Buzzer (5a passed): D6 -> 1k -> BC547, passive buzzer. Volume by ear: soft 50% / mid 18% / loud 6% duty
//
// Sound plan (locked 2026-10-14):
//   OK starts focus -> Breath bell once -> soft heartbeat loops the whole session -> Focus done chime
//   No halfway / last-minute cues. Heartbeat is ALWAYS soft, whatever the chime volume.
//
// Buttons:  MODE short = next sound   OK short = play it (on "1-min focus" = run the demo session)
//           BACK short = chime volume  MODE LONG = mute toggle (beep + icon)
// Serial @115200:
//   f  1-minute focus demo (bell -> heartbeat -> done)
//   h  next heartbeat variant (pick the most soothing one)
//   1..8 play a chime   m mute   v volume   a stress animation   ?  status
//
// Art: limengdu/reTerminal_Sticky_Bunny (MIT License, (c) 2026 limengdu) - see LICENSE_bunny_art.txt

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <core_esp8266_waveform.h>
#include "pet_frames.h"
#include "buzzer.h"      // all sound code lives here

#define TFT_CS  D8
#define TFT_DC  D3
#define TFT_RST -1
#define TFT_LED D4
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
const uint32_t SPI_HZ = 40000000;

#define C_NAVY  0x10A8
#define C_GREY  0x4208
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

// =====================================================================
// FAST DRAW (Stage 3b locked: one SPI.writeBytes per row)
// =====================================================================
uint16_t rowBuf[128] __attribute__((aligned(4)));   // SPI.writeBytes reads 32-bit words: buffer MUST be 4-byte aligned
static inline uint16_t be(uint16_t c) { return (c << 8) | (c >> 8); }

uint32_t fastFill(int x, int y, int w, int h, uint16_t col) {
  uint32_t t0 = micros();
  uint16_t c = be(col);
  for (int i = 0; i < w; i++) rowBuf[i] = c;
  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  for (int r = 0; r < h; r++) SPI.writeBytes((uint8_t*)rowBuf, w * 2);
  tft.endWrite();
  return micros() - t0;
}

uint32_t drawSprite(int x, int y, const Sprite& s, uint16_t bg) {
  uint32_t t0 = micros();
  const uint16_t INK = be(ST77XX_BLACK), BODY = be(ST77XX_WHITE), BG = be(bg);
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
        rowBuf[c] = (ib & bit) ? INK : ((mb & bit) ? BODY : BG);
    }
    SPI.writeBytes((uint8_t*)rowBuf, s.w * 2);
  }
  tft.endWrite();
  return micros() - t0;
}


// ---------------- 1-minute focus demo ----------------
enum Demo : uint8_t { D_OFF, D_BELL, D_BEAT, D_DONE };
Demo demo = D_OFF;
uint32_t demoStart = 0;
const uint32_t DEMO_MS = 60000;
int lastSecShown = -1;

// ---------------- test screen ----------------
uint8_t sel = 0;          // 0..N_CHIMES-1 = chimes, N_CHIMES = "1-min focus"
bool stress = false;
uint32_t lastAnim = 0; uint8_t animFrame = 0;
uint32_t maxLoopUs = 0, fillUs = 0;

const char* selName() { return sel < N_CHIMES ? CHIMES[sel].name : "1-min focus"; }

void drawHeader() {
  fastFill(0, 0, 128, 14, C_GREY);
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE, C_GREY);
  tft.setCursor(2, 3); tft.print("STAGE 5 SOUND");
  if (muted) {
    tft.fillRect(108, 4, 4, 6, ST77XX_RED); tft.fillTriangle(111, 7, 116, 2, 116, 12, ST77XX_RED);
    tft.drawLine(118, 4, 124, 10, ST77XX_RED); tft.drawLine(124, 4, 118, 10, ST77XX_RED);
  }
}

void drawInfo() {
  fastFill(0, 16, 128, 40, C_NAVY);
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE, C_NAVY);
  tft.setCursor(2, 18); tft.printf("Sound %u/%u", sel + 1, N_CHIMES + 1);
  tft.setTextSize(2); tft.setTextColor(C_AMBER, C_NAVY);
  tft.setCursor(2, 30); tft.print(selName());
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE, C_NAVY);
  tft.setCursor(2, 48); tft.printf("Vol %s  HB %u", VOL_NAME[volume], hbSel + 1);
}

void drawDemo(int secLeft) {
  fastFill(0, 60, 128, 40, C_NAVY);
  if (secLeft < 0) return;
  tft.setTextSize(3); tft.setTextColor(C_AMBER, C_NAVY);
  tft.setCursor(28, 66); tft.printf("0:%02d", secLeft);
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE, C_NAVY);
  tft.setCursor(2, 92); tft.print(demo == D_BELL ? "bell..." : demo == D_BEAT ? HB[hbSel].name : "done");
}

void drawAll() {
  fillUs = fastFill(0, 0, 128, 160, C_NAVY);
  drawHeader(); drawInfo();
  tft.setTextSize(1); tft.setTextColor(C_GREY, C_NAVY);
  tft.setCursor(2, 152); tft.print("MODE next OK play BACK vol");
}

void startDemo() {
  demo = D_BELL; demoStart = millis(); lastSecShown = -1;
  stress = false;
  if (muted) { demo = D_BEAT; startHeartbeat(); }
  else playChime(0);
  Serial.printf("Focus demo: bell -> heartbeat (%s) -> done in 60 s\n", HB[hbSel].name);
}

void stopDemo() { demo = D_OFF; stopSound(); drawDemo(-1); }

void tickDemo() {
  if (demo == D_OFF) return;
  uint32_t el = millis() - demoStart;
  if (demo == D_BELL && !buzzerBusy()) { demo = D_BEAT; startHeartbeat(); }
  if ((demo == D_BELL || demo == D_BEAT) && el >= DEMO_MS) {
    stopSound(); demo = D_DONE; playChime(1); drawDemo(0); Serial.println("Focus demo complete");
    return;
  }
  if (demo == D_DONE) { if (!buzzerBusy()) demo = D_OFF; return; }
  int secLeft = (int)((DEMO_MS - el + 999) / 1000);
  if (secLeft != lastSecShown) { lastSecShown = secLeft; drawDemo(secLeft); }
}

void setMuted(bool m) {
  muted = m;
  if (m) stopSound();
  playChime(m ? 6 : 7);
  if (!m && demo == D_BEAT) { demo = D_BELL; }      // heartbeat resumes after the unmute beep
}

void printStatus() {
  Serial.printf("[status] sound=%s vol=%s hb=%s muted=%d demo=%d stress=%d maxLoop=%lu us noteLateMax=%lu ms fill=%lu us heap=%u\n",
                selName(), VOL_NAME[volume], HB[hbSel].name, muted, demo, stress, (unsigned long)maxLoopUs,
                (unsigned long)noteLateMax, (unsigned long)fillUs, ESP.getFreeHeap());
  maxLoopUs = 0; noteLateMax = 0;
}

void nextHeartbeat() {
  hbSel = (hbSel + 1) % N_HB;
  Serial.printf("Heartbeat %u: %s\n", hbSel + 1, HB[hbSel].name);
  drawInfo();
  if (demo == D_BEAT) startHeartbeat();
  else if (demo == D_OFF) { buildHeartbeat(); playMel(hbNotes, false, HB_DUTY); }   // preview one beat
}

void playSel() {
  if (sel == N_CHIMES) { if (demo == D_OFF) startDemo(); else stopDemo(); return; }
  Serial.printf("Play: %s\n", CHIMES[sel].name); playChime(sel);
}

void handleEvent(const Event& e) {
  if (e.type == EV_LONG && e.btn == B_MODE) { setMuted(!muted); drawHeader(); Serial.printf("Mute %s\n", muted ? "ON" : "OFF"); return; }
  if (e.type != EV_SHORT) return;
  switch (e.btn) {
    case B_MODE: sel = (sel + 1) % (N_CHIMES + 1); drawInfo(); break;
    case B_OK:   playSel(); break;
    case B_BACK: volume = (volume + 1) % 3; drawInfo(); Serial.printf("Volume %s\n", VOL_NAME[volume]); if (demo == D_OFF) playChime(1); break;
    default: break;
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c >= '1' && c <= '0' + N_CHIMES) { sel = c - '1'; drawInfo(); playSel(); }
    else if (c == 'f') { sel = N_CHIMES; drawInfo(); playSel(); }
    else if (c == 'h') nextHeartbeat();
    else if (c == 'm') { setMuted(!muted); drawHeader(); Serial.printf("Mute %s\n", muted ? "ON" : "OFF"); }
    else if (c == 'v') { volume = (volume + 1) % 3; drawInfo(); Serial.printf("Volume %s\n", VOL_NAME[volume]); }
    else if (c == 'a') { stress = !stress; Serial.printf("Stress animation %s\n", stress ? "ON" : "OFF"); fastFill(0, 56, 128, 96, C_NAVY); }
    else if (c == '?') printStatus();
  }
}

uint32_t lastLog = 0;

void setup() {
  inputBegin();
  Serial.begin(115200);
  delay(50);
  analogWriteRange(1023); analogWriteFreq(1000); analogWrite(TFT_LED, 1023);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.setSPISpeed(SPI_HZ);
  buzzerBegin();        // MUST be after tft.initR(): SPI.begin() steals D6 (GPIO12 = MISO)
  drawAll();
  Serial.println("\nStage 5 v2. f focus demo, h heartbeat variant, 1-8 chimes, m mute, v volume, a stress, ? status");
  Serial.printf("Full-screen fast fill at boot: %lu us\n", (unsigned long)fillUs);
}

void loop() {
  uint32_t t0 = micros();
  inputPoll();
  buzzerTick();
  tickDemo();
  handleSerial();
  Event e;
  while (popEv(e)) handleEvent(e);

  if (stress && demo == D_OFF && millis() - lastAnim >= 80) {
    lastAnim = millis();
    animFrame = (animFrame + 1) % ADULT_N;
    drawSprite((128 - ADULT[0].w) / 2, 56, ADULT[animFrame], C_NAVY);
  }
  if (millis() - lastLog >= 15000) { lastLog = millis(); printStatus(); }

  uint32_t dt = micros() - t0;
  if (dt > maxLoopUs) maxLoopUs = dt;
  yield();
}
