/*
  TapCheck - first power-on check for the Tap panels.
  Change the panel line below: LB_TAP_128_CTP (1.28 inch round, 240 x 240),
  LB_TAP_150_CTP (1.5 inch, 240 x 320) or LB_TAP_196_CTP (1.96 inch, 240 x 296).

  1. COLOUR     Eight vertical bars, then full-screen RED, GREEN, BLUE, YELLOW,
     CYAN, MAGENTA, WHITE, BLACK, each with its own name on it.
       - word does not match the colour, red <-> blue swapped      = bgr is wrong
       - black comes out white, red comes out cyan                 = invert is wrong
       - stripes or confetti                                       = bus or colour depth
  2. BACKLIGHT  Sweeps 1% -> 100% -> 1%, the percentage on the screen.
     Brightest at 1% means the backlight polarity in panels.yaml is wrong.
     Skipped on a panel with no backlight pin (the 1.96 inch: always on).
  3. TOUCH      Four empty boxes (kept inside the circle on the round panel). Touch each one: it fills green. A white dot
     follows your finger and its coordinates are printed. All four green = PASS,
     and the test starts again.

  The glass has rounded corners (a full circle on the 1.28): nothing important is
  drawn in them.
*/
#include <LonelyBinaryDisplay.h>
#include <LB_TouchCST816.h>
#include <LB_TouchIT7259.h>

LB_Display display(LB_TAP_196_CTP);   // or LB_TAP_128_CTP, LB_TAP_150_CTP

static int BOX = 76;            // set in setup() from the screen size
static const int MARGIN = 8;
static bool hit[4];

// Text of `size` centred on x, with its middle at y.
static void centred(const char *s, int size, int x, int y, lb_color_t color) {
  display.setTextSize(size);
  display.setTextColor(color);
  display.drawString(s, x - display.textWidth(s) / 2, y - 4 * size);
}

static void page(lb_color_t bg, lb_color_t fg, const char *name) {
  display.fillScreen(bg);
  centred("LonelyBinary", 1, display.width() / 2, 40, fg);
  centred(name, 4, display.width() / 2, display.height() / 2, fg);
  display.flush();
  Serial.println(name);
  delay(1200);
}

static void bars() {
  static const lb_color_t c[8] = {LB_WHITE, LB_YELLOW, LB_CYAN, LB_GREEN,
                                  LB_MAGENTA, LB_RED, LB_BLUE, LB_BLACK};
  const int w = display.width() / 8;
  for (int i = 0; i < 8; i++)
    display.fillRect(i * w, 0, i == 7 ? display.width() - 7 * w : w, display.height(), c[i]);
  display.flush();
  Serial.println("bars");
  delay(2000);
}

static bool hasBacklightPin() {
  return display.panelDef()->bus == LB_BUS_PAR8 ? display.wiringPar8().backlight >= 0
                                                 : display.wiring().backlight >= 0;
}

static void backlightSweep() {
  if (!hasBacklightPin()) return;
  auto show = [](int pct) {
    display.backlight((pct * 255 + 50) / 100);
    display.fillScreen(LB_WHITE);
    centred("BACKLIGHT PWM", 1, display.width() / 2, 100, LB_BLACK);
    char b[8];
    snprintf(b, sizeof b, "%d%%", pct);
    centred(b, 5, display.width() / 2, display.height() / 2, LB_BLACK);
    display.flush();
    Serial.printf("backlight %d%%\n", pct);
    delay(150);
  };
  show(1);
  for (int p = 5; p <= 100; p += 5) show(p);
  for (int p = 95; p >= 5; p -= 5) show(p);
  show(1);
  display.backlight(255);
}

static void boxOrigin(int q, int &x, int &y) {
  // Four boxes round the centre, close enough in that their outer corners stay
  // on a round glass (about 0.43 of the short side from the middle).
  const int d = min(display.width(), display.height());
  const int off = d * 4 / 100;                     // the gap between boxes is 2 * off
  x = display.width() / 2 + ((q & 1) ? off : -off - BOX);
  y = display.height() / 2 + ((q >> 1) ? off : -off - BOX);
}

static void drawTouchScreen(bool touching, int tx, int ty) {
  display.fillScreen(LB_BLACK);
  for (int q = 0; q < 4; q++) {
    int x, y;
    boxOrigin(q, x, y);
    if (hit[q]) display.fillRect(x, y, BOX, BOX, LB_GREEN);
    else for (int t = 0; t < 4; t++) display.drawRect(x + t, y + t, BOX - 2 * t, BOX - 2 * t, LB_RGB(96, 96, 96));
  }
  char b[24];
  if (touching) snprintf(b, sizeof b, "X:%d Y:%d", tx, ty);
  else snprintf(b, sizeof b, "X:-- Y:--");
  centred(b, 2, display.width() / 2, display.height() / 2, LB_WHITE);
  if (touching) display.fillCircle(tx, ty, 5, LB_WHITE);
  display.flush();
}

static void touchTest() {
  memset(hit, 0, sizeof hit);
  const uint32_t start = millis();
  int lx = -1, ly = -1;
  bool was = false;
  drawTouchScreen(false, 0, 0);
  while (millis() - start < 30000) {
    int16_t x, y;
    const bool touching = display.touch() && display.touch()->getTouch(&x, &y);
    if (touching) {
      for (int q = 0; q < 4; q++) {
        int bx, by;
        boxOrigin(q, bx, by);
        if (x >= bx - MARGIN && x < bx + BOX + MARGIN && y >= by - MARGIN && y < by + BOX + MARGIN) hit[q] = true;
      }
    }
    if (touching != was || (touching && (x != lx || y != ly))) {
      drawTouchScreen(touching, x, y);
      if (touching) Serial.printf("touch %d, %d\n", x, y);
      lx = x; ly = y; was = touching;
    }
    if (hit[0] && hit[1] && hit[2] && hit[3]) {
      page(LB_GREEN, LB_BLACK, "PASS");
      return;
    }
    delay(10);
  }
  page(LB_RED, LB_WHITE, "TIMEOUT");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!display.begin(true)) {
    Serial.println("Display did not start. Check the ribbon cable and DC.");
    return;
  }
  display.printInfo();
  BOX = min(display.width(), display.height()) * 27 / 100;   // 27 % of the short side
  if (!display.touch()) Serial.println("No touch: the touch controller did not answer on SDA/SCL.");
}

void loop() {
  bars();
  page(LB_RED, LB_WHITE, "RED");
  page(LB_GREEN, LB_BLACK, "GREEN");
  page(LB_BLUE, LB_WHITE, "BLUE");
  page(LB_YELLOW, LB_BLACK, "YELLOW");
  page(LB_CYAN, LB_BLACK, "CYAN");
  page(LB_MAGENTA, LB_WHITE, "MAGENTA");
  page(LB_WHITE, LB_BLACK, "WHITE");
  page(LB_BLACK, LB_WHITE, "BLACK");
  backlightSweep();
  touchTest();
}
