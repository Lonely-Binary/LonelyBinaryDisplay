// GENERATED FROM panels.yaml BY tools/gen_panels.py — DO NOT EDIT
#pragma once

#include <stdint.h>

// Every panel in the range shares one 15-pin FPC and one breakout, so the
// pins depend on the MCU only — never on the panel. Written once here
// instead of once per example sketch.

struct LB_Wiring {
  int8_t  cs, rst, dc, mosi, sclk, backlight, miso;
  uint8_t spiHost;
  bool    psram;
};

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define LB_WIRING_NAME "ESP32-S3"
  static const LB_Wiring LB_WIRING = { 10, 42, 2, 11, 12, 41, -1, HSPI, true };
#else
  #define LB_WIRING_NAME "ESP32"
  // GPIO 41/42 do not exist on a classic ESP32 and 6-11 are the flash bus,
  // so the S3 pins above cannot be reused. These are the standard VSPI pins.
  static const LB_Wiring LB_WIRING = { 15, 4, 2, 23, 18, 32, -1, VSPI, false };
#endif
