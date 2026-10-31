/*
  Vga05_Bounce - animation: erase, move, draw, once per frame.

  YOU WILL LEARN
    - the loop that every animation is made of
    - waiting for the start of a frame, so the loop runs at the monitor's rate
    - why you redraw only what moved, not the whole screen
    - reading the frame timing: scanStats() tells you if the picture glitched

  HOW IT WORKS
    The screen is a block of memory that the hardware reads out 50 times a
    second, forever, whatever your code does. Drawing writes into that block.
    So: wipe each ball with a black square, move it, draw it again. That is a
    few thousand pixels a frame; repainting all 480,000 would be slow and,
    because the monitor is reading while you write, would tear.

    A little tearing on something small is normal and hard to see. For
    changing the whole picture at once, see the double buffering note in the
    README (display.framebuffer2() and display.present()).

  The numbers at the top (and on Serial): the monitor's frames per second,
  late frames (should stay 0), and how many milliseconds one pass of the loop
  took. A frame lasts 20 ms, so while "loop" stays under that the animation
  runs at the full rate. Drawing into this screen manages only 2 to 5 million
  pixels a second (a full-screen fill takes 0.23 s), because the picture being sent to the monitor uses most of
  the memory bandwidth - which is why the sketch moves only a few small balls
  and does not repaint the screen. Add balls (N) until "loop" passes 17 ms to
  see the limit for yourself.
*/
#include <LonelyBinaryDisplay.h>

LB_Display display(LB_VGA_800X600);

static const int N = 8;            // balls
static const int TOP = 40;         // the status line lives above this
static float bx[N], by[N], vx[N], vy[N];
static int8_t br[N];
static lb_color_t bc[N];
static int W, H;

static const lb_color_t BG = LB_RGB(8, 12, 18);

// Block until the monitor has started a new frame (or 40 ms have passed).
static void waitFrame() {
  uint32_t f0, f1, d;
  display.scanStats(&f0, &d, &d);
  const uint32_t t0 = millis();
  do {
    delay(1);
    display.scanStats(&f1, &d, &d);
  } while (f1 == f0 && millis() - t0 < 40);
}

void setup() {
  Serial.begin(115200);
  if (!display.begin()) {
    Serial.println("VGA did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  W = display.width();
  H = display.height();
  display.fillScreen(BG);
  display.drawRect(0, TOP, W, H - TOP, LB_RGB(60, 70, 90));

  randomSeed(esp_random());
  for (int i = 0; i < N; i++) {
    br[i] = 8 + random(0, 13);
    bx[i] = random(br[i] + 2, W - br[i] - 2);
    by[i] = random(TOP + br[i] + 2, H - br[i] - 2);
    vx[i] = (random(0, 2) ? 1 : -1) * (1.5f + random(0, 40) / 10.0f);
    vy[i] = (random(0, 2) ? 1 : -1) * (1.5f + random(0, 40) / 10.0f);
    bc[i] = LB_RGB(80 + random(0, 176), 80 + random(0, 176), 80 + random(0, 176));
  }
  display.setTextSize(2);
}

void loop() {
  waitFrame();
  const uint32_t t0 = micros();

  for (int i = 0; i < N; i++) {
    // 1 - wipe the ball where it was (the box around it, plus the border it may touch).
    const int r = br[i] + 1;
    display.fillRect((int)bx[i] - r, (int)by[i] - r, 2 * r + 1, 2 * r + 1, BG);

    // 2 - move it and bounce off the walls.
    bx[i] += vx[i];
    by[i] += vy[i];
    if (bx[i] < br[i] + 2)       { bx[i] = br[i] + 2;     vx[i] = -vx[i]; }
    if (bx[i] > W - br[i] - 3)   { bx[i] = W - br[i] - 3; vx[i] = -vx[i]; }
    if (by[i] < TOP + br[i] + 2) { by[i] = TOP + br[i] + 2; vy[i] = -vy[i]; }
    if (by[i] > H - br[i] - 3)   { by[i] = H - br[i] - 3;   vy[i] = -vy[i]; }
  }
  // 3 - draw them all in their new places (after every wipe, so one ball never
  // erases a piece of another).
  for (int i = 0; i < N; i++) {
    display.fillCircle((int)bx[i], (int)by[i], br[i], bc[i]);
  }
  display.flush();

  // The status line, twice a second.
  static uint32_t lastMs = 0, lastFrames = 0;
  static uint32_t loopUs = 0;
  loopUs = micros() - t0;
  if (millis() - lastMs >= 500) {
    uint32_t frames, late, maxUs;
    display.scanStats(&frames, &late, &maxUs);
    char line[96];
    snprintf(line, sizeof line, "%2lu fps   late frames %lu   loop %lu.%lu ms   ",
             (unsigned long)((frames - lastFrames) * 1000 / (millis() - lastMs)), (unsigned long)late,
             (unsigned long)(loopUs / 1000), (unsigned long)(loopUs % 1000 / 100));
    display.setTextColor(LB_WHITE, BG);
    display.drawString(line, 12, 12);
    Serial.println(line);
    lastMs = millis();
    lastFrames = frames;
  }
}
