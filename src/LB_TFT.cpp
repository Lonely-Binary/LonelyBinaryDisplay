#include "LB_TFT.h"
#include "LB_TFTInit.h"

#include <SPI.h>
#include <driver/gpio.h>

// ─── Controller differences ─────────────────────────────────────────────────
//
// Reset timing, from each Arduino_GFX driver's tftInit(). Note ST7789: it sends
// SWRESET even after a hardware reset - the else branch is commented out
// upstream - while NV3007 never sends it at all. Both reproduced as found.

struct LB_Ctl {
  uint16_t resetMs;
  bool swResetAlways;   // SWRESET even when there is a reset pin
  bool swResetNever;    // not even when there is not
};

static const LB_Ctl kCtl[] = {
  /* ST7735  */ { 50,  false, false },
  /* ST7789  */ { 120, true,  false },
  /* ST7796  */ { 120, false, false },
  /* NV3007  */ { 120, false, true  },
  /* ILI9341 */ { 150, false, false },
  /* ILI9488 */ { 150, false, false },
};

static const uint8_t *initTable(const LB_PanelDef *p) {
  switch (p->driver) {
    case LB_DRV_ST7735:  return LB_INITSEQ_ST7735;
    case LB_DRV_ST7789:  return LB_INITSEQ_ST7789;
    case LB_DRV_ST7796:  return LB_INITSEQ_ST7796;
    case LB_DRV_ILI9341: return LB_INITSEQ_ILI9341;
    // The 8-bit parallel ILI9488 driver - 16-bit colour. Over SPI the ILI9488
    // only takes 18-bit colour, which is a different init and a different
    // pixel format; that is not what this table is.
    case LB_DRV_ILI9488: return LB_INITSEQ_ILI9488;
    case LB_DRV_NV3007:
      // The 2.79" is the same silicon as the 1.68" but needs its own
      // voltage/gamma table — without it the panel comes up looking wrong.
      return p->initOps == LB_INIT_NV3007_279 ? LB_INITSEQ_NV3007_279
                                              : LB_INITSEQ_NV3007_168;
    case LB_DRV_RGB:
      break;  // no controller, so no init table: LB_RgbScreen drives these
  }
  return nullptr;
}

// MADCTL (0x36) is the same register on every controller we drive, and bit 3
// selects BGR. The rotation bits are not the same between families, so both
// mappings are transcribed here from the Arduino_GFX drivers. This is chip
// register semantics rather than library internals, so it is stable.
static uint8_t lb_madctl(LB_Driver drv, uint8_t r, bool bgr) {
  const uint8_t MY = 0x80, MX = 0x40, MV = 0x20, BGR = 0x08;
  uint8_t bits;
  if (drv == LB_DRV_ST7735) {
    switch (r & 3) {
      case 1:  bits = MY | MV; break;
      case 2:  bits = 0;       break;
      case 3:  bits = MX | MV; break;
      default: bits = MX | MY; break;
    }
  } else if (drv == LB_DRV_ILI9341 || drv == LB_DRV_ILI9488) {
    // A third mapping again — ILI9341 agrees with neither family above.
    // ILI9488 uses the same one.
    switch (r & 3) {
      case 1:  bits = MV;           break;
      case 2:  bits = MY;           break;
      case 3:  bits = MX | MY | MV; break;
      default: bits = MX;           break;
    }
  } else {  // ST7789 / ST7796 / NV3007 share one mapping
    switch (r & 3) {
      case 1:  bits = MX | MV; break;
      case 2:  bits = MX | MY; break;
      case 3:  bits = MY | MV; break;
      default: bits = 0;       break;
    }
  }
  return bits | (bgr ? BGR : 0);
}

// ─── LB_TFT ─────────────────────────────────────────────────────────────────

bool LB_TFT::begin(int32_t hz, bool bgr, bool invert) {
  _bgr = bgr;
  if (!_bus->begin(hz)) return false;
  reset();
  runTable(initTable(_panel));
  setInverted(invert);
  setRotation(_panel->rotation);
  _bus->beginWrite();
  window(0, 0, _w, _h);
  _bus->endWrite();
  return true;
}

void LB_TFT::reset() {
  const LB_Ctl &c = kCtl[_panel->driver];
  if (_rst >= 0) _bus->hardwareReset(_rst, c.resetMs);
  if (c.swResetAlways || (_rst < 0 && !c.swResetNever)) {
    _bus->beginWrite();
    _bus->command(0x01);
    _bus->endWrite();
    _bus->delayMs(c.resetMs);
  }
}

// cmd, nbytes, delay_ms, data... terminated by 0xFF 0xFF. A bare 0xFF cannot
// be the end marker on its own: NV3007 uses 0xFF as a real command (its
// register-bank unlock), always with one data byte.
void LB_TFT::runTable(const uint8_t *t) {
  if (!t) return;
  _bus->beginWrite();
  while (!(t[0] == 0xFF && t[1] == 0xFF)) {
    const uint8_t n = t[1], ms = t[2];
    _bus->command(t[0]);
    if (n) _bus->data(t + 3, n);
    if (ms) {
      _bus->endWrite();
      _bus->delayMs(ms);
      _bus->beginWrite();
    }
    t += 3 + n;
  }
  _bus->endWrite();
}

void LB_TFT::setInverted(bool invert) {
  _bus->beginWrite();
  _bus->command(invert ? 0x21 : 0x20);  // INVON : INVOFF
  _bus->endWrite();
}

void LB_TFT::setColorOrder(bool bgr) {
  _bgr = bgr;
  writeMadctl();
}

// The offset pairing is Arduino_TFT::setRotation()'s, kept exactly: col/row
// offset 1 are the gaps on the top-left side of controller RAM, 2 the gaps on
// the bottom-right, and rotating the scan moves which of them lands on x.
void LB_TFT::setRotation(uint8_t r) {
  const LB_PanelDef *p = _panel;
  _rotation = r & 3;
  const bool landscape = _rotation & 1;
  _w = landscape ? p->height : p->width;
  _h = landscape ? p->width : p->height;
  switch (_rotation) {
    case 1:  _xStart = p->rowOff1; _yStart = p->colOff2; break;
    case 2:  _xStart = p->colOff2; _yStart = p->rowOff2; break;
    case 3:  _xStart = p->rowOff2; _yStart = p->colOff1; break;
    default: _xStart = p->colOff1; _yStart = p->rowOff1; break;
  }
  _curX = _curY = _curW = _curH = 0xFFFF;
  writeMadctl();
}

void LB_TFT::writeMadctl() {
  const uint8_t v = lb_madctl(_panel->driver, _rotation, _bgr);
  _bus->beginWrite();
  _bus->command(0x36);
  _bus->data(&v, 1);
  _bus->endWrite();
}

void LB_TFT::cmd16x2(uint8_t c, uint16_t a, uint16_t b) {
  _bus->command(c);
  _bus->data32(((uint32_t)a << 16) | b);
}

// Skips CASET or RASET when it would repeat the last one, as Arduino_GFX did.
// NV3007 caches the two together — change either and both go out — and that
// difference is reproduced too; see the note at the top of LB_TFT.h.
void LB_TFT::window(int16_t x, int16_t y, int16_t w, int16_t h) {
  const bool xChanged = (uint16_t)x != _curX || (uint16_t)w != _curW;
  const bool yChanged = (uint16_t)y != _curY || (uint16_t)h != _curH;
  const bool both = _panel->driver == LB_DRV_NV3007 && (xChanged || yChanged);
  if (xChanged || both) {
    cmd16x2(0x2A, x + _xStart, x + w - 1 + _xStart);  // CASET
    _curX = x; _curW = w;
  }
  if (yChanged || both) {
    cmd16x2(0x2B, y + _yStart, y + h - 1 + _yStart);  // RASET
    _curY = y; _curH = h;
  }
  _bus->command(0x2C);  // RAMWR
}

void LB_TFT::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > _w) w = _w - x;
  if (y + h > _h) h = _h - y;
  if (w <= 0 || h <= 0) return;
  _bus->beginWrite();
  window(x, y, w, h);
  _bus->repeat(color, (uint32_t)w * h);
  _bus->endWrite();
}

void LB_TFT::pushImage(int16_t x, int16_t y, int16_t w, int16_t h,
                       const uint16_t *px) {
  // Clip, remembering where the visible part starts inside the source.
  int16_t sx = 0, sy = 0, cw = w, ch = h;
  if (x < 0) { sx = -x; cw += x; x = 0; }
  if (y < 0) { sy = -y; ch += y; y = 0; }
  if (x + cw > _w) cw = _w - x;
  if (y + ch > _h) ch = _h - y;
  if (cw <= 0 || ch <= 0) return;
  _bus->beginWrite();
  window(x, y, cw, ch);
  if (cw == w) {
    _bus->pixels(px + (size_t)sy * w, (uint32_t)cw * ch);  // rows contiguous
  } else {
    for (int16_t r = 0; r < ch; r++)
      _bus->pixels(px + (size_t)(sy + r) * w + sx, cw);
  }
  _bus->endWrite();
}

// ─── LB_TFTSpiBus ───────────────────────────────────────────────────────────

LB_TFTSpiBus::~LB_TFTSpiBus() {
  if (_spi) {
    _spi->end();
    delete _spi;
  }
}

bool LB_TFTSpiBus::begin(int32_t hz) {
  pinMode(_dc, OUTPUT);
  digitalWrite(_dc, HIGH);
  if (_cs >= 0) {
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
  }
  _spi = new SPIClass(_host);
  if (!_spi) return false;
  // No MISO, and CS is ours: the panel is write-only here.
  _spi->begin(_sclk, -1, _mosi, -1);
  _bus = _spi->bus();
  if (!_bus) return false;
  _div = spiFrequencyToClockDiv(_bus, hz);
  return true;
}

// A transaction per write, not one for the life of the panel: the bus is
// shared (an SD card on the same pins is common), and the transaction is what
// holds the others off. See the note on the class for why not SPIClass's.
void LB_TFTSpiBus::beginWrite() {
  spiTransaction(_bus, _div, SPI_MODE0, SPI_MSBFIRST);
  if (_cs >= 0) gpio_set_level((gpio_num_t)_cs, 0);
}

void LB_TFTSpiBus::endWrite() {
  if (_cs >= 0) gpio_set_level((gpio_num_t)_cs, 1);
  spiEndTransaction(_bus);
}

// The HAL writes block until the last bit is out, so DC can be flipped
// straight after without clipping the byte in flight.
void LB_TFTSpiBus::command(uint8_t c) {
  gpio_set_level((gpio_num_t)_dc, 0);
  spiWriteByteNL(_bus, c);
  gpio_set_level((gpio_num_t)_dc, 1);
}

void LB_TFTSpiBus::data(const uint8_t *d, uint32_t n) {
  spiWriteNL(_bus, d, n);
}

// The HAL swaps words to high byte first under MSBFIRST.
void LB_TFTSpiBus::data32(uint32_t v) {
  spiWriteLongNL(_bus, v);
}

void LB_TFTSpiBus::repeat(uint16_t color, uint32_t n) {
  // Text is mostly runs of one to a few pixels; skip the buffer for those.
  if (n <= 2) {
    if (n == 2) spiWriteLongNL(_bus, ((uint32_t)color << 16) | color);
    else if (n == 1) spiWriteShortNL(_bus, color);
    return;
  }
  // A block of the colour, high byte first, sent as many times as needed.
  // 128 pixels is four FIFO loads on the ESP32; larger buys little.
  uint8_t buf[256];
  const uint8_t hi = color >> 8, lo = color & 0xFF;
  const uint32_t fill = n < 128 ? n : 128;
  for (uint32_t i = 0; i < fill; i++) {
    buf[2 * i] = hi;
    buf[2 * i + 1] = lo;
  }
  while (n) {
    const uint32_t c = n < 128 ? n : 128;
    spiWriteNL(_bus, buf, c * 2);
    n -= c;
  }
}

// The HAL's 16-bit write sends each pixel high byte first, which is the
// order every one of these controllers expects.
void LB_TFTSpiBus::pixels(const uint16_t *px, uint32_t n) {
  spiWritePixelsNL(_bus, px, n * 2);
}

void LB_TFTSpiBus::hardwareReset(int8_t rst, uint16_t ms) {
  pinMode(rst, OUTPUT);
  digitalWrite(rst, HIGH);
  delay(100);
  digitalWrite(rst, LOW);
  delay(ms);
  digitalWrite(rst, HIGH);
  delay(ms);
}
