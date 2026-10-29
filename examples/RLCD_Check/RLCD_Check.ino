/*
  RLCD first-power-on check, for the reflective LCD series (2.13, 2.9, 4.2 inch).
  Change the panel line below. Three things to look at:

  1. PIXEL LAYOUT  The 8-pixel checkerboard must be sharp squares, edge to
     edge. Smeared, doubled or striped squares = the pixel packing is wrong.
     The 1-pixel frame must be complete on all four sides and one pixel thick.

  2. ORIENTATION   Corner marks: TL has one dot, TR two, BL three, BR four,
     and the text reads normally. Mirrored/rotated = layout or rotation wrong.

  3. INVERT        Every 4 s the panel flips polarity and says which one it is
     in. "DEFAULT" is what a sketch gets from the panel table and must look
     like black ink on a white page. If it is white on black, flip invert for
     this panel in panels.yaml.

  Only the 4.2 inch needs PSRAM (Tools > PSRAM > OPI on the ESP32-S3); the
  2.13 and 2.9 inch canvases fit in internal RAM.
*/
#include <LonelyBinaryDisplay.h>

LB_Display display(LB_RLCD_213);
//LB_Display display(LB_RLCD_290);
//LB_Display display(LB_RLCD_420);
//LB_Display display(LB_ST7305_400X300);

// Everything is sized from the screen: the 2.13 inch is only 122 pixels wide.
static bool big() { return display.width() >= 300; }

static void dots(int x, int y, int n) {
  const int step = big() ? 8 : 6, size = big() ? 5 : 4;
  for (int i = 0; i < n; i++) display.fillRect(x + i * step, y, size, size, LB_BLACK);
}

static void draw(bool isDefault) {
  const int W = display.width(), H = display.height();
  display.fillScreen(LB_WHITE);

  for (int y = 0; y < H; y += 8)
    for (int x = 0; x < W; x += 8)
      if (((x >> 3) + (y >> 3)) & 1) display.fillRect(x, y, 8, 8, LB_BLACK);

  // Clear panels for the corner marks and the label, so they read on the grid.
  const int ts = big() ? 2 : 1;
  const int pw = big() ? 70 : 48, ph = big() ? 30 : 20;
  const int px[4] = {8, W - 8 - pw, 8, W - 8 - pw};
  const int py[4] = {8, 8, H - 8 - ph, H - 8 - ph};
  const char *tag[4] = {"TL", "TR", "BL", "BR"};
  for (int i = 0; i < 4; i++) {
    display.fillRect(px[i], py[i], pw, ph, LB_WHITE);
    display.drawRect(px[i], py[i], pw, ph, LB_BLACK);
    display.setTextColor(LB_BLACK);
    display.setTextSize(ts);
    display.drawString(tag[i], px[i] + 6, py[i] + (big() ? 8 : 6));
    dots(px[i] + (big() ? 34 : 22), py[i] + (big() ? 12 : 8), i + 1);
  }

  const int lw = min(190, W - 16), lh = big() ? 60 : 40, lx = (W - lw) / 2, ly = (H - lh) / 2;
  display.fillRect(lx, ly, lw, lh, LB_WHITE);
  display.drawRect(lx, ly, lw, lh, LB_BLACK);
  display.setTextColor(LB_BLACK);
  display.setTextSize(ts);
  display.drawString(isDefault ? "DEFAULT" : "OPPOSITE", lx + 8, ly + 8);
  display.setTextSize(1);
  display.drawString(big() ? "DEFAULT = black on white" : "black on white", lx + 8, ly + lh - 14);

  display.drawRect(0, 0, W, H, LB_BLACK);  // 1-pixel frame, all four sides
  display.flush();
}

void setup() {
  Serial.begin(115200);
  if (!display.begin()) {
    Serial.println("panel did not start - check wiring (the 4.2 inch also needs PSRAM)");
    while (true) delay(1000);
  }
  display.printInfo();
}

void loop() {
  static bool isDefault = false;
  isDefault = !isDefault;
  const bool tableInvert = display.panelDef()->invert;
  display.setInverted(isDefault ? tableInvert : !tableInvert);
  draw(isDefault);
  Serial.printf("%s (setInverted(%d))\n", isDefault ? "DEFAULT" : "OPPOSITE",
                isDefault ? tableInvert : !tableInvert);
  delay(4000);
}
