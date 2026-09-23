#pragma once
/*
 * LB_TFTPar8Bus - an 8080-style 8-bit parallel bus, driven from GPIO.
 *
 * Eight data lines, WR and DC. CS, RD and RST are optional (-1): on many
 * parallel modules CS is tied low, RD tied high, and reset is left to SWRESET.
 *
 * One byte is: put it on D0-D7, pull WR low, let WR go high - the panel
 * latches on the rising edge. Each step is a single write to a GPIO set/clear
 * register, with the set mask for every possible byte looked up rather than
 * built bit by bit. That is how Arduino_GFX's ESP32PAR8 does it too.
 *
 * The byte stream is the same as over SPI - command bytes with DC low, data
 * with DC high, RGB565 high byte first - which is why LB_TFT does not care
 * which bus it is on, and why DriverDiff checks both the same way.
 */
#include "LB_TFT.h"

class LB_TFTPar8Bus : public LB_TFTBus
{
public:
  /* d: D0..D7 in that order. */
  LB_TFTPar8Bus(const int8_t d[8], int8_t wr, int8_t dc, int8_t cs = -1, int8_t rd = -1);
  ~LB_TFTPar8Bus();

  /* hz is ignored: the bus runs as fast as the GPIO writes go. */
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
  int8_t _d[8], _wr, _dc, _cs, _rd;
  /* Per bank: bank 0 is GPIO 0-31, bank 1 is 32 and up. */
  uint32_t _dataMask[2] = {0, 0};
  uint32_t *_set[2] = {nullptr, nullptr};  /* 256 entries each, bank 1 only if used */
  uint32_t _wrMask = 0, _dcMask = 0, _csMask = 0;
  uint8_t _wrBank = 0, _dcBank = 0, _csBank = 0;

  inline void put(uint8_t b);
  inline void strobe();
};
