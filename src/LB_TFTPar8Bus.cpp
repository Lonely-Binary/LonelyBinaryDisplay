#include "LB_TFTPar8Bus.h"

#include <soc/gpio_reg.h>

// W1TS / W1TC: writing a 1 sets / clears that pin, a 0 leaves it alone - so
// one register write moves any subset of pins without a read-modify-write.
#if SOC_GPIO_PIN_COUNT > 32
static volatile uint32_t *const kSet[2] = {(volatile uint32_t *)GPIO_OUT_W1TS_REG,
                                           (volatile uint32_t *)GPIO_OUT1_W1TS_REG};
static volatile uint32_t *const kClr[2] = {(volatile uint32_t *)GPIO_OUT_W1TC_REG,
                                           (volatile uint32_t *)GPIO_OUT1_W1TC_REG};
#else
static volatile uint32_t *const kSet[2] = {(volatile uint32_t *)GPIO_OUT_W1TS_REG, nullptr};
static volatile uint32_t *const kClr[2] = {(volatile uint32_t *)GPIO_OUT_W1TC_REG, nullptr};
#endif

static inline uint8_t bankOf(int8_t pin) { return pin >= 32 ? 1 : 0; }
static inline uint32_t maskOf(int8_t pin) { return 1UL << (pin & 31); }

LB_TFTPar8Bus::LB_TFTPar8Bus(const int8_t d[8], int8_t wr, int8_t dc, int8_t cs, int8_t rd)
    : _wr(wr), _dc(dc), _cs(cs), _rd(rd) {
  for (int i = 0; i < 8; i++) _d[i] = d[i];
}

LB_TFTPar8Bus::~LB_TFTPar8Bus() {
  free(_set[0]);
  free(_set[1]);
}

bool LB_TFTPar8Bus::begin(int32_t) {
  bool useBank1 = false;
  for (int i = 0; i < 8; i++) {
    if (_d[i] < 0) return false;
    useBank1 |= bankOf(_d[i]) == 1;
  }
  if ((useBank1 || _wr >= 32 || _dc >= 32 || _cs >= 32) && !kSet[1]) return false;

  for (int b = 0; b < 2; b++) {
    if (b == 1 && !useBank1) break;
    _set[b] = (uint32_t *)calloc(256, sizeof(uint32_t));
    if (!_set[b]) return false;
  }
  for (int i = 0; i < 8; i++) _dataMask[bankOf(_d[i])] |= maskOf(_d[i]);
  for (int v = 0; v < 256; v++)
    for (int i = 0; i < 8; i++)
      if (v & (1 << i)) _set[bankOf(_d[i])][v] |= maskOf(_d[i]);

  for (int i = 0; i < 8; i++) pinMode(_d[i], OUTPUT);
  pinMode(_wr, OUTPUT);
  digitalWrite(_wr, HIGH);
  pinMode(_dc, OUTPUT);
  digitalWrite(_dc, HIGH);
  if (_cs >= 0) { pinMode(_cs, OUTPUT); digitalWrite(_cs, HIGH); }
  if (_rd >= 0) { pinMode(_rd, OUTPUT); digitalWrite(_rd, HIGH); }  // never read

  _wrBank = bankOf(_wr); _wrMask = maskOf(_wr);
  _dcBank = bankOf(_dc); _dcMask = maskOf(_dc);
  if (_cs >= 0) { _csBank = bankOf(_cs); _csMask = maskOf(_cs); }
  return true;
}

// Put a byte on D0-D7. Clear all eight, then set the ones that are 1.
inline void LB_TFTPar8Bus::put(uint8_t b) {
  *kClr[0] = _dataMask[0];
  *kSet[0] = _set[0][b];
  if (_set[1]) {
    *kClr[1] = _dataMask[1];
    *kSet[1] = _set[1][b];
  }
}

// WR low then high; the panel latches on the rising edge.
inline void LB_TFTPar8Bus::strobe() {
  *kClr[_wrBank] = _wrMask;
  *kSet[_wrBank] = _wrMask;
}

void LB_TFTPar8Bus::beginWrite() {
  if (_csMask) *kClr[_csBank] = _csMask;
}

void LB_TFTPar8Bus::endWrite() {
  if (_csMask) *kSet[_csBank] = _csMask;
}

void LB_TFTPar8Bus::command(uint8_t c) {
  *kClr[_dcBank] = _dcMask;
  put(c);
  strobe();
  *kSet[_dcBank] = _dcMask;
}

// The hot loops below copy every register pointer and mask into locals first.
// A store through a volatile uint32_t * may alias any uint32_t member, so with
// members the compiler reloads them after every register write; that made a
// black fill twice as slow as Arduino_GFX (59.8 ms vs 30.9 ms on 320x480).
//
// Bank 1 (GPIO 32+) is the rare case, so the common one - every pin in bank 0
// - gets loops of its own with no per-byte check.

#define LB_PAR8_LOCALS                                   \
  volatile uint32_t *const dClr = kClr[0];               \
  volatile uint32_t *const dSet = kSet[0];               \
  volatile uint32_t *const wClr = kClr[_wrBank];         \
  volatile uint32_t *const wSet = kSet[_wrBank];         \
  const uint32_t dMask = _dataMask[0], wMask = _wrMask;  \
  const uint32_t *const tab = _set[0]

void LB_TFTPar8Bus::data(const uint8_t *d, uint32_t n) {
  if (_set[1]) { while (n--) { put(*d++); strobe(); } return; }
  LB_PAR8_LOCALS;
  while (n--) {
    *dClr = dMask; *dSet = tab[*d++];
    *wClr = wMask; *wSet = wMask;
  }
}

void LB_TFTPar8Bus::data32(uint32_t v) {
  const uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
  data(b, 4);
}

void LB_TFTPar8Bus::repeat(uint16_t color, uint32_t n) {
  const uint8_t hi = color >> 8, lo = color & 0xFF;
  if (_set[1]) {
    while (n--) { put(hi); strobe(); put(lo); strobe(); }
    return;
  }
  LB_PAR8_LOCALS;
  if (hi == lo) {
    // Black, white and every other colour whose two bytes match: the data
    // lines never change, so only WR moves - two strobes per pixel.
    *dClr = dMask; *dSet = tab[hi];
    while (n--) {
      *wClr = wMask; *wSet = wMask;
      *wClr = wMask; *wSet = wMask;
    }
    return;
  }
  const uint32_t mHi = tab[hi], mLo = tab[lo];
  while (n--) {
    *dClr = dMask; *dSet = mHi; *wClr = wMask; *wSet = wMask;
    *dClr = dMask; *dSet = mLo; *wClr = wMask; *wSet = wMask;
  }
}

void LB_TFTPar8Bus::pixels(const uint16_t *px, uint32_t n) {
  if (_set[1]) {
    while (n--) { const uint16_t c = *px++; put(c >> 8); strobe(); put(c); strobe(); }
    return;
  }
  LB_PAR8_LOCALS;
  while (n--) {
    const uint16_t c = *px++;
    *dClr = dMask; *dSet = tab[c >> 8];   *wClr = wMask; *wSet = wMask;
    *dClr = dMask; *dSet = tab[c & 0xFF]; *wClr = wMask; *wSet = wMask;
  }
}

void LB_TFTPar8Bus::hardwareReset(int8_t rst, uint16_t ms) {
  pinMode(rst, OUTPUT);
  digitalWrite(rst, HIGH);
  delay(100);
  digitalWrite(rst, LOW);
  delay(ms);
  digitalWrite(rst, HIGH);
  delay(ms);
}
