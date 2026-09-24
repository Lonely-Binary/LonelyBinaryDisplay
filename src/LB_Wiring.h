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

// The square series is 8-bit parallel with I2C touch, on a breakout of its
// own. Picked by the panel's `bus`, so a sketch never chooses between these.
// CS is tied low and RD high on that board.
struct LB_WiringPar8 {
  int8_t data[8];   // D0..D7
  int8_t wr, dc, rst, backlight;
  int8_t touchSda, touchScl, touchInt, touchRst;
};

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  static const LB_WiringPar8 LB_WIRING_PAR8 = { {4, 5, 6, 7, 15, 16, 17, 18}, 8, 9, 42, 41, 1, 2, 40, 39 };
#else
  static const LB_WiringPar8 LB_WIRING_PAR8 = { {5, 17, 16, 15, 13, 26, 14, 27}, 4, 23, 33, 32, 21, 22, 18, 19 };
#endif
