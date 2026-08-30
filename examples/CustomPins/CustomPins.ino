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

LB_Display display(LB_TFT_24);

void setup() {
  Serial.begin(115200);
  delay(500);

  // Start from the wiring for the board selected in Tools > Board...
  LB_Wiring pins = LB_WIRING;

  // ...then change only the pins that are different on your hardware.
  pins.cs   = 5;
  pins.dc   = 16;
  pins.rst  = 17;
  pins.mosi = 23;
  pins.sclk = 18;

  // Backlight options:
  //   a GPIO   -> dimmable with display.backlight(0..255)
  //   -1       -> no backlight pin (hard-wired on, or you drive it yourself);
  //               backlight() then does nothing instead of poking a stray pin
  pins.backlight = 4;

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
