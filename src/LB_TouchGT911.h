/*
 * LB_TouchGT911 — Goodix GT911 capacitive touch, I2C, up to 5 fingers.
 *
 *     LB_TouchGT911 touch(21, 22, 18, 19);   // SDA, SCL, INT, RST
 *     touch.begin();
 *     int16_t x, y;
 *     if (touch.getTouch(&x, &y)) { ... }
 *
 * Address. The GT911 picks 0x5D or 0x14 from the level on INT while RST
 * rises. With RST wired, begin() runs that sequence and asks for 0x5D. With
 * RST not wired (-1) the chip keeps whatever it chose at power-up, so begin()
 * tries both.
 *
 * Resolution is read out of the chip's own configuration, which the panel
 * maker programs to match the glass — nothing to set by hand.
 *
 * INT is only used for the address strap; readings are polled. Pass -1 if it
 * is not wired.
 *
 * Header-only on purpose: Arduino links every library that any .cpp in this
 * library includes. A LB_TouchGT911.cpp would drag Wire (and its global object,
 * about 22 KB) into every display sketch, touch or not. As a header it only
 * arrives when a sketch includes it.
 *
 * MIT License · Lonely Binary
 */
#ifndef LB_TOUCH_GT911_H
#define LB_TOUCH_GT911_H

#include <Wire.h>
#include "LB_Touch.h"

class LB_TouchGT911 : public LB_Touch {
 public:
  LB_TouchGT911(int8_t sda, int8_t scl, int8_t intPin = -1, int8_t rstPin = -1,
                TwoWire &wire = Wire, uint32_t hz = 400000)
      : _wire(wire), _sda(sda), _scl(scl), _int(intPin), _rst(rstPin), _hz(hz) {}

  bool begin() override;

  // 0x5D or 0x14 once begin() has found the chip, else 0.
  uint8_t address() const { return _addr; }

  // Product ID as the chip reports it, e.g. "911". Empty before begin().
  const char *productId() const { return _pid; }
  uint16_t firmwareVersion() const { return _fw; }

 protected:
  uint8_t readRaw(LB_TouchPoint *pts, uint8_t max) override;

 private:
  // Registers (GT911 programming guide).
  static constexpr uint16_t REG_CONFIG  = 0x8047;  // config version, then X/Y max
  static constexpr uint16_t REG_PID     = 0x8140;  // 4 ASCII bytes + 2 bytes firmware
  static constexpr uint16_t REG_STATUS  = 0x814E;  // bit 7 buffer ready, low nibble count
  static constexpr uint16_t REG_POINTS  = 0x814F;  // 8 bytes per point

  bool readReg(uint16_t reg, uint8_t *buf, uint8_t n);
  bool writeReg(uint16_t reg, uint8_t v);
  bool probe(uint8_t addr);

  TwoWire &_wire;
  int8_t   _sda, _scl, _int, _rst;
  uint32_t _hz;
  uint8_t  _addr = 0;
  char     _pid[5] = {0};
  uint16_t _fw = 0;

  // Last report. The chip refreshes at its own rate (about 100 Hz); between
  // reports the buffer-ready bit is clear and we answer from here.
  LB_TouchPoint _last[MAX_POINTS];
  uint8_t       _lastN = 0;
};


// ── Implementation ──────────────────────────────────────────────────────────

inline bool LB_TouchGT911::readReg(uint16_t reg, uint8_t *buf, uint8_t n) {
  _wire.beginTransmission(_addr);
  _wire.write(reg >> 8);
  _wire.write(reg & 0xFF);
  if (_wire.endTransmission() != 0) return false;
  if (_wire.requestFrom(_addr, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = _wire.read();
  return true;
}

inline bool LB_TouchGT911::writeReg(uint16_t reg, uint8_t v) {
  _wire.beginTransmission(_addr);
  _wire.write(reg >> 8);
  _wire.write(reg & 0xFF);
  _wire.write(v);
  return _wire.endTransmission() == 0;
}

inline bool LB_TouchGT911::probe(uint8_t addr) {
  _addr = addr;
  uint8_t id[6];
  if (readReg(REG_PID, id, 6)) {
    memcpy(_pid, id, 4);
    _pid[4] = 0;
    for (int i = 3; i >= 0 && (_pid[i] == ' ' || _pid[i] == 0); i--) _pid[i] = 0;
    _fw = id[4] | id[5] << 8;
    return true;
  }
  _addr = 0;
  return false;
}

inline bool LB_TouchGT911::begin() {
  if (_rst >= 0) {
    // Reset with INT held low across the RST edge → address 0x5D. Timings are
    // the datasheet minimums with margin: >10 ms low, >100 us INT setup, then
    // INT released after 5 ms and >50 ms before the first I2C access.
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    if (_int >= 0) { pinMode(_int, OUTPUT); digitalWrite(_int, LOW); }
    delay(11);
    digitalWrite(_rst, HIGH);
    delay(6);
    if (_int >= 0) pinMode(_int, INPUT);
    delay(55);
  }
  _wire.begin(_sda, _scl, _hz);

  if (!probe(0x5D) && !probe(0x14)) return false;

  uint8_t cfg[5];
  if (!readReg(REG_CONFIG, cfg, 5)) return false;
  _nativeW = cfg[1] | cfg[2] << 8;
  _nativeH = cfg[3] | cfg[4] << 8;

  _lastN = 0;
  writeReg(REG_STATUS, 0);
  return true;
}

inline uint8_t LB_TouchGT911::readRaw(LB_TouchPoint *pts, uint8_t max) {
  uint8_t st;
  if (_addr && readReg(REG_STATUS, &st, 1) && (st & 0x80)) {
    uint8_t n = st & 0x0F;
    if (n > MAX_POINTS) n = 0;   // out of range: the report is junk, drop it
    uint8_t raw[MAX_POINTS * 8];
    if (n == 0 || readReg(REG_POINTS, raw, n * 8)) {
      for (uint8_t i = 0; i < n; i++) {
        const uint8_t *q = raw + i * 8;
        _last[i].id   = q[0];
        _last[i].x    = q[1] | q[2] << 8;
        _last[i].y    = q[3] | q[4] << 8;
        _last[i].size = q[5] | q[6] << 8;
      }
      _lastN = n;
    }
    // Hand the buffer back; the chip stops updating until this is cleared.
    writeReg(REG_STATUS, 0);
  }
  const uint8_t n = _lastN < max ? _lastN : max;
  memcpy(pts, _last, n * sizeof(LB_TouchPoint));
  return n;
}

// Including this header is what lets LB_Display build a GT911 for a panel whose
// table entry names one (see LB_Touch::registerDriver). One registrar per
// translation unit; registering twice is harmless.
namespace {
struct LB_TouchGT911Registrar {
  LB_TouchGT911Registrar() {
    LB_Touch::registerDriver(LB_TOUCH_GT911, [](const LB_TouchPins &p) -> LB_Touch * {
      return new LB_TouchGT911(p.sda, p.scl, p.intr, p.rst);
    });
  }
} lb_touchGT911Registrar;
}  // namespace

#endif  // LB_TOUCH_GT911_H
