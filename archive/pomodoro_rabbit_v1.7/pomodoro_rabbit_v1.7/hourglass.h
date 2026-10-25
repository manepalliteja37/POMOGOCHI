// hourglass.h - Stage 2 hourglass (LOCKED geometry + volume-correct sand), now driven by session.h
// Neck at y 80, bulbs 54 rows, wooden caps, funnel top / mound bottom, falling stream.
// Only changed rows are redrawn; full redraw only when the Focus screen opens.
#pragma once
#include "gfx.h"
#include "session.h"

#define C_GLASS 0x9CF3
#define C_WOOD  0x8A22
#define C_HG_BG C_BLACK

const int CX = 64, NECK_Y = 80, H = 54;
uint8_t  hw[H];
uint16_t rw[H];
uint16_t tcum[H + 1], bcum[H + 1];
uint16_t P;
uint16_t lastTop = 0xFFFF, lastBot = 0xFFFF;

void buildGlass() {
  const uint8_t shoulder[5] = {12, 7, 4, 2, 1};
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

void drawTopRow(int d, uint16_t pix, uint16_t col) {
  int y = NECK_Y - 1 - d, L = CX - hw[d], w = rw[d], f = topFill(d, pix);
  int a = f / 2, b = f - a;
  hl(L, y, a, col); hl(L + a, y, w - a - b, C_HG_BG); hl(L + w - b, y, b, col);
}
void drawBotRow(int d, uint16_t pix, uint16_t col) {
  int y = NECK_Y + 1 + d, L = CX - hw[d], w = rw[d], k = botFill(d, pix);
  int side = (w - k) / 2;
  hl(L, y, side, C_HG_BG); hl(L + side, y, k, col); hl(L + side + k, y, w - side - k, C_HG_BG);
}
int topIdx(uint16_t p) { for (int d = 0; d < H; d++) if (p < tcum[d + 1]) return d; return H - 1; }
int botIdx(uint16_t p) { for (int e = 0; e < H; e++) if (p < bcum[e + 1]) return H - 1 - e; return 0; }

void drawGlassOutline() {
  int topY = NECK_Y - 1 - (H - 1), botY = NECK_Y + 1 + (H - 1);
  tft.fillRect(CX - 46, topY - 5, 93, 4, C_WOOD);
  tft.fillRect(CX - 46, botY + 2, 93, 4, C_WOOD);
  hl(CX - hw[H - 1] - 1, topY - 1, rw[H - 1] + 2, C_GLASS);
  hl(CX - hw[H - 1] - 1, botY + 1, rw[H - 1] + 2, C_GLASS);
  int px = 1;
  tft.drawPixel(CX - 1, NECK_Y, C_GLASS); tft.drawPixel(CX + 1, NECK_Y, C_GLASS);
  for (int d = 0; d < H; d++) {
    int x = hw[d] + 1, yt = NECK_Y - 1 - d, yb = NECK_Y + 1 + d;
    tft.drawLine(CX - px, yt + 1, CX - x, yt, C_GLASS);
    tft.drawLine(CX + px, yt + 1, CX + x, yt, C_GLASS);
    tft.drawLine(CX - px, yb - 1, CX - x, yb, C_GLASS);
    tft.drawLine(CX + px, yb - 1, CX + x, yb, C_GLASS);
    px = x;
  }
}

uint16_t sandTop() {
  uint32_t dur = timer.durationMs ? timer.durationMs : 1;
  return (uint16_t)(((uint64_t)P * timer.remaining()) / dur);
}

void updateSand(bool force) {
  uint16_t top = sandTop(), bot = P - top;
  if (force) { for (int d = 0; d < H; d++) { drawTopRow(d, top, C_AMBER); drawBotRow(d, bot, C_AMBER); } }
  else {
    if (top != lastTop) { int a = topIdx(top), b = topIdx(lastTop); for (int d = min(a, b); d <= max(a, b); d++) drawTopRow(d, top, C_AMBER); }
    if (bot != lastBot) { int a = botIdx(bot), b = botIdx(lastBot); for (int d = min(a, b); d <= max(a, b); d++) drawBotRow(d, bot, C_AMBER); }
  }
  lastTop = top; lastBot = bot;
}

uint8_t  streamPhase = 0;
int      streamDrawnTo = -1;
uint32_t lastStream = 0;
int streamEnd() { int d = 0; while (d < H && botFill(d, lastBot) == 0) d++; return d; }
void clearStream() {
  if (streamDrawnTo < 0) return;
  tft.drawPixel(CX, NECK_Y, C_HG_BG);
  for (int d = 0; d < streamDrawnTo && d < H; d++) if (botFill(d, lastBot) == 0) tft.drawPixel(CX, NECK_Y + 1 + d, C_HG_BG);
  streamDrawnTo = -1;
}
void animateStream() {
  if (!(timer.running && lastTop > 0)) { clearStream(); return; }
  if (millis() - lastStream < 80) return;
  lastStream = millis();
  streamPhase = (streamPhase + 1) % 3;
  int end = streamEnd();
  tft.drawPixel(CX, NECK_Y, C_AMBER);
  for (int d = 0; d < end; d++) tft.drawPixel(CX, NECK_Y + 1 + d, ((d + streamPhase) % 3) ? C_AMBER : C_HG_BG);
  streamDrawnTo = end;
}

// MM:SS (size 2) under the glass, only changed digits
char shownTime[6] = "";
void drawTime(int x, int y, uint32_t remMs, uint16_t bg, bool force) {
  uint32_t secs = (remMs + 999) / 1000;
  char buf[6]; snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  tft.setTextSize(2); tft.setTextColor(C_WHITE, bg);
  for (int i = 0; i < 5; i++) if (force || buf[i] != shownTime[i]) { tft.setCursor(x + i * 12, y); tft.print(buf[i]); }
  memcpy(shownTime, buf, 6);
}
