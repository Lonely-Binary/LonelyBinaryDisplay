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

// ---- GPIO wiring. Every pin this sketch uses is here - change the numbers to match
// ---- your board; nothing below refers to a GPIO by number. -1 = not connected.
#define PIN_HSYNC   2   // VGA connector
#define PIN_VSYNC   1
#define PIN_B0     11   // blue  - 5-bit R2R ladder, B0 = least significant
#define PIN_B1     12
#define PIN_B2     13
#define PIN_B3     14
#define PIN_B4     21
#define PIN_G0     17   // green - 6-bit ladder
#define PIN_G1     18
#define PIN_G2      8
#define PIN_G3      3
#define PIN_G4      9
#define PIN_G5     10
#define PIN_R0      7   // red   - 5-bit ladder
#define PIN_R1     15
#define PIN_R2     16
#define PIN_R3     38
#define PIN_R4     39

#define PIN_BOOT   0   // the on-board BOOT button

LB_Display display(LB_VGA_800X600);

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
    if (digitalRead(PIN_BOOT) == LOW) {
      while (digitalRead(PIN_BOOT) == LOW) delay(10);   // wait for release
      return true;
    }
    delay(10);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  LB_WiringRgb pins = display.wiringRgb();   // the board's default, then yours
  pins.hsync = PIN_HSYNC;  pins.vsync = PIN_VSYNC;
  const int8_t dataPins[16] = {PIN_B0, PIN_B1, PIN_B2, PIN_B3, PIN_B4,
                              PIN_G0, PIN_G1, PIN_G2, PIN_G3, PIN_G4, PIN_G5,
                              PIN_R0, PIN_R1, PIN_R2, PIN_R3, PIN_R4};
  for (int i = 0; i < 16; i++) pins.data[i] = dataPins[i];
  display.setWiring(pins);

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
