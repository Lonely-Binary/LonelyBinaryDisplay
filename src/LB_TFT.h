#pragma once
/*
 * LB_TFT - the panel driver: SPI, reset, init table, window, pixels.
 *
 * One class drives all five controllers. What differs between them is data:
 * the init table (LB_TFTInit.h, captured from what shipped before), the reset
 * timing, the MADCTL rotation bits, and one quirk in how NV3007 caches the
 * address window. Everything else - CASET/RASET/RAMWR, INVON/INVOFF, RGB565
 * sent high byte first - is the same MIPI DCS command set on every one.
 *
 * !! The byte stream is meant to be IDENTICAL to what Arduino_GFX sent !!
 *
 *   That is what makes this port checkable without a screen: record both on
 *   the same inputs and compare. Internal/LonelyBinaryDisplay/代码/DriverDiff
 *   does exactly that, for every panel in the table, at every rotation. It is
 *   also why the window cache below reproduces Arduino_GFX's, even the NV3007
 *   variant - a cache that skipped one more CASET would still draw correctly,
 *   but it would break the comparison that tells us the rest is right.
 *
 * The bus is an interface so the comparison can plug a recorder in; the
 * cost is one virtual call per command or per run of pixels, never per pixel.
 */
#include <Arduino.h>
#include "LB_Panels.h"

class LB_TFTBus
{
public:
  virtual ~LB_TFTBus() {}
  virtual bool begin(int32_t hz) = 0;
  virtual void beginWrite() = 0;
  virtual void endWrite() = 0;
  virtual void command(uint8_t c) = 0;
  virtual void data(const uint8_t *d, uint32_t n) = 0;
  /* Four data bytes, high byte first - CASET/RASET. Its own call because on
   * SPI it is one FIFO word rather than the general byte path, and it runs
   * twice per span in direct mode. */
  virtual void data32(uint32_t v)
  {
    const uint8_t d[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
    data(d, 4);
  }
  /* RGB565 as the CPU holds it; the bus sends the high byte first. */
  virtual void repeat(uint16_t color, uint32_t n) = 0;
  virtual void pixels(const uint16_t *px, uint32_t n) = 0;
  /* HIGH, wait, LOW, wait `ms`, HIGH, wait `ms` - the sequence every driver
   * used. A virtual so the recorder can log it instead of touching a pin. */
  virtual void hardwareReset(int8_t rst, uint16_t ms) = 0;
  virtual void delayMs(uint32_t ms) { delay(ms); }
};

/* The real bus. SPIClass only sets the pins up; every write then goes through
 * the core's HAL (esp32-hal-spi.h) directly, and CS/DC through the IDF GPIO
 * calls. Both for the same reason - in direct mode the fixed cost per span is
 * the whole cost, because a span is usually a handful of pixels:
 *
 *   SPIClass::beginTransaction + endTransaction   12.1 us   (measured)
 *
 * That is two locks plus a clock-divider check on every call, and it made
 * drawString 33% slower than Arduino_GFX, which calls the HAL the same way
 * this does. digitalWrite is avoided for the same reason. */
class LB_TFTSpiBus : public LB_TFTBus
{
public:
  LB_TFTSpiBus(int8_t dc, int8_t cs, int8_t sclk, int8_t mosi, uint8_t host)
      : _dc(dc), _cs(cs), _sclk(sclk), _mosi(mosi), _host(host) {}
  ~LB_TFTSpiBus();

  bool begin(int32_t hz) override;
  void beginWrite() override;
  void endWrite() override;
  void command(uint8_t c) override;
  void data(const uint8_t *d, uint32_t n) override;
  void data32(uint32_t v) override;
  void repeat(uint16_t color, uint32_t n) override;
  void pixels(const uint16_t *px, uint32_t n) override;
  void hardwareReset(int8_t rst, uint16_t ms) override;

private:
  int8_t _dc, _cs, _sclk, _mosi;
  uint8_t _host;
  uint32_t _div = 0;                  /* clock divider, computed once */
  class SPIClass *_spi = nullptr;
  struct spi_struct_t *_bus = nullptr; /* the HAL handle behind _spi */
};

class LB_TFT
{
public:
  LB_TFT(LB_TFTBus *bus, const LB_PanelDef *panel, int8_t rst)
      : _bus(bus), _panel(panel), _rst(rst) {}

  /* Reset, init table, inversion, MADCTL, full-screen window - the order
   * Arduino_TFT::begin() used. */
  bool begin(int32_t hz, bool bgr, bool invert);

  void setRotation(uint8_t r);   /* 0-3 */
  void setColorOrder(bool bgr);  /* rewrites MADCTL; works at any time */
  void setInverted(bool invert);

  int16_t width() const { return _w; }
  int16_t height() const { return _h; }
  uint8_t rotation() const { return _rotation; }

  /* Both clip to the screen, so callers can be sloppy. */
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  /* `px` is a w x h block, row-major, RGB565 as the CPU holds it. */
  void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *px);

private:
  LB_TFTBus *_bus;
  const LB_PanelDef *_panel;
  int8_t _rst;
  bool _bgr = false;
  uint8_t _rotation = 0;
  int16_t _w = 0, _h = 0;
  int16_t _xStart = 0, _yStart = 0;
  /* Last window sent. 0xFFFF never matches, so the next window goes out in
   * full - which is how Arduino_TFT invalidated it after a rotation. */
  uint16_t _curX = 0xFFFF, _curY = 0xFFFF, _curW = 0xFFFF, _curH = 0xFFFF;

  void reset();
  void runTable(const uint8_t *t);
  void writeMadctl();
  void window(int16_t x, int16_t y, int16_t w, int16_t h); /* inside a write */
  void cmd16x2(uint8_t c, uint16_t a, uint16_t b);
};
