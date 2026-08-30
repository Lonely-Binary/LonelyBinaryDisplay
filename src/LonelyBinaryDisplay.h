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

  // Bring up SPI, the panel and the backlight pin. Returns false if the panel
  // driver refuses to start (almost always a wiring fault — check DC first).
  //
  // useCanvas allocates a full RGB565 framebuffer and draws into that, pushing
  // to the panel on flush(). It removes the flicker you get when redrawing
  // large text in place, and it is what LVGL wants. It needs
  // width * height * 2 bytes, so it is PSRAM-only in practice: begin() returns
  // false rather than half-working if the allocation fails.
  bool begin(bool useCanvas = false);

  // The drawing surface. Declare the receiving variable with `auto` — see the
  // note at the top of this file.
  Arduino_GFX *gfx() const { return _gfx; }

  const LB_PanelDef *panel() const { return _panel; }
  bool hasCanvas() const { return _canvas != nullptr; }

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
  Arduino_DataBus   *_bus    = nullptr;
  Arduino_GFX       *_driver = nullptr;  // the panel itself
  Arduino_Canvas    *_canvas = nullptr;  // framebuffer, when requested
  Arduino_GFX       *_gfx    = nullptr;  // canvas if present, else driver
  bool               _blReady = false;

  Arduino_GFX *makeDriver();
  void backlightBegin();
};

#endif  // LONELY_BINARY_DISPLAY_H
