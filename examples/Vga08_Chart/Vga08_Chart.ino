/*
  Vga08_Chart - a live scrolling line chart and a bar chart, no chart library.

  YOU WILL LEARN
    - mapping a value to a pixel row (the one formula behind every chart)
    - keeping a history in a plain array and drawing it as connected lines
    - updating a picture WITHOUT blanking it first (see below)
    - reading the chip's own temperature with temperatureRead()

  WHAT IT SHOWS
    The chip temperature over the last 60 seconds (breathe on the board or
    touch the chip and watch it move), a sine wave so there is always
    something moving, and free heap and PSRAM as bars.

  WHY IT DOES NOT WIPE THE CHART
    The obvious way to update a chart is to clear its rectangle and draw it
    again. On this screen that blinks: drawing is slow (a few million pixels a
    second, because the picture being sent to the monitor uses most of the
    memory bandwidth), the monitor reads the screen while you draw, and for a
    moment it catches the chart empty.
    So nothing is cleared. The static parts (frame, titles, labels) are drawn
    once. Each update takes the OLD lines off by drawing them again in the
    background colour, puts the grid back, and draws the NEW lines - a few
    thousand pixels, over in a few milliseconds. Text is drawn with a
    background colour (setTextColor(fg, bg)) so a new number lands on top of the
    old one with no gap.

  Replace readTemperature() with any sensor you like - an analog pin, an I2C
  sensor, a serial value - and the chart plots that instead.
*/
#include <LonelyBinaryDisplay.h>
#include <math.h>

LB_Display display(LB_VGA_800X600);

static const lb_color_t BG   = LB_RGB(8, 12, 18);
static const lb_color_t GRID = LB_RGB(36, 46, 60);
static const lb_color_t TEMP = LB_RGB(255, 140, 0);
static const lb_color_t WAVE = LB_RGB(20, 200, 210);

// The plot area.
static const int PX = 120, PY = 70, PW = 650, PH = 300;
static const int POINTS = 120;                    // 120 samples, one every 0.5 s = 60 s
static float temp[POINTS], wave[POINTS];
static int count = 0;

// The lines as they are on screen now (pixel rows), so they can be taken off again.
static int16_t onScreenT[POINTS], onScreenW[POINTS];
static int onScreenCount = 0;

static float readTemperature() { return temperatureRead(); }       // degrees C, the chip's own sensor

// A value in [lo, hi] -> a y pixel in the plot (hi at the top).
static int yFor(float v, float lo, float hi) {
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  return PY + PH - (int)((v - lo) / (hi - lo) * PH);
}

// Sample k of n, newest on the right.
static int xFor(int k, int n) { return PX + (POINTS - n + k) * PW / (POINTS - 1); }

static void drawGrid() {
  for (int i = 1; i < 5; i++) display.drawFastHLine(PX + 1, PY + i * PH / 5, PW - 2, GRID);
  for (int i = 1; i < 6; i++) display.drawFastVLine(PX + i * PW / 6, PY + 1, PH - 2, GRID);
  display.drawRect(PX, PY, PW, PH, LB_GRAY);
}

static void drawLines(const int16_t *yt, const int16_t *yw, int n, lb_color_t ct, lb_color_t cw) {
  for (int k = 1; k < n; k++) {
    const int x0 = xFor(k - 1, n), x1 = xFor(k, n);
    display.drawLine(x0, yw[k - 1], x1, yw[k], cw);
    display.drawLine(x0, yt[k - 1], x1, yt[k], ct);
    display.drawLine(x0, yt[k - 1] + 1, x1, yt[k] + 1, ct);
  }
}

static void updateChart(float lo, float hi) {
  // 1 - take the old lines off, 2 - put back the grid they crossed, 3 - draw the new ones.
  drawLines(onScreenT, onScreenW, onScreenCount, BG, BG);
  drawGrid();
  for (int k = 0; k < count; k++) {
    onScreenT[k] = yFor(temp[k], lo, hi);
    onScreenW[k] = yFor(wave[k], -1.2f, 1.2f);
  }
  onScreenCount = count;
  drawLines(onScreenT, onScreenW, onScreenCount, TEMP, WAVE);

  // The scale on the left: fixed width, on a background, so each number replaces the last.
  display.setTextSize(1);
  display.setTextColor(LB_LIGHTGRAY, BG);
  for (int i = 0; i <= 5; i++) {
    char s[12];
    snprintf(s, sizeof s, "%7.1f", hi - (hi - lo) * i / 5);
    display.drawString(s, PX - 8 - 7 * 8, PY + i * PH / 5 - 8);
  }
}

// The fixed parts of a bar: its label and its frame.
static void barFrame(int x, int y, int w, const char *label) {
  display.setTextSize(2);
  display.setTextColor(LB_WHITE, BG);
  display.drawString(label, x, y);
  display.drawRect(x, y + 36, w, 22, LB_GRAY);
}

// The changing parts: the filled portion, and the number beside it.
static void barUpdate(int x, int y, int w, float frac, lb_color_t color, const char *value) {
  const int filled = (int)((w - 4) * frac);
  display.fillRect(x + 2, y + 38, filled, 18, color);
  display.fillRect(x + 2 + filled, y + 38, w - 4 - filled, 18, BG);
  display.setTextSize(2);
  display.setTextColor(color, BG);
  char v[16];
  snprintf(v, sizeof v, "%-9s", value);              // padded, so a shorter number covers a longer one
  display.drawString(v, x + w + 12, y + 32);
}

static const int BARX = 70, BARW = 420, BAR1Y = 440, BAR2Y = 510;

void setup() {
  Serial.begin(115200);
  if (!display.begin()) {
    Serial.println("VGA did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  display.fillScreen(BG);
  display.setTextSize(3);
  display.setTextColor(LB_WHITE, BG);
  display.drawString("Live chart", 70, 16);

  // Everything that never changes, drawn once.
  display.setTextSize(2);
  display.setTextColor(TEMP, BG);
  display.drawString("chip temperature (C)", PX, PY - 32);
  display.setTextColor(WAVE, BG);
  display.drawString("sine wave", PX + 360, PY - 32);
  display.setTextSize(1);
  display.setTextColor(LB_LIGHTGRAY, BG);
  display.drawString("60 s ago", PX, PY + PH + 8);
  display.drawString("now", PX + PW - 24, PY + PH + 8);
  drawGrid();
  barFrame(BARX, BAR1Y, BARW, "free heap");
  barFrame(BARX, BAR2Y, BARW, "free PSRAM (the screen uses about 2 MB)");
  display.flush();
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last < 500) return;
  last = millis();

  // New samples go on the end; the oldest falls off the front.
  if (count == POINTS) {
    memmove(temp, temp + 1, sizeof(float) * (POINTS - 1));
    memmove(wave, wave + 1, sizeof(float) * (POINTS - 1));
    count--;
  }
  temp[count] = readTemperature();
  wave[count] = sinf(millis() / 2000.0f);
  count++;

  // Scale the temperature axis to what we have seen, never narrower than 4 degrees.
  float lo = temp[0], hi = temp[0];
  for (int i = 1; i < count; i++) { lo = min(lo, temp[i]); hi = max(hi, temp[i]); }
  const float mid = (lo + hi) / 2, span = max(4.0f, hi - lo + 1.0f);
  updateChart(mid - span / 2, mid + span / 2);

  const uint32_t heapTotal = ESP.getHeapSize(), heapFree = ESP.getFreeHeap();
  char v[32];
  snprintf(v, sizeof v, "%lu KB", (unsigned long)(heapFree / 1024));
  barUpdate(BARX, BAR1Y, BARW, (float)heapFree / heapTotal, LB_GREEN, v);
  const uint32_t psramTotal = ESP.getPsramSize(), psramFree = ESP.getFreePsram();
  snprintf(v, sizeof v, "%lu KB", (unsigned long)(psramFree / 1024));
  barUpdate(BARX, BAR2Y, BARW, psramTotal ? (float)psramFree / psramTotal : 0, LB_CYAN, v);

  display.flush();
}
