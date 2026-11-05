/*
  Hdmi06_Bounce - animation: erase, move, draw, once per frame.

  YOU WILL LEARN
    - the loop that every animation is made of
    - waiting for the start of a frame, so the loop runs at the monitor's rate
    - why you redraw only what moved, not the whole screen
    - reading the frame timing: scanStats() tells you if the picture glitched

  HOW IT WORKS
    The screen is a block of memory that the hardware reads out about 60 times
    a second, forever, whatever your code does. Drawing writes into that block.
    So: wipe each ball with a black square, move it, draw it again. That is a
    few thousand pixels a frame; repainting all 307,200 would be slow and,
    because the monitor is reading while you write, would tear.

    A little tearing on something small is normal and hard to see. For
    changing the whole picture at once, see the double buffering note in the
    README (display.framebuffer2() and display.present()).

  The numbers at the top (and on Serial): the monitor's frames per second,
  late frames (should stay 0), and how many milliseconds one pass of the loop
  took. A frame lasts about 15 ms, so while "loop" stays under that the animation
  runs at the full rate. Drawing into this screen manages only 2 to 5 million
  pixels a second (a full-screen fill takes 0.23 s), because the picture being sent to the monitor uses most of
  the memory bandwidth - which is why the sketch moves only a few small balls
  and does not repaint the screen. Add balls (N) until "loop" passes 15 ms to
  see the limit for yourself.
*/
#include <LonelyBinaryDisplay.h>

// ---- GPIO wiring. Every pin this sketch uses is here - change the numbers to match
// ---- your board; nothing below refers to a GPIO by number. -1 = not connected.
#define PIN_DE     45   // to the SiI9022A HDMI chip
#define PIN_PCLK   12
#define PIN_HSYNC  38
#define PIN_VSYNC  39
#define PIN_B0     48   // blue  - 5 bits
#define PIN_B1     47
#define PIN_B2     21
#define PIN_B3     14
#define PIN_B4     13
#define PIN_G0     11   // green - 6 bits
#define PIN_G1     10
#define PIN_G2      9
#define PIN_G3     46
#define PIN_G4      3
#define PIN_G5      8
#define PIN_R0     18   // red   - 5 bits
#define PIN_R1     17
#define PIN_R2     16
#define PIN_R3     15
#define PIN_R4      6
#define PIN_SDA     5   // I2C to the SiI9022A (its setup)
#define PIN_SCL     4

LB_Display display(LB_HDMI_RGB);

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
  LB_WiringRgb pins = display.wiringRgb();   // the board's default, then yours
  pins.de = PIN_DE;        pins.pclk = PIN_PCLK;
  pins.hsync = PIN_HSYNC;  pins.vsync = PIN_VSYNC;
  pins.i2cSda = PIN_SDA;   pins.i2cScl = PIN_SCL;
  const int8_t dataPins[16] = {PIN_B0, PIN_B1, PIN_B2, PIN_B3, PIN_B4,
                              PIN_G0, PIN_G1, PIN_G2, PIN_G3, PIN_G4, PIN_G5,
                              PIN_R0, PIN_R1, PIN_R2, PIN_R3, PIN_R4};
  for (int i = 0; i < 16; i++) pins.data[i] = dataPins[i];
  display.setWiring(pins);

  display.setResolution(800, 600);      // B: 800 x 600 @ 50 Hz, the default
  // display.setResolution(640, 480);   // A: 640 x 480 @ 64.7 Hz - use this if the monitor shows no picture or streaks at the left
  if (!display.begin()) {
    Serial.println("HDMI did not start. Is Tools > PSRAM set to OPI PSRAM?");
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
