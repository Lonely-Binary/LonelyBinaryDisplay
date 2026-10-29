/*
 * LB_TouchIT7259 — ITE IT7259 capacitive touch, I2C, one finger plus gestures.
 *
 *     LB_TouchIT7259 touch(27, 21);   // SDA, SCL
 *     touch.begin();
 *     int16_t x, y;
 *     if (touch.getTouch(&x, &y)) { ... }
 *
 * The chip on the 1.28 inch Tap panel. It answers at 0x46 and keeps its latest
 * report in a 14-byte buffer at register 0xE0, laid out as the panel maker's
 * test code reads it:
 *
 *     [0]  flags: 0x08 a finger is down, 0x80 a gesture is in this report
 *     [1]  gesture: 0x20 tap, 0x22 flick, 0x23 double tap
 *     [2]  X, 0-239
 *     [4]  Y, 0-239
 *     [10] flick direction: 0x08 up, 0x0A right, 0x0C down, 0x0E left
 *
 * The chip detects taps and flicks itself. takeGesture() hands each one out
 * once; the report stays in the buffer until the next one replaces it, so the
 * driver remembers which it has already given.
 *
 * RST, when wired, is pulsed low in begin(). INT is not used: the chip is
 * polled. Pass -1 for either if it is not connected.
 *
 * Header-only on purpose, like LB_TouchGT911.h: a .cpp here would drag Wire
 * (about 22 KB) into every display sketch, touch or not.
 *
 * MIT License · Lonely Binary
 */
#ifndef LB_TOUCH_IT7259_H
#define LB_TOUCH_IT7259_H

#include <Wire.h>
#include "LB_Touch.h"

class LB_TouchIT7259 : public LB_Touch {
 public:
  enum Gesture : uint8_t {
    GESTURE_NONE,
    GESTURE_TAP,
    GESTURE_DOUBLE_TAP,
    GESTURE_FLICK_UP,
    GESTURE_FLICK_RIGHT,
    GESTURE_FLICK_DOWN,
    GESTURE_FLICK_LEFT,
  };

  LB_TouchIT7259(int8_t sda, int8_t scl, int8_t intPin = -1, int8_t rstPin = -1,
                 TwoWire &wire = Wire, uint32_t hz = 400000)
      : _wire(wire), _sda(sda), _scl(scl), _int(intPin), _rst(rstPin), _hz(hz) {
    _nativeW = 240;   // the chip reports in pixels of the 240 x 240 glass
    _nativeH = 240;
  }

  bool begin() override;

  // The newest gesture the chip has reported since the last call, once; then
  // GESTURE_NONE. Flick directions are in the chip's frame, not rotated.
  Gesture takeGesture() {
    const Gesture g = _gesture;
    _gesture = GESTURE_NONE;
    return g;
  }

 protected:
  uint8_t readRaw(LB_TouchPoint *pts, uint8_t max) override;

 private:
  static constexpr uint8_t ADDR          = 0x46;
  static constexpr uint8_t REG_POINT     = 0xE0;
  static constexpr uint8_t FLAG_POINT    = 0x08;
  static constexpr uint8_t FLAG_GESTURE  = 0x80;
  static constexpr uint8_t GEST_TAP      = 0x20;
  static constexpr uint8_t GEST_FLICK    = 0x22;
  static constexpr uint8_t GEST_DBL_TAP  = 0x23;
  static constexpr uint8_t DIR_UP        = 0x08;
  static constexpr uint8_t DIR_RIGHT     = 0x0A;
  static constexpr uint8_t DIR_DOWN      = 0x0C;
  static constexpr uint8_t DIR_LEFT      = 0x0E;
  static constexpr uint8_t REPORT_LEN    = 14;

  bool readReport(uint8_t *buf);

  TwoWire &_wire;
  int8_t   _sda, _scl, _int, _rst;
  uint32_t _hz;
  Gesture  _gesture = GESTURE_NONE;
  uint8_t  _lastReport[REPORT_LEN] = {0};
  bool     _haveLast = false;
};


// ── Implementation ──────────────────────────────────────────────────────────

inline bool LB_TouchIT7259::begin() {
  if (_int >= 0) pinMode(_int, INPUT);
  if (_rst >= 0) {
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, HIGH);
    delay(5);
    digitalWrite(_rst, LOW);
    delay(20);
    digitalWrite(_rst, HIGH);
    delay(150);   // generous: the chip loads its firmware after reset
  }
  _wire.begin(_sda, _scl, _hz);
  _wire.setTimeOut(50);   // a stuck bus must not freeze the sketch
  delay(50);
  _wire.beginTransmission(ADDR);
  return _wire.endTransmission() == 0;
}

inline bool LB_TouchIT7259::readReport(uint8_t *buf) {
  _wire.beginTransmission(ADDR);
  _wire.write(REG_POINT);
  if (_wire.endTransmission(false) != 0) return false;
  if (_wire.requestFrom((int)ADDR, (int)REPORT_LEN) != REPORT_LEN) {
    while (_wire.available()) (void)_wire.read();
    return false;
  }
  for (uint8_t i = 0; i < REPORT_LEN; i++) buf[i] = _wire.read();
  return true;
}

inline uint8_t LB_TouchIT7259::readRaw(LB_TouchPoint *pts, uint8_t max) {
  uint8_t buf[REPORT_LEN];
  if (max == 0 || !readReport(buf)) return 0;

  // A gesture stays in the buffer until the next report; only a changed
  // buffer is a new one.
  const bool isNew = !_haveLast || memcmp(buf, _lastReport, REPORT_LEN) != 0;
  memcpy(_lastReport, buf, REPORT_LEN);
  _haveLast = true;

  if (isNew && (buf[0] & FLAG_GESTURE)) {
    switch (buf[1]) {
      case GEST_TAP:     _gesture = GESTURE_TAP; break;
      case GEST_DBL_TAP: _gesture = GESTURE_DOUBLE_TAP; break;
      case GEST_FLICK:
        switch (buf[10]) {
          case DIR_UP:    _gesture = GESTURE_FLICK_UP; break;
          case DIR_RIGHT: _gesture = GESTURE_FLICK_RIGHT; break;
          case DIR_DOWN:  _gesture = GESTURE_FLICK_DOWN; break;
          case DIR_LEFT:  _gesture = GESTURE_FLICK_LEFT; break;
          default: break;
        }
        break;
      default: break;
    }
  }

  if (!(buf[0] & FLAG_POINT)) return 0;
  pts[0].x = buf[2];
  pts[0].y = buf[4];
  pts[0].size = 0;
  pts[0].id = 0;
  return 1;
}

// Including this header is what lets LB_Display build an IT7259 for a panel
// whose table entry names one (see LB_Touch::registerDriver).
namespace {
struct LB_TouchIT7259Registrar {
  LB_TouchIT7259Registrar() {
    LB_Touch::registerDriver(LB_TOUCH_IT7259, [](const LB_TouchPins &p) -> LB_Touch * {
      return new LB_TouchIT7259(p.sda, p.scl, p.intr, p.rst);
    });
  }
} lb_touchIT7259Registrar;
}  // namespace

#endif  // LB_TOUCH_IT7259_H
