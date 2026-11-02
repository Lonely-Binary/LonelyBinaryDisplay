/*
  RgbBacklight - the backlight PWM of the RGB series, swept for ever.

  The brightness climbs from 0 % to 100 %, falls back to 0 % and starts again,
  with the percentage in the middle of the screen. Use it to check that the
  backlight dims smoothly over the whole range, with no step, flicker or dead
  band, and that 0 % is really dark.

  WHAT YOU SHOULD SEE
    A grey screen with a big number in the middle, counting 0 -> 100 -> 0,
    the screen getting brighter as it counts up. Near 0 % the number is hard
    to read: the backlight is nearly off, which is the point.

  The backlight is driven by LEDC PWM (5 kHz, 8 bits) on the panel's
  backlight pin. On the IO-to-RGB adapter that pin enables the backlight's
  boost converter. display.backlight() takes 0-255; this sketch shows it as
  a percentage.

  Pick your panel on the LB_Display line below. ESP32-S3 only, and Tools >
  PSRAM must be OPI PSRAM.
*/
#include <LonelyBinaryDisplay.h>

// ---- GPIO wiring. Every pin this sketch uses is here - change the numbers to match
// ---- your board; nothing below refers to a GPIO by number. -1 = not connected.
#define PIN_BL     14   // backlight enable on the IO-to-RGB adapter (PWM)

LB_Display display(LB_RGB_1010_1024X600);
//LB_Display display(LB_RGB_700_1024X600);
//LB_Display display(LB_RGB_430_480X272);

static const uint16_t STEP_MS = 90;   // one percent every 90 ms: 9 s up, 9 s down

static const lb_color_t GREY = LB_RGB(96, 96, 96);   // mid grey: dimming shows on it
static int boxX, boxY, boxW, boxH;    // the box "100%" fills, centred

static void showPercent(int pct) {
  char s[8];
  snprintf(s, sizeof s, "%d%%", pct);
  // Clear the whole box (a shorter number must not leave the end of a longer
  // one behind), then centre this number in it.
  display.fillRect(boxX, boxY, boxW, boxH, GREY);
  display.drawString(s, (display.width() - display.textWidth(s)) / 2, boxY);
  display.flush();
}

static void setPercent(int pct) {
  display.backlight(pct * 255 / 100);
  showPercent(pct);
}

void setup() {
  Serial.begin(115200);
  LB_WiringRgb pins = display.wiringRgb();   // the board's default, then yours
  pins.backlight = PIN_BL;
  display.setWiring(pins);
  if (!display.begin()) {
    Serial.println("Display did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  display.printInfo();

  // The biggest whole scale at which "100%" fits in half the width and half
  // the height. Measured with the font, not guessed.
  const int W = display.width(), H = display.height();
  display.setTextSize(1);
  const int w1 = display.textWidth("100%"), h1 = display.fontHeight();
  int scale = min(W / 2 / w1, H / 2 / h1);
  if (scale < 1) scale = 1;
  display.setTextSize(scale);
  display.setTextColor(LB_WHITE);
  boxW = display.textWidth("100%");
  boxH = display.fontHeight();
  boxX = (W - boxW) / 2;
  boxY = (H - boxH) / 2;
  display.fillScreen(GREY);
  display.flush();
}

void loop() {
  for (int pct = 0; pct <= 100; pct++) { setPercent(pct); delay(STEP_MS); }
  Serial.println("100 %");
  for (int pct = 99; pct >= 1; pct--) { setPercent(pct); delay(STEP_MS); }
  Serial.println("0 %");
}
