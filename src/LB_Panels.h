// GENERATED FROM panels.yaml BY tools/gen_panels.py — DO NOT EDIT
#pragma once

#include <stdint.h>

enum LB_Driver : uint8_t {
  LB_DRV_ST7735,
  LB_DRV_ST7789,
  LB_DRV_ST7796,
  LB_DRV_NV3007,
  LB_DRV_ILI9341,
  LB_DRV_ILI9488,
  LB_DRV_RGB,
};

// Panels whose controller needs a vendor init table that differs from the
// driver's built-in default (same silicon, different voltage/gamma).
enum LB_InitOps : uint8_t { LB_INIT_NONE, LB_INIT_NV3007_279 };

// Which wiring set the panel uses: LB_WIRING (SPI) or LB_WIRING_PAR8.
enum LB_Bus : uint8_t { LB_BUS_SPI, LB_BUS_PAR8, LB_BUS_RGB };

// A board whose display is 16-bit parallel RGB: timings and every pin,
// fixed by the PCB. Data pins in LCD_CAM order: B0-B4, G0-G5, R0-R4.
struct LB_RgbBoard {
  uint32_t pclkHz;
  uint16_t hsPulse, hsBack, hsFront, vsPulse, vsBack, vsFront;
  bool     pclkActiveNeg;
  int8_t   de, vsync, hsync, pclk, backlight;
  int8_t   data[16];
  int8_t   touchSda, touchScl, touchInt, touchRst;      // I2C touch
  int8_t   touchSck, touchMiso, touchMosi, touchCs;     // SPI touch
};

enum LB_TouchCtl : uint8_t {
  LB_TOUCH_NONE,
  LB_TOUCH_GT911,
  LB_TOUCH_XPT2046,
};

struct LB_PanelDef {
  const char      *id;
  const char      *name;
  LB_Driver        driver;
  uint16_t         width;       // native size at `rotation`
  uint16_t         height;
  uint8_t          rotation;    // the orientation the panel ships in
  bool             bgr;
  bool             invert;
  bool             flipX;
  bool             flipY;
  int16_t          colOff1;     // applied at rotation 0 / 2
  int16_t          rowOff1;
  int16_t          colOff2;     // applied at rotation 1 / 3
  int16_t          rowOff2;
  int32_t          spiHz;
  bool             blActiveLow; // LOW turns the backlight ON
  LB_InitOps       initOps;
  // Appended, so older aggregate initialisers still compile (as SPI, no touch).
  LB_Bus           bus;
  LB_TouchCtl      touch;
  bool             touchSwapXY; // touch axes relative to display rotation 0
  bool             touchFlipX;
  bool             touchFlipY;
  const LB_RgbBoard *rgb;       // LB_BUS_RGB only, else nullptr
  int16_t          touchRaw[4]; // resistive: raw x at left, right; y at top, bottom
};

static const LB_RgbBoard LB_RGB_SUNTON_4827S043R = { 8000000, 4, 43, 8, 4, 12, 8, true, 40, 41, 39, 42, 2, {8, 3, 46, 9, 1, 5, 6, 7, 15, 16, 4, 45, 48, 47, 21, 14}, -1, -1, 18, -1, 12, 13, 11, 38 };
static const LB_RgbBoard LB_RGB_SUNTON_8048S070C = { 12500000, 30, 16, 210, 13, 10, 22, true, 41, 40, 39, 42, 2, {15, 7, 6, 5, 4, 9, 46, 3, 8, 16, 1, 14, 21, 47, 48, 45}, 19, 20, -1, 38, -1, -1, -1, -1 };

static const LB_PanelDef LB_PANELS[] = {
  { "tft_096", "0.96 inch", LB_DRV_ST7735, 80, 160, 0, true, false, false, false, 24, 0, 24, 0, 20000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
  { "tft_18", "1.8 inch", LB_DRV_ST7735, 128, 160, 0, false, true, true, true, 0, 0, 0, 0, 20000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
  { "tft_20", "2.0 inch", LB_DRV_ST7789, 240, 320, 0, false, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
  { "tft_24", "2.4 inch", LB_DRV_ST7789, 240, 320, 0, false, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
  { "tft_28", "2.8 inch", LB_DRV_ST7789, 240, 320, 0, false, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
  { "tft_35", "3.5 inch", LB_DRV_ST7796, 320, 480, 0, true, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
  { "narrow_114", "1.14 inch", LB_DRV_ST7789, 135, 240, 1, false, true, false, false, 52, 40, 53, 40, 8000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
  { "narrow_168", "1.68 inch", LB_DRV_NV3007, 142, 428, 1, false, false, false, false, 12, 0, 14, 0, 8000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
  { "narrow_19", "1.9 inch", LB_DRV_ST7789, 170, 320, 1, false, true, false, false, 35, 0, 35, 0, 8000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
  { "narrow_225", "2.25 inch", LB_DRV_ST7789, 76, 284, 1, false, false, false, false, 82, 18, 82, 18, 8000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
  { "narrow_279", "2.79 inch", LB_DRV_NV3007, 142, 428, 1, false, false, false, false, 12, 0, 14, 0, 20000000, true, LB_INIT_NV3007_279, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
  { "square_392_ctp", "3.92 inch square, capacitive touch", LB_DRV_ILI9488, 320, 320, 0, true, true, false, false, 0, 0, 0, 160, 0, false, LB_INIT_NONE, LB_BUS_PAR8, LB_TOUCH_GT911, false, false, false, nullptr, {0, 0, 0, 0} },
  { "sunton_4827s043r", "Sunton ESP32-4827S043R", LB_DRV_RGB, 480, 272, 0, false, false, false, false, 0, 0, 0, 0, 0, false, LB_INIT_NONE, LB_BUS_RGB, LB_TOUCH_XPT2046, false, false, false, &LB_RGB_SUNTON_4827S043R, {188, 3951, 254, 3791} },
  { "sunton_8048s070c", "Sunton ESP32-8048S070C", LB_DRV_RGB, 800, 480, 0, false, false, false, false, 0, 0, 0, 0, 0, false, LB_INIT_NONE, LB_BUS_RGB, LB_TOUCH_GT911, false, false, false, &LB_RGB_SUNTON_8048S070C, {0, 0, 0, 0} },
  { "ili9341_240x320", "ILI9341 240x320", LB_DRV_ILI9341, 240, 320, 0, true, false, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0} },
};

#define LB_PANEL_COUNT 15

// Pass one of these to LB_Display. Changing this constant is the only
// edit needed to swap panels — everything else is inherited.

// ── Lonely Binary panels ─────────────────────────────────────
#define LB_TFT_096          (&LB_PANELS[0])   // 0.96 inch 80x160 ST7735
#define LB_TFT_18           (&LB_PANELS[1])   // 1.8 inch 128x160 ST7735
#define LB_TFT_20           (&LB_PANELS[2])   // 2.0 inch 240x320 ST7789
#define LB_TFT_24           (&LB_PANELS[3])   // 2.4 inch 240x320 ST7789
#define LB_TFT_28           (&LB_PANELS[4])   // 2.8 inch 240x320 ST7789
#define LB_TFT_35           (&LB_PANELS[5])   // 3.5 inch 320x480 ST7796
#define LB_NARROW_114       (&LB_PANELS[6])   // 1.14 inch 135x240 ST7789
#define LB_NARROW_168       (&LB_PANELS[7])   // 1.68 inch 142x428 NV3007
#define LB_NARROW_19        (&LB_PANELS[8])   // 1.9 inch 170x320 ST7789
#define LB_NARROW_225       (&LB_PANELS[9])   // 2.25 inch 76x284 ST7789
#define LB_NARROW_279       (&LB_PANELS[10])   // 2.79 inch 142x428 NV3007
#define LB_SQUARE_392_CTP   (&LB_PANELS[11])   // 3.92 inch square, capacitive touch 320x320 ILI9488

// ── Panels we do not sell, but can drive ─────────────────────
// Named by controller, not by size, so they cannot be confused with
// the products above. If you bought a display from us it is up there.
#define LB_SUNTON_4827S043R (&LB_PANELS[12])   // RGB 480x272
#define LB_SUNTON_8048S070C (&LB_PANELS[13])   // RGB 800x480
#define LB_ILI9341_240X320  (&LB_PANELS[14])   // ILI9341 240x320
