/*
  LonelyBinaryDisplay — one-line setup for every Lonely Binary SPI display.

  Every panel in the range plugs into the same 15-pin FPC breakout, so swapping
  a screen is a hardware no-op. This library makes it a software no-op:

      #include <LonelyBinaryDisplay.h>

      LB_Display display(LB_TFT_24);      // <- the ONLY line that changes

      void setup() {
        display.begin();
        display.backlight(255);

        auto *gfx = display.gfx();        // house style: ALWAYS auto (see below)
        gfx->fillScreen(BLACK);
        gfx->setCursor(10, 10);
        gfx->print("Hello");
      }

  The panel constant resolves the driver IC, resolution, IPS flag, column/row
  offsets, SPI clock and backlight polarity from the table in LB_Panels.h; the
  GPIOs come from LB_Wiring.h and follow the board you picked in Tools > Board.
  Both files are generated from panels.yaml — see tools/gen_panels.py.

  WHY `auto *gfx`, NOT `Arduino_GFX *gfx`:
  On a TFT, gfx() hands back an Arduino_GFX (which derives from Print and
  Arduino_G). On an e-paper panel it hands back a GxEPD2_GFX (which derives
  from Adafruit_GFX). The two share no base class, but their drawing methods
  have identical names — so one body of code compiles against both, as long as
  the variable is declared `auto`. Writing the type out by hand forks every
  sketch and every lesson the day e-paper arrives.

  MIT License · Lonely Binary
*/
#ifndef LONELY_BINARY_DISPLAY_H
#define LONELY_BINARY_DISPLAY_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "LB_Colors.h"
#include "LB_Panels.h"
#include "LB_Wiring.h"

class LB_Display {
 public:
  explicit LB_Display(const LB_PanelDef *panel) : _panel(panel) {}
  ~LB_Display();

  // ── Using your own wiring ────────────────────────────────────────────────
  //
  // By default the GPIOs come from LB_Wiring.h, which is the Lonely Binary
  // breakout on the board you selected in Tools > Board. On your own PCB, or
  // on a dev board where those pins are taken, start from that default and
  // change only what differs — you inherit the SPI host that is correct for
  // your MCU, which is worth having because the bus constants are not the same
  // on an ESP32-S3 as on a classic ESP32 (VSPI does not even exist on the S3).
  //
  //     LB_Wiring pins = LB_WIRING;   // the kit wiring for this board
  //     pins.cs = 5;                  // ...change what is different
  //     pins.backlight = -1;          // -1 = this board has no backlight pin
  //     display.setWiring(pins);
  //     display.begin();
  //
  // Must be called before begin(); afterwards it does nothing.
  void setWiring(const LB_Wiring &wiring);

  // Override the panel table's SPI clock. Useful when your own wiring has
  // longer traces or a ribbon extension and the default rate is marginal —
  // symptoms are a scrambled or half-drawn image. Must be called before
  // begin(); pass 0 to go back to the panel default.
  void setSpiHz(int32_t hz) { _spiHzOverride = hz; }

  const LB_Wiring &wiring() const { return _wiring; }

  // ── Colour order and inversion ───────────────────────────────────────────
  //
  // Two panels with the same controller can be wired to their glass with the
  // red and blue channels swapped, and can sit at rest inverted or not. The
  // panel table carries the right answer for every screen we sell, so you
  // normally touch neither of these. They are here for a panel we have not
  // characterised yet, and for bench work.
  //
  // Read the symptom off the screen — send pure red, green and blue and see
  // what comes back:
  //
  //     red -> blue,   blue -> red        colour order is wrong
  //     black -> white                    inversion is wrong
  //     red -> yellow, green -> magenta,
  //     blue -> cyan                      BOTH are wrong
  //
  // setInverted() works at any time. setColorOrder() works at any time too on
  // ST7789 / ST7796 / NV3007, where the order is just a bit in MADCTL — but
  // NOT on ST7735, whose driver takes it as a constructor argument, so there
  // it has to precede begin(). It returns false if it could not be applied.
  enum ColorOrder { COLOR_AUTO, COLOR_RGB, COLOR_BGR };
  bool setColorOrder(ColorOrder order);
  void setInverted(bool inverted);

  bool colorOrderIsBGR() const;
  bool inverted() const { return _inverted; }

  // Backlight polarity. Like the two above, the panel table already knows this
  // for every screen we sell; this is for a bare panel, or for settling an
  // argument on the bench. Symptom of getting it wrong: backlight(255) is dark
  // and backlight(0) is bright — the whole scale runs backwards.
  //
  // Runtime, because duty is computed on every call.
  void setBacklightActiveLow(bool activeLow);
  bool backlightActiveLow() const { return _blActiveLow; }

  // Bring up SPI, the panel and the backlight, and turn the backlight on.
  // Returns false if the panel driver refuses to start (almost always a wiring
  // fault — check DC first).
  //
  // useCanvas allocates a full RGB565 framebuffer and draws into that, pushing
  // to the panel on flush(). It removes the flicker you get when redrawing
  // large text in place, and it is what LVGL wants. It needs
  // width * height * 2 bytes, so it is PSRAM-only in practice — but a canvas is
  // a preference, not a requirement: if the allocation fails, begin() says so
  // on Serial and draws straight at the panel instead of failing. So it is
  // always safe to ask for one; hasCanvas() reports which you got.
  bool begin(bool useCanvas = false);

  // The drawing surface. Declare the receiving variable with `auto` — see the
  // note at the top of this file.
  Arduino_GFX *gfx() const { return _gfx; }

  const LB_PanelDef *panel() const { return _panel; }
  bool hasCanvas() const { return _canvas != nullptr; }

  // The RGB565 framebuffer when begin(true) was used, else nullptr. LVGL
  // renders straight into this instead of into a buffer of its own, which
  // makes the flush callback a no-op — see the Lonely Binary LVGL library.
  uint16_t *framebuffer() const;

  // Push the framebuffer to the panel. No-op when begin() was called without a
  // canvas, so it is always safe to call.
  void flush();

  // Backlight, 0 = off, 255 = full. Panels wired active-low and panels with
  // PWM dimming are handled here; the caller never needs to know which is
  // which. On an on/off panel any non-zero level means on.
  void backlight(uint8_t level);

  // Rotation 0-3. The panel's column/row offsets differ between portrait and
  // landscape, and the driver applies the right pair — which is exactly the
  // bug you get when the offsets are hard-coded in a sketch.
  void setRotation(uint8_t r);
  uint8_t rotation() const { return _gfx ? _gfx->getRotation() : _panel->rotation; }

  int16_t width() const { return _gfx ? _gfx->width() : _panel->width; }
  int16_t height() const { return _gfx ? _gfx->height() : _panel->height; }

  // Colour bars, a backlight sweep (PWM panels) and a panel info page — the
  // "does my wiring work" check. This is what the per-panel test sketches did.
  void selfTest();

  // Human-readable panel + wiring summary, for Serial.
  void printInfo(Print &out = Serial) const;

 private:
  const LB_PanelDef *_panel;
  LB_Wiring          _wiring = LB_WIRING;  // kit default until setWiring()
  bool               _customWiring = false;
  int32_t            _spiHzOverride = 0;
  Arduino_DataBus   *_bus    = nullptr;
  Arduino_GFX       *_driver = nullptr;  // the panel itself
  Arduino_Canvas    *_canvas = nullptr;  // framebuffer, when requested
  Arduino_GFX       *_gfx    = nullptr;  // canvas if present, else driver
  bool               _blReady = false;
  ColorOrder         _colorOrder = COLOR_AUTO;
  bool               _blActiveLow = false;   // set from the panel in begin()
  bool               _blPolarityForced = false;
  uint8_t            _blLevel = 255;
  bool               _inverted = false;
  bool               _madctlOverride = false;

  Arduino_GFX *makeDriver();
  // The PANEL's rotation, which is not the same thing as rotation(). With a
  // canvas, _gfx is the framebuffer — rotation 0 — while the driver holds the
  // orientation MADCTL was actually programmed for. Anything that writes
  // MADCTL must use this one.
  uint8_t panelRotation() const;
  void backlightBegin();
  void applyColorOrder();
};

#endif  // LONELY_BINARY_DISPLAY_H
