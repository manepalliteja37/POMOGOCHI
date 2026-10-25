// Stage 0 - Display bring-up + backlight test
// Board: NodeMCU 1.0 (ESP-12E) | Erase Flash: Only Sketch
// Libraries: Adafruit GFX Library, Adafruit ST7735 and ST7789 Library
//
// Wiring (locked): SCK D5, MOSI D7, CS D8, DC D3, RST -> NodeMCU RST, LED D4, VCC 3V3

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS   D8   // GPIO15
#define TFT_DC   D3   // GPIO0
#define TFT_RST  -1   // tied to the NodeMCU RST pin
#define TFT_LED  D4   // GPIO2, PWM backlight

// Change this if colours/edges look wrong: INITR_BLACKTAB, INITR_GREENTAB, INITR_REDTAB
#define TFT_TAB  INITR_BLACKTAB

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

void heap(const char* step) {
  Serial.printf("[%s] free heap: %u bytes\n", step, ESP.getFreeHeap());
}

void label(const char* text, uint16_t fg) {
  tft.setTextSize(2);
  tft.setTextColor(fg);
  tft.setCursor(10, 70);
  tft.print(text);
}

void colourTest() {
  tft.fillScreen(ST77XX_RED);   label("RED",   ST77XX_WHITE); delay(1500);
  tft.fillScreen(ST77XX_GREEN); label("GREEN", ST77XX_BLACK); delay(1500);
  tft.fillScreen(ST77XX_BLUE);  label("BLUE",  ST77XX_WHITE); delay(1500);
  heap("colour");
}

void edgeTest() {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), ST77XX_WHITE);   // 1 px frame on all 4 edges
  tft.drawRect(4, 4, tft.width() - 8, tft.height() - 8, ST77XX_YELLOW);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 12);
  tft.printf("%dx%d portrait", tft.width(), tft.height());
  tft.setCursor(10, 26);
  tft.print("TOP");
  tft.setCursor(10, 140);
  tft.print("BOTTOM");
  tft.fillRect(34, 60, 60, 40, ST77XX_ORANGE);                   // amber block (hourglass colour)
  tft.setCursor(40, 76);
  tft.setTextColor(ST77XX_BLACK);
  tft.print("AMBER");
  delay(4000);
  heap("edges");
}

void backlightTest() {
  tft.fillScreen(ST77XX_WHITE);
  label("FADE", ST77XX_BLACK);
  for (int v = 1023; v >= 0; v -= 16) { analogWrite(TFT_LED, v); delay(20); }  // 100% -> 0%
  delay(500);
  for (int v = 0; v <= 1023; v += 16) { analogWrite(TFT_LED, v); delay(20); }  // 0% -> 100%
  analogWrite(TFT_LED, 1023);
  heap("backlight");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nStage 0 - display bring-up");

  analogWriteRange(1023);
  analogWriteFreq(1000);
  analogWrite(TFT_LED, 1023);      // backlight on

  tft.initR(TFT_TAB);
  tft.setRotation(0);              // portrait, 128x160
  heap("init");
}

void loop() {
  colourTest();
  edgeTest();
  backlightTest();
}