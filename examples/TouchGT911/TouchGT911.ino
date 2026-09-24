/*
  TouchGT911 — read a GT911 capacitive touch panel.

  Prints every finger that is down, in screen coordinates. No display needed:
  run it first to prove the touch wiring on its own, the same way SelfTest
  proves the display wiring.

  The GT911 knows its own resolution (the panel maker programs it), so there is
  nothing to calibrate. What it cannot know is which way up your screen is:
  call setRotation() with the same value you give the display, and the points
  follow it.

  The pins below are the ones on the Lonely Binary 320 x 320 parallel board
  with a classic ESP32. Change them to match your wiring. INT and RST may be
  -1 if they are not connected.
*/
#include <LB_TouchGT911.h>

LB_TouchGT911 touch(21 /* SDA */, 22 /* SCL */, 18 /* INT */, 19 /* RST */);

void setup() {
  Serial.begin(115200);
  delay(500);

  if (!touch.begin()) {
    Serial.println("GT911 not found - check SDA/SCL, and that the panel has power.");
    while (true) delay(1000);
  }
  Serial.printf("GT911 \"%s\" at 0x%02X, %d x %d\n", touch.productId(), touch.address(),
                touch.nativeWidth(), touch.nativeHeight());

  touch.setRotation(0);   // keep in step with display.setRotation()
}

void loop() {
  LB_TouchPoint p[LB_Touch::MAX_POINTS];
  static uint8_t was = 0;
  const uint8_t n = touch.read(p, LB_Touch::MAX_POINTS);
  for (uint8_t i = 0; i < n; i++)
    Serial.printf("finger %u: %d, %d%s", p[i].id, p[i].x, p[i].y, i + 1 < n ? "   " : "\n");
  if (was && !n) Serial.println("released");
  was = n;
  delay(20);
}
