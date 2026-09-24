/*
 * LB_TouchXPT2046 — XPT2046 (ADS7843) resistive touch, SPI, one finger.
 *
 *     LB_TouchXPT2046 touch(12, 13, 11, 38, 18);   // SCK, MISO, MOSI, CS, IRQ
 *     touch.begin();
 *     int16_t x, y;
 *     if (touch.getTouch(&x, &y)) { ... }
 *
 * A resistive panel is two resistive sheets; the chip measures where they
 * touch as a raw 12-bit number per axis. Those numbers do not start at 0 or
 * reach 4095 at the edges of the picture, and they differ from panel to
 * panel, so they are mapped to pixels with the raw values at the screen's
 * edges: setRawRange({left, right, top, bottom}). The panel table carries
 * them for a known board; rawX()/rawY() are there to measure your own.
 *
 * Pressure: the chip also measures how hard the sheets are pressed (Z). A
 * reading below the threshold is "not touched" - that is what stops a light
 * brush or noise from registering.
 *
 * Header-only, like LB_TouchGT911, and for the same reason: it is opt-in.
 *
 * MIT License · Lonely Binary
 */
#ifndef LB_TOUCH_XPT2046_H
#define LB_TOUCH_XPT2046_H

#include <SPI.h>
#include "LB_Touch.h"

class LB_TouchXPT2046 : public LB_Touch {
 public:
  LB_TouchXPT2046(int8_t sck, int8_t miso, int8_t mosi, int8_t cs, int8_t irq = -1,
                  uint32_t hz = 2000000)
      : _sck(sck), _miso(miso), _mosi(mosi), _cs(cs), _irq(irq), _hz(hz) {}
  ~LB_TouchXPT2046() { delete _spi; }

  bool begin() override;

  void setRawRange(const int16_t raw[4]) override {
    if (raw[0] != raw[1] && raw[2] != raw[3]) memcpy(_raw, raw, sizeof _raw);
  }
  void setScreenSize(int16_t w, int16_t h) override { _nativeW = w; _nativeH = h; }

  // Minimum pressure that counts as a touch. Lower = lighter touch, more noise.
  void setPressureThreshold(uint16_t z) { _zMin = z; }

  // The last raw reading, for calibrating a panel of your own.
  int16_t rawX() const { return _rx; }
  int16_t rawY() const { return _ry; }
  int16_t rawZ() const { return _rz; }

 protected:
  uint8_t readRaw(LB_TouchPoint *pts, uint8_t max) override;

 private:
  uint16_t sample(uint8_t cmd);

  int8_t _sck, _miso, _mosi, _cs, _irq;
  uint32_t _hz;
  SPIClass *_spi = nullptr;
  int16_t _raw[4] = {0, 4095, 0, 4095};
  uint16_t _zMin = 400;
  int16_t _rx = 0, _ry = 0, _rz = 0;
};

// ── Implementation ──────────────────────────────────────────────────────────

inline bool LB_TouchXPT2046::begin() {
  if (_nativeW == 0) { _nativeW = 4096; _nativeH = 4096; }   // raw until told the screen size
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  // PENIRQ is open-drain: it only ever pulls low. Without a pull-up it floats
  // high and every reading is skipped - measured on the Sunton 4827S043R,
  // which has none on the board.
  if (_irq >= 0) pinMode(_irq, INPUT_PULLUP);
  // Its own SPI host: on these boards the touch sits alone (or with the TF
  // card) on a bus of its own, never with the display.
  _spi = new SPIClass(FSPI);
  _spi->begin(_sck, _miso, _mosi, -1);
  // If MISO reads back all ones, there is no chip there.
  const uint16_t z1 = sample(0xB1);
  // The low two bits of every command (PD1 PD0) set the state the chip is left
  // in. 0x80 = powered down between conversions with the pen interrupt ARMED;
  // the 0xB1 above leaves it DISABLED, and IRQ would then never go low. So the
  // last command before waiting on IRQ must always be 0x80.
  sample(0x80);
  return z1 != 0xFFF;
}

// One conversion: send the command, read 12 bits.
inline uint16_t LB_TouchXPT2046::sample(uint8_t cmd) {
  _spi->beginTransaction(SPISettings(_hz, MSBFIRST, SPI_MODE0));
  digitalWrite(_cs, LOW);
  _spi->transfer(cmd);
  const uint16_t v = _spi->transfer16(0) >> 3;
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
  return v & 0xFFF;
}

inline uint8_t LB_TouchXPT2046::readRaw(LB_TouchPoint *pts, uint8_t max) {
  if (!_spi || !max) return 0;
  // Pen interrupt is active low while touched: skip the SPI entirely otherwise.
  if (_irq >= 0 && digitalRead(_irq)) { _rz = 0; return 0; }

  const int16_t z1 = sample(0xB1), z2 = sample(0xC1);
  _rz = z1 + 4095 - z2;
  if (_rz < _zMin) { sample(0x80); return 0; }   // re-arm IRQ on this exit too

  // Several readings per axis; the first after switching axis is noisy, and
  // the median of the rest rejects the odd spike a resistive sheet produces.
  int16_t xs[5], ys[5];
  sample(0xD1);
  for (auto &v : xs) v = sample(0xD1);
  sample(0x91);
  for (auto &v : ys) v = sample(0x91);
  sample(0x80);                                       // power down, IRQ armed
  auto median = [](int16_t *a) {
    for (int i = 1; i < 5; i++)
      for (int j = i; j > 0 && a[j - 1] > a[j]; j--) { int16_t t = a[j]; a[j] = a[j - 1]; a[j - 1] = t; }
    return a[2];
  };
  _rx = median(xs);
  _ry = median(ys);

  // Raw to pixels. Works whichever way round the raw axis runs.
  const int32_t x = (int32_t)(_rx - _raw[0]) * (_nativeW - 1) / (_raw[1] - _raw[0]);
  const int32_t y = (int32_t)(_ry - _raw[2]) * (_nativeH - 1) / (_raw[3] - _raw[2]);
  pts[0].x = x < 0 ? 0 : x >= _nativeW ? _nativeW - 1 : x;
  pts[0].y = y < 0 ? 0 : y >= _nativeH ? _nativeH - 1 : y;
  pts[0].size = _rz;
  pts[0].id = 0;
  return 1;
}

namespace {
struct LB_TouchXPT2046Registrar {
  LB_TouchXPT2046Registrar() {
    LB_Touch::registerDriver(LB_TOUCH_XPT2046, [](const LB_TouchPins &p) -> LB_Touch * {
      return new LB_TouchXPT2046(p.sck, p.miso, p.mosi, p.cs, p.intr);
    });
  }
} lb_touchXPT2046Registrar;
}  // namespace

#endif  // LB_TOUCH_XPT2046_H
