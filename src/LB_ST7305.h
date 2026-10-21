#pragma once
/*
 * LB_ST7305 - the panel driver for the ST7305 reflective monochrome LCD.
 *
 * It is an LCD, not e-paper: a frame goes out in a few milliseconds, nothing
 * ghosts, there is no BUSY pin, and the picture is gone when the power is. It
 * has no backlight - ambient light is the light.
 *
 * What makes it different from the controllers in LB_TFT is the pixel format.
 * The panel takes 1 bit per pixel, but not row by row: the frame is a run of
 * bytes, each holding a block of 2 columns x 4 rows, laid out column-pair
 * first. See flushFramebuffer() for the exact mapping.
 *
 * Drawing is done in an RGB565 canvas like every other Lonely Binary panel,
 * so a sketch written for a colour TFT runs unchanged; flush() thresholds the
 * canvas to black and white (luma >= 128 is white) and packs it. The canvas
 * lives in PSRAM (400 x 300 x 2 = 240 KB), the packed frame in internal RAM
 * (15 KB).
 *
 * Init values and the pixel layout were read from Waveshare's reference driver
 * for their ESP32-S3-RLCD-4.2 (via the Apache-2.0 ESPHome port by kylehase);
 * the code here is written for this library, not copied. UNVERIFIED on a
 * Lonely Binary bench until the first panel is run - see panels.yaml.
 */
#include "LB_TFT.h"

class LB_ST7305 : public LB_Screen
{
public:
  LB_ST7305(LB_TFTBus *bus, const LB_PanelDef *panel, int8_t rst)
      : _bus(bus), _panel(panel), _rst(rst) {}
  ~LB_ST7305() override;

  bool begin(int32_t hz, bool bgr, bool invert) override;
  void setRotation(uint8_t r) override; /* 0-3, done in software while packing */
  void setColorOrder(bool) override {}  /* monochrome: nothing to order */
  void setInverted(bool invert) override;

  int16_t width() const override { return _w; }
  int16_t height() const override { return _h; }
  uint8_t rotation() const override { return _rotation; }

  /* Only reached without a canvas, which this panel always has; they draw into
   * the canvas and leave flush() to show it. */
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *px) override;

  /* The canvas, allocated here: a 1-bit panel still draws in RGB565. */
  uint16_t *framebuffer() const override { return _fb; }
  void flushFramebuffer(const uint16_t *fb) override;

private:
  LB_TFTBus *_bus;
  const LB_PanelDef *_panel;
  int8_t _rst;
  uint8_t _rotation = 0;
  int16_t _nw = 0, _nh = 0; /* native size, rotation 0 */
  int16_t _w = 0, _h = 0;   /* current (rotated) size */
  uint16_t *_fb = nullptr;
  uint8_t *_packed = nullptr;

  void cmd(uint8_t c, const uint8_t *d = nullptr, uint8_t n = 0);
};
