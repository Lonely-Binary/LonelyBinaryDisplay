/*
  Vga06_Photo - a photo slideshow from pictures stored in the sketch.

  YOU WILL LEARN
    - that a picture in flash is only an array of bytes (see photos.h)
    - drawJpg(data, length, x, y, scale) decodes a JPEG straight to the screen
    - scale can be 1, 0.5, 0.25 or 0.125 - nothing in between
    - how to put your OWN pictures in with make_photos.py

  WHAT IT SHOWS
    First a contact sheet of every picture at half size, then each one full
    screen, a few seconds each, forever. Press BOOT to skip to the next.

  USING YOUR OWN PICTURES
    pip install pillow
    python3 make_photos.py holiday1.jpg holiday2.png ...      (up to 8)
    Then upload again. The script scales and crops to 800 x 600 and writes a
    baseline JPEG; the decoder cannot read progressive JPEGs and a progressive
    file would simply draw nothing.

  GOOD TO KNOW
    A full-screen JPEG takes about 1.3 seconds to decode (a half-size one 0.6 s),
    so the picture paints in from the top, line after line. That is the decoder
    at work, not a fault: the screen being sent to the monitor leaves little
    memory bandwidth for writing a new picture into it.
    Pictures live in flash, so they cost no RAM and no PSRAM bandwidth.
*/
#include <LonelyBinaryDisplay.h>
#include "photos.h"                    // made by make_photos.py

LB_Display display(LB_VGA_800X600);

static const int BUTTON_PIN = 0;
static const uint32_t SHOW_MS = 4000;

static void caption(int i) {
  char s[40];
  snprintf(s, sizeof s, " %d / %d ", i + 1, PHOTO_COUNT);
  display.setTextSize(2);
  display.setTextColor(LB_WHITE, LB_RGB(0, 0, 0));
  display.drawString(s, 12, display.height() - 40);
}

// Wait up to `ms`, or until BOOT is pressed. True if it was pressed.
static bool waitOrButton(uint32_t ms) {
  const uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      while (digitalRead(BUTTON_PIN) == LOW) delay(10);   // wait for release
      return true;
    }
    delay(10);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  if (!display.begin()) {
    Serial.println("VGA did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  const int W = display.width(), H = display.height();

  // The contact sheet: every picture at half size (400 x 300), two by two.
  display.fillScreen(LB_RGB(10, 14, 20));
  for (int i = 0; i < PHOTO_COUNT && i < 4; i++) {
    const int x = (i % 2) * (W / 2), y = (i / 2) * (H / 2);
    uint32_t t0 = millis();
    const bool ok = display.drawJpg(PHOTOS[i].data, PHOTOS[i].len, x, y, 0.5f);
    Serial.printf("picture %d: %s, %lu ms at half size\n", i, ok ? "ok" : "FAILED to decode",
                  (unsigned long)(millis() - t0));
  }
  display.flush();
  waitOrButton(SHOW_MS);
}

void loop() {
  for (int i = 0; i < PHOTO_COUNT; i++) {
    const uint32_t t0 = millis();
    const bool ok = display.drawJpg(PHOTOS[i].data, PHOTOS[i].len, 0, 0);
    if (ok) caption(i);
    display.flush();
    Serial.printf("picture %d full screen: %s, decoded in %lu ms\n", i, ok ? "ok" : "FAILED",
                  (unsigned long)(millis() - t0));
    waitOrButton(SHOW_MS);
  }
}
