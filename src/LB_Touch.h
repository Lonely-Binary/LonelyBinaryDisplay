/*
 * LB_Touch — what every touch controller has in common.
 *
 * A touch controller reports points in its own fixed frame. The sketch wants
 * them in the frame it is drawing in, which moves with setRotation(). This
 * class owns that conversion so every driver gets it for free, and so it is
 * written once:
 *
 *   native → rot0     a panel fact: how the touch layer is glued on relative
 *                     to the display's rotation 0. swapXY / flipX / flipY,
 *                     applied in that order. Most panels need none of them.
 *   rot0   → rotN     the same for every controller we drive: rotation N shows
 *                     the content turned N x 90 degrees clockwise (derived from
 *                     the MADCTL tables in LB_TFT.cpp — all three families
 *                     agree on the direction).
 *
 * A driver only implements begin() and readRaw(); readRaw() hands back points
 * in the controller's native frame.
 *
 * Polling faster than the controller reports is fine: a driver keeps the last
 * report, so "no new data yet" never reads as "finger lifted".
 *
 * MIT License · Lonely Binary
 */
#ifndef LB_TOUCH_H
#define LB_TOUCH_H

#include <Arduino.h>
#include "LB_Panels.h"

struct LB_TouchPins {
  int8_t sda, scl, intr, rst;     // I2C controllers. -1 = not wired
  int8_t sck, miso, mosi, cs;     // SPI controllers
};

struct LB_TouchPoint {
  int16_t  x;
  int16_t  y;
  uint16_t size;   // contact area, controller units; 0 if the chip has none
  uint8_t  id;     // stays the same for one finger while it is down
};

class LB_Touch {
 public:
  static constexpr uint8_t MAX_POINTS = 5;

  virtual ~LB_Touch() {}

  // Bring the controller up. Returns false if it does not answer.
  virtual bool begin() = 0;

  // Up to `max` points in the current rotation's frame. Returns how many
  // fingers are down (0 = none).
  uint8_t read(LB_TouchPoint *pts, uint8_t max = MAX_POINTS);

  // The first finger only — enough for buttons and sliders.
  bool getTouch(int16_t *x, int16_t *y);

  // Keep this in step with the display's setRotation(). 0-3.
  void setRotation(uint8_t r) { _rotation = r & 3; }
  uint8_t rotation() const { return _rotation; }

  // How the touch layer sits relative to display rotation 0. The panel table
  // carries this; call it yourself only for a panel it does not know.
  void setOrientation(bool swapXY, bool flipX, bool flipY) {
    _swapXY = swapXY; _flipX = flipX; _flipY = flipY;
  }

  // Resistive controllers report raw ADC counts, not pixels. The panel table
  // carries the raw values at the screen's edges (left, right, top, bottom)
  // and the screen size; a capacitive controller ignores both.
  virtual void setRawRange(const int16_t raw[4]) { (void)raw; }
  virtual void setScreenSize(int16_t w, int16_t h) { (void)w; (void)h; }

  // The controller's own resolution, in its native frame.
  int16_t nativeWidth() const { return _nativeW; }
  int16_t nativeHeight() const { return _nativeH; }

 // ── Registry ────────────────────────────────────────────────────────────
  //
  // LB_Display builds the touch driver its panel names, but it cannot name the
  // driver class itself: a driver on Wire would drag Wire (about 22 KB) into
  // every display sketch, touch or not, because Arduino links every library
  // any .cpp of this one includes. So each driver header registers a factory
  // when a sketch includes it, and LB_Display looks it up here.
  typedef LB_Touch *(*Factory)(const LB_TouchPins &pins);
  static void registerDriver(LB_TouchCtl ctl, Factory make);
  static LB_Touch *create(LB_TouchCtl ctl, const LB_TouchPins &pins);  // null if none

 protected:
  virtual uint8_t readRaw(LB_TouchPoint *pts, uint8_t max) = 0;

  int16_t _nativeW = 0;
  int16_t _nativeH = 0;

 private:
  uint8_t _rotation = 0;
  bool    _swapXY = false;
  bool    _flipX = false;
  bool    _flipY = false;
};

#endif  // LB_TOUCH_H
