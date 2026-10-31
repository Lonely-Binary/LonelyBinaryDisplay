/*
  Vga11_Dashboard - a status board for the ESP32-S3 itself, built from the
  pieces of the earlier examples: tiles, big numbers, bars, a sparkline.

  YOU WILL LEARN
    - laying a screen out as tiles on a grid
    - one function per kind of tile (a number, a bar, a sparkline)
    - drawing the fixed parts ONCE and updating only what changed, so nothing
      blinks (see Vga08_Chart for why wiping and redrawing is visible here)
    - the system information the chip can tell you about itself

  WHAT IT SHOWS
    CHIP TEMP    degrees now, and the last 60 seconds as a sparkline
    FREE HEAP    internal RAM left, as a bar
    FREE PSRAM   PSRAM left, as a bar (the screen itself takes about 2 MB)
    VIDEO        frames per second, late frames, longest frame

  To show your own readings, change the numbers handed to the tiles in
  updateTiles(); everything else stays.
*/
#include <LonelyBinaryDisplay.h>

LB_Display display(LB_VGA_800X600);

static const lb_color_t BG      = LB_RGB(12, 15, 20);
static const lb_color_t SURFACE = LB_RGB(20, 25, 35);
static const lb_color_t TEXT    = LB_RGB(230, 234, 242);
static const lb_color_t MUTED   = LB_RGB(138, 147, 166);
static const lb_color_t ACCENT  = LB_RGB(45, 212, 191);
static const lb_color_t C_OK    = LB_RGB(52, 210, 123);
static const lb_color_t C_WARN  = LB_RGB(246, 183, 60);
static const lb_color_t C_CRIT  = LB_RGB(242, 112, 91);

// Tile grid: 2 columns x 2 rows under a 56 px header.
static const int MARGIN = 12, HEADER = 56;
static int TW, TH;                                  // tile size, set in setup()

static int tileX(int i) { return MARGIN + (i % 2) * (TW + MARGIN); }
static int tileY(int i) { return HEADER + MARGIN + (i / 2) * (TH + MARGIN); }

static lb_color_t level(float v, float warn, float crit) { return v >= crit ? C_CRIT : v >= warn ? C_WARN : C_OK; }

// The fixed parts of a tile, drawn once: its card, its title and its unit.
static const int NUM_CHARS = 4;                     // the number field is 4 characters wide
static void tileFrame(int i, const char *title, const char *unit) {
  display.fillRoundRect(tileX(i), tileY(i), TW, TH, 12, SURFACE);
  display.setTextSize(2);
  display.setTextColor(MUTED, SURFACE);
  display.drawString(title, tileX(i) + 18, tileY(i) + 14);
  display.setTextSize(3);
  display.drawString(unit, tileX(i) + 18 + NUM_CHARS * 8 * 7 + 10, tileY(i) + 52 + 16 * 7 - 16 * 3 - 4);
}

// A big number. It is padded to a fixed width and drawn on the card colour, so
// the new number covers the old one with no gap in between.
static void bigNumber(int i, const char *value, lb_color_t color) {
  char padded[8];
  snprintf(padded, sizeof padded, "%-*s", NUM_CHARS, value);
  display.setTextSize(7);
  display.setTextColor(color, SURFACE);
  display.drawString(padded, tileX(i) + 18, tileY(i) + 52);
}

// A bar: the track is a rectangle, the filled part is drawn over it and the
// rest is painted back, so only the boundary moves.
static void bar(int i, float frac, lb_color_t color) {
  const int x = tileX(i) + 18, y = tileY(i) + TH - 54, w = TW - 36;
  const int filled = max(4, (int)((w - 4) * constrain(frac, 0.0f, 1.0f)));
  display.fillRect(x + 2, y + 2, filled, 20, color);
  display.fillRect(x + 2 + filled, y + 2, w - 4 - filled, 20, BG);
}

static void barFrame(int i) {
  const int x = tileX(i) + 18, y = tileY(i) + TH - 54, w = TW - 36;
  display.fillRect(x, y, w, 24, BG);
}

static const int HIST = 60;
static float hist[HIST];
static int histCount = 0;
static int16_t spX[HIST], spY[HIST];                // the sparkline as drawn now, to take it off again
static int spCount = 0;

static void sparkline(int i) {
  const int x = tileX(i) + 18, y = tileY(i) + TH - 80, w = TW - 36, h = 58;
  // Take the old line off (draw it again in the card colour), then draw the new one.
  for (int k = 1; k < spCount; k++) {
    display.drawLine(spX[k - 1], spY[k - 1], spX[k], spY[k], SURFACE);
    display.drawLine(spX[k - 1], spY[k - 1] + 1, spX[k], spY[k] + 1, SURFACE);
  }
  spCount = 0;
  if (histCount < 2) return;
  float lo = hist[0], hi = hist[0];
  for (int k = 1; k < histCount; k++) { lo = min(lo, hist[k]); hi = max(hi, hist[k]); }
  if (hi - lo < 2) { const float m = (hi + lo) / 2; lo = m - 1; hi = m + 1; }
  for (int k = 0; k < histCount; k++) {
    spX[k] = x + k * w / (HIST - 1);
    spY[k] = y + h - (int)((hist[k] - lo) / (hi - lo) * h);
  }
  spCount = histCount;
  for (int k = 1; k < spCount; k++) {
    display.drawLine(spX[k - 1], spY[k - 1], spX[k], spY[k], ACCENT);
    display.drawLine(spX[k - 1], spY[k - 1] + 1, spX[k], spY[k] + 1, ACCENT);
  }
}

static void header() {
  display.fillRect(0, 0, display.width(), HEADER, BG);
  display.setTextSize(3);
  display.setTextColor(TEXT, BG);
  display.drawString("ESP32-S3 status", MARGIN, 16);
  display.setTextSize(2);
  display.setTextColor(MUTED, BG);
  char s[64];
  snprintf(s, sizeof s, "%s rev %d   %d MHz   %lu MB flash", ESP.getChipModel(), ESP.getChipRevision(),
           getCpuFrequencyMhz(), (unsigned long)(ESP.getFlashChipSize() >> 20));
  display.drawString(s, display.width() - MARGIN - display.textWidth(s), 20);
}

static void updateTiles() {
  char v[24];

  // 0 - chip temperature, with its history.
  const float t = temperatureRead();
  if (histCount == HIST) { memmove(hist, hist + 1, sizeof(float) * (HIST - 1)); histCount--; }
  hist[histCount++] = t;
  snprintf(v, sizeof v, "%.1f", t);
  bigNumber(0, v, level(t, 55, 70));
  sparkline(0);

  // 1 - free heap.
  const float heapFree = ESP.getFreeHeap(), heapAll = ESP.getHeapSize();
  snprintf(v, sizeof v, "%.0f", heapFree / 1024);
  bigNumber(1, v, ACCENT);
  bar(1, heapFree / heapAll, ACCENT);

  // 2 - free PSRAM.
  const float psFree = ESP.getFreePsram(), psAll = ESP.getPsramSize();
  snprintf(v, sizeof v, "%.1f", psFree / (1024 * 1024));
  bigNumber(2, v, ACCENT);
  bar(2, psAll ? psFree / psAll : 0, ACCENT);

  // 3 - the video signal itself.
  static uint32_t lastFrames = 0, lastMs = 0;
  uint32_t frames = 0, late = 0, maxUs = 0;
  display.scanStats(&frames, &late, &maxUs);
  const uint32_t now = millis();
  const float fps = lastMs ? (frames - lastFrames) * 1000.0f / (now - lastMs) : 0;
  lastFrames = frames;
  lastMs = now;
  snprintf(v, sizeof v, "%.0f", fps);
  bigNumber(3, v, level(late, 1, 10));
  display.setTextSize(2);
  display.setTextColor(MUTED, SURFACE);
  char s[48];
  snprintf(s, sizeof s, "late frames  %-8lu", (unsigned long)late);
  display.drawString(s, tileX(3) + 18, tileY(3) + TH - 78);
  snprintf(s, sizeof s, "longest      %lu.%lu ms   ", (unsigned long)(maxUs / 1000), (unsigned long)(maxUs % 1000 / 100));
  display.drawString(s, tileX(3) + 18, tileY(3) + TH - 46);

  display.flush();
  Serial.printf("temp %.1f C   heap %.0f KB   psram %.1f MB free   %.0f fps   %lu late frames\n", t,
                heapFree / 1024, psFree / 1048576, fps, (unsigned long)late);
}

void setup() {
  Serial.begin(115200);
  if (!display.begin()) {
    Serial.println("VGA did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  TW = (display.width() - 3 * MARGIN) / 2;
  TH = (display.height() - HEADER - 3 * MARGIN) / 2;
  display.fillScreen(BG);
  header();

  // The fixed parts, once.
  tileFrame(0, "CHIP TEMP", "C");
  tileFrame(1, "FREE HEAP", "KB");
  tileFrame(2, "FREE PSRAM", "MB");
  tileFrame(3, "VIDEO", "fps");
  barFrame(1);
  barFrame(2);
  updateTiles();
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 1000) {
    last = millis();
    updateTiles();
  }
}
