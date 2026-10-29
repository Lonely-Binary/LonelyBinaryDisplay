/*
  SquareCheck — first power-on check for a square SPI panel.

  1. BACKLIGHT  Sweeps 0% -> 100% -> 0%, the percentage printed on the screen.
     Brightest at 0% means the backlight polarity in panels.yaml is wrong.
  2. COLOUR     Full-screen RED, GREEN, BLUE, each with its own name on it.
     If the words do not match what you see, the colour order is wrong
     (red <-> blue = bgr). If black comes out white, invert is wrong.
  3. EDGES      White frame with a coloured bar on each edge. All four bars and
     the whole frame must be visible; a missing line means the offsets are off.

  Everything repeats every few seconds. Serial prints the panel details.
*/
#include <LonelyBinaryDisplay.h>

// Change this one line for another size:  LB_SQUARE_085   LB_SQUARE_133   LB_SQUARE_154   LB_SQUARE_200   LB_SQUARE_231   LB_SQUARE_392
LB_Display display(LB_SQUARE_392);
//LB_Display display(LB_SQUARE_085);
//LB_Display display(LB_SQUARE_133);
//LB_Display display(LB_SQUARE_154);
//LB_Display display(LB_SQUARE_200);
//LB_Display display(LB_SQUARE_231);

static void colourPage(lb_color_t bg, const char *name) {
  display.fillScreen(bg);
  display.setTextColor(LB_WHITE);
  display.setTextSize(3);
  display.drawString(name, 8, display.height() / 2 - 12);
  display.setTextSize(1);
  display.drawString("LonelyBinary", 8, 6);
  display.flush();
}

// One step of the sweep: set the level and print it on the screen.
static void showLevel(int pct) {
  display.backlight((pct * 255 + 50) / 100);
  display.fillScreen(LB_WHITE);
  display.setTextColor(LB_BLACK);
  display.setTextSize(1);
  display.drawString("BACKLIGHT PWM", 8, 6);
  display.setTextSize(4);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  display.drawString(buf, 8, display.height() / 2 - 16);
  display.flush();
  Serial.printf("backlight %d%%\n", pct);
  delay(150);
}

// 0% -> 100% -> 0%, in 5% steps.
static void fadeBacklight() {
  for (int p = 0; p <= 100; p += 5) showLevel(p);
  for (int p = 100; p >= 0; p -= 5) showLevel(p);
  display.backlight(255);
}

static void edgePage() {
  const int w = display.width(), h = display.height();
  display.fillScreen(LB_BLACK);
  display.drawRect(0, 0, w, h, LB_WHITE);
  display.fillRect(w / 4, 0, w / 2, 4, LB_RED);            // top
  display.fillRect(w / 4, h - 4, w / 2, 4, LB_GREEN);      // bottom
  display.fillRect(0, h / 4, 4, h / 2, LB_BLUE);           // left
  display.fillRect(w - 4, h / 4, 4, h / 2, LB_YELLOW);     // right
  display.setTextColor(LB_WHITE);
  display.setTextSize(2);
  display.drawString("EDGES", 30, h / 2 - 16);
  display.setTextSize(1);
  char wh[16];
  snprintf(wh, sizeof(wh), "%dx%d", w, h);
  display.drawString(wh, 40, h / 2 + 4);
  display.flush();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!display.begin(true)) {
    Serial.println("Display did not start. Check the ribbon cable and DC.");
    return;
  }
  display.printInfo();
}

void loop() {
  colourPage(LB_RED, "RED");     delay(1500);
  colourPage(LB_GREEN, "GREEN"); delay(1500);
  colourPage(LB_BLUE, "BLUE");   delay(1500);
  edgePage();                    delay(2500);
  fadeBacklight();
}
