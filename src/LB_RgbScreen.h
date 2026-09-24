#pragma once
/*
 * LB_RgbScreen - a 16-bit parallel RGB panel, scanned out by the S3's LCD_CAM.
 *
 * There is no controller to talk to and no RAM on the panel: the S3 sends
 * every pixel of every frame, continuously, from a framebuffer in PSRAM. So
 * this driver owns that framebuffer, and drawing is writing into it. What
 * makes the new pixels reach the glass is only the CPU cache: they sit in it
 * until written back to PSRAM, where the scan-out DMA reads them. That write-
 * back is flushFramebuffer(), which is why a sketch calls flush().
 *
 * Built on ESP-IDF's esp_lcd RGB driver. On a chip without LCD_CAM RGB (the
 * classic ESP32) begin() returns false and says why.
 *
 * Tearing: drawing into the buffer that is being scanned out shows half-drawn
 * frames. Fine for small changes; not for a translucent overlay redrawn every
 * second (measured on the 7" board: an LVGL message box over live tiles tore
 * and flickered). So there are two buffers when PSRAM allows, and present()
 * swaps them at the vertical blank. The LVGL library uses that; a plain canvas
 * sketch draws into the first one as before.
 *
 * Rotation: only 0 for now - there is no MADCTL to turn the picture, so it
 * would have to be done in software.
 */
#include "LB_TFT.h"

class LB_RgbScreen : public LB_Screen
{
public:
  LB_RgbScreen(const LB_PanelDef *panel) : _panel(panel) {}
  ~LB_RgbScreen();

  bool begin(int32_t hz, bool bgr, bool invert) override;
  void setRotation(uint8_t r) override;
  void setColorOrder(bool) override {}   /* fixed by the wiring: see the panel table */
  void setInverted(bool) override {}     /* no controller to invert */
  int16_t width() const override { return _panel->width; }
  int16_t height() const override { return _panel->height; }
  uint8_t rotation() const override { return 0; }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *px) override;
  uint16_t *framebuffer() const override { return _fb; }
  void flushFramebuffer(const uint16_t *fb) override;
  uint16_t *framebuffer2() const override { return _fb2; }
  void present(const uint16_t *fb) override;

private:
  const LB_PanelDef *_panel;
  void *_handle = nullptr;   /* esp_lcd_panel_handle_t, kept opaque here */
  uint16_t *_fb = nullptr;
  uint16_t *_fb2 = nullptr;      /* second buffer, when there was PSRAM for it */
  void *_vsync = nullptr;        /* SemaphoreHandle_t, given at every vertical blank */
  void sync(int16_t x, int16_t y, int16_t w, int16_t h);
};
