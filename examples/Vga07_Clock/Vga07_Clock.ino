/*
  Vga07_Clock - an analog clock and a big digital one.

  YOU WILL LEARN
    - sin() and cos() to place things on a circle
    - repainting just the clock's own area once a second instead of the whole screen
    - drawing a thick line out of two triangles (the library's lines are 1 px)
    - keeping time with millis()

  The board has no battery-backed clock, so the clock starts from the moment
  this sketch was compiled (__DATE__ and __TIME__) and counts from there. After
  a reset it starts again from that same moment. To get the real time, connect
  to Wi-Fi once, fetch it, then switch Wi-Fi off again - see the README note on
  Wi-Fi and the picture.

  Press BOOT to add one hour (for setting the time without re-uploading).
*/
#include <LonelyBinaryDisplay.h>
#include <math.h>

LB_Display display(LB_VGA_800X600);

static const lb_color_t BG    = LB_RGB(8, 12, 18);
static const lb_color_t FACE  = LB_RGB(16, 24, 34);
static const lb_color_t TEAL  = LB_RGB(20, 137, 143);
static const int CX = 215, CY = 300, R = 200;      // the clock face

static uint32_t startSeconds;                      // seconds since midnight at compile time
static int hourOffset = 0;

static uint32_t compileSeconds() {
  int h, m, s;
  sscanf(__TIME__, "%d:%d:%d", &h, &m, &s);
  return h * 3600 + m * 60 + s;
}

// A thick hand from the centre: a long thin quad made of two triangles.
static void hand(float angle, int length, int width, lb_color_t color) {
  const float dx = sinf(angle), dy = -cosf(angle);
  const float px = -dy * width / 2, py = dx * width / 2;
  const int x1 = CX + dx * length, y1 = CY + dy * length;
  display.fillTriangle(CX + px, CY + py, CX - px, CY - py, x1, y1, color);
  display.fillCircle(CX, CY, width, color);
}

static void drawFace() {
  display.fillCircle(CX, CY, R, FACE);
  display.drawCircle(CX, CY, R, TEAL);
  display.drawCircle(CX, CY, R - 1, TEAL);
  for (int i = 0; i < 60; i++) {
    const float a = i * 6 * M_PI / 180;
    const bool five = i % 5 == 0;
    const int r1 = five ? R - 26 : R - 12, r2 = R - 4;
    display.drawLine(CX + sinf(a) * r1, CY - cosf(a) * r1, CX + sinf(a) * r2, CY - cosf(a) * r2,
                     five ? LB_WHITE : LB_RGB(110, 120, 135));
  }
  display.setTextSize(3);
  display.setTextColor(LB_WHITE);
  for (int n = 1; n <= 12; n++) {
    const float a = n * 30 * M_PI / 180;
    char s[4];
    snprintf(s, sizeof s, "%d", n);
    display.drawString(s, CX + sinf(a) * (R - 54) - display.textWidth(s) / 2,
                       CY - cosf(a) * (R - 54) - display.fontHeight() / 2);
  }
}

// The three hands and the hub. With erase = true they are drawn in the face
// colour instead, which removes them exactly: same geometry, same pixels.
// They are shorter than the ring of numbers (radius 122), so erasing one never
// damages a digit or a tick mark.
static void drawHands(uint32_t t, bool erase) {
  const int h = (t / 3600) % 24, m = (t / 60) % 60, s = t % 60;
  const float sa = s * 6 * M_PI / 180;
  hand(((h % 12) + m / 60.0f) * 30 * M_PI / 180, 80, 10, erase ? FACE : LB_WHITE);
  hand((m + s / 60.0f) * 6 * M_PI / 180, 108, 7, erase ? FACE : LB_CYAN);
  display.drawLine(CX - sinf(sa) * 24, CY + cosf(sa) * 24, CX + sinf(sa) * 112, CY - cosf(sa) * 112,
                   erase ? FACE : LB_RED);
  display.fillCircle(CX, CY, 6, erase ? FACE : LB_RED);
}

static void drawTime(uint32_t t, uint32_t previous) {
  const int h = (t / 3600) % 24, m = (t / 60) % 60, s = t % 60;

  // Analog: the face is drawn once, in setup(). Each second only the hands
  // change, so take the old ones off and put the new ones on - a few hundred
  // pixels, not the 125,000 of the whole face. (Repainting the face every second
  // shows: the monitor catches it empty for a moment, and the clock blinks.)
  if (previous != 0xFFFFFFFF) drawHands(previous, true);
  drawHands(t, false);

  // Digital, in the big size. A background colour on the text means each new
  // time overwrites the old one cleanly.
  char buf[16];
  snprintf(buf, sizeof buf, "%02d:%02d", h, m);
  display.setTextSize(8);
  display.setTextColor(LB_WHITE, BG);
  display.drawString(buf, 440, 170);
  snprintf(buf, sizeof buf, "%02d", s);
  display.setTextSize(5);
  display.setTextColor(TEAL, BG);
  display.drawString(buf, 440, 170 + 16 * 8 + 14);
}

void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);
  if (!display.begin()) {
    Serial.println("VGA did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  startSeconds = compileSeconds();
  display.fillScreen(BG);
  drawFace();

  display.setTextSize(3);
  display.setTextColor(LB_LIGHTGRAY);
  display.drawString(__DATE__, 440, 400);
  display.setTextSize(2);
  display.setTextColor(LB_GRAY);
  display.drawString("Starts from the time", 440, 460);
  display.drawString("this sketch was", 440, 492);
  display.drawString("compiled. BOOT adds", 440, 524);
  display.drawString("an hour.", 440, 556);
  display.flush();
}

void loop() {
  static uint32_t lastSecond = 0xFFFFFFFF, drawn = 0xFFFFFFFF;
  static bool wasDown = false;
  const bool down = digitalRead(0) == LOW;
  if (down && !wasDown) { hourOffset++; lastSecond = 0xFFFFFFFF; }   // redraw now
  wasDown = down;

  const uint32_t t = startSeconds + millis() / 1000 + hourOffset * 3600;
  if (t != lastSecond) {
    lastSecond = t;
    drawTime(t, drawn);
    drawn = t;
    display.flush();
  }
  delay(20);
}
