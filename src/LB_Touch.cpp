#include "LB_Touch.h"

uint8_t LB_Touch::read(LB_TouchPoint *pts, uint8_t max) {
  if (max > MAX_POINTS) max = MAX_POINTS;
  const uint8_t n = readRaw(pts, max);

  // Size of the rot0 frame: swapping the axes swaps the extents too.
  const int16_t w0 = _swapXY ? _nativeH : _nativeW;
  const int16_t h0 = _swapXY ? _nativeW : _nativeH;

  for (uint8_t i = 0; i < n; i++) {
    int16_t x = pts[i].x, y = pts[i].y;
    if (_swapXY) { const int16_t t = x; x = y; y = t; }
    if (_flipX) x = w0 - 1 - x;
    if (_flipY) y = h0 - 1 - y;

    // rot0 → rotN. Rotation N turns the content N x 90 degrees clockwise, so
    // the point under the finger moves the other way.
    switch (_rotation) {
      case 1:  pts[i].x = y;          pts[i].y = w0 - 1 - x; break;
      case 2:  pts[i].x = w0 - 1 - x; pts[i].y = h0 - 1 - y; break;
      case 3:  pts[i].x = h0 - 1 - y; pts[i].y = x;          break;
      default: pts[i].x = x;          pts[i].y = y;          break;
    }
  }
  return n;
}

bool LB_Touch::getTouch(int16_t *x, int16_t *y) {
  LB_TouchPoint p[MAX_POINTS];
  if (read(p, MAX_POINTS) == 0) return false;
  if (x) *x = p[0].x;
  if (y) *y = p[0].y;
  return true;
}

static LB_Touch::Factory s_factories[8];

void LB_Touch::registerDriver(LB_TouchCtl ctl, Factory make) {
  if (ctl < sizeof s_factories / sizeof s_factories[0]) s_factories[ctl] = make;
}

LB_Touch *LB_Touch::create(LB_TouchCtl ctl, const LB_TouchPins &pins) {
  if (ctl == LB_TOUCH_NONE || ctl >= sizeof s_factories / sizeof s_factories[0]) return nullptr;
  return s_factories[ctl] ? s_factories[ctl](pins) : nullptr;
}
