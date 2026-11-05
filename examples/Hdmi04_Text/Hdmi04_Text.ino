/*
  Hdmi04_Text - fonts, sizes, colours and alignment.

  YOU WILL LEARN
    - two built-in fonts: LB_Font8x16 (the workhorse) and LB_Font5x8 (small)
    - setTextSize(n) scales a font by a whole number
    - setTextColor(fg) draws on whatever is there; setTextColor(fg, bg) paints
      the whole letter box, which is what you want for a number that changes
    - textWidth() and fontHeight() to centre or right-align text
    - how many characters fit: 100 x 37 in the 8x16 font, 160 x 75 in 5x8
      (at 800 x 600; more at a bigger resolution, see Hdmi03_Resolutions)
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

// Draw `s` with its right edge at x.
static void drawRight(const char *s, int x, int y) { display.drawString(s, x - display.textWidth(s), y); }

// Draw `s` centred on x.
static void drawCentre(const char *s, int x, int y) { display.drawString(s, x - display.textWidth(s) / 2, y); }

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
  const int W = display.width(), H = display.height();
  display.fillScreen(LB_RGB(8, 12, 18));

  // Title, centred. Every position below follows on from the line above it,
  // so the page also fits when you pick a bigger resolution.
  display.setFont(&LB_Font8x16);
  display.setTextSize(3);
  display.setTextColor(LB_WHITE);
  char title[32];
  snprintf(title, sizeof title, "Text on %d x %d", W, H);
  drawCentre(title, W / 2, 12);

  // The same words at every size.
  display.setTextColor(LB_CYAN);
  int y = 70;
  for (int s = 1; s <= 4; s++) {
    display.setTextSize(s);
    char line[32];
    snprintf(line, sizeof line, "size %d: Lonely Binary", s);
    display.drawString(line, 24, y);
    y += display.fontHeight() + 6;
  }

  // The small font. It is 5x8, so it fits more characters per line.
  display.setFont(&LB_Font5x8);
  display.setTextSize(1);
  display.setTextColor(LB_LIGHTGRAY);
  char small[96];
  snprintf(small, sizeof small, "5x8 font, size 1: %d characters across the screen, %d lines down.", W / 5, H / 8);
  display.drawString(small, 24, y + 4);
  display.setTextSize(2);
  display.drawString("5x8 font, size 2", 24, y + 18);
  y += 44;

  // Colours: any LB_RGB(r, g, b).
  display.setFont(&LB_Font8x16);
  display.setTextSize(2);
  const lb_color_t PAL[6] = {LB_RED, LB_ORANGE, LB_YELLOW, LB_GREEN, LB_CYAN, LB_MAGENTA};
  const char *NAME[6] = {"red", "orange", "yellow", "green", "cyan", "magenta"};
  int x = 24;
  for (int i = 0; i < 6; i++) {
    display.setTextColor(PAL[i]);
    x = display.drawString(NAME[i], x, y) + 16;         // drawString returns where it ended
  }
  y += display.fontHeight() + 12;

  // Foreground on a background: the box behind each letter is painted too.
  display.setTextColor(LB_BLACK, LB_YELLOW);
  x = display.drawString(" black on yellow ", 24, y) + 24;
  display.setTextColor(LB_WHITE, LB_RGB(20, 137, 143));
  display.drawString(" white on teal ", x, y);
  y += display.fontHeight() + 14;

  // Alignment.
  display.setTextColor(LB_WHITE);
  display.drawString("left", 24, y);
  drawCentre("centre", W / 2, y);
  drawRight("right", W - 24, y);
  y += display.fontHeight() + 14;

  display.setTextSize(1);
  display.setTextColor(LB_GRAY);
  display.drawString("UTF-8 in, always. Missing characters draw as a box:  \xC3\xA9 \xE4\xB8\xAD", 24, y);

  display.flush();
}

void loop() {}
