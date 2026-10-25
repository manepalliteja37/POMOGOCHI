// gfx.h - display basics shared by every screen (Stage 8a)
// Locked config: BLACKTAB, rotation 0, CS D8, DC D3, RST -1, LED D4 PWM, SPI 40 MHz.
// Rules: clears use fastFill() (never fillScreen); SPI buffers aligned(4); sprites drawn row by row.
#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "pet_frames.h"
#include "appdata.h"
#include "netsetup.h"

#define TFT_CS  D8
#define TFT_DC  D3
#define TFT_RST -1
#define TFT_LED D4
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
const uint32_t SPI_HZ = 40000000;

#define C_BLACK 0x0000
#define C_WHITE 0xFFFF
#define C_NAVY  0x10A8
#define C_GREY  0x4208
#define C_SEL   0x31A6
#define C_AMBER 0xFD20
#define C_GREEN 0x07E0
#define C_RED   0xF800
#define C_DONE  0x8410
#define C_DIM   0x7BEF
#define C_BARBG 0x2104
#define C_ORANGE 0xFC60
#define C_YELLOW 0xFFE0

enum Screen : uint8_t { SC_ONBOARD, SC_HOME, SC_SETUP, SC_FOCUS, SC_BREAK, SC_TODO, SC_WIFI, SC_INFO };
const char* SCREEN_NAME[] = { "Onboarding", "Home", "Setup", "Focus", "Break", "To-do", "Wi-Fi help", "Info" };
Screen   scr = SC_HOME;
bool     uiFull = true;
void goScreen(Screen s) { scr = s; uiFull = true; }

uint16_t rowBuf[128] __attribute__((aligned(4)));
static inline uint16_t be(uint16_t c) { return (c << 8) | (c >> 8); }

void fastFill(int x, int y, int w, int h, uint16_t col) {
  if (w <= 0 || h <= 0) return;
  uint16_t c = be(col);
  for (int i = 0; i < w; i++) rowBuf[i] = c;
  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  for (int r = 0; r < h; r++) SPI.writeBytes((uint8_t*)rowBuf, w * 2);
  tft.endWrite();
}

void drawSprite(int x, int y, const Sprite& s, uint16_t bg, uint16_t body = C_WHITE) {
  const uint16_t INK = be(C_BLACK), BODY = be(body), BG = be(bg);
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
}

void applyBacklight() { analogWrite(TFT_LED, constrain(settings.backlight, 30, 1023)); }
void applySettings() {
  muted = settings.muted; volume = min((int)settings.volume, 2); hbSel = min((int)settings.hbSel, N_HB - 1);
  applyBacklight();
}

void screenBegin() {
  analogWriteRange(1023); analogWriteFreq(1000); analogWrite(TFT_LED, 1023);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.setSPISpeed(SPI_HZ);
  tft.setTextWrap(false);
  fastFill(0, 0, 128, 160, C_NAVY);
}

// text helpers
void txt(int x, int y, const char* s, uint16_t fg, uint16_t bg, uint8_t size = 1) {
  tft.setTextSize(size); tft.setTextColor(fg, bg); tft.setCursor(x, y); tft.print(s);
}
void txtC(int y, const char* s, uint16_t fg, uint16_t bg, uint8_t size = 1) {   // centred
  int w = strlen(s) * 6 * size; txt((128 - w) / 2, y, s, fg, bg, size);
}

// ---- top bar (0-13): time | Wi-Fi | mute | streak (Home + Setup) ----
void drawTopBar() {
  fastFill(0, 0, 128, 14, C_GREY);
  txt(2, 3, clockHHMM().c_str(), C_WHITE, C_GREY);
  if (netState == NET_PORTAL) txt(66, 3, "AP", C_AMBER, C_GREY);
  else {
    uint16_t c = netState == NET_ONLINE ? C_GREEN : netState == NET_CONNECTING ? C_AMBER : C_RED;
    for (uint8_t i = 0; i < 3; i++) tft.fillRect(66 + i * 4, 11 - (i + 1) * 3, 3, (i + 1) * 3, c);
    if (netState == NET_LOST || netState == NET_OFF) tft.drawLine(65, 2, 77, 12, C_WHITE);
  }
  if (muted) {
    tft.fillRect(82, 5, 3, 5, C_RED); tft.fillTriangle(84, 7, 88, 3, 88, 11, C_RED);
    tft.drawLine(90, 5, 94, 9, C_RED); tft.drawLine(94, 5, 90, 9, C_RED);
  }
  tft.fillTriangle(100, 11, 106, 11, 103, 2, C_AMBER);
  tft.setTextColor(C_WHITE, C_GREY); tft.setCursor(109, 3); tft.print(pet.streakCur);
}

// cycle dots ●●○○ centred at (cx, cy)
void drawDots(int cx, int cy, uint8_t n, uint8_t r, uint16_t bg) {
  int gap = r * 2 + 6, x0 = cx - (3 * gap) / 2;
  for (uint8_t i = 0; i < 4; i++) {
    int x = x0 + i * gap;
    tft.fillCircle(x, cy, r, bg);
    if (i < n) tft.fillCircle(x, cy, r, C_AMBER); else tft.drawCircle(x, cy, r, C_DIM);
  }
}
