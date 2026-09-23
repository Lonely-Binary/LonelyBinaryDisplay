#pragma once
/*
 * LB_TFTPanel - the LB_Panel adapter over LB_TFT.
 *
 * This is what lets Lonely Binary GFX draw on a TFT, so the same code runs on
 * a TFT, on VGA and on e-paper.
 *
 * With a framebuffer, the canvas draws straight into it and flush() pushes the
 * whole frame. Without one, every span goes over the bus as it is drawn.
 *
 * Why exposing the framebuffer is safe here, when it was not over Arduino_GFX:
 *
 *   Rotation is done by the panel (one MADCTL write), so the framebuffer is
 *   always in the coordinates the canvas draws in - the panel's CURRENT shape.
 *   That holds because LB_Display refuses to switch between portrait and
 *   landscape while a framebuffer exists, so the shape can never change under
 *   it. Arduino_Canvas kept a rotation of its own on top of the driver's, and
 *   the two could disagree; there is only one here.
 */
#include <LonelyBinaryGFX.h>
#include "LB_TFT.h"

class LB_TFTPanel : public LB_Panel
{
public:
  void attach(LB_TFT *tft, uint16_t *fb)
  {
    _tft = tft;
    _fb = fb;
  }

  /* The current (rotated) size - see hardwareRotation(). */
  int16_t panelWidth() const override { return _tft ? _tft->width() : 0; }
  int16_t panelHeight() const override { return _tft ? _tft->height() : 0; }
  lb_format_t format() const override { return LB_FMT_RGB565; }

  uint8_t *buffer() const override { return (uint8_t *)_fb; }
  uint32_t stride() const override { return _fb ? (uint32_t)panelWidth() * 2 : 0; }

  /* Only called when buffer() is null. */
  void writeSpan(int16_t x, int16_t y, int16_t w, lb_color_t raw) override
  {
    if (_tft) _tft->fillRect(x, y, w, 1, (uint16_t)raw);
  }

  void flush(lb_flush_t = LB_FLUSH_FULL) override
  {
    if (_tft && _fb) _tft->pushImage(0, 0, _tft->width(), _tft->height(), _fb);
  }

  /* The panel rotates with one MADCTL write, so the canvas must not also
   * rotate the coordinates. */
  bool hardwareRotation() const override { return true; }
  void setPanelRotation(uint8_t) override {} /* LB_Display::setRotation owns this */

private:
  LB_TFT *_tft = nullptr;
  uint16_t *_fb = nullptr;
};
