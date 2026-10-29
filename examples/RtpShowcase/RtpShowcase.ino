/*
  RtpShowcase - a touch show-reel for the resistive-touch (RTP) panels.

  Four things to try, from a home screen of big buttons:

    PAINT      a colour painting page: eight colours, brush size, clear
    WHACK      whack-a-mole: tap the moles before they duck back, five lives
    MIXER      three sliders mix a colour; a fourth sets the backlight (5-100 %)
    CALIBRATE  the 4-point touch calibration again

  The first time it runs on a panel it asks for the calibration: four crosses, one
  near each corner; press each with a stylus or a fingernail and hold still until it
  turns green. The
  result is kept in the chip's flash (one slot per panel), so later starts go
  straight to the home screen. A resistive layer needs a firm press: a finger pad
  brushing the glass is not enough.

  Everything is laid out from the screen size, so it runs on every RTP panel:
  change the panel line below (comment one out, uncomment another).

  BACK (top left of every page) returns to the home screen.
*/
#include <LonelyBinaryDisplay.h>
#include <LB_TouchNS2009.h>
#include <LB_TouchCalibration.h>
#include <Preferences.h>

LB_Display display(LB_RTP_180);
//LB_Display display(LB_RTP_240);
//LB_Display display(LB_RTP_280);
//LB_Display display(LB_RTP_430);

// Types first: the Arduino build puts function prototypes above the first function.
struct Rect { int x, y, w, h; };
enum App { A_NONE, A_PAINT, A_WHACK, A_MIXER, A_CAL };

static LB_TouchNS2009 *tp;
static int W, H, TS;                   // the screen, and the text size that suits it (1 or 2)

// ── backlight ───────────────────────────────────────────────────────────────

// The 4.3 inch module's backlight is hungry: at full brightness it makes the dev
// board's LDO too hot. It is held to 50 %. Every other panel runs the whole range.
// pct is 5-100 of what the panel is allowed; never lower, or the screen goes dark
// and you cannot see to turn it back up.
static int gBacklightPct = 100;

static void setBacklight(int pct) {
  gBacklightPct = constrain(pct, 5, 100);
  const int ceiling = display.panelDef() == LB_RTP_430 ? 50 : 100;
  display.backlight((255 * gBacklightPct / 100) * ceiling / 100);
}

// ── touch state ─────────────────────────────────────────────────────────────

struct Pen { bool down = false, pressed = false, released = false; int x = 0, y = 0; };
static Pen pen;

static void poll() {
  int16_t x, y;
  const bool d = tp->getTouch(&x, &y);
  pen.pressed = d && !pen.down;
  pen.released = !d && pen.down;
  pen.down = d;
  if (d) { pen.x = x; pen.y = y; }
}

// ── drawing helpers ─────────────────────────────────────────────────────────

static bool inside(const Rect &r, int x, int y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }

static void label(const char *s, int cx, int cy, int size, lb_color_t c) {
  display.setTextSize(size);
  display.setTextColor(c);
  display.drawString(s, cx - display.textWidth(s) / 2, cy - display.fontHeight() / 2);
}

// The colours are bright and saturated on purpose: the small TN panel (the 1.8 inch) loses
// the middle of the range when seen off-axis, and dark or muted colours simply go black.

// A button whose label is as big as fits inside it, never bigger than the page text.
static void button(const Rect &r, lb_color_t fill, const char *text) {
  int size = TS;
  while (size > 1) {
    display.setTextSize(size);
    if (display.textWidth(text) <= r.w - 6 && display.fontHeight() <= r.h - 2) break;
    size--;
  }
  display.fillRoundRect(r.x, r.y, r.w, r.h, 4, fill);
  display.drawRoundRect(r.x, r.y, r.w, r.h, 4, LB_WHITE);
  label(text, r.x + r.w / 2, r.y + r.h / 2, size, LB_WHITE);
}

static Rect backBtn() { const int c = max(18, H / 7); return {2, 2, c * 2, c}; }

static void header(const char *title) {
  display.fillScreen(LB_BLACK);
  button(backBtn(), LB_RGB(200, 30, 30), "BACK");
  label(title, W / 2 + backBtn().w / 2, backBtn().y + backBtn().h / 2, TS, LB_WHITE);
}

// ── home ────────────────────────────────────────────────────────────────────

static App home() {
  display.fillScreen(LB_BLACK);
  label("RTP SHOWCASE", W / 2, max(10, H / 12), TS, LB_RGB(80, 220, 200));
  const int top = max(22, H / 6), gap = max(6, W / 40);
  const int bw = (W - gap * 3) / 2, bh = (H - top - gap * 3) / 2;
  const Rect r[4] = {{gap, top + gap, bw, bh}, {gap * 2 + bw, top + gap, bw, bh},
                     {gap, top + gap * 2 + bh, bw, bh}, {gap * 2 + bw, top + gap * 2 + bh, bw, bh}};
  const char *names[4] = {"PAINT", "WHACK", "MIXER", "CALIBRATE"};
  const lb_color_t cols[4] = {LB_RGB(230, 0, 140), LB_RGB(0, 190, 60), LB_RGB(40, 80, 255), LB_RGB(255, 160, 0)};
  for (int i = 0; i < 4; i++) button(r[i], cols[i], names[i]);
  while (true) {
    poll();
    if (pen.pressed)
      for (int i = 0; i < 4; i++)
        if (inside(r[i], pen.x, pen.y)) return (App)(i + 1);
    delay(8);
  }
}

// ── paint ───────────────────────────────────────────────────────────────────

static void appPaint() {
  static const lb_color_t PALETTE[8] = {LB_RED, LB_RGB(255, 140, 0), LB_YELLOW, LB_GREEN,
                                        LB_CYAN, LB_BLUE, LB_MAGENTA, LB_WHITE};
  // The toolbar: back, eight colours, brush smaller, bigger, clear, and a preview of
  // the brush. Small cells, as many rows as the width needs; on a small screen the
  // clear button is just an X so its label fits.
  const bool wide = W >= 300;
  const int cell = wide ? 28 : max(16, min(22, W / 10));
  const int perRow = max(1, W / cell);
  const int items = 13;                                  // back, 8 colours, -, +, clear, preview
  const int rows = (items + perRow - 1) / perRow;
  const int barH = rows * cell + 2;
  auto cellRect = [&](int i) { return Rect{(i % perRow) * cell + 1, (i / perRow) * cell + 1, cell - 2, cell - 2}; };

  int color = 0, brush = 3;
  auto drawBar = [&]() {
    display.fillRect(0, 0, W, barH, LB_RGB(24, 24, 28));
    button(cellRect(0), LB_RGB(200, 30, 30), "<");
    for (int i = 0; i < 8; i++) {
      const Rect r = cellRect(1 + i);
      display.fillRect(r.x, r.y, r.w, r.h, PALETTE[i]);
      if (i == color) { display.drawRect(r.x, r.y, r.w, r.h, LB_BLACK); display.drawRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, LB_WHITE); }
    }
    button(cellRect(9), LB_RGB(90, 90, 120), "-");
    button(cellRect(10), LB_RGB(90, 90, 120), "+");
    button(cellRect(11), LB_RGB(200, 30, 30), wide ? "CLR" : "X");
    const Rect pv = cellRect(12);                        // the brush as it will paint
    display.fillRect(pv.x, pv.y, pv.w, pv.h, LB_BLACK);
    display.fillCircle(pv.x + pv.w / 2, pv.y + pv.h / 2, min(brush, pv.w / 2 - 1), PALETTE[color]);
  };
  display.fillScreen(LB_BLACK);
  drawBar();

  int lx = -1, ly = -1;
  while (true) {
    poll();
    if (pen.pressed && pen.y < barH) {                   // a toolbar button
      for (int i = 0; i < items; i++) {
        if (!inside(cellRect(i), pen.x, pen.y)) continue;
        if (i == 0) return;
        if (i == 12) continue;                           // the preview is not a button
        if (i >= 1 && i <= 8) color = i - 1;
        if (i == 9) brush = max(1, brush - 1);
        if (i == 10) brush = min(12, brush + 1);
        if (i == 11) display.fillRect(0, barH, W, H - barH, LB_BLACK);
        drawBar();
      }
    } else if (pen.down && pen.y >= barH) {              // paint: a line from the last point, as round as the brush
      if (lx < 0) { lx = pen.x; ly = pen.y; }
      const int dx = pen.x - lx, dy = pen.y - ly;
      const int steps = max(1, max(abs(dx), abs(dy)) / max(1, brush / 2));
      for (int s = 0; s <= steps; s++)
        display.fillCircle(lx + dx * s / steps, ly + dy * s / steps, brush, PALETTE[color]);
      lx = pen.x; ly = pen.y;
    } else {
      lx = -1;
    }
    delay(4);
  }
}

// ── whack-a-mole ────────────────────────────────────────────────────────────
//
// Moles pop out of a grid of holes; tap one before it ducks back. A mole that gets
// away costs a life; five lives. The longer you last, the quicker they are. The
// best score is kept in the chip's flash, one per panel.

static uint32_t bestScore() {
  Preferences p;
  uint32_t v = 0;
  if (p.begin("lbtouch", true)) { v = p.getUInt((String("hi_") + display.panelDef()->id).c_str(), 0); p.end(); }
  return v;
}

static void saveBest(uint32_t v) {
  Preferences p;
  if (p.begin("lbtouch", false)) { p.putUInt((String("hi_") + display.panelDef()->id).c_str(), v); p.end(); }
}

// One game. Returns the score, or -1 if BACK was pressed.
static int playWhack() {
  const Rect back = backBtn();
  const int barH = back.h + 4;
  const int cols = W >= 300 ? 4 : 3;
  const int cw = W / cols;
  const int rows = constrain((H - barH + cw / 2) / cw, 2, 4);
  const int ch = (H - barH) / rows;
  const int r = max(8, min(cw, ch) / 2 - 5);
  const int ox = (W - cols * cw) / 2;
  const int holes = cols * rows;
  auto centre = [&](int i, int &x, int &y) { x = ox + (i % cols) * cw + cw / 2; y = barH + (i / cols) * ch + ch / 2; };

  const lb_color_t grass = LB_RGB(0, 110, 20), dirt = LB_RGB(70, 40, 10), fur = LB_RGB(200, 140, 80);
  // A hole, empty or with a mole in it; `hit` shows the mole flattened and red.
  auto drawHole = [&](int i, bool mole, bool hit) {
    int x, y;
    centre(i, x, y);
    display.fillRect(x - cw / 2, y - ch / 2, cw, ch, grass);
    display.fillCircle(x, y + r / 4, r, dirt);
    if (!mole) return;
    const lb_color_t body = hit ? LB_RGB(255, 60, 30) : fur;
    display.fillCircle(x, y - r / 8, r * 4 / 5, body);
    display.fillCircle(x - r / 2, y - r * 3 / 4, r / 5, body);          // ears
    display.fillCircle(x + r / 2, y - r * 3 / 4, r / 5, body);
    display.fillCircle(x - r / 3, y - r / 4, max(2, r / 7), LB_WHITE);   // eyes
    display.fillCircle(x + r / 3, y - r / 4, max(2, r / 7), LB_WHITE);
    display.fillCircle(x - r / 3, y - r / 4, max(1, r / 14), LB_BLACK);
    display.fillCircle(x + r / 3, y - r / 4, max(1, r / 14), LB_BLACK);
    display.fillCircle(x, y + r / 8, max(2, r / 8), LB_RGB(255, 150, 170));  // nose
  };

  int score = 0, lives = 5, mole = -1, last = -1;
  uint32_t upAt = 0, nextAt = millis() + 700;
  auto status = [&]() {
    display.fillRect(back.x + back.w + 2, 0, W - back.x - back.w - 2, barH, LB_BLACK);
    // Next to the BACK button: the long wording if it fits, else a short one.
    char b[40];
    display.setTextSize(1);
    snprintf(b, sizeof b, "SCORE %d   LIVES %d", score, lives);
    if (display.textWidth(b) > W - back.x - back.w - 10) snprintf(b, sizeof b, "S%d  L%d", score, lives);
    display.setTextColor(lives > 1 ? LB_WHITE : LB_RED);
    display.drawString(b, W - display.textWidth(b) - 6, (barH - display.fontHeight()) / 2);
  };

  display.fillScreen(LB_BLACK);
  button(back, LB_RGB(200, 30, 30), "BACK");
  for (int i = 0; i < holes; i++) drawHole(i, false, false);
  status();

  while (lives > 0) {
    poll();
    if (pen.pressed && inside(back, pen.x, pen.y)) return -1;
    const uint32_t now = millis();
    const uint32_t stay = max(450, 1100 - score * 25);          // how long a mole stays up
    if (mole < 0) {
      if (now >= nextAt) {
        do mole = random(holes); while (mole == last);
        last = mole;
        upAt = now;
        drawHole(mole, true, false);
      }
    } else {
      int x, y;
      centre(mole, x, y);
      const int d2 = (pen.x - x) * (pen.x - x) + (pen.y - y) * (pen.y - y);
      if (pen.pressed && d2 <= (r + 6) * (r + 6)) {             // whacked
        score++;
        drawHole(mole, true, true);
        delay(120);
        drawHole(mole, false, false);
        mole = -1;
        nextAt = now + random(200, 600);
        status();
      } else if (now - upAt > stay) {                           // it got away
        lives--;
        drawHole(mole, false, false);
        mole = -1;
        nextAt = now + 350;
        status();
      }
    }
    delay(6);
  }
  return score;
}

static void appWhack() {
  while (true) {
    const int score = playWhack();
    if (score < 0) return;
    const uint32_t best = bestScore();
    const bool record = (uint32_t)score > best;
    if (record) saveBest(score);
    display.fillScreen(LB_BLACK);
    button(backBtn(), LB_RGB(200, 30, 30), "BACK");
    label("GAME OVER", W / 2, H / 4, TS * 2, LB_RGB(80, 220, 200));
    char b[40];
    snprintf(b, sizeof b, "SCORE %d", score);
    label(b, W / 2, H / 2 - 4 * TS, TS * 2, LB_WHITE);
    snprintf(b, sizeof b, record ? "NEW BEST!" : "BEST %lu", (unsigned long)best);
    label(b, W / 2, H / 2 + 12 * TS * 2, TS, record ? LB_YELLOW : LB_RGB(200, 200, 200));
    label("tap to play again", W / 2, H - 12 * TS, TS, LB_RGB(200, 200, 200));
    delay(400);                                                  // so the last tap does not restart it
    while (true) {
      poll();
      if (pen.pressed) {
        if (inside(backBtn(), pen.x, pen.y)) return;
        break;
      }
      delay(8);
    }
  }
}

// ── mixer ───────────────────────────────────────────────────────────────────
//
// Three sliders mix a colour; the fourth, L, is the backlight (5-100 %).

static void appMixer() {
  header("MIXER");
  const int top = backBtn().y + backBtn().h + 6;
  const int sliderH = max(14, H / 11);
  const int pitch = sliderH + 5;
  const int x0 = 8 + 14, x1 = W - 8;                    // track ends (room for the letter)
  const int swatchTop = top + 4 * pitch + 2;
  const Rect swatch = {8, swatchTop, W - 16, H - swatchTop - 4};
  int v[3] = {200, 90, 40};
  auto drawSlider = [&](int i) {                         // i 0-2 colour, 3 backlight
    const int y = top + i * pitch;
    static const lb_color_t tint[4] = {LB_RED, LB_GREEN, LB_BLUE, LB_YELLOW};
    const int val = i < 3 ? v[i] : (gBacklightPct - 5) * 255 / 95;     // the track always spans 0-255
    display.fillRect(0, y, W, sliderH + 4, LB_BLACK);
    const char l[2] = {"RGBL"[i], 0};
    label(l, 12, y + sliderH / 2, TS, tint[i]);
    display.fillRect(x0, y + sliderH / 2 - 2, x1 - x0, 4, LB_RGB(100, 100, 100));
    const int kx = x0 + val * (x1 - x0) / 255;
    display.fillRect(x0, y + sliderH / 2 - 2, kx - x0, 4, tint[i]);
    display.fillCircle(kx, y + sliderH / 2, sliderH / 2, LB_WHITE);
  };
  auto drawSwatch = [&]() {
    display.fillRect(swatch.x, swatch.y, swatch.w, swatch.h, LB_RGB(v[0], v[1], v[2]));
    display.drawRect(swatch.x, swatch.y, swatch.w, swatch.h, LB_WHITE);
    char b[24];
    snprintf(b, sizeof b, "#%02X%02X%02X", v[0], v[1], v[2]);
    const bool light = v[0] * 3 + v[1] * 6 + v[2] > 1500;
    const lb_color_t ink = light ? LB_BLACK : LB_WHITE;
    const int big = swatch.h >= 60 ? TS * 2 : TS;
    label(b, swatch.x + swatch.w / 2, swatch.y + swatch.h / 2 - (swatch.h >= 60 ? 8 * TS : 0), big, ink);
    if (swatch.h >= 60) {
      snprintf(b, sizeof b, "LIGHT %d%%", gBacklightPct);
      label(b, swatch.x + swatch.w / 2, swatch.y + swatch.h / 2 + 12 * TS, TS, ink);
    }
  };
  for (int i = 0; i < 4; i++) drawSlider(i);
  drawSwatch();
  int grab = -1;                                         // which slider the finger is on
  while (true) {
    poll();
    if (pen.pressed && inside(backBtn(), pen.x, pen.y)) return;
    if (pen.pressed) {
      grab = -1;
      for (int i = 0; i < 4; i++) {
        const int y = top + i * pitch;
        if (pen.y >= y - 2 && pen.y < y + sliderH + 4) grab = i;
      }
    }
    if (!pen.down) grab = -1;
    if (grab >= 0) {
      const int nv = constrain((pen.x - x0) * 255 / (x1 - x0), 0, 255);
      if (grab < 3) {
        if (nv != v[grab]) { v[grab] = nv; drawSlider(grab); drawSwatch(); }
      } else {
        const int pct = 5 + nv * 95 / 255;               // 5-100 %
        if (pct != gBacklightPct) { setBacklight(pct); drawSlider(3); drawSwatch(); }
      }
    }
    delay(8);
  }
}

// ── start and run ───────────────────────────────────────────────────────────

static void calibrateAndSave() {
  LB_TouchCal cal;
  if (lb_calibrate_touch(display, tp, cal)) lb_save_touch_cal(display, cal);
}

void setup() {
  Serial.begin(115200);
  delay(1000);                                   // the panel needs a moment after power-up
  // Direct drawing, no canvas: painting stays quick on every size, and the 4.3 inch
  // would not leave much memory for a 261 KB canvas.
  if (!display.begin(false)) {
    Serial.println("Display did not start. Check the ribbon cable and DC.");
    while (true) delay(1000);
  }
  setBacklight(100);
  W = display.width();
  H = display.height();
  TS = W >= 300 ? 2 : 1;
  tp = static_cast<LB_TouchNS2009 *>(display.touch());
  if (!tp) {
    Serial.println("No touch: the NS2009 did not answer on SDA/SCL.");
    display.fillScreen(LB_BLACK);
    label("NO TOUCH", W / 2, H / 2, TS * 2, LB_RED);
    while (true) delay(1000);
  }
  LB_TouchCal cal;
  if (lb_load_touch_cal(display, cal)) lb_apply_touch_cal(display, tp, cal);
  else calibrateAndSave();
}

void loop() {
  switch (home()) {
    case A_PAINT:   appPaint(); break;
    case A_WHACK:   appWhack(); break;
    case A_MIXER:   appMixer(); break;
    case A_CAL:     calibrateAndSave(); break;
    default: break;
  }
}
