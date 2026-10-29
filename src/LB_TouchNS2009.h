/*
 * LB_TouchNS2009 — NS2009 (TSC2007-compatible) resistive touch, I2C, one point.
 *
 *     LB_TouchNS2009 touch(4, 6, 5);   // SDA, SCL, IRQ
 *     touch.begin();
 *     int16_t x, y;
 *     if (touch.getTouch(&x, &y)) { ... }
 *
 * A resistive panel is two sheets that touch where you press. The chip measures
 * where, as a raw 12-bit number per axis, and how hard (Z1). The raw numbers are
 * not pixels, and they differ from panel to panel and with how the layer was
 * glued on - even which way round X and Y are. So they are mapped to pixels with
 * the raw values at the screen's edges, which the panel table carries:
 * setRawRange({left, right, top, bottom}). A range may run backwards (left >
 * right), which is how a mirrored layer is handled. If the layer's X runs along
 * the screen's Y, set swapXY with setOrientation(): the ranges are then in the
 * swapped frame (the first pair along the screen's Y, the second along its X).
 * The RtpCheck example works all this out from four taps.
 *
 * Needs a firm press: a nail or a stylus. A finger pad brushing the glass is not
 * enough, and that is how resistive layers are, not a fault.

 * A press is judged by the pressure reading alone. The chip's PENIRQ line is not
 * wired on many modules, and a floating input then sits high for ever, so it is
 * never used to decide anything (the pin is only given a pull-up).
 *
 * The chip's address is one of 0x48-0x4B, set by two pins on the module. begin()
 * finds it. Each reading is the middle of seven samples, so it does not jitter;
 * a press counts when Z1 is above a threshold.
 *
 * Header-only, like the other touch drivers: Wire is only linked in when a sketch
 * includes this.
 *
 * MIT License · Lonely Binary
 */
#ifndef LB_TOUCH_NS2009_H
#define LB_TOUCH_NS2009_H

#include <Wire.h>
#include "LB_Touch.h"

class LB_TouchNS2009 : public LB_Touch {
 public:
  LB_TouchNS2009(int8_t sda, int8_t scl, int8_t irq = -1, TwoWire &wire = Wire, uint32_t hz = 200000)
      : _wire(wire), _sda(sda), _scl(scl), _irq(irq), _hz(hz) {}

  bool begin() override;

  void setRawRange(const int16_t raw[4]) override {
    if (raw[0] != raw[1] && raw[2] != raw[3]) memcpy(_raw, raw, sizeof _raw);
  }
  void setScreenSize(int16_t w, int16_t h) override { _pw = w; _ph = h; }

  // Minimum Z1 that counts as a press. Raise it if noise registers as touches,
  // lower it if a firm press is missed.
  void setPressureThreshold(uint16_t z) { _zMin = z; }

  uint8_t address() const { return _addr; }

  // The last reading, raw, for calibrating a panel of your own.
  int16_t rawX() const { return _rx; }
  int16_t rawY() const { return _ry; }
  int16_t rawZ() const { return _rz; }

 protected:
  uint8_t readRaw(LB_TouchPoint *pts, uint8_t max) override;

 private:
  static constexpr uint8_t CMD_X = 0xC0, CMD_Y = 0xD0, CMD_Z1 = 0xE0;
  static constexpr uint8_t SAMPLES = 7;

  bool readChannel(uint8_t addr, uint8_t cmd, uint16_t &v);
  bool sample(int16_t &x, int16_t &y, int16_t &z);

  TwoWire &_wire;
  int8_t   _sda, _scl, _irq;
  uint32_t _hz;
  uint8_t  _addr = 0;
  uint16_t _zMin = 50;
  int16_t  _pw = 0, _ph = 0;                       // the screen at rotation 0
  int16_t  _raw[4] = {0, 4095, 0, 4095};           // see the header comment
  int16_t  _rx = 0, _ry = 0, _rz = 0;
  LB_TouchPoint _last = {0, 0, 0, 0};
  uint8_t  _missed = 255;                          // reads since the last good one
};


// ── Implementation ──────────────────────────────────────────────────────────

inline bool LB_TouchNS2009::readChannel(uint8_t addr, uint8_t cmd, uint16_t &v) {
  _wire.beginTransmission(addr);
  _wire.write(cmd);
  if (_wire.endTransmission(true) != 0) return false;
  delayMicroseconds(200);                          // one conversion
  if (_wire.requestFrom(addr, (uint8_t)2) != 2) return false;
  const uint8_t hi = _wire.read(), lo = _wire.read();
  v = (uint16_t)(((uint16_t)hi << 4) | (lo >> 4)) & 0x0FFF;
  return true;
}

inline bool LB_TouchNS2009::begin() {
  if (_irq >= 0) pinMode(_irq, INPUT_PULLUP);      // PENIRQ is open drain
  _wire.begin(_sda, _scl, _hz);
  _wire.setTimeOut(50);
  for (uint8_t a = 0x48; a <= 0x4B; a++) {
    uint16_t z;
    _wire.beginTransmission(a);
    if (_wire.endTransmission() != 0) continue;    // nothing at this address
    if (readChannel(a, CMD_Z1, z)) { _addr = a; return true; }
  }
  return false;
}

// The middle of SAMPLES readings: sorted, the top and bottom quarter dropped, the
// rest averaged. Any one reading below the pressure threshold throws the lot away,
// so the instant a stylus lifts cannot leak a stray value in.
inline bool LB_TouchNS2009::sample(int16_t &x, int16_t &y, int16_t &z) {
  uint16_t xs[SAMPLES], ys[SAMPLES], zs[SAMPLES];
  for (uint8_t i = 0; i < SAMPLES; i++) {
    if (!readChannel(_addr, CMD_Z1, zs[i])) return false;
    if (zs[i] < _zMin || zs[i] > 4000) return false;   // not pressed: do not bother reading X and Y
    if (!readChannel(_addr, CMD_X, xs[i]) || !readChannel(_addr, CMD_Y, ys[i])) return false;
  }
  auto sortN = [](uint16_t *a) {
    for (uint8_t i = 1; i < SAMPLES; i++) {
      const uint16_t v = a[i];
      int8_t j = i - 1;
      while (j >= 0 && a[j] > v) { a[j + 1] = a[j]; j--; }
      a[j + 1] = v;
    }
  };
  sortN(xs); sortN(ys); sortN(zs);
  const uint8_t drop = SAMPLES / 4, n = SAMPLES - 2 * drop;
  uint32_t sx = 0, sy = 0, sz = 0;
  for (uint8_t i = drop; i < SAMPLES - drop; i++) { sx += xs[i]; sy += ys[i]; sz += zs[i]; }
  x = sx / n; y = sy / n; z = sz / n;
  return true;
}

inline uint8_t LB_TouchNS2009::readRaw(LB_TouchPoint *pts, uint8_t max) {
  if (!_addr || max == 0) return 0;

  // The touch frame: the screen's, or with the axes swapped, the screen turned
  // on its side. LB_Touch::read() turns it back after this returns.
  _nativeW = swapXY() ? _ph : _pw;
  _nativeH = swapXY() ? _pw : _ph;

  int16_t x, y, z;
  const bool pressed = sample(x, y, z);
  if (pressed) {
    _rx = x; _ry = y; _rz = z;
    int32_t tx = (int32_t)(x - _raw[0]) * (_nativeW - 1) / (_raw[1] - _raw[0]);
    int32_t ty = (int32_t)(y - _raw[2]) * (_nativeH - 1) / (_raw[3] - _raw[2]);
    _last.x = constrain(tx, 0, _nativeW - 1);
    _last.y = constrain(ty, 0, _nativeH - 1);
    _last.size = z;
    _missed = 0;
  } else if (_missed < 255) {
    _missed++;
  }
  // A stroke can drop a reading or two; only a few misses in a row mean a lift.
  if (_missed >= 3) return 0;
  pts[0] = _last;
  return 1;
}

// Including this header is what lets LB_Display build an NS2009 for a panel whose
// table entry names one (see LB_Touch::registerDriver).
namespace {
struct LB_TouchNS2009Registrar {
  LB_TouchNS2009Registrar() {
    LB_Touch::registerDriver(LB_TOUCH_NS2009, [](const LB_TouchPins &p) -> LB_Touch * {
      return new LB_TouchNS2009(p.sda, p.scl, p.intr);
    });
  }
} lb_touchNS2009Registrar;
}  // namespace

#endif  // LB_TOUCH_NS2009_H
