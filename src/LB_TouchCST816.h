/*
 * LB_TouchCST816 — Hynitron CST816D / CST816S capacitive touch, I2C, one finger.
 *
 *     LB_TouchCST816 touch(4, 6, 5, 7);   // SDA, SCL, INT, RST
 *     touch.begin();
 *     int16_t x, y;
 *     if (touch.getTouch(&x, &y)) { ... }
 *
 * The chip on the 1.5 inch Tap panel. It answers at 0x15; register 0x01 holds
 * the report: gesture, finger count, then X and Y as 12-bit numbers.
 *
 * !! This driver writes NOTHING to the chip !!
 *   The vendor found that writing any configuration register (they tried 0xFE,
 *   auto-sleep) left a CST816D answering I2C and reporting its ID, but never
 *   scanning: coordinates stuck at 0, INT never pulled. So begin() only resets
 *   the chip and reads.
 *
 * The touch layer extends past the picture, so the raw numbers are not pixels.
 * The panel table carries the raw value at each edge of the screen
 * (setRawRange), and this class maps to pixels with it, as LB_TouchXPT2046 does.
 * Without it, raw numbers come back as they are, clamped to the screen.
 *
 * INT is only pulled up so it does not float; readings are polled. RST is
 * pulsed in begin() when wired. Pass -1 for either if it is not connected.
 *
 * Header-only, like the other touch drivers, so Wire is only linked in when a
 * sketch includes this.
 *
 * MIT License · Lonely Binary
 */
#ifndef LB_TOUCH_CST816_H
#define LB_TOUCH_CST816_H

#include <Wire.h>
#include "LB_Touch.h"

class LB_TouchCST816 : public LB_Touch {
 public:
  LB_TouchCST816(int8_t sda, int8_t scl, int8_t intPin = -1, int8_t rstPin = -1,
                 TwoWire &wire = Wire, uint32_t hz = 400000)
      : _wire(wire), _sda(sda), _scl(scl), _int(intPin), _rst(rstPin), _hz(hz) {}

  bool begin() override;

  void setRawRange(const int16_t raw[4]) override {
    if (raw[0] != raw[1] && raw[2] != raw[3]) memcpy(_raw, raw, sizeof _raw);
  }
  void setScreenSize(int16_t w, int16_t h) override { _nativeW = w; _nativeH = h; _scaled = true; }

  // The chip's ID register: 0xB6 on the CST816D. 0 before begin().
  uint8_t chipId() const { return _id; }

  // The last raw reading, for calibrating a panel of your own.
  int16_t rawX() const { return _rx; }
  int16_t rawY() const { return _ry; }

 protected:
  uint8_t readRaw(LB_TouchPoint *pts, uint8_t max) override;

 private:
  static constexpr uint8_t ADDR = 0x15;
  static constexpr uint8_t REG_REPORT = 0x01;   // gesture, fingers, X hi, X lo, Y hi, Y lo
  static constexpr uint8_t REG_ID = 0xA7;

  bool readReg(uint8_t reg, uint8_t *buf, uint8_t n);

  TwoWire &_wire;
  int8_t   _sda, _scl, _int, _rst;
  uint32_t _hz;
  uint8_t  _id = 0;
  bool     _scaled = false;
  int16_t  _raw[4] = {0, 0, 0, 0};   // raw x at left, right; y at top, bottom
  int16_t  _rx = 0, _ry = 0;
};


// ── Implementation ──────────────────────────────────────────────────────────

inline bool LB_TouchCST816::readReg(uint8_t reg, uint8_t *buf, uint8_t n) {
  _wire.beginTransmission(ADDR);
  _wire.write(reg);
  if (_wire.endTransmission(true) != 0) return false;
  if (_wire.requestFrom((int)ADDR, (int)n) != n) {
    while (_wire.available()) (void)_wire.read();
    return false;
  }
  for (uint8_t i = 0; i < n; i++) buf[i] = _wire.read();
  return true;
}

inline bool LB_TouchCST816::begin() {
  if (_int >= 0) pinMode(_int, INPUT_PULLUP);
  if (_rst >= 0) {
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(20);
    digitalWrite(_rst, HIGH);
    delay(120);   // the chip needs tens of ms before it answers
  }
  _wire.begin(_sda, _scl, _hz);
  _wire.setTimeOut(50);

  if (readReg(REG_ID, &_id, 1)) return true;
  // Flying wires and weak pull-ups: 400 kHz may not hold, 100 kHz usually does.
  _wire.setClock(100000);
  return readReg(REG_ID, &_id, 1);
}

inline uint8_t LB_TouchCST816::readRaw(LB_TouchPoint *pts, uint8_t max) {
  uint8_t b[6];
  if (max == 0 || !readReg(REG_REPORT, b, 6)) return 0;
  if ((b[1] & 0x0F) == 0) return 0;

  _rx = ((b[2] & 0x0F) << 8) | b[3];
  _ry = ((b[4] & 0x0F) << 8) | b[5];

  int32_t x = _rx, y = _ry;
  if (_scaled && _raw[0] != _raw[1] && _raw[2] != _raw[3]) {
    x = (x - _raw[0]) * (_nativeW - 1) / (_raw[1] - _raw[0]);
    y = (y - _raw[2]) * (_nativeH - 1) / (_raw[3] - _raw[2]);
  }
  if (_scaled) {
    x = constrain(x, 0, _nativeW - 1);
    y = constrain(y, 0, _nativeH - 1);
  }
  pts[0].x = x;
  pts[0].y = y;
  pts[0].size = 0;
  pts[0].id = 0;
  return 1;
}

// Including this header is what lets LB_Display build a CST816 for a panel whose
// table entry names one (see LB_Touch::registerDriver).
namespace {
struct LB_TouchCST816Registrar {
  LB_TouchCST816Registrar() {
    LB_Touch::registerDriver(LB_TOUCH_CST816, [](const LB_TouchPins &p) -> LB_Touch * {
      return new LB_TouchCST816(p.sda, p.scl, p.intr, p.rst);
    });
  }
} lb_touchCST816Registrar;
}  // namespace

#endif  // LB_TOUCH_CST816_H
