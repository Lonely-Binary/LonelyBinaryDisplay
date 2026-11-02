/*
  VcomTune — stop a screen from shimmering.

  If grey, skin-tone or mid-blue areas of a still picture seem to flicker, as
  if the backlight were unsteady, the panel's VCOM (common-electrode) voltage
  is a little off for that glass. Black, white and saturated colours look fine
  either way, which is how you tell it from a real backlight fault.

  This sketch fills the screen with the patterns where that flicker is easiest
  to see — solid mid grey, dark grey and teal; 1-pixel checks and 1-pixel
  stripes of the same tones; a grey ramp — and lets you step VCOM until they
  are steady:

      Serial Monitor (115200, any line ending):
        +  /  -     VCOM one step up / down
        a number    jump to that value, 0-63 (0x00-0x3F)
        s           print the line to paste into your sketch
      BOOT button   one step up;  hold it for one step down

  Look at the checked and striped squares from about arm's length. Step
  through the range; the right value is the one where they are steadiest.
  Then either
    - put it in your sketch, after begin():   display.setVcom(0x05);
    - or, to make it the panel's default for every sketch, add it to the
      panel's entry in panels.yaml (`vcom: 0x05`) and run
      python3 tools/gen_panels.py.

  VCOM is only adjustable on the ST7796 (3.5 inch, 0.5 V + 0.025 V x value)
  and ST7789 (0.1 V + 0.025 V x value) panels. LB_TFT_35 already ships with
  0x05; the others start at their controller's init value.

  Needs a framebuffer: pick Tools > PSRAM > "OPI PSRAM" on an ESP32-S3.
*/
#include <LonelyBinaryDisplay.h>

// ---- GPIO wiring. Every pin this sketch uses is here - change the numbers to match
// ---- your board; nothing below refers to a GPIO by number. -1 = not connected.
#if CONFIG_IDF_TARGET_ESP32S3   // ESP32-S3 (Lonely Binary kit, FloppyDisk board)
  #define PIN_CS    10
  #define PIN_RST   42
  #define PIN_DC     2
  #define PIN_MOSI  11
  #define PIN_SCLK  12
  #define PIN_MISO  -1   // not used by the display
  #define PIN_BL    41   // backlight (PWM)
#else                           // classic ESP32
  #define PIN_CS    15
  #define PIN_RST    4
  #define PIN_DC     2
  #define PIN_MOSI  23
  #define PIN_SCLK  18
  #define PIN_MISO  -1   // not used by the display
  #define PIN_BL    32   // backlight (PWM)
#endif
#define PIN_KEY      0   // BOOT button, LOW when pressed; -1 = no button

LB_Display display(LB_TFT_35);
//LB_Display display(LB_TFT_20);
//LB_Display display(LB_TFT_24);
//LB_Display display(LB_TFT_28);

const lb_color_t PANEL = LB_RGB(24, 28, 32);
const lb_color_t TEXT  = LB_RGB(230, 230, 230);
const lb_color_t DIM   = LB_RGB(140, 140, 140);

int16_t headerH, footerH;
int16_t firstVcom;   // what the panel started with, shown as "start"

void drawPatterns() {
  const int16_t W = display.width(), H = display.height();
  display.fillScreen(LB_BLACK);

  // Three columns: mid grey, dark grey, teal. In each, a solid block, then the
  // tone in 1-pixel checks and in 1-pixel horizontal stripes against black.
  // The checks and stripes are where a wrong VCOM shows first: the panel
  // drives neighbouring pixels at opposite polarity, and any offset between
  // the two shows up as a flicker the solid block hides.
  const lb_color_t tones[] = {LB_RGB(128, 128, 128), LB_RGB(64, 64, 64), LB_RGB(20, 137, 143)};
  const int16_t gap = W / 40 + 2;
  const int16_t colW = (W - 4 * gap) / 3;
  const int16_t rampH = H / 12;
  const int16_t top = headerH + gap;
  const int16_t blockH = (H - footerH - rampH - top - 4 * gap) / 3;

  for (int i = 0; i < 3; i++) {
    const int16_t x = gap + i * (colW + gap);
    int16_t y = top;
    display.fillRect(x, y, colW, blockH, tones[i]);               // solid
    y += blockH + gap;
    for (int16_t yy = 0; yy < blockH; yy++)                         // checks
      for (int16_t xx = (yy & 1); xx < colW; xx += 2)
        display.drawPixel(x + xx, y + yy, tones[i]);
    y += blockH + gap;
    for (int16_t yy = 0; yy < blockH; yy += 2)                      // stripes
      display.drawFastHLine(x, y + yy, colW, tones[i]);
  }

  // Grey ramp, black to white, across the full width.
  const int16_t rampY = H - footerH - gap - rampH;
  for (int16_t x = 0; x < W; x++) {
    const uint8_t v = (uint8_t)((int32_t)x * 255 / (W - 1));
    display.drawFastVLine(x, rampY, rampH, LB_RGB(v, v, v));
  }
}

void drawLabels() {
  const int16_t W = display.width(), H = display.height();
  char s[64];

  display.fillRect(0, 0, W, headerH, PANEL);
  display.setTextColor(TEXT);
  display.setTextSize(W >= 400 ? 2 : 1);
  snprintf(s, sizeof s, "VCOM 0x%02X (%d) = %.3f V", display.vcom(), display.vcom(), display.vcomVolts());
  display.drawString(s, 6, 4);

  display.fillRect(0, H - footerH, W, footerH, PANEL);
  display.setTextColor(DIM);
  display.setTextSize(1);
  snprintf(s, sizeof s, "start 0x%02X   Serial: + - number s   BOOT +1, hold -1", firstVcom);
  display.drawString(s, 6, H - footerH + (footerH - display.fontHeight()) / 2);

  display.flush();
}

void printPasteLine() {
  Serial.printf("display.setVcom(0x%02X);   // %.3f V   (panels.yaml: vcom: 0x%02X)\n",
                display.vcom(), display.vcomVolts(), display.vcom());
}

void set(int v) {
  v = constrain(v, 0, 0x3F);
  if (!display.setVcom((uint8_t)v)) return;
  drawLabels();
  Serial.printf("VCOM 0x%02X (%d) = %.3f V\n", v, v, display.vcomVolts());
}

void setup() {
  Serial.begin(115200);
  delay(500);
  if (PIN_KEY >= 0) pinMode(PIN_KEY, INPUT_PULLUP);

  LB_Wiring pins = LB_WIRING;   // keeps the SPI bus that fits this chip
  pins.cs = PIN_CS;  pins.rst = PIN_RST;  pins.dc = PIN_DC;
  pins.mosi = PIN_MOSI;  pins.sclk = PIN_SCLK;  pins.miso = PIN_MISO;
  pins.backlight = PIN_BL;
  display.setWiring(pins);

  if (!display.begin(true)) {
    Serial.println("Display did not start. Check the ribbon cable and DC.");
    return;
  }
  display.printInfo();
  if (display.vcom() < 0) {
    Serial.println("This panel's controller has no adjustable VCOM (ST7796 and ST7789 only).");
    return;
  }
  if (!display.hasCanvas())
    Serial.println("No framebuffer - drawing will be slow. Enable Tools > PSRAM.");

  firstVcom = display.vcom();
  headerH = display.width() >= 400 ? 24 : 14;
  footerH = 14;
  drawPatterns();
  drawLabels();
  Serial.println("+ / - to step, a number (0-63) to jump, s to print the line to paste");
  printPasteLine();
}

void loop() {
  if (display.vcom() < 0) return;

  // BOOT: a short press steps up, holding it past 600 ms steps down once.
  static uint32_t downAt = 0;
  static bool held = false;
  if (PIN_KEY >= 0) {
    const bool down = digitalRead(PIN_KEY) == LOW;
    if (down && !downAt) { downAt = millis(); held = false; }
    if (down && downAt && !held && millis() - downAt > 600) { held = true; set(display.vcom() - 1); }
    if (!down && downAt) {
      if (!held && millis() - downAt > 30) set(display.vcom() + 1);   // 30 ms: debounce
      downAt = 0;
    }
  }

  if (Serial.available()) {
    const int c = Serial.peek();
    if (isdigit(c)) {
      set(Serial.parseInt());
    } else {
      Serial.read();
      if (c == '+' || c == '=') set(display.vcom() + 1);
      else if (c == '-') set(display.vcom() - 1);
      else if (c == 's' || c == 'S') printPasteLine();
    }
  }
}
