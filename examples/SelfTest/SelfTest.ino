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

LB_Display display(LB_TFT_24);

void setup() {
  Serial.begin(115200);
  delay(500);

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
