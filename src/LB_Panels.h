// GENERATED FROM panels.yaml BY tools/gen_panels.py — DO NOT EDIT
#pragma once

#include <stdint.h>

enum LB_Driver : uint8_t {
  LB_DRV_ST7735,
  LB_DRV_ST7789,
  LB_DRV_ST7796,
  LB_DRV_NV3007,
};

// Panels whose controller needs a vendor init table that differs from the
// driver's built-in default (same silicon, different voltage/gamma).
enum LB_InitOps : uint8_t { LB_INIT_NONE, LB_INIT_NV3007_279 };

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
};

static const LB_PanelDef LB_PANELS[] = {
  { "tft_096", "0.96 inch", LB_DRV_ST7735, 80, 160, 0, true, false, false, false, 24, 0, 24, 0, 20000000, true, LB_INIT_NONE },
  { "tft_18", "1.8 inch", LB_DRV_ST7735, 128, 160, 0, false, false, true, true, 0, 0, 0, 0, 20000000, false, LB_INIT_NONE },
  { "tft_20", "2.0 inch", LB_DRV_ST7789, 240, 320, 0, false, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE },
  { "tft_24", "2.4 inch", LB_DRV_ST7789, 240, 320, 0, false, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE },
  { "tft_28", "2.8 inch", LB_DRV_ST7789, 240, 320, 0, false, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE },
  { "tft_35", "3.5 inch", LB_DRV_ST7796, 320, 480, 0, true, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE },
  { "narrow_114", "1.14 inch", LB_DRV_ST7789, 135, 240, 1, false, true, false, false, 52, 40, 53, 40, 8000000, true, LB_INIT_NONE },
  { "narrow_168", "1.68 inch", LB_DRV_NV3007, 142, 428, 1, false, false, false, false, 12, 0, 14, 0, 8000000, true, LB_INIT_NONE },
  { "narrow_19", "1.9 inch", LB_DRV_ST7789, 170, 320, 1, false, true, false, false, 35, 0, 35, 0, 8000000, true, LB_INIT_NONE },
  { "narrow_225", "2.25 inch", LB_DRV_ST7789, 76, 284, 1, false, false, false, false, 82, 18, 82, 18, 8000000, true, LB_INIT_NONE },
  { "narrow_279", "2.79 inch", LB_DRV_NV3007, 142, 428, 1, false, false, false, false, 12, 0, 14, 0, 20000000, true, LB_INIT_NV3007_279 },
};

#define LB_PANEL_COUNT 11

// Pass one of these to LB_Display. Changing this constant is the only
// edit needed to swap panels — everything else is inherited.
#define LB_TFT_096    (&LB_PANELS[0])   // 0.96 inch 80x160 ST7735
#define LB_TFT_18     (&LB_PANELS[1])   // 1.8 inch 128x160 ST7735
#define LB_TFT_20     (&LB_PANELS[2])   // 2.0 inch 240x320 ST7789
#define LB_TFT_24     (&LB_PANELS[3])   // 2.4 inch 240x320 ST7789
#define LB_TFT_28     (&LB_PANELS[4])   // 2.8 inch 240x320 ST7789
#define LB_TFT_35     (&LB_PANELS[5])   // 3.5 inch 320x480 ST7796
#define LB_NARROW_114 (&LB_PANELS[6])   // 1.14 inch 135x240 ST7789
#define LB_NARROW_168 (&LB_PANELS[7])   // 1.68 inch 142x428 NV3007
#define LB_NARROW_19  (&LB_PANELS[8])   // 1.9 inch 170x320 ST7789
#define LB_NARROW_225 (&LB_PANELS[9])   // 2.25 inch 76x284 ST7789
#define LB_NARROW_279 (&LB_PANELS[10])   // 2.79 inch 142x428 NV3007
