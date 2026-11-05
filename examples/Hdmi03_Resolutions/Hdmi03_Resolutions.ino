/*
  Hdmi03_Resolutions - pick the screen size: setResolution(), and BOOT to try
  each one on your monitor.

  YOU WILL LEARN
    - display.setResolution(width, height) before begin() picks the size
    - display.resolutionCount() / resolution(i, ...) list what the board takes
    - width() and height() follow the size you picked, so a sketch that lays
      out with them (every example here does) works at any of them
    - refreshHz() and pixelClockHz() tell you the timing behind it

  WHAT IT SHOWS
    A frame one pixel in from every edge, corner marks, a grid and the size in
    big letters. Press BOOT for the next resolution: the board restarts into
    it (the screen is black for a second or two while the monitor locks on).

  HOW TO READ IT
    - All four sides of the white frame visible: the monitor shows the whole
      picture. A side missing: the monitor (usually a TV) zooms in a little -
      look for "Just Scan", "1:1" or "Screen Fit" in its picture menu.
    - "No signal" at one size but not the others: that monitor does not take
      that timing. Every size here was tested on the board, but not every
      monitor takes every size; 800 x 600 is the default and 640 x 480 the safest.

  WHY A RESTART
    The size is fixed when the picture starts, so changing it means starting
    again. The choice is kept in RTC memory, which survives a restart but not
    a power cycle: unplug the board and it comes back at the default size.

  BOARD SETTINGS: as Hdmi01_Hello.
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
#define PIN_BOOT    0   // the BOOT button

LB_Display display(LB_HDMI_RGB);

// Survives ESP.restart(), not a power cycle. The magic number tells a value we
// wrote from the noise RTC memory holds after power-up.
RTC_NOINIT_ATTR static uint32_t savedMagic, savedIndex;
static const uint32_t MAGIC = 0x52455331;   // "RES1"

static void drawTestCard(uint8_t index) {
  const int W = display.width(), H = display.height();
  display.fillScreen(LB_RGB(10, 14, 22));

  // A grid every 40 pixels, and the centre lines.
  for (int x = 40; x < W; x += 40) display.drawFastVLine(x, 0, H, LB_RGB(30, 40, 60));
  for (int y = 40; y < H; y += 40) display.drawFastHLine(0, y, W, LB_RGB(30, 40, 60));
  display.drawFastVLine(W / 2, 0, H, LB_RGB(70, 90, 120));
  display.drawFastHLine(0, H / 2, W, LB_RGB(70, 90, 120));

  // The outermost pixels: if a side of this frame is missing, the monitor cut it off.
  display.drawRect(0, 0, W, H, LB_WHITE);
  display.drawRect(4, 4, W - 8, H - 8, LB_RGB(20, 137, 143));
  const int c = 30;   // corner marks
  display.fillRect(0, 0, c, 6, LB_RED);         display.fillRect(0, 0, 6, c, LB_RED);
  display.fillRect(W - c, 0, c, 6, LB_GREEN);   display.fillRect(W - 6, 0, 6, c, LB_GREEN);
  display.fillRect(0, H - 6, c, 6, LB_BLUE);    display.fillRect(0, H - c, 6, c, LB_BLUE);
  display.fillRect(W - c, H - 6, c, 6, LB_YELLOW); display.fillRect(W - 6, H - c, 6, c, LB_YELLOW);

  // The size, big, and the timing behind it.
  char line[48];
  display.setTextColor(LB_WHITE, LB_RGB(10, 14, 22));
  display.setTextSize(W >= 1000 ? 7 : 5);
  snprintf(line, sizeof line, " %d x %d ", W, H);
  display.drawString(line, (W - display.textWidth(line)) / 2, H / 2 - display.fontHeight() - 8);

  display.setTextSize(2);
  display.setTextColor(LB_CYAN, LB_RGB(10, 14, 22));
  snprintf(line, sizeof line, " %.1f Hz   pixel clock %.2f MHz ", display.refreshHz(), display.pixelClockHz() / 1e6);
  display.drawString(line, (W - display.textWidth(line)) / 2, H / 2 + 16);

  display.setTextColor(LB_LIGHTGRAY, LB_RGB(10, 14, 22));
  snprintf(line, sizeof line, " resolution %u of %u - press BOOT for the next ", index + 1, display.resolutionCount());
  display.drawString(line, (W - display.textWidth(line)) / 2, H / 2 + 56);

  display.flush();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT, INPUT_PULLUP);

  LB_WiringRgb pins = display.wiringRgb();   // the board's default, then yours
  pins.de = PIN_DE;        pins.pclk = PIN_PCLK;
  pins.hsync = PIN_HSYNC;  pins.vsync = PIN_VSYNC;
  pins.i2cSda = PIN_SDA;   pins.i2cScl = PIN_SCL;
  const int8_t dataPins[16] = {PIN_B0, PIN_B1, PIN_B2, PIN_B3, PIN_B4,
                              PIN_G0, PIN_G1, PIN_G2, PIN_G3, PIN_G4, PIN_G5,
                              PIN_R0, PIN_R1, PIN_R2, PIN_R3, PIN_R4};
  for (int i = 0; i < 16; i++) pins.data[i] = dataPins[i];
  display.setWiring(pins);

  // Which size this time: the one saved before the restart, or the default (0).
  uint8_t index = 0;
  if (savedMagic == MAGIC && savedIndex < display.resolutionCount()) index = savedIndex;
  uint16_t w, h;
  float hz;
  display.resolution(index, &w, &h, &hz);
  display.setResolution(w, h, (uint8_t)(hz + 0.5f));   // must come before begin(); the rate tells 800 x 600 @ 50 from @ 56
  display.printResolutions();                   // the full list, on Serial

  if (!display.begin()) {
    Serial.println("HDMI did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  Serial.printf("now at %d x %d, %.1f Hz\n", display.width(), display.height(), display.refreshHz());
  drawTestCard(index);
}

void loop() {
  // Act when BOOT is RELEASED: GPIO0 is a boot-mode pin, and holding it down
  // through a reset would start the chip's downloader instead of this sketch.
  static bool down = false;
  if (digitalRead(PIN_BOOT) == LOW) {
    down = true;
  } else if (down) {
    down = false;
    uint8_t next = (savedMagic == MAGIC ? savedIndex + 1 : 1) % display.resolutionCount();
    savedMagic = MAGIC;
    savedIndex = next;
    Serial.printf("switching to resolution %u...\n", next + 1);
    Serial.flush();
    ESP.restart();
  }
  delay(20);
}
