/*
  A reflective LCD (RLCD): 2.13, 2.9 or 4.2 inch, ST7305.

  Not e-paper: flush() takes milliseconds and the picture is gone without
  power. There is no backlight. Only the 4.2 inch needs PSRAM (Tools > PSRAM >
  OPI on the ESP32-S3), because its canvas is 240 KB.

  The sketch draws in colour as usual; flush() turns it into black and white
  (anything with luma >= 50 % is white).
*/
#include <LonelyBinaryDisplay.h>

LB_Display display(LB_RLCD_213);
//LB_Display display(LB_RLCD_290);
//LB_Display display(LB_RLCD_420);

void setup() {
  Serial.begin(115200);
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
