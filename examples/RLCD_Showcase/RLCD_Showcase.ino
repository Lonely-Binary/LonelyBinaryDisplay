/*
  RLCD_Showcase - a reflective LCD show-reel (2.13, 2.9 and 4.2 inch).

  What a reflective LCD is good at, one scene after another, forever:

    1  DANCE             a 10 s video, turned into 1-bit video
    2  DITHERED PLASMA   a moving picture in pure black and white
    3  3D WIREFRAME      smooth motion, no ghosting - and the frame rate
    4  GAME OF LIFE      a classic, drawn two pixels a cell
    5  DASHBOARD         a live trace, big digits and bars
    6  CLOCK             analog and digital
    7  LOW POWER         the panel drops to its slow, few-microamp mode and
                         updates once a second

  dance_213.h (250 x 122) and dance_290.h (384 x 168) are made from a video by
  tools/video_to_rlcd.py; run that script on any clip (see its header) to make
  one for another size, or from your own video. A clip smaller than the screen
  is centred; one larger than the screen is skipped.

  The BOOT button (GPIO 0) skips to the next scene.

  Not e-paper: every frame is on screen in milliseconds and nothing ghosts. No
  backlight: it reads best in daylight, and a lamp over your shoulder is the
  "backlight". The clock shows the time this sketch was compiled.

  Change the size line below (213, 290 or 420). Only the 4.2 inch needs PSRAM.
*/
#include <LonelyBinaryDisplay.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>

// Which panel this is for: 213, 290 or 420. It picks the panel and the video made for its size.
#define RLCD_SIZE 213
//#define RLCD_SIZE 290
//#define RLCD_SIZE 420

#if RLCD_SIZE == 213
  #define RLCD_PANEL LB_RLCD_213
  #include "dance_213.h"           // the 1-bit video, made by tools/video_to_rlcd.py
#elif RLCD_SIZE == 290
  #define RLCD_PANEL LB_RLCD_290
  #include "dance_290.h"
#else
  #define RLCD_PANEL LB_RLCD_420   // the 384 x 168 video, centred
  #include "dance_290.h"
#endif

LB_Display display(RLCD_PANEL);

static int W, H;
static uint16_t *fb;               // the canvas, written directly where that is faster

static const uint16_t BLACK_PX = 0x0000, WHITE_PX = 0xFFFF;

// The panel itself refreshes at about 32 Hz in its fast mode, so frames are paced
// to that: sending more would only be shown as a number, never on the glass.
static const uint32_t FRAME_MS = 31;

// The BOOT button on the dev board: press it to skip to the next scene.
static const int SKIP_PIN = 0;
static bool g_unchanged = false;   // a scene sets this when it has nothing new to show

// Types first: the Arduino build puts function prototypes above the first function.
struct Clock { tm t; float sec; };

// ── helpers ─────────────────────────────────────────────────────────────────

static int8_t sinT[256];           // sin(i / 256 turns) * 127

static inline int sin8(int i) { return sinT[i & 255]; }

static void text(const char *s, int x, int y, int size, bool black = true) {
  display.setTextSize(size);
  display.setTextColor(black ? LB_BLACK : LB_WHITE);
  display.drawString(s, x, y);
}

static void textCentered(const char *s, int cx, int y, int size, bool black = true) {
  display.setTextSize(size);
  display.setTextColor(black ? LB_BLACK : LB_WHITE);
  display.drawString(s, cx - display.textWidth(s) / 2, y);
}

// Layout is worked out from the measured text, never from a guessed font size:
// the default font is 8 x 16 a character at size 1.
static int textH(int size) {
  display.setTextSize(size);
  return display.fontHeight();
}

// The largest size at which `s` fits in maxW x maxH.
static int fitSize(const char *s, int maxW, int maxH, int maxSize = 8) {
  for (int sz = maxSize; sz > 1; sz--) {
    display.setTextSize(sz);
    if (display.textWidth(s) <= maxW && display.fontHeight() <= maxH) return sz;
  }
  return 1;
}

static void thickLine(int x0, int y0, int x1, int y1, int w, lb_color_t c) {
  display.drawLine(x0, y0, x1, y1, c);
  if (w < 2) return;
  const int dx = abs(x1 - x0), dy = abs(y1 - y0);
  if (dx >= dy) display.drawLine(x0, y0 + 1, x1, y1 + 1, c);
  else          display.drawLine(x0 + 1, y0, x1 + 1, y1, c);
}

static Clock readClock() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  Clock c;
  localtime_r(&tv.tv_sec, &c.t);
  c.sec = c.t.tm_sec + tv.tv_usec / 1e6f;
  return c;
}

// Start from the moment this sketch was compiled.
static void setTimeFromCompile() {
  char mon[4];
  int d, y, h, m, s;
  sscanf(__DATE__, "%3s %d %d", mon, &d, &y);
  sscanf(__TIME__, "%d:%d:%d", &h, &m, &s);
  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon = (strstr(months, mon) - months) / 3;
  t.tm_mday = d;
  t.tm_hour = h; t.tm_min = m; t.tm_sec = s;
  t.tm_isdst = -1;
  timeval tv = {mktime(&t), 0};
  settimeofday(&tv, nullptr);
}

static const char *DAYS[]   = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char *MONTHS[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// ── 1  the video ────────────────────────────────────────────────────────────
//
// dance.h is a stream of frames, each the previous one XORed with the next
// and run-length coded (the format is in tools/video_to_rlcd.py). It is decoded
// in order into a 1-bit frame and copied to the canvas, centred.

static uint8_t *clipFrame = nullptr;   // the current frame, 1 bit a pixel, 1 = black
static uint32_t clipPos = 0, clipIdx = 0;   // read position in the stream; frames decoded so far

static bool clipFits() { return lb_clip_w <= W && lb_clip_h <= H; }

static void clipRewind() {
  clipPos = 11;                                   // past the 11-byte header
  clipIdx = 0;
  memset(clipFrame, 0, (size_t)((lb_clip_w + 7) / 8) * lb_clip_h);
}

// Decode the next frame into clipFrame. Bytes are read one at a time: a flash
// read of a 16-bit value at an odd address is not allowed on this chip.
static void clipStep() {
  const uint32_t len = pgm_read_byte(lb_clip + clipPos) | (uint32_t)pgm_read_byte(lb_clip + clipPos + 1) << 8;
  const uint8_t *p = lb_clip + clipPos + 2;
  uint32_t i = 0, at = 0;
  while (i < len) {
    const uint8_t tok = pgm_read_byte(p + i++);
    if (tok & 0x80) {
      at += (tok & 0x7F) + 1;                     // unchanged bytes
    } else {
      for (int k = 0; k <= tok; k++) clipFrame[at++] ^= pgm_read_byte(p + i++);
    }
  }
  clipPos += 2 + len;
  clipIdx++;
}

static void sceneClip(float t, bool first) {
  static uint32_t shown = 0;
  if (first) { clipRewind(); shown = 0; display.fillScreen(LB_BLACK); }
  const uint32_t target = (uint32_t)(t * lb_clip_fps) % lb_clip_frames;   // the scene plays the clip twice
  if (target + 1 < clipIdx) { clipRewind(); shown = 0; }                  // wrapped round to the start
  while (clipIdx <= target) clipStep();           // always decode in order; a late frame is just not shown
  g_unchanged = !first && clipIdx == shown;       // same picture as last time: send nothing
  if (g_unchanged) return;
  shown = clipIdx;

  const int ox = (W - lb_clip_w) / 2, oy = (H - lb_clip_h) / 2;
  const int rowBytes = (lb_clip_w + 7) / 8;
  for (int y = 0; y < lb_clip_h; y++) {
    uint16_t *row = fb + (size_t)(oy + y) * W + ox;
    const uint8_t *src = clipFrame + (size_t)y * rowBytes;
    for (int x = 0; x < lb_clip_w; x++) row[x] = (src[x >> 3] >> (7 - (x & 7))) & 1 ? BLACK_PX : WHITE_PX;
  }
}

// ── 2  dithered plasma ──────────────────────────────────────────────────────
//
// The panel has two tones. A plasma is computed as 16 grey levels and each pixel
// is compared with a 4 x 4 Bayer matrix, so the greys come out as patterns of
// dots - the same trick a newspaper photo uses.

static void scenePlasma(float t, bool) {
  static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  const int ti = (int)(t * 50);
  const int sx = 1024 / W + 1, sy = 1024 / H + 1;   // spread the pattern over the screen
  for (int y = 0; y < H; y++) {
    uint16_t *row = fb + (size_t)y * W;
    const int a = sin8((y * sy) / 3 + ti * 2);
    for (int x = 0; x < W; x++) {
      const int v = sin8((x * sx) / 3 + ti) + a + sin8(((x + y) * sx) / 6 - ti);   // -381 .. 381
      const int level = (v + 381) * 16 / 763;                                       // 0 .. 15
      row[x] = level > bayer[y & 3][x & 3] ? WHITE_PX : BLACK_PX;
    }
  }
}

// ── 3  3D wireframe ─────────────────────────────────────────────────────────

static float fps = 0;

static void sceneCube(float t, bool) {
  display.fillScreen(LB_WHITE);
  static const float V[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                                {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
  static const uint8_t E[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                   {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  const float a = t * 0.9f, b = t * 0.6f;
  const float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
  const float scale = min(W, H) * 0.30f;
  const int cx = W / 2, cy = H / 2;
  int px[8], py[8];
  for (int i = 0; i < 8; i++) {
    float x = V[i][0], y = V[i][1], z = V[i][2];
    float x1 = x * ca + z * sa, z1 = -x * sa + z * ca;     // about Y
    float y1 = y * cb - z1 * sb, z2 = y * sb + z1 * cb;    // about X
    const float k = scale * 3.2f / (3.6f + z2);            // a little perspective
    px[i] = cx + (int)(x1 * k);
    py[i] = cy + (int)(y1 * k);
  }
  for (const auto &e : E) thickLine(px[e[0]], py[e[0]], px[e[1]], py[e[1]], 2, LB_BLACK);
  char b2[24];
  snprintf(b2, sizeof b2, "%.0f FPS", fps);
  text(b2, 4, 2, 1);
  text("NO GHOSTING", 4, H - textH(1) - 2, 1);
}

// ── 4  game of life ─────────────────────────────────────────────────────────

static const int CELL = 2;
static uint8_t *life[2];
static int lw, lh, cur = 0;

static void lifeSeed() {
  for (int i = 0; i < lw * lh; i++) life[cur][i] = random(100) < 28;
}

static void sceneLife(float, bool first) {
  if (first) lifeSeed();
  uint8_t *a = life[cur], *b = life[cur ^ 1];
  int alive = 0;
  for (int y = 0; y < lh; y++) {
    const int ym = (y + lh - 1) % lh, yp = (y + 1) % lh;
    for (int x = 0; x < lw; x++) {
      const int xm = (x + lw - 1) % lw, xp = (x + 1) % lw;
      const int n = a[ym * lw + xm] + a[ym * lw + x] + a[ym * lw + xp] +
                    a[y * lw + xm] + a[y * lw + xp] +
                    a[yp * lw + xm] + a[yp * lw + x] + a[yp * lw + xp];
      const uint8_t s = a[y * lw + x] ? (n == 2 || n == 3) : (n == 3);
      b[y * lw + x] = s;
      alive += s;
    }
  }
  cur ^= 1;
  if (alive < lw * lh / 60) lifeSeed();                  // died out: start again
  for (int y = 0; y < lh; y++)
    for (int x = 0; x < lw; x++) {
      const uint16_t c = life[cur][y * lw + x] ? BLACK_PX : WHITE_PX;
      for (int dy = 0; dy < CELL; dy++)
        for (int dx = 0; dx < CELL; dx++) fb[(size_t)(y * CELL + dy) * W + x * CELL + dx] = c;
    }
}

// ── 5  dashboard ────────────────────────────────────────────────────────────

static void sceneDashboard(float t, bool) {
  display.fillScreen(LB_WHITE);
  const bool wide = W >= H;
  const int fh = textH(1);
  // Three blocks: the big number, the trace, the bars.
  const int numW = wide ? W * 42 / 100 : W, numH = wide ? H - 34 : H / 4;
  const int trX = wide ? numW + 2 : 4, trY = wide ? 6 : numH + 4;
  const int trW = wide ? W - trX - 6 : W - 8, trH = wide ? H - 44 : H / 3;

  // Big number: a temperature that wanders.
  const float temp = 24.0f + 3.0f * sinf(t * 0.7f) + 0.6f * sinf(t * 3.1f);
  char b[16];
  snprintf(b, sizeof b, "%.1f", temp);
  const int ts = fitSize(b, numW - 12, numH - fh - 8);
  const int blockH = textH(ts) + 4 + fh;
  const int ny = wide ? max(4, (H - 34 - blockH) / 2) : 6;
  textCentered(b, numW / 2, ny, ts);
  textCentered("TEMP C", numW / 2, ny + textH(ts) + 4, 1);

  // Trace: a ring buffer of samples.
  static int16_t samples[512];
  static int head = 0;
  const int n = min(trW - 2, 510);
  samples[head % 512] = (int16_t)(sinf(t * 2.2f) * 0.6f * (trH / 2) + sinf(t * 7.3f) * 0.25f * (trH / 2) +
                                  random(-2, 3));
  head++;
  display.drawRect(trX, trY, trW, trH, LB_BLACK);
  display.drawLine(trX, trY + trH / 2, trX + trW - 1, trY + trH / 2, LB_BLACK);
  int py = 0;
  for (int i = 0; i < n; i++) {
    const int idx = (head - n + i + 512 * 4) % 512;
    const int y = trY + trH / 2 - samples[idx];
    if (i) thickLine(trX + i, py, trX + i + 1, y, 1, LB_BLACK);
    py = y;
  }

  // Bars along the bottom.
  const int bars = 10, bh = wide ? 26 : H / 6, by = H - bh - 4;
  const int bw = (W - 8) / bars;
  for (int i = 0; i < bars; i++) {
    const int h = (int)((0.5f + 0.5f * sinf(t * 1.3f + i * 0.7f)) * (bh - 4)) + 2;
    display.fillRect(4 + i * bw + 1, by + bh - h, bw - 3, h, LB_BLACK);
  }
  display.drawLine(4, by + bh, W - 4, by + bh, LB_BLACK);
}

// ── 6  clock ────────────────────────────────────────────────────────────────

static void drawAnalog(const Clock &c, int cx, int cy, int r) {
  display.drawCircle(cx, cy, r, LB_BLACK);
  display.drawCircle(cx, cy, r - 1, LB_BLACK);
  for (int i = 0; i < 60; i++) {
    const float a = i * 0.10472f;
    const int r0 = (i % 5 == 0) ? r - 9 : r - 4;
    display.drawLine(cx + sinf(a) * r0, cy - cosf(a) * r0, cx + sinf(a) * (r - 2), cy - cosf(a) * (r - 2), LB_BLACK);
  }
  const float ha = ((c.t.tm_hour % 12) + c.t.tm_min / 60.0f) * 0.5236f;
  const float ma = (c.t.tm_min + c.sec / 60.0f) * 0.10472f;
  const float sa = c.sec * 0.10472f;
  thickLine(cx, cy, cx + sinf(ha) * r * 0.5f, cy - cosf(ha) * r * 0.5f, 2, LB_BLACK);
  thickLine(cx, cy, cx + sinf(ma) * r * 0.78f, cy - cosf(ma) * r * 0.78f, 2, LB_BLACK);
  display.drawLine(cx, cy, cx + sinf(sa) * (r - 6), cy - cosf(sa) * (r - 6), LB_BLACK);
  display.fillCircle(cx, cy, 3, LB_BLACK);
}

static void sceneClock(float, bool) {
  display.fillScreen(LB_WHITE);
  const Clock c = readClock();
  const bool wide = W >= H;
  const int r = min(W, H) / 2 - 4;
  drawAnalog(c, wide ? H / 2 : W / 2, wide ? H / 2 : W / 2, r);

  // The digital readout fills the room beside (or below) the dial, stacked from
  // the measured heights: time, date, seconds.
  const int areaX = wide ? H : 0, areaW = wide ? W - H : W;
  const int areaY = wide ? 0 : W, areaH = wide ? H : H - W;
  const int cx = areaX + areaW / 2;
  char t[8], d[16], sec[8];
  snprintf(t, sizeof t, "%02d:%02d", c.t.tm_hour, c.t.tm_min);
  snprintf(d, sizeof d, "%s %d %s", DAYS[c.t.tm_wday], c.t.tm_mday, MONTHS[c.t.tm_mon]);
  snprintf(sec, sizeof sec, ":%02d", c.t.tm_sec);
  const int ts = fitSize(t, areaW - 8, areaH / 2);
  const int total = textH(ts) + 6 + textH(1) + 6 + textH(2);
  int y = areaY + max(0, (areaH - total) / 2);
  textCentered(t, cx, y, ts);                 y += textH(ts) + 6;
  textCentered(d, cx, y, 1);                  y += textH(1) + 6;
  textCentered(sec, cx, y, 2);
}

// ── 7  low power ────────────────────────────────────────────────────────────
//
// The panel's slow mode: about 8 refreshes a second on the 2.13 inch, a few
// microamps for the screen. The sketch sends one frame a second, which is all a
// clock needs. The mode is left again at the end of the scene.

static void sceneLowPower(float, bool first) {
  static int lastSec = -1;
  if (first) { display.lowPower(true); lastSec = -1; }
  const Clock c = readClock();
  g_unchanged = c.t.tm_sec == lastSec;                   // nothing new: no redraw, and no frame sent
  if (g_unchanged) return;
  lastSec = c.t.tm_sec;
  display.fillScreen(LB_WHITE);
  display.drawRect(0, 0, W, H, LB_BLACK);
  char b[16];
  snprintf(b, sizeof b, "%02d:%02d:%02d", c.t.tm_hour, c.t.tm_min, c.t.tm_sec);
  const int ts = fitSize(b, W - 16, H / 2);
  const int total = textH(ts) + 8 + textH(1) + 2 + textH(1);
  int y = max(2, (H - total) / 2);
  textCentered(b, W / 2, y, ts);              y += textH(ts) + 8;
  textCentered("LOW POWER MODE", W / 2, y, 1); y += textH(1) + 2;
  textCentered("one frame a second", W / 2, y, 1);
}

// ── the show ────────────────────────────────────────────────────────────────

struct Scene {
  const char *name;
  uint32_t ms;                       // how long it runs
  void (*frame)(float t, bool first);
  bool direct;                       // writes the canvas itself; no fillScreen needed
};

static const Scene SCENES[] = {
  {"DANCE", (uint32_t)lb_clip_frames * 2000UL / lb_clip_fps, sceneClip, true},
  {"DITHERED PLASMA", 8000, scenePlasma, true},
  {"3D WIREFRAME",    8000, sceneCube, false},
  {"GAME OF LIFE",   10000, sceneLife, true},
  {"DASHBOARD",       8000, sceneDashboard, false},
  {"CLOCK",           7000, sceneClock, false},
  {"LOW POWER",      10000, sceneLowPower, false},
};
static const int SCENE_COUNT = sizeof SCENES / sizeof SCENES[0];

// A caption in the corner for the first two seconds of each scene.
static void caption(int index) {
  const Scene &s = SCENES[index];
  char b[40];
  snprintf(b, sizeof b, "%d/%d %s  BOOT:NEXT", index + 1, SCENE_COUNT, s.name);
  const int fh = textH(1);
  if (display.textWidth(b) + 8 > W) snprintf(b, sizeof b, "%d/%d %s", index + 1, SCENE_COUNT, s.name);   // no room for the hint
  const int w = display.textWidth(b) + 8, h = fh + 4;
  display.fillRect(0, H - h, w, h, LB_WHITE);
  display.drawRect(0, H - h, w, h, LB_BLACK);
  text(b, 4, H - h + 2, 1);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!display.begin()) {
    Serial.println("panel did not start - check wiring (the 4.2 inch also needs PSRAM)");
    while (true) delay(1000);
  }
  display.printInfo();
  W = display.width();
  H = display.height();
  fb = display.framebuffer();
  if (!fb) { Serial.println("no canvas"); while (true) delay(1000); }

  for (int i = 0; i < 256; i++) sinT[i] = (int8_t)(sinf(i * 6.2831853f / 256.0f) * 127);
  lw = W / CELL;
  lh = H / CELL;
  life[0] = (uint8_t *)calloc(lw * lh, 1);
  life[1] = (uint8_t *)calloc(lw * lh, 1);
  pinMode(SKIP_PIN, INPUT_PULLUP);
  setTimeFromCompile();
  clipFrame = (uint8_t *)calloc((size_t)((lb_clip_w + 7) / 8) * lb_clip_h, 1);
}

void loop() {
  for (int i = 0; i < SCENE_COUNT; i++) {
    const Scene &s = SCENES[i];
    if (s.frame == sceneClip && (!clipFits() || !clipFrame)) continue;   // the film is for a bigger screen than this
    Serial.printf("scene %d: %s\n", i + 1, s.name);
    const uint32_t start = millis();
    uint32_t frames = 0, fpsAt = start;
    bool first = true;
    while (millis() - start < s.ms) {
      static bool wasDown = false;
      const bool down = digitalRead(SKIP_PIN) == LOW;
      const bool pressed = down && !wasDown;
      wasDown = down;
      if (pressed) { Serial.println("  BOOT: next scene"); delay(40); break; }
      const uint32_t frameStart = millis();
      const float t = (frameStart - start) / 1000.0f;
      g_unchanged = false;
      s.frame(t, first);
      first = false;
      if (g_unchanged) { delay(20); continue; }              // low power: send nothing
      if (frameStart - start < 2000) caption(i);
      display.flush();
      frames++;
      const uint32_t spent = millis() - frameStart;
      if (spent < FRAME_MS) delay(FRAME_MS - spent);
      if (millis() - fpsAt >= 1000) {
        fps = frames * 1000.0f / (millis() - fpsAt);
        Serial.printf("  %.1f fps\n", fps);
        frames = 0;
        fpsAt = millis();
      }
    }
    if (i == SCENE_COUNT - 1) display.lowPower(false);   // back to the fast refresh for the next lap
  }
}
