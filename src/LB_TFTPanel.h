#pragma once
/*
 * LB_TFTPanel - the LB_Panel adapter over an Arduino_GFX driver.
 *
 * This is what lets Lonely Binary GFX draw on a TFT, so the same code runs on
 * a TFT, on VGA and on e-paper.
 *
 * !! It deliberately does NOT expose the framebuffer !!
 *
 *   buffer() returns nullptr even when a canvas is allocated, so every span
 *   goes through Arduino_GFX's drawFastHLine instead of straight into memory.
 *   That costs one virtual call per horizontal run - not per pixel - which is
 *   not measurable.
 *
 *   What it buys is correctness we cannot otherwise check. Rotation on this
 *   library is subtle: with a canvas, the framebuffer keeps the shape it was
 *   allocated with while the driver holds the orientation MADCTL was actually
 *   programmed for, and portrait<->landscape is refused outright. Writing into
 *   that buffer by hand means reimplementing those rules, and getting them
 *   wrong produces a picture that is skewed or mirrored - the kind of fault
 *   that only shows up on a real panel.
 *
 *   Letting Arduino_GFX keep doing the coordinate mapping means the behaviour
 *   is exactly what it already was. Direct buffer access is a later
 *   optimisation, for someone with the panels on the bench.
 */
#include <Arduino_GFX_Library.h>
#include <LonelyBinaryGFX.h>

class LB_TFTPanel : public LB_Panel
{
public:
  void attach(Arduino_GFX *gfx, Arduino_Canvas *canvas)
  {
    _gfx = gfx;
    _canvas = canvas;
  }

  int16_t panelWidth() const override { return _gfx ? _gfx->width() : 0; }
  int16_t panelHeight() const override { return _gfx ? _gfx->height() : 0; }
  lb_format_t format() const override { return LB_FMT_RGB565; }

  /* Deliberately null - see the note above. */
  uint8_t *buffer() const override { return nullptr; }
  uint32_t stride() const override { return 0; }

  void writeSpan(int16_t x, int16_t y, int16_t w, lb_color_t raw) override
  {
    if (_gfx) _gfx->drawFastHLine(x, y, w, (uint16_t)raw);
  }

  void flush(lb_flush_t = LB_FLUSH_FULL) override
  {
    if (_canvas) _canvas->flush();
  }

  /* Arduino_GFX applies rotation itself - one MADCTL bit on the panel - so the
   * canvas must not also rotate the coordinates. */
  bool hardwareRotation() const override { return true; }
  void setPanelRotation(uint8_t) override {} /* LB_Display::setRotation owns this */

private:
  Arduino_GFX *_gfx = nullptr;
  Arduino_Canvas *_canvas = nullptr;
};
