/*
  SelfTest — does my screen and wiring work?

  Shows solid colours, a backlight sweep (on panels that dim) and a colour-bar
  page with the panel's identity, then prints the same details to Serial.

  TO USE A DIFFERENT SCREEN, CHANGE ONE LINE — the LB_* constant below.
  Everything else (driver IC, resolution, offsets, SPI clock, backlight
  polarity) comes from the panel table inside the library.

      LB_TFT_096   LB_TFT_18   LB_TFT_20   LB_TFT_24   LB_TFT_28   LB_TFT_35
      LB_NARROW_114   LB_NARROW_168   LB_NARROW_19
      LB_NARROW_225   LB_NARROW_279

  Pick your board first: Tools > Board > "ESP32S3 Dev Module" for an ESP32-S3,
  or "ESP32 Dev Module" for a classic ESP32. The GPIOs follow automatically.
*/
#include <LonelyBinaryDisplay.h>

// ---- GPIO wiring. Every pin this sketch uses is here - change the numbers to match
// ---- your board; nothing below refers to a GPIO by number. -1 = not connected.
#if CONFIG_IDF_TARGET_ESP32S3   // ESP32-S3 (Lonely Binary kit)
  #define PIN_CS    10
  #define PIN_RST   42
  #define PIN_DC     2
  #define PIN_MOSI  11
  #define PIN_SCLK  12
  #define PIN_MISO  -1   // not used by the display
  #define PIN_BL    41   // backlight (PWM)
#else                           // classic ESP32
  #define PIN_CS    15
  #define PIN_RST    4
  #define PIN_DC     2
  #define PIN_MOSI  23
  #define PIN_SCLK  18
  #define PIN_MISO  -1   // not used by the display
  #define PIN_BL    32   // backlight (PWM)
#endif

LB_Display display(LB_TFT_24);
//LB_Display display(LB_TFT_096);
//LB_Display display(LB_TFT_18);
//LB_Display display(LB_TFT_20);
//LB_Display display(LB_TFT_28);
//LB_Display display(LB_TFT_35);
//LB_Display display(LB_NARROW_114);
//LB_Display display(LB_NARROW_168);
//LB_Display display(LB_NARROW_19);
//LB_Display display(LB_NARROW_225);
//LB_Display display(LB_NARROW_279);

void setup() {
  Serial.begin(115200);
  delay(500);

  LB_Wiring pins = LB_WIRING;   // keeps the SPI bus that fits this chip
  pins.cs = PIN_CS;  pins.rst = PIN_RST;  pins.dc = PIN_DC;
  pins.mosi = PIN_MOSI;  pins.sclk = PIN_SCLK;  pins.miso = PIN_MISO;
  pins.backlight = PIN_BL;
  display.setWiring(pins);

  if (!display.begin()) {
    Serial.println("Display did not start. Check the ribbon cable and DC.");
    return;
  }

  display.printInfo();
  display.selfTest();
}

void loop() {
  delay(1000);
}
