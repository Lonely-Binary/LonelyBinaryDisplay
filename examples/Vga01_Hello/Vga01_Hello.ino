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

LB_Display display(LB_VGA_800X600);

void setup() {
  Serial.begin(115200);

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
