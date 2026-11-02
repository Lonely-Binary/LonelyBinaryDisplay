/*
  A reflective LCD (RLCD): 2.13, 2.9 or 4.2 inch, ST7305.

  Not e-paper: flush() takes milliseconds and the picture is gone without
  power. There is no backlight. Only the 4.2 inch needs PSRAM (Tools > PSRAM >
  OPI on the ESP32-S3), because its canvas is 240 KB.

  The sketch draws in colour as usual; flush() turns it into black and white
  (anything with luma >= 50 % is white).
*/
#include <LonelyBinaryDisplay.h>

//LB_Display display(LB_RLCD_213);
//LB_Display display(LB_RLCD_290);
// ---- GPIO wiring. Every pin this sketch uses is here - change the numbers to match
// ---- your board; nothing below refers to a GPIO by number. -1 = not connected.
#if CONFIG_IDF_TARGET_ESP32S3   // ESP32-S3 (Lonely Binary kit)
  #define PIN_CS    10
  #define PIN_RST   42
  #define PIN_DC     2
  #define PIN_MOSI  11
  #define PIN_SCLK  12
  #define PIN_MISO  -1   // not used by the display
#else                           // classic ESP32
  #define PIN_CS    15
  #define PIN_RST    4
  #define PIN_DC     2
  #define PIN_MOSI  23
  #define PIN_SCLK  18
  #define PIN_MISO  -1   // not used by the display
#endif

LB_Display display(LB_RLCD_420);

void setup() {
  Serial.begin(115200);
  LB_Wiring pins = LB_WIRING;   // keeps the SPI bus that fits this chip
  pins.cs = PIN_CS;  pins.rst = PIN_RST;  pins.dc = PIN_DC;
  pins.mosi = PIN_MOSI;  pins.sclk = PIN_SCLK;  pins.miso = PIN_MISO;
  display.setWiring(pins);

  if (!display.begin()) {
    Serial.println("panel did not start - check wiring (the 4.2 inch also needs PSRAM)");
    while (true) delay(1000);
  }
  display.printInfo();

  const int W = display.width(), H = display.height();
  const int s = W >= 300 ? 3 : 2;               // text scale: the 2.13 inch is only 122 wide
  display.fillScreen(LB_WHITE);
  display.setTextColor(LB_BLACK);
  display.setTextSize(s);
  display.drawString("Lonely", 8, 8);
  display.drawString("Binary", 8, 8 + 9 * s);
  display.setTextSize(1);
  display.drawString(display.panelDef()->name, 8, 8 + 20 * s);
  display.drawRect(0, 0, W, H, LB_BLACK);
  display.fillRect(8, H / 2, W - 16, H / 5, LB_BLACK);
  display.setTextColor(LB_WHITE);
  display.setTextSize(s - 1);
  display.drawString("INVERSE", 16, H / 2 + H / 10 - 4 * (s - 1));
  display.flush();
}

void loop() {}
