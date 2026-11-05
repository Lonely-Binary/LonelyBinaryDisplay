/*
  Hdmi01_Hello - the smallest HDMI sketch: a screen, some text.

  Start here. The Hdmi examples go from this to sound, photos and an LVGL
  dashboard, one idea each, in order.

  YOU WILL LEARN
    - the lines every sketch on this board starts with
    - that the screen is 800 x 600 unless you ask for another size
      (Hdmi03_Resolutions), and that (0, 0) is the top left
    - that you must call display.flush() to make what you drew appear

  HARDWARE
    Lonely Binary ESP32-S3 RGB to HDMI board with its ESP32-S3 (N16R8),
    an HDMI monitor or TV, and a USB cable.

  BOARD SETTINGS (Tools menu)
    Board            ESP32S3 Dev Module
    PSRAM            OPI PSRAM        <- the picture is scanned out of PSRAM
    USB CDC On Boot  Enabled          <- Serial goes to the native USB port
    Partition Scheme Huge APP         <- only the larger examples need it

  The board has no screen of its own: the ESP32-S3 sends every pixel, 50 times
  a second, to the SiI9022A chip on the board, which turns it into HDMI.
  begin() sets that chip up. Unplugging and plugging the HDMI cable back in is
  fine - the library notices and sets the chip up again by itself.
*/
#include <LonelyBinaryDisplay.h>

// ---- GPIO wiring. Every pin this sketch uses is here - change the numbers to match
// ---- your board; nothing below refers to a GPIO by number. -1 = not connected.
#define PIN_DE     45   // to the SiI9022A
#define PIN_PCLK   12
#define PIN_HSYNC  38
#define PIN_VSYNC  39
#define PIN_B0     48   // blue  - the 5 top bits of the SiI9022A's blue input
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
  if (!display.begin()) {                       // starts the picture and the HDMI chip
    Serial.println("HDMI did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }

  display.fillScreen(LB_BLACK);                 // paint the whole screen

  display.setTextColor(LB_WHITE);
  display.setTextSize(6);                       // the 8x16 font, 6 times bigger
  const char *text = "Hello, HDMI!";
  int x = (display.width() - display.textWidth(text)) / 2;
  int y = (display.height() - display.fontHeight()) / 2;
  display.drawString(text, x, y);

  display.setTextSize(2);
  display.setTextColor(LB_CYAN);
  char line[48];
  snprintf(line, sizeof line, "%d x %d from an ESP32-S3", display.width(), display.height());
  display.drawString(line, 20, 20);

  display.flush();                              // make it appear

  display.printInfo();                          // what the library set up, on Serial
}

void loop() {
  // The HDMI hot-plug line, for the curious: is a monitor plugged in and on?
  static bool was = !display.hdmiConnected();
  if (display.hdmiConnected() != was) {
    was = display.hdmiConnected();
    Serial.println(was ? "monitor connected" : "no monitor");
  }
  delay(200);
}
