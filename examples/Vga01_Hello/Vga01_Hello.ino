/*
  Vga01_Hello - the smallest VGA sketch: a screen, some text.

  Start here. The Vga examples go from this to a game and an LVGL dashboard,
  one idea each, in order.

  YOU WILL LEARN
    - the four lines every sketch on this board starts with
    - that the screen is 800 x 600, and that (0, 0) is the top left
    - that you must call display.flush() to make what you drew appear

  HARDWARE
    Lonely Binary ESP32-S3 VGA board, a VGA monitor and a USB cable.

  BOARD SETTINGS (Tools menu)
    Board            ESP32S3 Dev Module
    PSRAM            OPI PSRAM        <- the picture is scanned out of PSRAM
    USB CDC On Boot  Disabled         <- Serial goes to the UART chip
    Partition Scheme Huge APP         <- only the larger examples need it

  Keep Wi-Fi and Bluetooth off while the picture matters: the radio and the
  screen share the PSRAM bus, and the picture shakes while the radio is on.
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

void setup() {
  Serial.begin(115200);

  LB_WiringRgb pins = display.wiringRgb();   // the board's default, then yours
  pins.hsync = PIN_HSYNC;  pins.vsync = PIN_VSYNC;
  const int8_t dataPins[16] = {PIN_B0, PIN_B1, PIN_B2, PIN_B3, PIN_B4,
                              PIN_G0, PIN_G1, PIN_G2, PIN_G3, PIN_G4, PIN_G5,
                              PIN_R0, PIN_R1, PIN_R2, PIN_R3, PIN_R4};
  for (int i = 0; i < 16; i++) pins.data[i] = dataPins[i];
  display.setWiring(pins);

  if (!display.begin()) {                       // starts the VGA signal
    Serial.println("VGA did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }

  display.fillScreen(LB_BLACK);                 // paint the whole screen

  display.setTextColor(LB_WHITE);
  display.setTextSize(8);                       // the 8x16 font, 8 times bigger
  const char *text = "Hello, VGA!";
  int x = (display.width() - display.textWidth(text)) / 2;
  int y = (display.height() - display.fontHeight()) / 2;
  display.drawString(text, x, y);

  display.setTextSize(2);
  display.setTextColor(LB_CYAN);
  display.drawString("800 x 600 from an ESP32-S3", 20, 20);

  display.flush();                              // make it appear
}

void loop() {}
