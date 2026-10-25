// screen.h - Stage 7 test screen: top bar + web address + to-do list + focus test bar
// Display config is the locked one: BLACKTAB, rotation 0, CS D8, DC D3, RST -1, LED D4, SPI 40 MHz.
// Rules kept: clears use fastFill() (never fillScreen), SPI buffers aligned(4).
#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "appdata.h"
#include "netsetup.h"

#define TFT_CS  D8
#define TFT_DC  D3
#define TFT_RST -1
#define TFT_LED D4
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
const uint32_t SPI_HZ = 40000000;

#define C_NAVY  0x10A8
#define C_GREY  0x4208
#define C_SEL   0x31A6
#define C_AMBER 0xFD20
#define C_GREEN 0x07E0
#define C_DONE  0x8410

uint16_t rowBuf[128] __attribute__((aligned(4)));
static inline uint16_t be(uint16_t c) { return (c << 8) | (c >> 8); }

void fastFill(int x, int y, int w, int h, uint16_t col) {
  uint16_t c = be(col);
  for (int i = 0; i < w; i++) rowBuf[i] = c;
  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  for (int r = 0; r < h; r++) SPI.writeBytes((uint8_t*)rowBuf, w * 2);
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
  fastFill(0, 0, 128, 160, C_NAVY);
  tft.setTextWrap(false);
}

// ---- top bar (0-13): time | Wi-Fi | mute | streak ----
void drawTop() {
  fastFill(0, 0, 128, 14, C_GREY);
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE, C_GREY);
  tft.setCursor(2, 3); tft.print(clockHHMM());
  if (netState == NET_PORTAL) { tft.setTextColor(C_AMBER, C_GREY); tft.setCursor(66, 3); tft.print("AP"); }
  else {
    uint16_t c = netState == NET_ONLINE ? C_GREEN : netState == NET_CONNECTING ? C_AMBER : ST77XX_RED;
    for (uint8_t i = 0; i < 3; i++) tft.fillRect(66 + i * 4, 11 - (i + 1) * 3, 3, (i + 1) * 3, c);
    if (netState == NET_LOST || netState == NET_OFF) tft.drawLine(65, 2, 77, 12, ST77XX_WHITE);
  }
  if (muted) {
    tft.fillRect(82, 5, 3, 5, ST77XX_RED); tft.fillTriangle(84, 7, 88, 3, 88, 11, ST77XX_RED);
    tft.drawLine(90, 5, 94, 9, ST77XX_RED); tft.drawLine(94, 5, 90, 9, ST77XX_RED);
  }
  tft.fillTriangle(100, 11, 106, 11, 103, 2, C_AMBER);          // flame
  tft.setTextColor(ST77XX_WHITE, C_GREY); tft.setCursor(109, 3); tft.print(pet.streakCur);
}

// ---- line 16-27: where to open the page ----
void drawInfo() {
  fastFill(0, 14, 128, 16, C_NAVY);
  tft.setTextSize(1); tft.setTextColor(C_AMBER, C_NAVY); tft.setCursor(2, 18);
  switch (netState) {
    case NET_ONLINE:     tft.print("Web "); tft.print(WiFi.localIP()); break;
    case NET_CONNECTING: tft.print("Wi-Fi connecting..."); break;
    case NET_LOST:       tft.print("Wi-Fi lost, retrying"); break;
    case NET_PORTAL:     tft.print("Setup: Pomodoro-Setup"); break;
    default:             tft.print("Wi-Fi off");
  }
}

// ---- setup help (replaces the list while the Pomodoro-Setup hotspot is on) ----
void drawSetupHelp() {
  fastFill(0, 30, 128, 14, C_AMBER);
  tft.setTextColor(ST77XX_BLACK, C_AMBER); tft.setCursor(2, 33); tft.print("WI-FI SETUP");
  fastFill(0, 44, 128, 100, C_NAVY);
  static const char* const L[] = { "1. On your phone open", "   Wi-Fi settings", "2. Join (no password)", "*  Pomodoro-Setup",
                                   "3. Page opens itself", "*  or 192.168.4.1", "4. Pick your Wi-Fi /", "   hotspot + password" };
  for (uint8_t i = 0; i < 8; i++) {
    bool hi = L[i][0] == '*';
    tft.setTextColor(hi ? C_AMBER : ST77XX_WHITE, C_NAVY); tft.setCursor(2, 47 + i * 12); tft.print(hi ? L[i] + 1 : L[i]);
  }
}

// ---- to-do list (30-141): header + 7 rows x 14 px ----
void drawTodos() {
  if (netState == NET_PORTAL) { drawSetupHelp(); return; }
  uint8_t done = 0;
  for (uint8_t i = 0; i < todos.count; i++) done += todos.item[i].done ? 1 : 0;
  fastFill(0, 30, 128, 14, C_GREY);
  tft.setTextColor(ST77XX_WHITE, C_GREY); tft.setCursor(2, 33); tft.print("TO-DO");
  char b[14]; snprintf(b, sizeof(b), "%u/%u done", done, todos.count);
  tft.setCursor(126 - strlen(b) * 6, 33); tft.print(b);
  for (uint8_t r = 0; r < 7; r++) {
    int y = 44 + r * 14;
    bool sel = (r == todoSel && r < todos.count);
    uint16_t bg = sel ? C_SEL : C_NAVY;
    fastFill(0, y, 128, 14, bg);
    if (r >= todos.count) continue;
    const TodoItem& it = todos.item[r];
    if (sel) { tft.setTextColor(C_AMBER, bg); tft.setCursor(1, y + 4); tft.print('>'); }
    tft.drawRect(8, y + 3, 8, 8, it.done ? C_DONE : ST77XX_WHITE);
    if (it.done) tft.fillRect(10, y + 5, 4, 4, C_GREEN);
    char line[19]; size_t n = strlen(it.text);
    if (n <= 18) strcpy(line, it.text); else { memcpy(line, it.text, 17); line[17] = '~'; line[18] = 0; }   // Stage 8 scrolls it
    tft.setTextColor(it.done ? C_DONE : ST77XX_WHITE, bg); tft.setCursor(20, y + 4); tft.print(line);
  }
  if (!todos.count) {
    tft.setTextColor(ST77XX_WHITE, C_NAVY);
    tft.setCursor(4, 64); tft.print("No tasks.");
    tft.setCursor(4, 80); tft.print("Add them at");
    tft.setTextColor(C_AMBER, C_NAVY); tft.setCursor(4, 94);
    if (netState == NET_ONLINE) { tft.print("http://"); tft.print(WiFi.localIP()); } else tft.print("the web page");
  }
}

// ---- bottom bar (144-159): 1-min focus test ----
void drawTimer() {
  fastFill(0, 144, 128, 16, C_GREY);
  tft.setTextSize(1); tft.setCursor(2, 148);
  if (ft.run) {
    tft.setTextColor(C_AMBER, C_GREY);
    tft.printf("FOCUS %d:%02d", ft.lastSec / 60, ft.lastSec % 60);
    tft.setTextColor(ST77XX_WHITE, C_GREY); tft.printf("  skip %u", ft.skips);
  } else { tft.setTextColor(ST77XX_WHITE, C_GREY); tft.print("MODE=focus OK=tick"); }
}

String   shownHHMM;
NetState shownNet = NET_OFF;
bool     shownMute = false;
uint16_t shownStreak = 0xFFFF;

void screenTick() {
  static uint32_t lastChk = 0;
  static bool first = true;
  if (first || millis() - lastChk >= 250) {
    lastChk = millis();
    String hm = clockHHMM();
    bool netChanged = netState != shownNet;
    if (first || hm != shownHHMM || netChanged || muted != shownMute || pet.streakCur != shownStreak) {
      shownHHMM = hm; shownNet = netState; shownMute = muted; shownStreak = pet.streakCur;
      drawTop();
      if (first || netChanged) { drawInfo(); uiTodo = true; }
    }
    first = false;
  }
  if (uiTodo)  { uiTodo = false;  drawTodos(); }
  if (uiTimer) { uiTimer = false; drawTimer(); }
}
