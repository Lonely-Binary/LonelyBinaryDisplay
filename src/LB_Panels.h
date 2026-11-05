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
  LB_DRV_ILI9327,
  LB_DRV_ILI9486,
  LB_DRV_ST7305,
  LB_DRV_ILI9342,
  LB_DRV_GC9A01,
  LB_DRV_GC9306,
  LB_DRV_GC9307,
  LB_DRV_NV3041A,
};

// Panels whose controller needs a vendor init table that differs from the
// driver's built-in default (same silicon, different voltage/gamma).
enum LB_InitOps : uint8_t { LB_INIT_NONE, LB_INIT_NV3007_279, LB_INIT_ST7305_213, LB_INIT_ST7305_290, LB_INIT_ST7305_420 };

// Which wiring set the panel uses: LB_WIRING (SPI) or LB_WIRING_PAR8.
enum LB_Bus : uint8_t { LB_BUS_SPI, LB_BUS_PAR8, LB_BUS_RGB };

// One resolution of an RGB board: the size and the timing that produces it.
// clkSrc: 0 = the LCD peripheral's default clock, 1 = PLL160M, 2 = PLL240M
// (an integer divide of one of these has no cycle-to-cycle jitter).
struct LB_RgbMode {
  uint16_t width, height;
  uint32_t pclkHz;
  uint16_t hsPulse, hsBack, hsFront, vsPulse, vsBack, vsFront;
  bool     syncIdleHigh;   // negative sync pulses
  uint8_t  clkSrc;
};

// Transmitter chips an RGB board may carry, set up over I2C by begin().
enum LB_HdmiTx : uint8_t { LB_HDMI_TX_NONE, LB_HDMI_TX_SII9022 };

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
  // Appended, so older initialisers still compile (as false, false).
  bool     syncIdleHigh;   // sync pulses are active LOW (idle high); false = idle low, as a DE-mode panel is run
  bool     directScan;     // scanned straight out of ONE PSRAM frame buffer: no bounce buffers, no second buffer
  // Appended in 0.2.0 (older initialisers get the defaults: one resolution, no transmitter).
  uint8_t  clkSrc;                  // as LB_RgbMode::clkSrc, for the default timing above
  const LB_RgbMode *modes;          // every resolution setResolution() accepts, the default first; nullptr = only the default
  uint8_t  modeCount;
  LB_HdmiTx hdmiTx;                 // a transmitter to set up over I2C, or LB_HDMI_TX_NONE
  int8_t   i2cSda, i2cScl;          // its I2C pins
  uint8_t  i2cAddr;                 // its 7-bit address
  int8_t   i2sBclk, i2sWs, i2sDout; // the board's I2S audio input (HDMI sound), -1 = none
};

// A loose 8-bit parallel module: its own pins, -1 = tied off on the module.
struct LB_Par8Board {
  int8_t data[8];   // D0..D7
  int8_t wr, dc, rd, cs, rst, backlight;
  int8_t touchSda, touchScl, touchInt, touchRst;   // I2C touch on the module; -1 = not wired
};

// A loose SPI module: its own pins. The SPI host is not here, it follows the MCU.
struct LB_SpiBoard {
  int8_t cs, rst, dc, mosi, miso, sclk, backlight;
  int8_t touchCs, touchIrq;   // resistive touch sharing the bus; -1 = none
};

enum LB_TouchCtl : uint8_t {
  LB_TOUCH_NONE,
  LB_TOUCH_GT911,
  LB_TOUCH_XPT2046,
  LB_TOUCH_IT7259,
  LB_TOUCH_CST816,
  LB_TOUCH_NS2009,
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
  const LB_Par8Board *par8;     // LB_BUS_PAR8 with its own pins, else nullptr
  const LB_SpiBoard  *spi;      // LB_BUS_SPI with its own pins, else nullptr
  // VCOM, written after the init table (ST7796 VCMPCTL / ST7789 VCOMS).
  // vcomSet false = keep the init table's value; an older initialiser gets that.
  bool             vcomSet;
  uint8_t          vcom;
};

static const LB_RgbMode LB_MODES_VGA_16BIT[] = {
  { 800, 600, 32000000, 72, 128, 24, 2, 22, 2, false, 0 },   // 800x600 @ 49.9 Hz
  { 640, 480, 25000000, 96, 48, 16, 2, 33, 10, true, 0 },   // 640x480 @ 59.5 Hz
};
static const LB_RgbMode LB_MODES_HDMI_RGB[] = {
  { 800, 600, 32000000, 72, 128, 24, 2, 22, 2, false, 1 },   // 800x600 @ 49.9 Hz
  { 640, 480, 27178000, 96, 48, 16, 2, 33, 10, true, 0 },   // 640x480 @ 64.7 Hz
  { 720, 480, 27000000, 62, 60, 16, 6, 30, 9, true, 0 },   // 720x480 @ 59.9 Hz
  { 720, 576, 27000000, 64, 68, 12, 5, 39, 6, true, 0 },   // 720x576 @ 49.9 Hz
  { 800, 600, 36000000, 72, 128, 24, 2, 22, 1, false, 0 },   // 800x600 @ 56.2 Hz
  { 1024, 768, 32000000, 32, 80, 48, 3, 10, 1, false, 1 },   // 1024x768 @ 34.6 Hz
  { 1280, 720, 32000000, 32, 64, 48, 5, 20, 5, false, 1 },   // 1280x720 @ 30.0 Hz
  { 1366, 768, 32000000, 32, 56, 40, 5, 14, 3, false, 1 },   // 1366x768 @ 27.1 Hz
  { 1280, 800, 32000000, 32, 64, 48, 6, 19, 3, false, 1 },   // 1280x800 @ 27.1 Hz
};
static const LB_RgbMode LB_MODES_HDMI_VGA[] = {
  { 720, 576, 26666667, 64, 68, 12, 5, 39, 5, true, 1 },   // 720x576 @ 49.4 Hz
  { 720, 480, 26666667, 62, 60, 16, 6, 30, 9, true, 1 },   // 720x480 @ 59.2 Hz
  { 640, 480, 24000000, 96, 48, 16, 2, 33, 10, true, 2 },   // 640x480 @ 57.1 Hz
};
static const LB_RgbBoard LB_RGB_RGB_430_480X272 = { 9000000, 4, 43, 8, 4, 12, 8, true, 42, -1, -1, 41, 14, {8, 18, 17, 16, 15, 13, 12, 11, 10, 9, 3, 40, 39, 38, 45, 21}, -1, -1, -1, -1, -1, -1, -1, -1, false, false, 0, nullptr, 1, LB_HDMI_TX_NONE, -1, -1, 0x00, -1, -1, -1 };
static const LB_RgbBoard LB_RGB_RGB_700_1024X600 = { 30000000, 10, 160, 20, 2, 10, 5, true, 42, -1, -1, 41, 14, {8, 18, 17, 16, 15, 13, 12, 11, 10, 9, 3, 40, 39, 38, 45, 21}, -1, -1, -1, -1, -1, -1, -1, -1, false, false, 0, nullptr, 1, LB_HDMI_TX_NONE, -1, -1, 0x00, -1, -1, -1 };
static const LB_RgbBoard LB_RGB_RGB_1010_1024X600 = { 30000000, 10, 160, 20, 2, 10, 5, true, 42, -1, -1, 41, 14, {8, 18, 17, 16, 15, 13, 12, 11, 10, 9, 3, 40, 39, 38, 45, 21}, -1, -1, -1, -1, -1, -1, -1, -1, false, false, 0, nullptr, 1, LB_HDMI_TX_NONE, -1, -1, 0x00, -1, -1, -1 };
static const LB_RgbBoard LB_RGB_SUNTON_4827S043R = { 10000000, 4, 43, 8, 4, 12, 8, true, 40, 41, 39, 42, 2, {8, 3, 46, 9, 1, 5, 6, 7, 15, 16, 4, 45, 48, 47, 21, 14}, -1, -1, 18, -1, 12, 13, 11, 38, true, true, 0, nullptr, 1, LB_HDMI_TX_NONE, -1, -1, 0x00, -1, -1, -1 };
static const LB_RgbBoard LB_RGB_SUNTON_8048S070C = { 12500000, 30, 16, 210, 13, 10, 22, true, 41, 40, 39, 42, 2, {15, 7, 6, 5, 4, 9, 46, 3, 8, 16, 1, 14, 21, 47, 48, 45}, 19, 20, -1, 38, -1, -1, -1, -1, false, false, 0, nullptr, 1, LB_HDMI_TX_NONE, -1, -1, 0x00, -1, -1, -1 };
static const LB_RgbBoard LB_RGB_VGA_16BIT = { 32000000, 72, 128, 24, 2, 22, 2, false, -1, 1, 2, -1, -1, {11, 12, 13, 14, 21, 17, 18, 8, 3, 9, 10, 7, 15, 16, 38, 39}, -1, -1, -1, -1, -1, -1, -1, -1, false, false, 0, LB_MODES_VGA_16BIT, 2, LB_HDMI_TX_NONE, -1, -1, 0x00, -1, -1, -1 };
static const LB_RgbBoard LB_RGB_HDMI_RGB = { 32000000, 72, 128, 24, 2, 22, 2, false, 45, 39, 38, 12, -1, {48, 47, 21, 14, 13, 11, 10, 9, 46, 3, 8, 18, 17, 16, 15, 6}, -1, -1, -1, -1, -1, -1, -1, -1, false, false, 1, LB_MODES_HDMI_RGB, 9, LB_HDMI_TX_SII9022, 5, 4, 0x3B, 42, 41, 40 };
static const LB_RgbBoard LB_RGB_HDMI_VGA = { 26666667, 64, 68, 12, 5, 39, 5, false, -1, 5, 4, -1, -1, {17, 16, 15, 7, 6, 10, 9, 46, 3, 8, 18, 38, 39, 40, 41, 42}, -1, -1, -1, -1, -1, -1, -1, -1, true, false, 1, LB_MODES_HDMI_VGA, 3, LB_HDMI_TX_NONE, -1, -1, 0x00, -1, -1, -1 };
static const LB_SpiBoard LB_SPI_ILI9486_480X320 = { 16, -1, 0, 23, 19, 18, 2, 15, 4 };
static const LB_Par8Board LB_PAR8_TAP_196_CTP = { {5, 6, 4, 7, 15, 1, 10, 11}, 2, 42, -1, 41, -1, -1, 13, 12, -1, -1 };
static const LB_Par8Board LB_PAR8_RTP_350 = { {13, 12, 11, 10, 1, 15, 7, 4}, 42, 41, -1, -1, 2, -1, 6, 5, -1, -1 };
static const LB_Par8Board LB_PAR8_ILI9327_400X240 = { {14, 12, 13, 15, 2, 4, 16, 17}, 33, 27, 5, 26, 18, 32, -1, -1, -1, -1 };

static const LB_PanelDef LB_PANELS[] = {
  { "tft_096", "0.96 inch", LB_DRV_ST7735, 80, 160, 0, true, false, false, false, 24, 0, 24, 0, 20000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "tft_18", "1.8 inch", LB_DRV_ST7735, 128, 160, 0, false, true, true, true, 0, 0, 0, 0, 20000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "tft_20", "2.0 inch", LB_DRV_ST7789, 240, 320, 0, false, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "tft_24", "2.4 inch", LB_DRV_ST7789, 240, 320, 0, false, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "tft_28", "2.8 inch", LB_DRV_ST7789, 240, 320, 0, false, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "tft_35", "3.5 inch", LB_DRV_ST7796, 320, 480, 0, true, true, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, true, 0x05 },
  { "narrow_114", "1.14 inch", LB_DRV_ST7789, 135, 240, 1, false, true, false, false, 52, 40, 53, 40, 8000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "narrow_168", "1.68 inch", LB_DRV_NV3007, 142, 428, 1, false, false, false, false, 12, 0, 14, 0, 8000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "narrow_19", "1.9 inch", LB_DRV_ST7789, 170, 320, 1, false, true, false, false, 35, 0, 35, 0, 8000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "narrow_225", "2.25 inch", LB_DRV_ST7789, 76, 284, 1, false, false, false, false, 82, 18, 82, 18, 8000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "narrow_279", "2.79 inch", LB_DRV_NV3007, 142, 428, 1, false, false, false, false, 12, 0, 14, 0, 20000000, true, LB_INIT_NV3007_279, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "square_392_ctp", "3.92 inch square, capacitive touch", LB_DRV_ILI9488, 320, 320, 0, true, true, false, false, 0, 0, 0, 160, 0, false, LB_INIT_NONE, LB_BUS_PAR8, LB_TOUCH_GT911, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "tap_128_ctp", "1.28 inch round, capacitive touch", LB_DRV_GC9A01, 240, 240, 0, true, true, false, false, 0, 0, 0, 0, 20000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_IT7259, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "tap_150_ctp", "1.5 inch, capacitive touch", LB_DRV_GC9306, 240, 320, 0, true, false, false, false, 0, 0, 0, 0, 10000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_CST816, false, false, false, nullptr, {-21, 247, -28, 272}, nullptr, nullptr, false, 0x00 },
  { "tap_196_ctp", "1.96 inch, capacitive touch", LB_DRV_GC9307, 240, 296, 0, true, false, false, false, 0, 0, 0, 24, 0, false, LB_INIT_NONE, LB_BUS_PAR8, LB_TOUCH_CST816, false, false, false, nullptr, {0, 239, 0, 295}, &LB_PAR8_TAP_196_CTP, nullptr, false, 0x00 },
  { "rlcd_213", "2.13 inch reflective LCD", LB_DRV_ST7305, 122, 250, 3, false, false, false, false, 0, 0, 0, 0, 20000000, false, LB_INIT_ST7305_213, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "rlcd_290", "2.9 inch reflective LCD", LB_DRV_ST7305, 168, 384, 1, false, false, false, false, 0, 0, 0, 0, 20000000, false, LB_INIT_ST7305_290, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "rlcd_420", "4.2 inch reflective LCD", LB_DRV_ST7305, 400, 300, 0, false, false, false, false, 0, 0, 0, 0, 20000000, false, LB_INIT_ST7305_420, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "rtp_180", "1.8 inch, resistive touch", LB_DRV_ST7735, 128, 160, 1, false, true, false, false, 2, 1, 2, 1, 20000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NS2009, true, false, false, nullptr, {3824, 217, 185, 3717}, nullptr, nullptr, false, 0x00 },
  { "rtp_240", "2.4 inch, resistive touch", LB_DRV_ST7789, 240, 320, 1, false, false, false, false, 0, 0, 0, 0, 40000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NS2009, false, false, false, nullptr, {189, 3796, 169, 3720}, nullptr, nullptr, false, 0x00 },
  { "rtp_280", "2.8 inch, resistive touch", LB_DRV_ST7789, 240, 320, 1, false, false, false, false, 0, 0, 0, 0, 40000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NS2009, false, false, false, nullptr, {321, 3786, 163, 3284}, nullptr, nullptr, false, 0x00 },
  { "rtp_430", "4.3 inch, resistive touch", LB_DRV_NV3041A, 480, 272, 0, false, false, false, false, 0, 0, 0, 0, 20000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NS2009, false, false, false, nullptr, {150, 3943, 3674, 194}, nullptr, nullptr, false, 0x00 },
  { "rtp_350", "3.5 inch, resistive touch", LB_DRV_ST7796, 320, 480, 1, true, false, false, false, 0, 0, 0, 0, 0, false, LB_INIT_NONE, LB_BUS_PAR8, LB_TOUCH_NS2009, false, false, false, nullptr, {200, 3900, 200, 3900}, &LB_PAR8_RTP_350, nullptr, false, 0x00 },
  { "square_085", "0.85 inch square", LB_DRV_ST7735, 128, 128, 0, true, true, false, false, 2, 3, 2, 1, 20000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "square_133", "1.33 inch square", LB_DRV_ST7789, 240, 240, 0, false, true, false, false, 0, 0, 0, 80, 20000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "square_154", "1.54 inch square", LB_DRV_ST7789, 240, 240, 0, false, true, false, false, 0, 0, 0, 80, 20000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "square_200", "2.0 inch square", LB_DRV_ILI9342, 320, 240, 2, true, true, false, false, 0, 0, 0, 0, 40000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "square_231", "2.31 inch square", LB_DRV_ILI9342, 320, 240, 2, true, false, false, false, 0, 0, 0, 0, 40000000, true, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "square_392", "3.92 inch square", LB_DRV_ST7796, 320, 320, 0, true, true, false, false, 0, 0, 0, 160, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "rgb_430_480x272", "4.3 inch RGB", LB_DRV_RGB, 480, 272, 0, false, false, false, false, 0, 0, 0, 0, 0, false, LB_INIT_NONE, LB_BUS_RGB, LB_TOUCH_NONE, false, false, false, &LB_RGB_RGB_430_480X272, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "rgb_700_1024x600", "7 inch RGB", LB_DRV_RGB, 1024, 600, 0, false, false, false, false, 0, 0, 0, 0, 0, false, LB_INIT_NONE, LB_BUS_RGB, LB_TOUCH_NONE, false, false, false, &LB_RGB_RGB_700_1024X600, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "rgb_1010_1024x600", "10.1 inch RGB", LB_DRV_RGB, 1024, 600, 0, false, false, false, false, 0, 0, 0, 0, 0, false, LB_INIT_NONE, LB_BUS_RGB, LB_TOUCH_NONE, false, false, false, &LB_RGB_RGB_1010_1024X600, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "sunton_4827s043r", "Sunton ESP32-4827S043R", LB_DRV_RGB, 480, 272, 0, false, false, false, false, 0, 0, 0, 0, 0, false, LB_INIT_NONE, LB_BUS_RGB, LB_TOUCH_XPT2046, false, false, false, &LB_RGB_SUNTON_4827S043R, {188, 3951, 254, 3791}, nullptr, nullptr, false, 0x00 },
  { "sunton_8048s070c", "Sunton ESP32-8048S070C", LB_DRV_RGB, 800, 480, 0, false, false, false, false, 0, 0, 0, 0, 0, false, LB_INIT_NONE, LB_BUS_RGB, LB_TOUCH_GT911, false, false, false, &LB_RGB_SUNTON_8048S070C, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "vga_16bit", "Lonely Binary ESP32-S3 VGA 16-bit", LB_DRV_RGB, 800, 600, 0, false, false, false, false, 0, 0, 0, 0, 0, false, LB_INIT_NONE, LB_BUS_RGB, LB_TOUCH_NONE, false, false, false, &LB_RGB_VGA_16BIT, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "hdmi_rgb", "Lonely Binary ESP32-S3 RGB to HDMI", LB_DRV_RGB, 800, 600, 0, false, false, false, false, 0, 0, 0, 0, 0, false, LB_INIT_NONE, LB_BUS_RGB, LB_TOUCH_NONE, false, false, false, &LB_RGB_HDMI_RGB, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "hdmi_vga", "Lonely Binary ESP32-S3 VGA to HDMI", LB_DRV_RGB, 720, 576, 0, false, false, false, false, 0, 0, 0, 0, 0, false, LB_INIT_NONE, LB_BUS_RGB, LB_TOUCH_NONE, false, false, false, &LB_RGB_HDMI_VGA, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "ili9341_240x320", "ILI9341 240x320", LB_DRV_ILI9341, 240, 320, 0, true, false, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "ili9327_400x240", "ILI9327 400x240", LB_DRV_ILI9327, 240, 400, 1, true, false, false, false, 0, 0, 0, 32, 0, false, LB_INIT_NONE, LB_BUS_PAR8, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, &LB_PAR8_ILI9327_400X240, nullptr, false, 0x00 },
  { "ili9486_480x320", "ILI9486 480x320", LB_DRV_ILI9486, 320, 480, 3, true, false, false, false, 0, 0, 0, 0, 20000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, &LB_SPI_ILI9486_480X320, false, 0x00 },
  { "ili9341_320x240", "ILI9341 320x240", LB_DRV_ILI9341, 240, 320, 1, true, false, false, false, 0, 0, 0, 0, 40000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
  { "st7305_400x300", "ST7305 400x300 RLCD", LB_DRV_ST7305, 400, 300, 0, false, true, false, false, 0, 0, 0, 0, 10000000, false, LB_INIT_NONE, LB_BUS_SPI, LB_TOUCH_NONE, false, false, false, nullptr, {0, 0, 0, 0}, nullptr, nullptr, false, 0x00 },
};

#define LB_PANEL_COUNT 42

// Pass one of these to LB_Display. Changing this constant is the only
// edit needed to swap panels — everything else is inherited.

// ── Lonely Binary panels ─────────────────────────────────────
#define LB_TFT_096           (&LB_PANELS[0])   // 0.96 inch 80x160 ST7735
#define LB_TFT_18            (&LB_PANELS[1])   // 1.8 inch 128x160 ST7735
#define LB_TFT_20            (&LB_PANELS[2])   // 2.0 inch 240x320 ST7789
#define LB_TFT_24            (&LB_PANELS[3])   // 2.4 inch 240x320 ST7789
#define LB_TFT_28            (&LB_PANELS[4])   // 2.8 inch 240x320 ST7789
#define LB_TFT_35            (&LB_PANELS[5])   // 3.5 inch 320x480 ST7796
#define LB_NARROW_114        (&LB_PANELS[6])   // 1.14 inch 135x240 ST7789
#define LB_NARROW_168        (&LB_PANELS[7])   // 1.68 inch 142x428 NV3007
#define LB_NARROW_19         (&LB_PANELS[8])   // 1.9 inch 170x320 ST7789
#define LB_NARROW_225        (&LB_PANELS[9])   // 2.25 inch 76x284 ST7789
#define LB_NARROW_279        (&LB_PANELS[10])   // 2.79 inch 142x428 NV3007
#define LB_SQUARE_392_CTP    (&LB_PANELS[11])   // 3.92 inch square, capacitive touch 320x320 ILI9488
#define LB_TAP_128_CTP       (&LB_PANELS[12])   // 1.28 inch round, capacitive touch 240x240 GC9A01
#define LB_TAP_150_CTP       (&LB_PANELS[13])   // 1.5 inch, capacitive touch 240x320 GC9306
#define LB_TAP_196_CTP       (&LB_PANELS[14])   // 1.96 inch, capacitive touch 240x296 GC9307
#define LB_RLCD_213          (&LB_PANELS[15])   // 2.13 inch reflective LCD 122x250 ST7305
#define LB_RLCD_290          (&LB_PANELS[16])   // 2.9 inch reflective LCD 168x384 ST7305
#define LB_RLCD_420          (&LB_PANELS[17])   // 4.2 inch reflective LCD 400x300 ST7305
#define LB_RTP_180           (&LB_PANELS[18])   // 1.8 inch, resistive touch 128x160 ST7735
#define LB_RTP_240           (&LB_PANELS[19])   // 2.4 inch, resistive touch 240x320 ST7789
#define LB_RTP_280           (&LB_PANELS[20])   // 2.8 inch, resistive touch 240x320 ST7789
#define LB_RTP_430           (&LB_PANELS[21])   // 4.3 inch, resistive touch 480x272 NV3041A
#define LB_RTP_350           (&LB_PANELS[22])   // 3.5 inch, resistive touch 320x480 ST7796
#define LB_SQUARE_085        (&LB_PANELS[23])   // 0.85 inch square 128x128 ST7735
#define LB_SQUARE_133        (&LB_PANELS[24])   // 1.33 inch square 240x240 ST7789
#define LB_SQUARE_154        (&LB_PANELS[25])   // 1.54 inch square 240x240 ST7789
#define LB_SQUARE_200        (&LB_PANELS[26])   // 2.0 inch square 320x240 ILI9342
#define LB_SQUARE_231        (&LB_PANELS[27])   // 2.31 inch square 320x240 ILI9342
#define LB_SQUARE_392        (&LB_PANELS[28])   // 3.92 inch square 320x320 ST7796
#define LB_RGB_430_480X272   (&LB_PANELS[29])   // 4.3 inch RGB 480x272 RGB
#define LB_RGB_700_1024X600  (&LB_PANELS[30])   // 7 inch RGB 1024x600 RGB
#define LB_RGB_1010_1024X600 (&LB_PANELS[31])   // 10.1 inch RGB 1024x600 RGB
#define LB_VGA_16BIT         (&LB_PANELS[34])   // Lonely Binary ESP32-S3 VGA 16-bit 800x600 RGB
#define LB_HDMI_RGB          (&LB_PANELS[35])   // Lonely Binary ESP32-S3 RGB to HDMI 800x600 RGB
#define LB_HDMI_VGA          (&LB_PANELS[36])   // Lonely Binary ESP32-S3 VGA to HDMI 720x576 RGB

// ── Panels we do not sell, but can drive ─────────────────────
// Named by controller, not by size, so they cannot be confused with
// the products above. If you bought a display from us it is up there.
#define LB_SUNTON_4827S043R  (&LB_PANELS[32])   // RGB 480x272
#define LB_SUNTON_8048S070C  (&LB_PANELS[33])   // RGB 800x480
#define LB_ILI9341_240X320   (&LB_PANELS[37])   // ILI9341 240x320
#define LB_ILI9327_400X240   (&LB_PANELS[38])   // ILI9327 240x400
#define LB_ILI9486_480X320   (&LB_PANELS[39])   // ILI9486 320x480
#define LB_ILI9341_320X240   (&LB_PANELS[40])   // ILI9341 240x320
#define LB_ST7305_400X300    (&LB_PANELS[41])   // ST7305 400x300
