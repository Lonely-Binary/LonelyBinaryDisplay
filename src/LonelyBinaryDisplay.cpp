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
    case LB_DRV_ILI9341:
      // Hardcodes BGR in its own MADCTL, so like ST7789 and ST7796 the colour
      // order comes from our register rewrite rather than a constructor arg.
      return new Arduino_ILI9341(_bus, _wiring.rst, p->rotation, p->invert,
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
  } else if (drv == LB_DRV_ILI9341) {
    // A third mapping again — ILI9341 agrees with neither family above.
    switch (r & 3) {
      case 1:  bits = MV;           break;
      case 2:  bits = MY;           break;
      case 3:  bits = MX | MY | MV; break;
      default: bits = MX;           break;
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

uint8_t LB_Display::panelRotation() const {
  return _driver ? _driver->getRotation() : _panel->rotation;
}

bool LB_Display::colorOrderIsBGR() const {
  if (_colorOrder == COLOR_RGB) return false;
  if (_colorOrder == COLOR_BGR) return true;
  return _panel->bgr;  // COLOR_AUTO: whatever the panel table says
}

bool LB_Display::setColorOrder(ColorOrder order) {
  if (!_gfx) {                 // before begin(): always fine
    _colorOrder = order;
    return true;
  }
  if (_panel->driver == LB_DRV_ST7735) {
    // Arduino_GFX takes the order as a constructor argument for this driver,
    // and the object is already built.
    Serial.println(F("[LB_Display] setColorOrder() must precede begin() on an "
                     "ST7735 panel."));
    return false;
  }
  // Everywhere else it is only a bit in MADCTL, so it can be changed live.
  _colorOrder = order;
  applyColorOrder();
  setInverted(_panel->invert);

  backlightBegin();
  backlight(255);
  return true;
}

void LB_Display::setInverted(bool inverted) {
  _inverted = inverted;
  // Arduino_GFX computes (_ips ^ i), and we hand it the panel's `invert` flag
  // as `ips`, so passing the difference here lands on the requested state.
  if (_driver) _driver->invertDisplay(inverted != _panel->invert);
}

// ST7735 took the colour order in its constructor; the others hardcode a bit
// in MADCTL, so we own that register for them and always write it.
//
// The rotation fed to lb_madctl() has to be the PANEL's, not _gfx's: with a
// canvas in play _gfx is the framebuffer and reports rotation 0, so using it
// reprogrammed a landscape panel back to portrait scan order and the screen
// turned to garbage. Found on the 1.9" with a canvas allocated.
//
// It is tempting to skip the write when the request happens to match the
// driver's own hardcoded default — and that is a bug, found on a 1.9" ST7789:
// after forcing BGR, asking for RGB again matched ST7789's default, the write
// was skipped, and the panel stayed in BGR. The driver only ever writes MADCTL
// from setRotation(), so "switching back" has no other path. Always write.
void LB_Display::applyColorOrder() {
  if (_panel->driver == LB_DRV_ST7735) return;  // handled at construction
  _madctlOverride = true;
  _bus->beginWrite();
  _bus->writeC8D8(0x36, lb_madctl(_panel->driver, panelRotation(), colorOrderIsBGR()));
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

  // Bring the panel up FIRST. Arduino_TFT only swaps WIDTH/HEIGHT for a
  // landscape rotation inside setRotation(), which begin() is what calls — so
  // reading _driver->width() before this point returns the portrait size and a
  // canvas built from it comes out the wrong shape. That bug reached hardware:
  // a 170x320 panel at rotation 1 reported 170x320 instead of 320x170 and LVGL
  // drew into a slice of the screen.
  if (!_driver->begin(spiHz)) {
    Serial.println(F("[LB_Display] panel begin failed — check the wiring, "
                     "DC in particular."));
    return false;
  }
  _gfx = _driver;

  if (useCanvas) {
    // Now the driver knows its rotated size, so the framebuffer matches it.
    _canvas = new Arduino_Canvas(_driver->width(), _driver->height(), _driver);
    // GFX_SKIP_OUTPUT_BEGIN: the panel is already up, do not re-init it.
    if (_canvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
      _gfx = _canvas;
    } else {
      // width * height * 2 bytes would not fit. Fall back to drawing straight
      // at the panel rather than failing: begin(true) is a preference, not a
      // requirement, and hasCanvas() reports which one you got.
      delete _canvas;
      _canvas = nullptr;
      Serial.printf("[LB_Display] no room for a %d x %d framebuffer "
                    "(%lu bytes) — drawing direct instead. Enable "
                    "Tools > PSRAM if your board has it.\n",
                    _driver->width(), _driver->height(),
                    (unsigned long)_driver->width() * _driver->height() * 2);
    }
  }

  applyColorOrder();
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
  r &= 3;
  if (_canvas && ((r ^ rotation()) & 1)) {
    // Portrait <-> landscape would need a framebuffer of the other shape, and
    // the one we allocated is the wrong way round. Refuse rather than render
    // into a mis-shaped buffer.
    Serial.println(F("[LB_Display] cannot switch between portrait and "
                     "landscape while a canvas is allocated — set the rotation "
                     "before begin(true), or use begin() without a canvas."));
    return;
  }
  _gfx->setRotation(r);
  // The driver just rewrote MADCTL from its own idea of the colour order, so
  // ours has to go back on top.
  if (_madctlOverride) {
    _bus->beginWrite();
    _bus->writeC8D8(0x36,
                    lb_madctl(_panel->driver, panelRotation(), colorOrderIsBGR()));
    _bus->endWrite();
  }
}

// ─── Backlight ───────────────────────────────────────────────────────────────

void LB_Display::backlightBegin() {
  if (!_blPolarityForced) _blActiveLow = _panel->blActiveLow;
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

void LB_Display::setBacklightActiveLow(bool activeLow) {
  _blActiveLow = activeLow;
  _blPolarityForced = true;
  backlight(_blLevel);   // re-apply at the new polarity straight away
}

void LB_Display::backlight(uint8_t level) {
  if (!_blReady || _wiring.backlight < 0) return;

  // Active-low panels want a LOW duty cycle to be bright. Getting this
  // backwards is the classic "why is my screen dark at 255" bug — it is
  // decided by the panel table, not by the sketch.
  _blLevel = level;
  uint8_t duty = _blActiveLow ? (uint8_t)(255 - level) : level;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(_wiring.backlight, duty);
#else
  ledcWrite(LB_BL_PWM_CHANNEL, duty);
#endif
}

// ─── Diagnostics ─────────────────────────────────────────────────────────────

void LB_Display::printInfo(Print &out) const {
  static const char *kDrivers[] = {"ST7735", "ST7789", "ST7796", "NV3007",
                                   "ILI9341"};
  out.println(F("---- Lonely Binary Display ----"));
  out.printf("Panel      : %s (%s)\n", _panel->name, _panel->id);
  out.printf("Driver IC  : %s\n", kDrivers[_panel->driver]);
  out.printf("Resolution : %dx%d  (native %dx%d, rotation %d)\n",
             width(), height(), _panel->width, _panel->height, _panel->rotation);
  out.printf("SPI clock  : %ld Hz%s\n",
             (long)(_spiHzOverride ? _spiHzOverride : _panel->spiHz),
             _spiHzOverride ? "  (overridden)" : "");
  out.printf("Backlight  : PWM, active %s%s%s\n",
             _blActiveLow ? "LOW" : "HIGH",
             _blPolarityForced ? " (forced)" : "",
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
