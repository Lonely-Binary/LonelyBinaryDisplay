/*
  CustomPins — using a Lonely Binary panel on YOUR wiring.

  By default this library uses the pins of the Lonely Binary breakout for the
  board you selected in Tools > Board. If the display is on your own PCB, or on
  a dev board where those GPIOs are already taken, override them here.

  THE RULE: start from LB_WIRING and change only what differs.

  Do not build an LB_Wiring from scratch. The `spiHost` field is a bus number
  whose meaning is not the same on every chip — VSPI does not even exist as a
  name on the ESP32-S3 — so copying the default gives you a bus that is already
  correct for the MCU you are compiling for.

  The panel constant does not change: driver IC, resolution, offsets, colour
  order and backlight polarity are properties of the SCREEN, not of the wiring.
*/
#include <LonelyBinaryDisplay.h>

// ---- GPIO wiring. Every pin this sketch uses is here - change the numbers to match
// ---- your board; nothing below refers to a GPIO by number. -1 = not connected.
#define PIN_CS    5
#define PIN_DC    16
#define PIN_RST   17
#define PIN_MOSI  23
#define PIN_SCLK  18
#define PIN_BL    4    // backlight: a GPIO = dimmable, -1 = none (hard-wired on)

LB_Display display(LB_TFT_24);
//LB_Display display(LB_TFT_096);
//LB_Display display(LB_TFT_18);
//LB_Display display(LB_TFT_20);
//LB_Display display(LB_TFT_28);
//LB_Display display(LB_TFT_35);

void setup() {
  Serial.begin(115200);
  delay(500);

  // Start from the wiring for the board selected in Tools > Board...
  LB_Wiring pins = LB_WIRING;

  // ...then change only the pins that are different on your hardware.
  pins.cs   = PIN_CS;
  pins.dc   = PIN_DC;
  pins.rst  = PIN_RST;
  pins.mosi = PIN_MOSI;
  pins.sclk = PIN_SCLK;

  // Backlight options:
  //   a GPIO   -> dimmable with display.backlight(0..255)
  //   -1       -> no backlight pin (hard-wired on, or you drive it yourself);
  //               backlight() then does nothing instead of poking a stray pin
  pins.backlight = PIN_BL;

  // Both of these must come before begin().
  display.setWiring(pins);

  // Optional: long ribbon extensions or hand-wired jumpers may not survive the
  // panel's rated clock. Symptoms are a scrambled or half-drawn image — drop
  // the rate until it is stable. Pass 0 to go back to the panel default.
  // display.setSpiHz(20000000);

  if (!display.begin()) {
    Serial.println("Display did not start. Check DC and RESET first — those");
    Serial.println("two are the usual culprits when the pins are custom.");
    return;
  }

  // printInfo() reports the pins actually in use and marks them as custom,
  // which makes a wiring mistake obvious in the serial log.
  display.printInfo();
  display.selfTest();
}

void loop() {
  delay(1000);
}
