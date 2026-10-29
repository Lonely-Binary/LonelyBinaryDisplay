/*
  RtpCheck - first power-on check for a resistive-touch (RTP) panel, then a
  4-point touch calibration.

  1. CALIBRATION  Four crosses, one near each corner. Press the centre of each
     with a stylus or a fingernail and HOLD IT STILL for a moment; lift when the
     cross turns green. A resistive layer needs a firm press - a finger pad
     brushing the glass is not enough.
  2. BACKLIGHT  Sweeps 0% -> 100% -> 0%, the percentage printed on the screen.
     Brightest at 0% means the backlight polarity in panels.yaml is wrong.
  3. COLOUR     Full-screen RED, GREEN, BLUE, each with its own name on it.
     If the words do not match what you see, the colour order is wrong
     (red <-> blue = bgr). If black comes out white, invert is wrong.
  4. EDGES      White frame with a coloured bar on each edge. All four bars and
     the whole frame must be visible; a missing line means the offsets are off.
  5. BACKLIGHT  The last page, for you to use: - and + set the backlight in 5 %
     steps (hold to repeat) and show the percentage. REDO calibrates again.

  Why calibrate: a resistive layer reports raw numbers, not pixels, and every
  layer is glued on a little differently - the axes can even be swapped. Four
  taps are enough to work out all of it. The result is applied at once and
  printed on Serial in the form panels.yaml takes, so it can be made permanent
  for the panel.

  Change the panel line below for another size.
*/
#include <LonelyBinaryDisplay.h>
#include <LB_TouchNS2009.h>
#include <LB_TouchCalibration.h>

LB_Display display(LB_RTP_180);
//LB_Display display(LB_RTP_240);
//LB_Display display(LB_RTP_280);
//LB_Display display(LB_RTP_430);

static LB_TouchNS2009 *touch() { return static_cast<LB_TouchNS2009 *>(display.touch()); }

// ── backlight ───────────────────────────────────────────────────────────────

// The 4.3 inch module's backlight is hungry: at full brightness it makes the dev
// board's LDO too hot. It is held to 50 %. Every other panel runs the whole range.
static int backlightCeiling() { return display.panelDef() == LB_RTP_430 ? 50 : 100; }

// pct is 0-100 of what the panel is allowed.
static void setBacklight(int pct) {
  display.backlight((pct * 255 / 100) * backlightCeiling() / 100);
}

// ── drawing helpers ─────────────────────────────────────────────────────────

static void centred(const char *s, int size, int x, int y, lb_color_t color) {
  display.setTextSize(size);
  display.setTextColor(color);
  display.drawString(s, x - display.textWidth(s) / 2, y);
}

static void colourPage(lb_color_t bg, lb_color_t fg, const char *name) {
  display.fillScreen(bg);
  centred("LonelyBinary", 1, display.width() / 2, 6, fg);
  centred(name, 2, display.width() / 2, display.height() / 2 - 16, fg);
  display.flush();
  Serial.println(name);
}

static void showLevel(int pct) {
  setBacklight(pct);
  display.fillScreen(LB_WHITE);
  centred("BACKLIGHT", 1, display.width() / 2, 8, LB_BLACK);
  char b[8];
  snprintf(b, sizeof b, "%d%%", pct);
  centred(b, 3, display.width() / 2, display.height() / 2 - 24, LB_BLACK);
  display.flush();
  Serial.printf("backlight %d%%\n", pct);
  delay(150);
}

static void fadeBacklight() {
  for (int p = 0; p <= 100; p += 5) showLevel(p);
  for (int p = 100; p >= 0; p -= 5) showLevel(p);
  setBacklight(100);
}

static void edgePage() {
  const int w = display.width(), h = display.height();
  display.fillScreen(LB_BLACK);
  display.drawRect(0, 0, w, h, LB_WHITE);
  display.fillRect(w / 4, 0, w / 2, 4, LB_RED);            // top
  display.fillRect(w / 4, h - 4, w / 2, 4, LB_GREEN);      // bottom
  display.fillRect(0, h / 4, 4, h / 2, LB_BLUE);           // left
  display.fillRect(w - 4, h / 4, 4, h / 2, LB_YELLOW);     // right
  centred("EDGES", 2, w / 2, h / 2 - 20, LB_WHITE);
  char wh[16];
  snprintf(wh, sizeof wh, "%dx%d", w, h);
  centred(wh, 1, w / 2, h / 2 + 4, LB_WHITE);
  display.flush();
}

// ── 4-point calibration ─────────────────────────────────────────────────────
//
// The four crosses, the fit and the check are in LB_TouchCalibration.h, shared
// with the other resistive examples. It applies the result and prints it for panels.yaml.

static bool calibrate() {
  LB_TouchCal cal;
  return lb_calibrate_touch(display, touch(), cal);
}

// ── the backlight page ──────────────────────────────────────────────────────
//
// The last page: - and + set the backlight in 5 % steps (hold to repeat), the
// percentage is shown large with a bar, REDO runs the calibration again. The
// percentage is of what the panel is allowed (see backlightCeiling()).

static int backlightPct = 100;

static void backlightPage() {
  const int w = display.width(), h = display.height();
  struct R { int x, y, w, h; };
  auto in = [](const R &r, int x, int y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; };

  // Laid out from the top: title (and REDO beside it), the percentage as large as
  // fits, a bar, then - and + side by side along the bottom. Nothing overlaps on
  // a small screen because each thing has its own band.
  const int pad = 8;
  const R redo = {w - 54, 2, 50, 16};
  display.setTextSize(1);
  const int titleH = display.fontHeight();
  int ps = 6;                                              // the biggest percentage text that fits its band
  const int percentRoom = h * 28 / 100;
  while (ps > 1) {
    display.setTextSize(ps);
    if (display.textWidth("100%") <= w - 2 * pad && display.fontHeight() <= percentRoom) break;
    ps--;
  }
  display.setTextSize(ps);
  const int percentH = display.fontHeight();
  const int percentY = titleH + 8;
  const R bar = {pad, percentY + percentH + 8, w - 2 * pad, max(8, h / 16)};
  const int bh = max(26, h / 4);
  const int bw = (w - 3 * pad) / 2;
  const R minus = {pad, h - bh - pad, bw, bh}, plus = {2 * pad + bw, h - bh - pad, bw, bh};
  const int symbolSize = max(1, min(4, bh / 16));

  auto button = [&](const R &r, const char *t, lb_color_t fill, int size) {
    display.fillRoundRect(r.x, r.y, r.w, r.h, 6, fill);
    display.drawRoundRect(r.x, r.y, r.w, r.h, 6, LB_WHITE);
    display.setTextSize(size);
    display.setTextColor(LB_WHITE);
    display.drawString(t, r.x + r.w / 2 - display.textWidth(t) / 2, r.y + r.h / 2 - display.fontHeight() / 2);
  };
  auto draw = [&]() {
    display.fillScreen(LB_BLACK);
    display.setTextSize(1);
    display.setTextColor(LB_WHITE);
    display.drawString("BACKLIGHT", pad, 2);
    button(redo, "REDO", LB_RGB(40, 90, 160), 1);
    char b[8];
    snprintf(b, sizeof b, "%d%%", backlightPct);
    display.setTextSize(ps);
    display.setTextColor(LB_YELLOW);
    display.drawString(b, w / 2 - display.textWidth(b) / 2, percentY);
    display.drawRect(bar.x, bar.y, bar.w, bar.h, LB_WHITE);
    display.fillRect(bar.x + 1, bar.y + 1, (bar.w - 2) * backlightPct / 100, bar.h - 2, LB_YELLOW);
    button(minus, "-", LB_RGB(70, 70, 90), symbolSize);
    button(plus, "+", LB_RGB(70, 70, 90), symbolSize);
    display.flush();
  };
  setBacklight(backlightPct);
  draw();
  Serial.println("backlight page: - and + change it, REDO calibrates again");

  bool wasDown = false;
  uint32_t held = 0;
  while (true) {
    int16_t x, y;
    const bool down = display.touch()->getTouch(&x, &y);
    if (down && !wasDown && in(redo, x, y)) {
      while (display.touch()->getTouch(&x, &y)) delay(10);
      return;
    }
    if (down && (in(minus, x, y) || in(plus, x, y))) {
      if (!wasDown || millis() - held > 220) {          // a press steps once; holding steps again and again
        held = millis();
        // Never fully off: you could not see to turn it back up.
        backlightPct = constrain(backlightPct + (in(plus, x, y) ? 5 : -5), 5, 100);
        setBacklight(backlightPct);
        Serial.printf("backlight %d%%\n", backlightPct);
        draw();
      }
    }
    wasDown = down;
    delay(10);
  }
}

// ── the sequence ────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  delay(500);   // the panel needs a moment after power-up, or it comes up scrambled
  if (!display.begin(true)) {
    Serial.println("Display did not start. Check the ribbon cable and DC.");
    return;
  }
  setBacklight(100);   // begin() lights it fully; hold it to the panel's ceiling at once
  display.printInfo();
  if (!display.touch()) Serial.println("No touch: the NS2009 did not answer on SDA/SCL.");

  while (display.touch() && !calibrate()) {}   // first the touch, so the last page can be used
  fadeBacklight();
  colourPage(LB_RED, LB_WHITE, "RED");     delay(1500);
  colourPage(LB_GREEN, LB_BLACK, "GREEN"); delay(1500);
  colourPage(LB_BLUE, LB_WHITE, "BLUE");   delay(1500);
  edgePage();                              delay(2500);
}

void loop() {
  if (!display.touch()) { delay(1000); return; }
  backlightPage();                  // returns when REDO is pressed
  while (!calibrate()) {}
}
