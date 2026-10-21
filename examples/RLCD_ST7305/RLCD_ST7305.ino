/*
  ST7305 400x300 reflective LCD (RLCD).

  Not e-paper: flush() takes milliseconds and the picture is gone without
  power. There is no backlight. Needs PSRAM (Tools > PSRAM > OPI on the
  ESP32-S3), because the canvas is 240 KB.

  The sketch draws in colour as usual; flush() turns it into black and white
  (anything with luma >= 50 % is white).
*/
#include <LonelyBinaryDisplay.h>

LB_Display display(LB_ST7305_400X300);

void setup() {
  Serial.begin(115200);
  if (!display.begin()) {
    Serial.println("panel did not start - check wiring and PSRAM");
    while (true) delay(1000);
  }
  display.printInfo();

  display.fillScreen(LB_WHITE);
  display.setTextColor(LB_BLACK);
  display.setTextSize(3);
  display.drawString("Lonely Binary", 20, 20);
  display.setTextSize(2);
  display.drawString("ST7305 400x300 RLCD", 20, 70);
  display.drawRect(0, 0, display.width(), display.height(), LB_BLACK);
  display.fillRect(20, 120, 160, 60, LB_BLACK);
  display.setTextColor(LB_WHITE);
  display.drawString("INVERSE", 40, 140);
  display.flush();
}

void loop() {}
