/*
  Hdmi02_ColorBars - wiring self-test. Run this first on a new board or monitor.

  The board sends 16 GPIO lines (5 red, 6 green, 5 blue) and a pixel clock to
  the SiI9022A HDMI chip. A broken or swapped line does not stop the
  picture, it only makes one colour wrong - and that is hard to see in a photo
  and easy to see here.

  WHAT YOU SHOULD SEE (top to bottom)
    1  eight colour bars: white yellow cyan green magenta red blue black
    2  four smooth ramps: grey, red, green, blue - from black to full
    3  sixteen small patches, one per data line, each named after its line

  HOW TO READ IT
    - A bar of the wrong colour: that colour's top line is wrong or swapped.
    - A ramp with one big step in it instead of a smooth slope: a low-order
      line (B0, G0, R0...) is stuck or shorted.
    - A patch that is black when its neighbours are not: that line is open.
      The patches for the lowest bits (B0, R0, G0) are very dim by design - a
      single least-significant bit is 1/32 of the full brightness. Compare
      them with the black one beside them, not with the bright ones.
    - The edges are cut off: many TVs zoom in a little ("overscan"). Look for
      "Just Scan", "1:1" or "Screen Fit" in the TV's picture menu.
    - No picture at all: check the cable and the TV's input. printInfo() on
      Serial says whether a monitor is plugged in ("HDMI connected").

  Also prints the panel details and the frame timing to Serial.
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

// The names of the 16 data lines, in the order of the bits of an RGB565 word.
static const char *BIT_NAME[16] = {"B0", "B1", "B2", "B3", "B4", "G0", "G1", "G2",
                                   "G3", "G4", "G5", "R0", "R1", "R2", "R3", "R4"};

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
  display.printInfo();

  const int W = display.width(), H = display.height();
  uint16_t *fb = display.framebuffer();          // the screen as 16-bit RGB565 words
  display.fillScreen(LB_BLACK);

  // 1 - the colour bars.
  static const lb_color_t BARS[8] = {LB_WHITE, LB_YELLOW, LB_CYAN, LB_GREEN,
                                     LB_MAGENTA, LB_RED, LB_BLUE, LB_BLACK};
  for (int i = 0; i < 8; i++) display.fillRect(i * W / 8, 0, W / 8, 220, BARS[i]);

  // 2 - the ramps: one column per brightness step, so a missing bit shows as a jump.
  for (int x = 0; x < W; x++) {
    const uint8_t v = (uint8_t)(x * 255 / (W - 1));
    display.drawFastVLine(x, 230, 40, LB_RGB(v, v, v));
    display.drawFastVLine(x, 270, 40, LB_RGB(v, 0, 0));
    display.drawFastVLine(x, 310, 40, LB_RGB(0, v, 0));
    display.drawFastVLine(x, 350, 40, LB_RGB(0, 0, v));
  }

  // 3 - one patch per data line. Written as raw RGB565 words, so exactly one
  // GPIO is high in each patch. R and G and B end up as three groups.
  display.setTextSize(2);
  display.setTextColor(LB_WHITE);
  const int cell = W / 16;
  for (int b = 0; b < 16; b++) {
    const uint16_t word = (uint16_t)(1u << b);
    for (int y = 410; y < 520; y++)
      for (int x = b * cell + 4; x < (b + 1) * cell - 4; x++) fb[y * W + x] = word;
    display.drawString(BIT_NAME[b], b * cell + 8, 530);
  }
  display.drawString("one data line per patch - the lowest bits are dim", 8, 565);

  display.flush();
}

void loop() {
  // Frame timing, once a second: a healthy picture reports 50 frames a second
  // and no late ones.
  static uint32_t lastFrames = 0, lastMs = 0;
  if (millis() - lastMs >= 1000) {
    uint32_t frames, late, maxUs;
    if (display.scanStats(&frames, &late, &maxUs)) {
      Serial.printf("%lu frames/s, %lu late frames in total, longest frame %lu us\n",
                    (unsigned long)(frames - lastFrames), (unsigned long)late, (unsigned long)maxUs);
      lastFrames = frames;
    }
    lastMs = millis();
  }
}
