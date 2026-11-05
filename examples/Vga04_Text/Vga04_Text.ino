/*
  Vga04_Text - fonts, sizes, colours and alignment.

  YOU WILL LEARN
    - two built-in fonts: LB_Font8x16 (the workhorse) and LB_Font5x8 (small)
    - setTextSize(n) scales a font by a whole number
    - setTextColor(fg) draws on whatever is there; setTextColor(fg, bg) paints
      the whole letter box, which is what you want for a number that changes
    - textWidth() and fontHeight() to centre or right-align text
    - how many characters fit: 100 x 37 in the 8x16 font, 160 x 75 in 5x8
*/
#include <LonelyBinaryDisplay.h>

// ---- GPIO wiring. Every pin this sketch uses is here - change the numbers to match
// ---- your board; nothing below refers to a GPIO by number. -1 = not connected.
#define PIN_HSYNC   2   // VGA connector
#define PIN_VSYNC   1
#define PIN_B0     11   // blue  - 5-bit R2R ladder, B0 = least significant
#define PIN_B1     12
#define PIN_B2     13
#define PIN_B3     14
#define PIN_B4     21
#define PIN_G0     17   // green - 6-bit ladder
#define PIN_G1     18
#define PIN_G2      8
#define PIN_G3      3
#define PIN_G4      9
#define PIN_G5     10
#define PIN_R0      7   // red   - 5-bit ladder
#define PIN_R1     15
#define PIN_R2     16
#define PIN_R3     38
#define PIN_R4     39

LB_Display display(LB_VGA_16BIT);

// Draw `s` with its right edge at x.
static void drawRight(const char *s, int x, int y) { display.drawString(s, x - display.textWidth(s), y); }

// Draw `s` centred on x.
static void drawCentre(const char *s, int x, int y) { display.drawString(s, x - display.textWidth(s) / 2, y); }

void setup() {
  Serial.begin(115200);
  LB_WiringRgb pins = display.wiringRgb();   // the board's default, then yours
  pins.hsync = PIN_HSYNC;  pins.vsync = PIN_VSYNC;
  const int8_t dataPins[16] = {PIN_B0, PIN_B1, PIN_B2, PIN_B3, PIN_B4,
                              PIN_G0, PIN_G1, PIN_G2, PIN_G3, PIN_G4, PIN_G5,
                              PIN_R0, PIN_R1, PIN_R2, PIN_R3, PIN_R4};
  for (int i = 0; i < 16; i++) pins.data[i] = dataPins[i];
  display.setWiring(pins);

  if (!display.begin()) {
    Serial.println("VGA did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  const int W = display.width();
  display.fillScreen(LB_RGB(8, 12, 18));

  // Title, centred.
  display.setFont(&LB_Font8x16);
  display.setTextSize(4);
  display.setTextColor(LB_WHITE);
  drawCentre("Text on 800 x 600", W / 2, 16);

  // The same word at every size.
  display.setTextColor(LB_CYAN);
  int y = 100;
  for (int s = 1; s <= 5; s++) {
    display.setTextSize(s);
    char line[32];
    snprintf(line, sizeof line, "size %d: Lonely Binary", s);
    display.drawString(line, 24, y);
    y += display.fontHeight() + 6;
  }

  // The small font. It is 5x8, so it fits twice the characters per line.
  display.setFont(&LB_Font5x8);
  display.setTextSize(1);
  display.setTextColor(LB_LIGHTGRAY);
  display.drawString("5x8 font, size 1: 160 characters across the screen, 75 lines down - for dense readouts.", 24, y + 8);
  display.setTextSize(2);
  display.drawString("5x8 font, size 2", 24, y + 28);

  // Colours: any LB_RGB(r, g, b).
  display.setFont(&LB_Font8x16);
  display.setTextSize(3);
  const lb_color_t PAL[6] = {LB_RED, LB_ORANGE, LB_YELLOW, LB_GREEN, LB_CYAN, LB_MAGENTA};
  const char *NAME[6] = {"red", "orange", "yellow", "green", "cyan", "magenta"};
  int x = 24;
  for (int i = 0; i < 6; i++) {
    display.setTextColor(PAL[i]);
    x = display.drawString(NAME[i], x, 380) + 24;       // drawString returns where it ended
  }

  // Foreground on a background: the box behind each letter is painted too.
  display.setTextSize(3);
  display.setTextColor(LB_BLACK, LB_YELLOW);
  display.drawString(" black on yellow ", 24, 440);
  display.setTextColor(LB_WHITE, LB_RGB(20, 137, 143));
  display.drawString(" white on teal ", 400, 440);

  // Alignment.
  display.setTextSize(2);
  display.setTextColor(LB_WHITE);
  display.drawString("left", 24, 520);
  drawCentre("centre", W / 2, 520);
  drawRight("right", W - 24, 520);

  display.setTextSize(1);
  display.setTextColor(LB_GRAY);
  display.drawString("UTF-8 in, always. Characters the font does not have are drawn as a box:  \xC3\xA9 \xE4\xB8\xAD", 24, 570);

  display.flush();
}

void loop() {}
