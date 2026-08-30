#include "LonelyBinaryDisplay.h"

// ─── Backlight PWM ───────────────────────────────────────────────────────────
// ESP32 core 3.x replaced ledcSetup()/ledcAttachPin() with a single
// ledcAttach(), and moved ledcWrite() from a channel to a pin. Both spellings
// live here so sketches compile on either core — written once, not once per
// example.
#define LB_BL_PWM_FREQ 5000
#define LB_BL_PWM_BITS 8
#define LB_BL_PWM_CHANNEL 0

LB_Display::~LB_Display() {
  delete _canvas;
  delete _driver;
  delete _bus;
}

Arduino_GFX *LB_Display::makeDriver() {
  const LB_PanelDef *p = _panel;
  // Note on the `ips` argument below: in Arduino_GFX that flag does exactly
  // one thing — decide whether the panel is inverted at rest — so our single
  // `invert` field is what belongs there. Passing a separate "is it an IPS
  // panel" value would silently fight setInverted().
  switch (p->driver) {
    case LB_DRV_ST7735:
      // The only driver with a colour-order argument. Using it means no
      // MADCTL patching is needed for these panels.
      return new Arduino_ST7735(_bus, _wiring.rst, p->rotation, p->invert /* Arduino_GFX calls this `ips`; it only controls inversion */,
                                p->width, p->height,
                                p->colOff1, p->rowOff1, p->colOff2, p->rowOff2,
                                colorOrderIsBGR());
    case LB_DRV_ST7789:
      return new Arduino_ST7789(_bus, _wiring.rst, p->rotation, p->invert /* Arduino_GFX calls this `ips`; it only controls inversion */,
                                p->width, p->height,
                                p->colOff1, p->rowOff1, p->colOff2, p->rowOff2);
    case LB_DRV_ST7796:
      return new Arduino_ST7796(_bus, _wiring.rst, p->rotation, p->invert /* Arduino_GFX calls this `ips`; it only controls inversion */,
                                p->width, p->height,
                                p->colOff1, p->rowOff1, p->colOff2, p->rowOff2);
    case LB_DRV_NV3007:
      // The 2.79" is the same silicon as the 1.68" but needs its own
      // voltage/gamma table — without it the panel comes up looking wrong.
      if (p->initOps == LB_INIT_NV3007_279) {
        return new Arduino_NV3007(_bus, _wiring.rst, p->rotation, p->invert /* Arduino_GFX calls this `ips`; it only controls inversion */,
                                  p->width, p->height,
                                  p->colOff1, p->rowOff1, p->colOff2, p->rowOff2,
                                  nv3007_279_init_operations,
                                  sizeof(nv3007_279_init_operations));
      }
      return new Arduino_NV3007(_bus, _wiring.rst, p->rotation, p->invert /* Arduino_GFX calls this `ips`; it only controls inversion */,
                                p->width, p->height,
                                p->colOff1, p->rowOff1, p->colOff2, p->rowOff2);
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
  } else {  // ST7789 / ST7796 / NV3007 share one mapping
    switch (r & 7) {
      case 1:  bits = MX | MV;      break;
      case 2:  bits = MX | MY;      break;
      case 3:  bits = MY | MV;      break;
      case 4:  bits = MX;           break;
      case 5:  bits = MX | MY | MV; break;
      case 6:  bits = MY;           break;
      case 7:  bits = MV;           break;
      default: bits = 0;            break;
    }
  }
  return bits | (bgr ? BGR : 0);
}

bool LB_Display::colorOrderIsBGR() const {
  if (_colorOrder == COLOR_RGB) return false;
  if (_colorOrder == COLOR_BGR) return true;
  return _panel->bgr;  // COLOR_AUTO: whatever the panel table says
}

void LB_Display::setColorOrder(ColorOrder order) {
  if (_gfx) return;  // the driver has already been constructed
  _colorOrder = order;
}

void LB_Display::setInverted(bool inverted) {
  _inverted = inverted;
  // Arduino_GFX computes (_ips ^ i), and we hand it the panel's `invert` flag
  // as `ips`, so passing the difference here lands on the requested state.
  if (_driver) _driver->invertDisplay(inverted != _panel->invert);
}

// ST7735 took the colour order in its constructor; the others hardcode a bit
// in MADCTL, so re-send the register whenever our choice differs from theirs.
void LB_Display::applyColorOrder() {
  if (_panel->driver == LB_DRV_ST7735) return;  // handled at construction
  const bool want = colorOrderIsBGR();
  const bool driverDefault = (_panel->driver == LB_DRV_ST7796);  // others: RGB
  if (want == driverDefault) { _madctlOverride = false; return; }
  _madctlOverride = true;
  _bus->beginWrite();
  _bus->writeC8D8(0x36, lb_madctl(_panel->driver, rotation(), want));
  _bus->endWrite();
}

void LB_Display::setWiring(const LB_Wiring &wiring) {
  if (_gfx) return;  // too late; begin() has already built the bus
  _wiring = wiring;
  _customWiring = true;
}

bool LB_Display::begin(bool useCanvas) {
  if (_gfx) return true;  // already up

  const int32_t spiHz = _spiHzOverride ? _spiHzOverride : _panel->spiHz;

  _bus = new Arduino_ESP32SPI(_wiring.dc, _wiring.cs, _wiring.sclk,
                              _wiring.mosi, GFX_NOT_DEFINED /* MISO unused */,
                              _wiring.spiHost, true /* shared bus */);
  _driver = makeDriver();
  if (!_driver) return false;

  if (useCanvas) {
    // Canvas dimensions follow the panel's shipping rotation, which the driver
    // has already applied.
    _canvas = new Arduino_Canvas(_driver->width(), _driver->height(), _driver);
    // Arduino_Canvas::begin() brings up the output panel too.
    if (!_canvas->begin(spiHz)) {
      Serial.println(F("[LB_Display] canvas begin failed."));
      Serial.println(F("[LB_Display] A framebuffer needs width*height*2 bytes. "
                       "Set Tools > PSRAM to Enabled, or call begin() without "
                       "a canvas."));
      return false;
    }
    _gfx = _canvas;
  } else {
    if (!_driver->begin(spiHz)) {
      Serial.println(F("[LB_Display] panel begin failed — check the wiring, "
                       "DC in particular."));
      return false;
    }
    _gfx = _driver;
  }

  applyColorOrder();
  setInverted(_panel->invert);

  backlightBegin();
  backlight(255);
  return true;
}

uint16_t *LB_Display::framebuffer() const {
  return _canvas ? _canvas->getFramebuffer() : nullptr;
}

void LB_Display::flush() {
  if (_canvas) _canvas->flush();
}

void LB_Display::setRotation(uint8_t r) {
  if (!_gfx) return;
  _gfx->setRotation(r & 3);
  // The driver just rewrote MADCTL from its own idea of the colour order, so
  // ours has to go back on top.
  if (_madctlOverride) {
    _bus->beginWrite();
    _bus->writeC8D8(0x36, lb_madctl(_panel->driver, r & 3, colorOrderIsBGR()));
    _bus->endWrite();
  }
}

// ─── Backlight ───────────────────────────────────────────────────────────────

void LB_Display::backlightBegin() {
  // Every panel in the range dims, so the backlight is always driven by LEDC
  // rather than digitalWrite. backlight(255) is simply full brightness, which
  // makes the on/off case a special case of the same call.
  if (_wiring.backlight < 0) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(_wiring.backlight, LB_BL_PWM_FREQ, LB_BL_PWM_BITS);
#else
  ledcSetup(LB_BL_PWM_CHANNEL, LB_BL_PWM_FREQ, LB_BL_PWM_BITS);
  ledcAttachPin(_wiring.backlight, LB_BL_PWM_CHANNEL);
#endif
  _blReady = true;
}

void LB_Display::backlight(uint8_t level) {
  if (!_blReady || _wiring.backlight < 0) return;

  // Active-low panels want a LOW duty cycle to be bright. Getting this
  // backwards is the classic "why is my screen dark at 255" bug — it is
  // decided by the panel table, not by the sketch.
  uint8_t duty = _panel->blActiveLow ? (uint8_t)(255 - level) : level;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(_wiring.backlight, duty);
#else
  ledcWrite(LB_BL_PWM_CHANNEL, duty);
#endif
}

// ─── Diagnostics ─────────────────────────────────────────────────────────────

void LB_Display::printInfo(Print &out) const {
  static const char *kDrivers[] = {"ST7735", "ST7789", "ST7796", "NV3007"};
  out.println(F("---- Lonely Binary Display ----"));
  out.printf("Panel      : %s (%s)\n", _panel->name, _panel->id);
  out.printf("Driver IC  : %s\n", kDrivers[_panel->driver]);
  out.printf("Resolution : %dx%d  (native %dx%d, rotation %d)\n",
             width(), height(), _panel->width, _panel->height, _panel->rotation);
  out.printf("SPI clock  : %ld Hz%s\n",
             (long)(_spiHzOverride ? _spiHzOverride : _panel->spiHz),
             _spiHzOverride ? "  (overridden)" : "");
  out.printf("Backlight  : PWM, active %s%s\n",
             _panel->blActiveLow ? "LOW" : "HIGH",
             _wiring.backlight < 0 ? "  (no pin — not driven)" : "");
  out.printf("Board      : %s%s\n", LB_WIRING_NAME,
             _customWiring ? "  (custom wiring)" : "");
  out.printf("Pins       : CS=%d RST=%d DC=%d MOSI=%d SCLK=%d BL=%d\n",
             _wiring.cs, _wiring.rst, _wiring.dc,
             _wiring.mosi, _wiring.sclk, _wiring.backlight);
  out.printf("Colour     : %s%s, %sinverted%s\n",
             colorOrderIsBGR() ? "BGR" : "RGB",
             _colorOrder == COLOR_AUTO ? "" : " (forced)",
             _inverted ? "" : "not ",
             _inverted == _panel->invert ? "" : " (forced)");
  out.printf("Framebuffer: %s\n", _canvas ? "yes (PSRAM canvas)" : "no (direct)");
  out.println(F("-------------------------------"));
}

void LB_Display::selfTest() {
  if (!_gfx) return;
  auto *g = _gfx;
  const int16_t w = width(), h = height();

  // 1. Solid colours — confirms the panel, the wiring and the colour order.
  const uint16_t solids[] = {LB_RED, LB_GREEN, LB_BLUE};
  const char *names[] = {"RED", "GREEN", "BLUE"};
  for (int i = 0; i < 3; i++) {
    g->fillScreen(solids[i]);
    g->setTextColor(LB_WHITE);
    g->setTextSize(2);
    g->setCursor(4, h / 2 - 8);
    g->print(names[i]);
    flush();
    delay(700);
  }

  // 2. Backlight sweep. Every panel in the range dims, so this runs on all of
  //    them — and it is the quickest way to spot a polarity mistake, because a
  //    panel wired the other way round sweeps backwards.
  if (_wiring.backlight >= 0) {
    g->fillScreen(LB_WHITE);
    g->setTextColor(LB_BLACK);
    g->setTextSize(1);
    g->setCursor(4, 4);
    g->print("Backlight sweep");
    flush();
    for (int v = 255; v >= 30; v -= 5) { backlight(v); delay(8); }
    for (int v = 30; v <= 255; v += 5) { backlight(v); delay(8); }
  }

  // 3. Colour bars + panel identity.
  g->fillScreen(LB_BLACK);
  const uint16_t bars[] = {LB_RED, LB_GREEN, LB_BLUE, LB_YELLOW,
                           LB_MAGENTA, LB_CYAN, LB_WHITE, LB_GREY};
  const int16_t barTop = h / 3;
  const int16_t barH = (h - barTop) / 8;
  for (int i = 0; i < 8; i++) {
    g->fillRect(0, barTop + i * barH, w, barH, bars[i]);
  }

  g->setTextColor(LB_WHITE);
  g->setTextSize(w >= 200 ? 2 : 1);
  g->setCursor(4, 6);
  g->print("LonelyBinary");
  g->setTextSize(1);
  g->setCursor(4, barTop - 22);
  g->print(_panel->name);
  g->setCursor(4, barTop - 12);
  g->printf("%dx%d", w, h);
  flush();
}
