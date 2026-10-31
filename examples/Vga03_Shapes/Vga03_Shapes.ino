/*
  Vga03_Shapes - the coordinate system, and every shape you can draw.

  YOU WILL LEARN
    - x grows to the right, y grows DOWNWARD, (0, 0) is the top left corner
    - drawXxx() is an outline, fillXxx() is solid
    - colours are LB_RGB(red, green, blue), each 0..255, or a name like LB_ORANGE
    - how to measure the screen with width() and height() instead of typing 800

  A grid every 100 pixels is drawn first, so you can read coordinates straight
  off the monitor.
*/
#include <LonelyBinaryDisplay.h>

LB_Display display(LB_VGA_800X600);

static const lb_color_t GRID = LB_RGB(40, 48, 60);

void setup() {
  Serial.begin(115200);
  if (!display.begin()) {
    Serial.println("VGA did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  const int W = display.width(), H = display.height();
  display.fillScreen(LB_RGB(10, 14, 20));

  // The grid, with the numbers along the top and left.
  display.setTextSize(1);
  display.setTextColor(LB_RGB(120, 140, 160));
  for (int x = 0; x < W; x += 100) {
    display.drawFastVLine(x, 0, H, GRID);
    char n[8];
    snprintf(n, sizeof n, "%d", x);
    display.drawString(n, x + 3, 3);
  }
  for (int y = 0; y < H; y += 100) {
    display.drawFastHLine(0, y, W, GRID);
    if (y) {
      char n[8];
      snprintf(n, sizeof n, "%d", y);
      display.drawString(n, 3, y + 3);
    }
  }

  // Rectangles: outline, solid, rounded.
  display.drawRect(120, 40, 160, 100, LB_CYAN);
  display.fillRect(320, 40, 160, 100, LB_ORANGE);
  display.drawRoundRect(520, 40, 160, 100, 20, LB_GREEN);
  display.fillRoundRect(700, 40, 80, 100, 20, LB_MAGENTA);

  // Circles and triangles.
  display.drawCircle(200, 260, 60, LB_YELLOW);
  display.fillCircle(360, 260, 60, LB_RED);
  display.drawTriangle(480, 320, 560, 200, 640, 320, LB_WHITE);
  display.fillTriangle(660, 320, 720, 200, 780, 320, LB_BLUE);

  // A fan of lines from one corner, a colour wheel's worth of hue.
  for (int i = 0; i <= 24; i++) {
    const int x = 120 + i * 20;
    const uint8_t r = (uint8_t)(255 - i * 10), b = (uint8_t)(i * 10);
    display.drawLine(40, 580, x, 380, LB_RGB(r, 120, b));
  }

  // Nested squares, to show the order things are painted in: later wins.
  for (int i = 0; i < 8; i++) {
    const int s = 160 - i * 20;
    display.fillRect(560 + (160 - s) / 2, 380 + (160 - s) / 2, s, s, i % 2 ? LB_BLACK : LB_RGB(20, 137, 143));
  }

  // The four corners, so a monitor that crops the picture shows it at once.
  display.fillRect(0, 0, 8, 8, LB_RED);
  display.fillRect(W - 8, 0, 8, 8, LB_GREEN);
  display.fillRect(0, H - 8, 8, 8, LB_BLUE);
  display.fillRect(W - 8, H - 8, 8, 8, LB_WHITE);

  display.setTextSize(2);
  display.setTextColor(LB_WHITE, LB_RGB(10, 14, 20));
  display.drawString("drawRect  fillRect  drawRoundRect  fillRoundRect", 120, 160);

  display.flush();
}

void loop() {}
