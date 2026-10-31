#include "LB_ST7305.h"

#include <esp_heap_caps.h>

static void lb_lumaTables();

// ─── The ST7305 panels ──────────────────────────────────────────────────────
//
// One controller, four glasses, and what differs between them is data: the
// voltages and timing in the init table, the RAM window the glass sits in, and
// how pixels are packed into the bytes of that window. Chosen by the panel's
// init_ops.
//
// Init tables use LB_TFT's format: cmd, nbytes, delay_ms, data...  terminated
// by 0xFF, 0xFF. `pre` runs up to and including the panel setting, `tail` turns
// the display on; inversion goes between them or after them (invertFirst), in
// the order each vendor's own code used.
//
// Layout 0 (the 400 x 300 glass): the frame is (w/2) x (h/4) bytes, column-pair
// major, each byte a 2-wide, 4-tall block counted from the bottom of the panel.
// Layout 1 (2.13" and 2.9"): the frame is (colOffset + w)/4 x (h/2) bytes,
// row-pair major, each byte a 4-wide, 2-tall block counted from the top.

// Waveshare ESP32-S3-RLCD-4.2 reference values. Run on a bench 2026-10-21.
static const uint8_t kPreWaveshare42[] = {
  0xD6,2,0, 0x17,0x02,
  0xD1,1,0, 0x01,
  0xC0,2,0, 0x11,0x04,
  0xC1,4,0, 0x69,0x69,0x69,0x69,
  0xC2,4,0, 0x19,0x19,0x19,0x19,
  0xC4,4,0, 0x4B,0x4B,0x4B,0x4B,
  0xC5,4,0, 0x19,0x19,0x19,0x19,
  0xD8,2,0, 0x80,0xE9,
  0xB2,1,0, 0x02,
  0xB3,10,0, 0xE5,0xF6,0x05,0x46,0x77,0x77,0x77,0x77,0x76,0x45,
  0xB4,8,0, 0x05,0x46,0x77,0x77,0x77,0x77,0x76,0x45,
  0x62,3,0, 0x32,0x03,0x1F,
  0xB7,1,0, 0x13,
  0xB0,1,0, 0x64,
  0x11,0,200,
  0xC9,1,0, 0x00,
  0x36,1,0, 0x48,
  0x3A,1,0, 0x11,
  0xB9,1,0, 0x20,
  0xB8,1,0, 0x29,
  0xFF,0xFF,
};
static const uint8_t kTailWaveshare42[] = {
  0x35,1,0, 0x00,
  0xD0,1,0, 0xFF,
  0x38,0,0,
  0x29,0,0,
  0xFF,0xFF,
};

// OSPTEK YDP213H001-V3, 2.13": the vendor bring-up values, from their test sketch.
static const uint8_t kPre213[] = {
  0xD6,2,0, 0x17,0x02,
  0xD1,1,0, 0x01,
  0xC0,2,0, 0x0E,0x05,
  0xC1,4,0, 0x3C,0x3E,0x3C,0x3C,
  0xC2,4,0, 0x23,0x21,0x23,0x23,
  0xC4,4,0, 0x5A,0x5C,0x5A,0x5A,
  0xC5,4,0, 0x37,0x35,0x37,0x37,
  0xD8,2,0, 0xA6,0xE9,
  0xB2,1,0, 0x15,
  0xB3,10,0, 0xE5,0xF6,0x17,0x77,0x77,0x77,0x77,0x77,0x77,0x71,
  0xB4,8,0, 0x05,0x46,0x77,0x77,0x77,0x77,0x76,0x45,
  0x62,3,0, 0x32,0x03,0x1F,
  0xB7,1,0, 0x13,
  0xB0,1,0, 0x3F,
  0x11,0,120,
  0xC9,1,0, 0x00,
  0x36,1,0, 0x48,
  0x3A,1,0, 0x11,
  0xB9,1,0, 0x20,
  0xB8,1,0, 0x29,
  0x2A,2,0, 0x19,0x23,
  0x2B,2,0, 0x00,0x7C,
  0xFF,0xFF,
};
static const uint8_t kTail213[] = {
  0x35,1,0, 0x00,
  0xD0,1,0, 0xFF,
  0x38,0,0,
  0x29,0,0,
  0xFF,0xFF,
};
static const uint8_t kAfterClr[] = { 0xBB,1,0, 0x4F, 0xFF,0xFF };   // CLRAM: clear the RAM

// OSPTEK YDP290H001/H004-V3, 2.9". No oscillator write: the NVM default is used.
static const uint8_t kPre290[] = {
  0xD6,2,0, 0x13,0x02,
  0xD1,1,0, 0x01,
  0xC0,2,0, 0x08,0x06,
  0xC1,4,0, 0x3C,0x3E,0x3C,0x3C,
  0xC2,4,0, 0x23,0x21,0x23,0x23,
  0xC4,4,0, 0x5A,0x5C,0x5A,0x5A,
  0xC5,4,0, 0x37,0x35,0x37,0x37,
  0xB2,1,0, 0x05,
  0xB3,10,0, 0xE5,0xF6,0x17,0x77,0x77,0x77,0x77,0x77,0x77,0x71,
  0xB4,8,0, 0x05,0x46,0x77,0x77,0x77,0x77,0x76,0x45,
  0x62,3,0, 0x32,0x03,0x1F,
  0xB7,1,0, 0x13,
  0xB0,1,0, 0x60,
  0x11,0,100,
  0xC9,1,0, 0x00,
  0x36,1,0, 0x00,
  0x3A,1,0, 0x11,
  0xB9,1,0, 0x20,
  0xB8,1,0, 0x29,
  0x2A,2,0, 0x17,0x24,
  0x2B,2,0, 0x00,0xBF,
  0xFF,0xFF,
};
static const uint8_t kTail290[] = {
  0x35,1,0, 0x00,
  0xD0,1,0, 0xFF,
  0x38,0,0,
  0x29,0,100,
  0xFF,0xFF,
};

// OSPTEK YDP420HT004-V3, 4.2": the highest drive voltages of the three.
static const uint8_t kPre420[] = {
  0xD6,2,0, 0x13,0x02,
  0xD1,1,0, 0x01,
  0xC0,2,0, 0x12,0x0A,
  0xC1,4,0, 115,0x3E,0x3C,0x3C,
  0xC2,4,0, 0x00,0x21,0x23,0x23,
  0xC4,4,0, 50,0x5C,0x5A,0x5A,
  0xC5,4,0, 50,0x35,0x37,0x37,
  0xD8,2,0, 0x80,0xE9,
  0xB2,1,0, 0x12,
  0xB3,10,0, 0xE5,0xF6,0x17,0x77,0x77,0x77,0x77,0x77,0x77,0x71,
  0xB4,8,0, 0x05,0x46,0x77,0x77,0x77,0x77,0x76,0x45,
  0x62,3,0, 0x32,0x03,0x1F,
  0xB7,1,0, 0x13,
  0xB0,1,0, 0x64,
  0x11,0,120,
  0xC9,1,0, 0x00,
  0x36,1,0, 0x48,
  0x3A,1,0, 0x11,
  0xB9,1,0, 0x20,
  0xB8,1,0, 0x29,
  0x2A,2,0, 0x12,0x2A,
  0x2B,2,0, 0x00,0xC7,
  0xFF,0xFF,
};

struct LB_St7305Cfg {
  LB_InitOps op;
  uint16_t   w, h;           // the size the library draws in at rotation 0
  uint8_t    layout;         // 0 or 1, see above
  uint8_t    colOffset;      // layout 1: columns of RAM before the glass
  bool       blackIsOne;     // a set bit is black (INVOFF) - false: a set bit is white (INVON)
  bool       invertFirst;    // inversion goes before the tail, as the Waveshare code did
  uint8_t    col[2], row[2]; // the RAM window sent with every frame
  const uint8_t *pre, *tail, *after;
  const uint8_t *pm;         // C1, C2, C4, C5 (four bytes each) for the power-mode switch, or null: not supported
  bool       flipY;          // the glass shows the native rows bottom to top: send them reversed
};

// The voltage set each OSPTEK vendor sketch writes when it switches power mode.
// The 2.13 uses its init values; the 2.9 and 4.2 use the 6 V set (the 2.9's init
// uses 4.9 V, and the vendor's switch code does not put that back - as shipped).
static const uint8_t kPm213[] = {0x3C,0x3E,0x3C,0x3C, 0x23,0x21,0x23,0x23, 0x5A,0x5C,0x5A,0x5A, 0x37,0x35,0x37,0x37};
static const uint8_t kPm6V[]  = {115,0x3E,0x3C,0x3C,   0x00,0x21,0x23,0x23, 50,0x5C,0x5A,0x5A,   50,0x35,0x37,0x37};

static const LB_St7305Cfg kCfg[] = {
  { LB_INIT_NONE,       400, 300, 0, 0,  false, true,  {0x12, 0x2A}, {0x00, 0xC7}, kPreWaveshare42, kTailWaveshare42, nullptr,   nullptr, false },
  { LB_INIT_ST7305_420, 400, 300, 0, 0,  true,  false, {0x12, 0x2A}, {0x00, 0xC7}, kPre420,         kTail213,         kAfterClr, kPm6V, false },
  { LB_INIT_ST7305_213, 122, 250, 1, 10, true,  false, {0x19, 0x23}, {0x00, 0x7C}, kPre213,         kTail213,         kAfterClr, kPm213, false },
  { LB_INIT_ST7305_290, 168, 384, 1, 0,  true,  false, {0x17, 0x24}, {0x00, 0xBF}, kPre290,         kTail290,         nullptr,   kPm6V, true },
};

static const LB_St7305Cfg *cfgFor(const LB_PanelDef *p) {
  for (const LB_St7305Cfg &c : kCfg)
    if (c.op == p->initOps && c.w == p->width && c.h == p->height) return &c;
  return nullptr;
}

LB_ST7305::~LB_ST7305() {
  free(_fb);
  free(_packed);
}

void LB_ST7305::cmd(uint8_t c, const uint8_t *d, uint8_t n) {
  _bus->beginWrite();
  _bus->command(c);
  if (n) _bus->data(d, n);
  _bus->endWrite();
}

void LB_ST7305::runTable(const uint8_t *t) {
  while (t && !(t[0] == 0xFF && t[1] == 0xFF)) {
    cmd(t[0], t + 3, t[1]);
    if (t[2]) _bus->delayMs(t[2]);
    t += 3 + t[1];
  }
}

bool LB_ST7305::begin(int32_t hz, bool, bool invert) {
  const LB_St7305Cfg *cfg = cfgFor(_panel);
  if (!cfg) {
    Serial.println(F("[LB_ST7305] no init values for this panel size."));
    return false;
  }
  _cfg = cfg;
  lb_lumaTables();
  _nw = _panel->width;
  _nh = _panel->height;

  // The canvas: RGB565 like every Lonely Binary panel. PSRAM when there is
  // some; the small panels fit in internal RAM.
  const size_t bytes = (size_t)_nw * _nh * 2;
  _fb = (uint16_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!_fb) _fb = (uint16_t *)heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  if (!_fb) {
    Serial.printf("[LB_ST7305] no room for the %u KB canvas - enable Tools > PSRAM.\n", (unsigned)(bytes / 1024));
    return false;
  }
  memset(_fb, 0xFF, bytes); /* white */
  _packedLen = cfg->layout == 0 ? (size_t)_nw * _nh / 8
                                : (size_t)((cfg->colOffset + _nw + 3) / 4) * (_nh / 2);
  _packed = (uint8_t *)heap_caps_malloc(_packedLen, MALLOC_CAP_8BIT);
  if (!_packed) return false;

  if (!_bus->begin(hz)) return false;
  if (_rst >= 0) _bus->hardwareReset(_rst, 50);

  runTable(cfg->pre);
  if (cfg->invertFirst) setInverted(invert);
  runTable(cfg->tail);
  if (!cfg->invertFirst) setInverted(invert);
  runTable(cfg->after);

  setRotation(_panel->rotation);
  flushFramebuffer(_fb); /* start from a clean white frame, not whatever RAM held */
  return true;
}

void LB_ST7305::setInverted(bool invert) {
  cmd(invert ? 0x21 : 0x20);
}

void LB_ST7305::setRotation(uint8_t r) {
  _rotation = r & 3;
  const bool swap = _rotation & 1;
  _w = swap ? _nh : _nw;
  _h = swap ? _nw : _nh;
}

void LB_ST7305::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (!_fb) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > _w) w = _w - x;
  if (y + h > _h) h = _h - y;
  for (int16_t r = 0; r < h; r++)
    for (int16_t c = 0; c < w; c++) _fb[(size_t)(y + r) * _w + x + c] = color;
}

void LB_ST7305::pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *px) {
  if (!_fb || !px) return;
  for (int16_t r = 0; r < h; r++) {
    const int16_t yy = y + r;
    if (yy < 0 || yy >= _h) continue;
    for (int16_t c = 0; c < w; c++) {
      const int16_t xx = x + c;
      if (xx >= 0 && xx < _w) _fb[(size_t)yy * _w + xx] = px[(size_t)r * w + c];
    }
  }
}

// RGB565 -> white? Rec.601 luma at or above half scale, i.e.
// (R * 77 + G * 150 + B * 29) >> 8 >= 128 with R, G, B scaled to 8 bits. Luma is
// linear in R, G and B, so it splits across the two bytes of the pixel: one
// 256-entry table for the high byte (R and the top of G), one for the low byte
// (the rest of G and B), and the test is two loads, an add and a compare. The
// tables hold the unshifted sums, so the result is bit-for-bit the formula
// above. Filled on the first begin().
static uint16_t lb_lumaHi[256], lb_lumaLo[256];
static void lb_lumaTables() {
  if (lb_lumaHi[0xFF]) return;                                    // 52696 once filled
  for (int i = 0; i < 256; i++) {
    const uint32_t r = (i >> 3) << 3, gh = (i & 7) << 5;          // the high byte: RRRRRGGG
    const uint32_t gl = (i >> 5) << 2, b = (i & 0x1F) << 3;       // the low byte:  GGGBBBBB
    lb_lumaHi[i] = r * 77 + gh * 150;
    lb_lumaLo[i] = gl * 150 + b * 29;
  }
}
static inline uint32_t lb_white1(uint16_t c) {           /* 1 = white: the sums stay below 65536 */
  return ((uint32_t)lb_lumaHi[c >> 8] + lb_lumaLo[c & 0xFF]) >> 15;
}
static inline bool lb_white(uint16_t c) { return lb_white1(c); }

// Where native row ny starts in the canvas, and how far apart its pixels are.
// Native is the panel's own orientation at rotation 0, rows counted from the
// top; the canvas is _w wide at the current rotation. Worked out once a row,
// not once a pixel: with a switch and two multiplies per pixel the 4.2 inch
// took 40 ms to pack, which is most of a frame.
static inline const uint16_t *lb_row(const uint16_t *fb, uint8_t rotation, int16_t ny,
                                     int16_t w, int16_t nw, int16_t nh, int32_t *step) {
  switch (rotation) {
    case 1:  *step = w;  return fb + (nh - 1 - ny);                   /* x = nh-1-ny, y = nx */
    case 2:  *step = -1; return fb + (size_t)(nh - 1 - ny) * w + (nw - 1);
    case 3:  *step = -w; return fb + (size_t)(nw - 1) * w + ny;       /* x = ny, y = nw-1-nx */
    default: *step = 1;  return fb + (size_t)ny * w;
  }
}

// The pack is the whole cost of a flush on the 4.2 inch (the 15 KB frame takes
// 6.5 ms on the wire). It is written for speed and built at -O2: 52 ms as first
// written, 18.5 ms now, against a 31 ms panel frame.
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("O2")))
#endif
void LB_ST7305::flushFramebuffer(const uint16_t *fb) {
  if (!fb || !_packed || !_cfg) return;
  const LB_St7305Cfg &c = *_cfg;
  const bool oneIsBlack = c.blackIsOne;
  uint8_t *out = _packed;

  if (c.layout == 0) {
    // (_nw/2) x (_nh/4) bytes, column-pair major:
    //
    //     byte index = (x / 2) * (_nh / 4) + (iy / 4)         iy = _nh - 1 - y
    //     bit        = 7 - ((iy % 4) * 2 + (x % 2))
    //
    // i.e. each byte is a 2-wide, 4-tall block, rows counted from the bottom of
    // the panel, bit 7 the block's top-left pixel in that bottom-up order.
    //
    // Walked in the canvas's row order, not the panel's byte order. The canvas
    // is in PSRAM, and reading it a column at a time (an 800-byte stride from
    // one pixel to the next) missed the cache on every pixel: 46 ms of a 52 ms
    // flush on the 4.2 inch. Row by row it streams, and the packed frame sits
    // in internal RAM, where the scattered writes cost nothing.
    const int16_t bytesPerCol = _nh / 4;
    const uint32_t ink = oneIsBlack ? 3 : 0;          // a set bit is black: flip both bits of the pair
    memset(out, 0, _packedLen);
    for (int16_t ny = 0; ny < _nh; ny++) {
      const int16_t iy = _nh - 1 - ny;
      uint8_t *o = out + (iy >> 2);
      const uint8_t shift = 6 - ((iy & 3) << 1);     // the pair's low bit (lx = 1); lx = 0 is one higher
      int32_t step;
      const uint16_t *px = lb_row(fb, _rotation, ny, _w, _nw, _nh, &step);
      if (step == 1 && !((uintptr_t)px & 3)) {
        // Rotation 0: the row is contiguous and word-aligned, so take two
        // pixels a load. (Rotation 2 would be contiguous too, but backwards.)
        const uint32_t *p2 = (const uint32_t *)px;
        for (int16_t bx = 0; bx < _nw / 2; bx++, o += bytesPerCol) {
          const uint32_t p = p2[bx];
          const uint32_t pair = (lb_white1((uint16_t)p) << 1) | lb_white1((uint16_t)(p >> 16));
          *o |= (uint8_t)((pair ^ ink) << shift);
        }
      } else {
        for (int16_t bx = 0; bx < _nw / 2; bx++, px += 2 * step, o += bytesPerCol) {
          const uint32_t pair = (lb_white1(px[0]) << 1) | lb_white1(px[step]);
          *o |= (uint8_t)((pair ^ ink) << shift);
        }
      }
    }
  } else {
    // ((colOffset + _nw) / 4) x (_nh / 2) bytes, row-pair major:
    //
    //     rx         = x + colOffset
    //     byte index = (y / 2) * dataW + rx / 4
    //     bit        = 7 - ((rx % 4) * 2 + (y % 2))
    //
    // each byte a 4-wide, 2-tall block, rows counted from the top.
    const int16_t dataW = (c.colOffset + _nw + 3) / 4;
    memset(out, 0, _packedLen);
    for (int16_t ny = 0; ny < _nh; ny++) {
      const int16_t ry = c.flipY ? _nh - 1 - ny : ny;         // the RAM row this canvas row lands in
      uint8_t *row = out + (size_t)(ry >> 1) * dataW;
      const uint8_t ly = ry & 1;
      int32_t step;
      const uint16_t *px = lb_row(fb, _rotation, ny, _w, _nw, _nh, &step);
      for (int16_t nx = 0; nx < _nw; nx++, px += step) {
        if (lb_white(*px) == oneIsBlack) continue;
        const int16_t rx = nx + c.colOffset;
        row[rx >> 2] |= 1 << (7 - (((rx & 3) << 1) | ly));
      }
    }
    // RAM columns off the glass are blank, i.e. white: a clear bit on every
    // layout-1 panel so far (blackIsOne), a set one on a glass where a set bit
    // is white.
    if (!oneIsBlack) {
      for (int16_t by = 0; by < _nh / 2; by++) {
        uint8_t *row = out + (size_t)by * dataW;
        for (int16_t rx = 0; rx < dataW * 4; rx++) {
          if (rx >= c.colOffset && rx < c.colOffset + _nw) continue;
          row[rx >> 2] |= 3 << (6 - ((rx & 3) << 1));
        }
      }
    }
  }

  if (!_low) cmd(0x38);   // fast refresh - but not when the sketch asked for the slow one
  cmd(0x29);
  cmd(0x2A, c.col, 2);
  cmd(0x2B, c.row, 2);
  _bus->beginWrite();
  _bus->command(0x2C);
  _bus->data(_packed, (uint32_t)_packedLen);
  _bus->endWrite();
}

// The vendor sketches' mode switch. Low: set the voltages, then LPM (0x39). High:
// HPM (0x38), wait for the booster, then the voltages again.
void LB_ST7305::setLowPower(bool low) {
  if (!_cfg || !_cfg->pm || low == _low) return;
  const uint8_t *pm = _cfg->pm;
  auto volts = [&]() {
    cmd(0xC1, pm, 4);
    cmd(0xC2, pm + 4, 4);
    cmd(0xC4, pm + 8, 4);
    cmd(0xC5, pm + 12, 4);
    const uint8_t sel = 0x00;
    cmd(0xC9, &sel, 1);
    _bus->delayMs(20);
  };
  if (low) {
    volts();
    cmd(0x39);
    _bus->delayMs(100);
  } else {
    cmd(0x38);
    _bus->delayMs(300);
    volts();
  }
  _low = low;
}
