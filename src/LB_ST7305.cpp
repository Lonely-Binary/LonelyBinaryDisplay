#include "LB_ST7305.h"

#include <esp_heap_caps.h>

// The only ST7305 panel in the table is the 400 x 300 one. Its RAM window is
// not 0..: the glass starts at column 0x12 of controller RAM. Values from the
// reference driver, not measured here.
static const uint8_t kColStart = 0x12, kColEnd = 0x2A;
static const uint8_t kRowStart = 0x00, kRowEnd = 0xC7;

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

#define LB_CMD(c, ...)                                  \
  do {                                                  \
    static const uint8_t d_[] = {__VA_ARGS__};          \
    cmd(c, d_, sizeof(d_));                             \
  } while (0)

bool LB_ST7305::begin(int32_t hz, bool, bool invert) {
  _nw = _panel->width;
  _nh = _panel->height;
  if (_nw != 400 || _nh != 300) return false; /* the window above is this panel's */

  const size_t bytes = (size_t)_nw * _nh * 2;
  _fb = (uint16_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!_fb) {
    Serial.println(F("[LB_ST7305] needs PSRAM for the 240 KB canvas - "
                     "enable Tools > PSRAM (OPI on the ESP32-S3 RLCD board)."));
    return false;
  }
  memset(_fb, 0xFF, bytes); /* white */
  _packed = (uint8_t *)heap_caps_malloc((size_t)_nw * _nh / 8, MALLOC_CAP_8BIT);
  if (!_packed) return false;

  if (!_bus->begin(hz)) return false;
  if (_rst >= 0) _bus->hardwareReset(_rst, 50);

  LB_CMD(0xD6, 0x17, 0x02);                                        /* NVM load */
  LB_CMD(0xD1, 0x01);                                              /* booster on */
  LB_CMD(0xC0, 0x11, 0x04);                                        /* gate voltage */
  LB_CMD(0xC1, 0x69, 0x69, 0x69, 0x69);                            /* VSHP */
  LB_CMD(0xC2, 0x19, 0x19, 0x19, 0x19);                            /* VSLP */
  LB_CMD(0xC4, 0x4B, 0x4B, 0x4B, 0x4B);                            /* VSHN */
  LB_CMD(0xC5, 0x19, 0x19, 0x19, 0x19);                            /* VSLN */
  LB_CMD(0xD8, 0x80, 0xE9);                                        /* oscillator */
  LB_CMD(0xB2, 0x02);                                              /* frame rate */
  LB_CMD(0xB3, 0xE5, 0xF6, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45); /* gate EQ, high power */
  LB_CMD(0xB4, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45);    /* gate EQ, low power */
  LB_CMD(0x62, 0x32, 0x03, 0x1F);                                  /* gate timing */
  LB_CMD(0xB7, 0x13);                                              /* source EQ */
  const uint8_t lines = (uint8_t)(_nh / 3);                        /* 0x64 for 300 */
  cmd(0xB0, &lines, 1);                                            /* gate lines */
  cmd(0x11);                                                       /* sleep out */
  _bus->delayMs(200);
  LB_CMD(0xC9, 0x00);                                              /* source voltage select */
  LB_CMD(0x36, 0x48);                                              /* MADCTL: MX, DO */
  LB_CMD(0x3A, 0x11);                                              /* 1 bit per pixel */
  LB_CMD(0xB9, 0x20);                                              /* mono gamma */
  LB_CMD(0xB8, 0x29);                                              /* panel setting */
  setInverted(invert);
  LB_CMD(0x35, 0x00);                                              /* TE line on */
  LB_CMD(0xD0, 0xFF);                                              /* auto power down */
  cmd(0x38);                                                       /* high power mode */
  cmd(0x29);                                                       /* display on */

  setRotation(_panel->rotation);
  flushFramebuffer(_fb); /* start from a clean white frame, not whatever RAM held */
  return true;
}

void LB_ST7305::setInverted(bool invert) {
  cmd(invert ? 0x21 : 0x20);
}

void LB_ST7305::setRotation(uint8_t r) {
  _rotation = r & 3;
  const bool landscape = !(_rotation & 1);
  _w = landscape ? _nw : _nh;
  _h = landscape ? _nh : _nw;
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

// RGB565 -> white? Rec.601 luma, half scale.
static inline bool lb_white(uint16_t c) {
  const uint32_t r = (c >> 8) & 0xF8, g = (c >> 3) & 0xFC, b = (c << 3) & 0xF8;
  return ((r * 77 + g * 150 + b * 29) >> 8) >= 128;
}

// The frame is (_nw/2) x (_nh/4) bytes, stored column-pair major:
//
//     byte index = (x / 2) * (_nh / 4) + (iy / 4)         iy = _nh - 1 - y
//     bit        = 7 - ((iy % 4) * 2 + (x % 2))           1 = white, 0 = black
//
// i.e. each byte is a 2-wide, 4-tall block, rows counted from the bottom of
// the panel, bit 7 the block's top-left pixel in that bottom-up order.
void LB_ST7305::flushFramebuffer(const uint16_t *fb) {
  if (!fb || !_packed) return;
  const int16_t bytesPerCol = _nh / 4;
  uint8_t *out = _packed;
  for (int16_t bx = 0; bx < _nw / 2; bx++) {
    for (int16_t by = 0; by < bytesPerCol; by++) {
      uint8_t v = 0;
      for (uint8_t ly = 0; ly < 4; ly++) {
        const int16_t ny = _nh - 1 - (by * 4 + ly);
        for (uint8_t lx = 0; lx < 2; lx++) {
          const int16_t nx = bx * 2 + lx;
          size_t src;
          switch (_rotation) {
            case 1:  src = (size_t)nx * _w + (_nh - 1 - ny); break;       /* x = nh-1-ny, y = nx */
            case 2:  src = (size_t)(_nh - 1 - ny) * _w + (_nw - 1 - nx); break;
            case 3:  src = (size_t)(_nw - 1 - nx) * _w + ny; break;       /* x = ny, y = nw-1-nx */
            default: src = (size_t)ny * _w + nx; break;
          }
          if (lb_white(fb[src])) v |= 1 << (7 - ((ly << 1) | lx));
        }
      }
      *out++ = v;
    }
  }

  cmd(0x38);
  cmd(0x29);
  const uint8_t col[] = {kColStart, kColEnd}, row[] = {kRowStart, kRowEnd};
  cmd(0x2A, col, 2);
  cmd(0x2B, row, 2);
  _bus->beginWrite();
  _bus->command(0x2C);
  _bus->data(_packed, (uint32_t)_nw * _nh / 8);
  _bus->endWrite();
}
