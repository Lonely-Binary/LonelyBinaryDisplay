/*
 * LB_TouchCalibration — a 4-point calibration for a resistive touch layer.
 *
 *     #include <LB_TouchNS2009.h>
 *     #include <LB_TouchCalibration.h>
 *
 *     LB_TouchCal cal;
 *     if (lb_load_touch_cal(display, cal)) lb_apply_touch_cal(display, display.touch(), cal);
 *     else {
 *       lb_calibrate_touch(display, static_cast<LB_TouchNS2009 *>(display.touch()), cal);
 *       lb_save_touch_cal(display, cal);       // kept across power-offs
 *     }
 *
 * A resistive layer reports raw numbers, not pixels, and every layer is glued on a
 * little differently - the axes can even be swapped. This shows four crosses, one
 * near each corner; the user presses each one and holds still. From the four
 * readings it works out which raw axis runs along which screen axis and
 * the raw value at each screen edge, which is exactly what a panel table's
 * touch: raw / swap_xy hold. The result is applied to the driver at once.
 *
 * The crosses are drawn where the picture is, whatever the screen's rotation; the
 * fit is done in the panel's own frame, which is the frame the driver's mapping
 * lives in (LB_Touch turns the point to the rotation afterwards).
 *
 * Works with any touch driver that has rawX() and rawY() for its last reading
 * (LB_TouchNS2009, LB_TouchXPT2046). The result is also printed on Serial in the
 * form panels.yaml takes, so it can be made permanent for the panel.
 *
 * MIT License · Lonely Binary
 */
#ifndef LB_TOUCH_CALIBRATION_H
#define LB_TOUCH_CALIBRATION_H

#include <Preferences.h>
#include <LonelyBinaryDisplay.h>

struct LB_TouchCal {
  int16_t raw[4];   // raw value at the left, right, top, bottom edge (in the touch frame)
  bool    swapXY;   // the layer's X runs along the screen's Y
};

namespace lb_cal_detail {

struct Point { int tx, ty, nx, ny; float rx, ry; };   // on screen, in the panel's frame, the raw reading

inline void centred(LB_Display &d, const char *s, int x, int y, lb_color_t c) {
  d.setTextSize(1);
  d.setTextColor(c);
  d.drawString(s, x - d.textWidth(s) / 2, y);
}

inline void cross(LB_Display &d, int cx, int cy, lb_color_t c) {
  d.drawLine(cx - 10, cy, cx + 10, cy, c);
  d.drawLine(cx, cy - 10, cx, cy + 10, c);
  d.drawCircle(cx, cy, 5, c);
  d.fillRect(cx - 1, cy - 1, 3, 3, c);
}

// Where a point on the screen as you see it sits in the panel's own frame.
inline void toPanelFrame(LB_Display &d, int vx, int vy, int &nx, int &ny) {
  const int w0 = d.panelDef()->width, h0 = d.panelDef()->height;
  switch (d.rotation()) {
    case 1:  nx = w0 - 1 - vy; ny = vx; break;
    case 2:  nx = w0 - 1 - vx; ny = h0 - 1 - vy; break;
    case 3:  nx = vy;          ny = h0 - 1 - vx; break;
    default: nx = vx;          ny = vy; break;
  }
}

// Least squares for v = m * u + q, and how well a line fits (r2).
inline void fitLine(const float *u, const float *v, int n, float &m, float &q, float &r2) {
  float su = 0, sv = 0, suu = 0, suv = 0, svv = 0;
  for (int i = 0; i < n; i++) { su += u[i]; sv += v[i]; suu += u[i] * u[i]; suv += u[i] * v[i]; svv += v[i] * v[i]; }
  const float d = n * suu - su * su;
  m = d != 0 ? (n * suv - su * sv) / d : 0;
  q = (sv - m * su) / n;
  const float vr = n * svv - sv * sv;
  r2 = (d != 0 && vr != 0) ? (n * suv - su * sv) * (n * suv - su * sv) / (d * vr) : 0;
}

// What the driver does with a raw reading, to check a result before trusting it.
inline void mapRaw(float rx, float ry, bool swap, const int16_t raw[4], int w, int h, int &px, int &py) {
  const int tw = swap ? h : w, th = swap ? w : h;
  const float tx = (rx - raw[0]) * (tw - 1) / (raw[1] - raw[0]);
  const float ty = (ry - raw[2]) * (th - 1) / (raw[3] - raw[2]);
  px = (int)(swap ? ty : tx);
  py = (int)(swap ? tx : ty);
}

// Press-and-hold capture of one point. False if the stylus lifted too soon or slid.
template <class T>
inline bool capture(LB_Display &d, T *t, int tx, int ty, float &rx, float &ry) {
  int16_t x, y;
  while (!t->getTouch(&x, &y)) delay(5);                    // wait for the press
  float sx = 0, sy = 0, lo[2] = {1e9f, 1e9f}, hi[2] = {-1e9f, -1e9f};
  int n = 0;
  const uint32_t start = millis();
  while (t->getTouch(&x, &y) && millis() - start < 1500) {
    if (millis() - start > 150) {                           // ignore the first moments of the press
      const float a = t->rawX(), b = t->rawY();
      sx += a; sy += b; n++;
      lo[0] = min(lo[0], a); hi[0] = max(hi[0], a);
      lo[1] = min(lo[1], b); hi[1] = max(hi[1], b);
    }
    if (n >= 20) break;
    delay(8);
  }
  cross(d, tx, ty, n >= 20 ? LB_GREEN : LB_RED);
  d.flush();
  while (t->getTouch(&x, &y)) delay(10);                    // wait for the lift
  delay(150);
  if (n < 20 || hi[0] - lo[0] > 120 || hi[1] - lo[1] > 120) return false;
  rx = sx / n;
  ry = sy / n;
  return true;
}

}  // namespace lb_cal_detail

// Run the calibration, repeating until it is accurate to within `maxErrorPx` (or
// `tries` runs). Applies the result to `touch` and fills `cal`. True if accepted.
template <class T>
inline bool lb_calibrate_touch(LB_Display &d, T *touch, LB_TouchCal &cal, bool report = true,
                               int maxErrorPx = 8, int tries = 5) {
  using namespace lb_cal_detail;
  if (!touch) return false;
  const int vw = d.width(), vh = d.height(), inset = max(14, min(vw, vh) / 8);   // the screen as you see it
  const int w = d.panelDef()->width, h = d.panelDef()->height;                    // the panel's own frame

  for (int attempt = 0; attempt < tries; attempt++) {
    Point pts[4] = {{inset, inset}, {vw - 1 - inset, inset}, {inset, vh - 1 - inset},
                    {vw - 1 - inset, vh - 1 - inset}};
    for (Point &p : pts) toPanelFrame(d, p.tx, p.ty, p.nx, p.ny);

    for (int i = 0; i < 4; i++) {
      bool ok = false;
      while (!ok) {
        d.fillScreen(LB_BLACK);
        centred(d, "CALIBRATE", vw / 2, vh / 2 - 28, LB_WHITE);
        char b[24];
        snprintf(b, sizeof b, "Press %d of 4", i + 1);
        centred(d, b, vw / 2, vh / 2 - 8, LB_WHITE);
        centred(d, "and hold still", vw / 2, vh / 2 + 12, LB_WHITE);
        cross(d, pts[i].tx, pts[i].ty, LB_WHITE);
        d.flush();
        ok = capture(d, touch, pts[i].tx, pts[i].ty, pts[i].rx, pts[i].ry);
        if (!ok) {
          d.fillScreen(LB_BLACK);
          centred(d, "Hold it still", vw / 2, vh / 2 - 8, LB_RED);
          centred(d, "until green", vw / 2, vh / 2 + 12, LB_RED);
          d.flush();
          delay(900);
        }
      }
      if (report)
        Serial.printf("point %d: target (%d,%d), on the panel (%d,%d)  raw (%.0f,%.0f)\n", i + 1, pts[i].tx, pts[i].ty,
                      pts[i].nx, pts[i].ny, pts[i].rx, pts[i].ry);
    }

    // Which raw axis runs along which screen axis? The one that fits better.
    float rx[4], ry[4], sx[4], sy[4];
    for (int i = 0; i < 4; i++) { rx[i] = pts[i].rx; ry[i] = pts[i].ry; sx[i] = pts[i].nx; sy[i] = pts[i].ny; }
    float m, q, r2xx, r2xy, r2yy, r2yx;
    fitLine(rx, sx, 4, m, q, r2xx);
    fitLine(ry, sx, 4, m, q, r2xy);
    fitLine(ry, sy, 4, m, q, r2yy);
    fitLine(rx, sy, 4, m, q, r2yx);
    const bool swap = (r2xy + r2yx) > (r2xx + r2yy);

    int16_t raw[4];
    float mm, qq, r2;
    if (!swap) {
      fitLine(rx, sx, 4, mm, qq, r2); raw[0] = (int16_t)((0 - qq) / mm); raw[1] = (int16_t)((w - 1 - qq) / mm);
      fitLine(ry, sy, 4, mm, qq, r2); raw[2] = (int16_t)((0 - qq) / mm); raw[3] = (int16_t)((h - 1 - qq) / mm);
    } else {   // raw X runs along the screen's Y, raw Y along its X
      fitLine(rx, sy, 4, mm, qq, r2); raw[0] = (int16_t)((0 - qq) / mm); raw[1] = (int16_t)((h - 1 - qq) / mm);
      fitLine(ry, sx, 4, mm, qq, r2); raw[2] = (int16_t)((0 - qq) / mm); raw[3] = (int16_t)((w - 1 - qq) / mm);
    }

    int worst = 0;
    for (int i = 0; i < 4; i++) {
      int px, py;
      mapRaw(rx[i], ry[i], swap, raw, w, h, px, py);
      worst = max(worst, max(abs(px - pts[i].nx), abs(py - pts[i].ny)));
    }
    if (report) {
      Serial.printf("\nworst error %d px, swap_xy %s\nfor panels.yaml:\n", worst, swap ? "true" : "false");
      Serial.printf("    touch:\n      controller: NS2009\n      swap_xy: %s\n      raw: [%d, %d, %d, %d]\n",
                    swap ? "true" : "false", raw[0], raw[1], raw[2], raw[3]);
    }

    if (worst > maxErrorPx) {   // a point was pressed well off the cross
      d.fillScreen(LB_BLACK);
      centred(d, "Not accurate", vw / 2, vh / 2 - 16, LB_RED);
      char b[24];
      snprintf(b, sizeof b, "off by %d px", worst);
      centred(d, b, vw / 2, vh / 2 + 2, LB_RED);
      centred(d, "Try again", vw / 2, vh / 2 + 20, LB_WHITE);
      d.flush();
      delay(1800);
      continue;
    }

    memcpy(cal.raw, raw, sizeof raw);
    cal.swapXY = swap;
    touch->setOrientation(swap, false, false);
    touch->setRawRange(raw);
    touch->setScreenSize(w, h);
    d.fillScreen(LB_BLACK);
    centred(d, "Calibrated", vw / 2, vh / 2 - 16, LB_GREEN);
    char b[24];
    snprintf(b, sizeof b, "error %d px", worst);
    centred(d, b, vw / 2, vh / 2 + 2, LB_WHITE);
    d.flush();
    delay(1000);
    return true;
  }
  return false;
}

// Apply a saved result to a touch driver.
inline void lb_apply_touch_cal(LB_Display &d, LB_Touch *touch, const LB_TouchCal &cal) {
  if (!touch) return;
  touch->setOrientation(cal.swapXY, false, false);
  touch->setRawRange(cal.raw);
  touch->setScreenSize(d.panelDef()->width, d.panelDef()->height);
}

// Saved in the chip's flash, one slot per panel, so a new calibration of one size
// does not touch another's.
inline bool lb_load_touch_cal(LB_Display &d, LB_TouchCal &cal) {
  Preferences p;
  if (!p.begin("lbtouch", true)) return false;
  const bool ok = p.getBytesLength(d.panelDef()->id) == sizeof(LB_TouchCal);
  if (ok) p.getBytes(d.panelDef()->id, &cal, sizeof cal);
  p.end();
  return ok;
}

inline void lb_save_touch_cal(LB_Display &d, const LB_TouchCal &cal) {
  Preferences p;
  if (!p.begin("lbtouch", false)) return;
  p.putBytes(d.panelDef()->id, &cal, sizeof cal);
  p.end();
}

#endif  // LB_TOUCH_CALIBRATION_H
