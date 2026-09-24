/*
  LonelyBinaryDisplay — one-line setup for every Lonely Binary display.

  Every panel in the range plugs into the same 15-pin FPC breakout, so swapping
  a screen is a hardware no-op. This library makes it a software no-op:

      #include <LonelyBinaryDisplay.h>

      LB_Display display(LB_TFT_24);      // <- the ONLY line that changes

      void setup() {
        display.begin();
        display.fillScreen(LB_BLACK);
        display.setTextColor(LB_WHITE);
        display.drawString("Hello", 10, 10);
        display.flush();
      }

  The panel constant resolves the driver IC, resolution, inversion, column/row
  offsets, SPI clock and backlight polarity from the table in LB_Panels.h; the
  GPIOs come from LB_Wiring.h and follow the board you picked in Tools > Board.
  Both files are generated from panels.yaml — see tools/gen_panels.py.

  The panel driver is this library's own (LB_TFT.h); nothing from Arduino_GFX
  is used any more.

  MIT License · Lonely Binary
*/
#ifndef LONELY_BINARY_DISPLAY_H
#define LONELY_BINARY_DISPLAY_H

#include <Arduino.h>
#include "LB_TFTPanel.h"
#include "LB_TFTPar8Bus.h"
#include "LB_RgbScreen.h"
#include "LB_Touch.h"
#include "LB_Colors.h"
#include "LB_Panels.h"
#include "LB_Wiring.h"

class LB_Display : private LB_TFTPanel, public LB_Canvas {
 public:
  /*
   * LB_Display IS a drawing surface, the same way LB_VGA is. That is what lets
   * one function serve every product:
   *
   *     void drawGauge(LB_Canvas &c, float value);   // TFT, VGA and e-paper
   *
   * Before this the library handed back an Arduino_GFX and e-paper would have
   * handed back a GxEPD2_GFX, which share no base class - hence the old rule
   * about always writing `auto *gfx`. Both the rule and gfx() are gone: the
   * driver is our own now, and there is no Arduino_GFX object to hand back.
   *
   * Base order and private inheritance: LB_TFTPanel must be constructed before
   * LB_Canvas is handed a reference to it, and both declare flush(), so the
   * using-declarations below pick the canvas side explicitly. Private
   * inheritance alone would not do it - C++ looks names up before it checks
   * access, so the call would stay ambiguous.
   */
  explicit LB_Display(const LB_PanelDef *panel)
      : LB_TFTPanel(), LB_Canvas(*static_cast<LB_TFTPanel *>(this)), _panel(panel),
        _inverted(panel->invert) {}

  using LB_Canvas::flush;
  using LB_Canvas::sleep;
  using LB_Canvas::supportsPartial;
  using LB_Canvas::paletteSize;
  using LB_Canvas::setPaletteColor;
  using LB_Canvas::width;
  using LB_Canvas::height;
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

  // The same for the square series, which is 8-bit parallel with I2C touch:
  //
  //     LB_WiringPar8 pins = LB_WIRING_PAR8;
  //     pins.data[5] = 12;            // this board has D5 on GPIO12
  //     pins.rst = -1;                // panel reset not wired
  //     display.setWiring(pins);
  //
  // Which of the two applies is decided by the panel constant, not by you.
  void setWiring(const LB_WiringPar8 &wiring);

  // Override the panel table's SPI clock. Useful when your own wiring has
  // longer traces or a ribbon extension and the default rate is marginal —
  // symptoms are a scrambled or half-drawn image. Must be called before
  // begin(); pass 0 to go back to the panel default.
  void setSpiHz(int32_t hz) { _spiHzOverride = hz; }

  const LB_Wiring &wiring() const { return _wiring; }
  const LB_WiringPar8 &wiringPar8() const { return _wiringPar8; }

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
  // Both work at any time, before or after begin(), on every controller: the
  // colour order is one bit in MADCTL and inversion is one command. (Under
  // Arduino_GFX, ST7735 took the order as a constructor argument, so there it
  // had to precede begin(). That restriction went with it.) setColorOrder()
  // still returns bool so existing sketches compile; it is always true.
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
  // width * height * 2 bytes: PSRAM is used when the board has it, and the
  // smaller panels (up to about 128 x 160) also fit in internal RAM. A canvas is
  // a preference, not a requirement: if the allocation fails, begin() says so
  // on Serial and draws straight at the panel instead of failing. So it is
  // always safe to ask for one; hasCanvas() reports which you got.
  bool begin(bool useCanvas = false);

  // True once begin() has brought the panel up.
  bool begun() const { return _tft != nullptr; }

  // Push a w x h block of RGB565 pixels (as the CPU holds them, row-major).
  // Clipped to the screen. With a framebuffer this copies into it and shows on
  // the next flush(), like everything else drawn through the canvas; without
  // one it goes straight to the panel. This is what the LVGL library uses for
  // its partial-render mode.
  void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *px);

  /* Renamed from panel(): LB_Canvas::panel() returns the LB_Panel this canvas
   * draws through, which is a different thing with the same old name. */
  const LB_PanelDef *panelDef() const { return _panel; }
  bool hasCanvas() const { return _fb != nullptr; }

  // The RGB565 framebuffer when begin(true) was used, else nullptr. LVGL
  // renders straight into this instead of into a buffer of its own, which
  // makes the flush callback a no-op — see the Lonely Binary LVGL library.
  uint16_t *framebuffer() const { return _fb; }

  // Double buffering, on a panel scanned out of memory (the RGB boards): the
  // second buffer, or nullptr; and present(buf), which puts `buf` on screen at
  // the next vertical blank and returns once it is. The LVGL library uses these
  // to draw without tearing. Plain drawing ignores them and uses framebuffer().
  uint16_t *framebuffer2() const { return _tft ? _tft->framebuffer2() : nullptr; }
  void present(const uint16_t *buf) { if (_tft) _tft->present(buf); }


  // Backlight, 0 = off, 255 = full. Panels wired active-low and panels with
  // PWM dimming are handled here; the caller never needs to know which is
  // which. On an on/off panel any non-zero level means on.
  void backlight(uint8_t level);

  // ── Touch ──────────────────────────────────────────────────────────────
  //
  // For a panel with touch (the _ctp / _rtp constants), begin() also brings up
  // the touch controller, and setRotation() keeps its coordinates in step with
  // the picture. Include the driver header to enable it:
  //
  //     #include <LonelyBinaryDisplay.h>
  //     #include <LB_TouchGT911.h>       // LB_SQUARE_392_CTP
  //
  // (The LVGL library includes it for you.) Without that include the display
  // still works; touch() returns nullptr and begin() says why on Serial.
  //
  //     int16_t x, y;
  //     if (display.touch() && display.touch()->getTouch(&x, &y)) { ... }
  //
  // nullptr for a panel without touch, or if the controller did not answer.
  LB_Touch *touch() const { return _touch; }

  // Rotation 0-3. The panel's column/row offsets differ between portrait and
  // landscape, and the driver applies the right pair — which is exactly the
  // bug you get when the offsets are hard-coded in a sketch.
  void setRotation(uint8_t r);
  uint8_t rotation() const { return _tft ? _tft->rotation() : _panel->rotation; }


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
  LB_WiringPar8      _wiringPar8 = LB_WIRING_PAR8;
  LB_TFTBus         *_bus    = nullptr;
  LB_Touch          *_touch  = nullptr;
  LB_Screen         *_tft    = nullptr;  // the panel itself: LB_TFT or LB_RgbScreen
  uint16_t          *_fb     = nullptr;  // framebuffer, when requested
  bool               _fbInPsram = false;
  bool               _fbOwned = true;    // false: the RGB driver's, not ours to free
  bool               _blReady = false;
  ColorOrder         _colorOrder = COLOR_AUTO;
  bool               _blActiveLow = false;   // set from the panel in begin()
  bool               _blPolarityForced = false;
  uint8_t            _blLevel = 255;
  bool               _inverted;         // starts as the panel's, see ctor

  void backlightBegin();
  void touchBegin();
  bool par8() const { return _panel->bus == LB_BUS_PAR8; }
  bool rgb() const { return _panel->bus == LB_BUS_RGB; }
  int8_t blPin() const {
    return rgb() ? _panel->rgb->backlight : par8() ? _wiringPar8.backlight : _wiring.backlight;
  }
};

#endif  // LONELY_BINARY_DISPLAY_H
