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
  delete _tft;
  delete _bus;
  free(_fb);
}

bool LB_Display::colorOrderIsBGR() const {
  if (_colorOrder == COLOR_RGB) return false;
  if (_colorOrder == COLOR_BGR) return true;
  return _panel->bgr;  // COLOR_AUTO: whatever the panel table says
}

bool LB_Display::setColorOrder(ColorOrder order) {
  _colorOrder = order;
  if (_tft) _tft->setColorOrder(colorOrderIsBGR());
  return true;
}

void LB_Display::setInverted(bool inverted) {
  _inverted = inverted;
  if (_tft) _tft->setInverted(inverted);
}

void LB_Display::setWiring(const LB_Wiring &wiring) {
  if (_tft) return;  // too late; begin() has already built the bus
  _wiring = wiring;
  _customWiring = true;
}

bool LB_Display::begin(bool useCanvas) {
  if (_tft) return true;  // already up

  const int32_t spiHz = _spiHzOverride ? _spiHzOverride : _panel->spiHz;

  _bus = new LB_TFTSpiBus(_wiring.dc, _wiring.cs, _wiring.sclk, _wiring.mosi,
                          _wiring.spiHost);
  _tft = new LB_TFT(_bus, _panel, _wiring.rst);
  // Colour order and inversion go in here, so anything set before begin()
  // is what the panel comes up with. _inverted starts as the panel table's
  // value (see the constructor) - if it started as false, every panel whose
  // table says invert: true would come up as a photo negative.
  if (!_tft->begin(spiHz, colorOrderIsBGR(), _inverted)) {
    Serial.println(F("[LB_Display] panel begin failed — check the wiring, "
                     "DC in particular."));
    delete _tft;
    _tft = nullptr;
    return false;
  }

  if (useCanvas) {
    // The panel is already at its rotation, so this is the shape the canvas
    // draws in. PSRAM first; the smaller panels also fit in internal RAM.
    const size_t bytes = (size_t)_tft->width() * _tft->height() * 2;
    _fb = (uint16_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    _fbInPsram = _fb != nullptr;
    if (!_fb) _fb = (uint16_t *)heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    if (_fb) {
      memset(_fb, 0, bytes);
    } else {
      // begin(true) is a preference, not a requirement: draw straight at the
      // panel rather than failing, and hasCanvas() reports which one you got.
      Serial.printf("[LB_Display] no room for a %d x %d framebuffer "
                    "(%lu bytes) — drawing direct instead. Enable "
                    "Tools > PSRAM if your board has it.\n",
                    _tft->width(), _tft->height(), (unsigned long)bytes);
    }
  }

  // Attach the backlight PWM and turn it on, exactly as the MicroPython begin()
  // does. Without this, _blReady stays false and backlight() returns at its
  // first line — so display.backlight(255) did nothing, and selfTest()'s
  // backlight sweep was silently skipped.
  backlightBegin();
  backlight(255);

  // Hand the driver and the framebuffer (if any) to the LB_Panel adapter, so
  // LB_Canvas draws through it.
  attach(_tft, _fb);
  return true;
}

void LB_Display::pushImage(int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint16_t *px) {
  if (!_tft || !px) return;
  if (!_fb) {
    _tft->pushImage(x, y, w, h, px);
    return;
  }
  // Into the framebuffer, clipped; it reaches the panel on flush().
  const int16_t fw = _tft->width(), fh = _tft->height();
  for (int16_t r = 0; r < h; r++) {
    const int16_t yy = y + r;
    if (yy < 0 || yy >= fh) continue;
    int16_t x0 = x < 0 ? 0 : x;
    int16_t x1 = x + w > fw ? fw : x + w;
    if (x1 <= x0) return;
    memcpy(_fb + (size_t)yy * fw + x0, px + (size_t)r * w + (x0 - x),
           (size_t)(x1 - x0) * 2);
  }
}

void LB_Display::setRotation(uint8_t r) {
  if (!_tft) return;
  r &= 3;
  if (_fb && ((r ^ _tft->rotation()) & 1)) {
    // Portrait <-> landscape would need a framebuffer of the other shape, and
    // the one we allocated is the wrong way round. Refuse rather than render
    // into a mis-shaped buffer.
    Serial.println(F("[LB_Display] cannot switch between portrait and "
                     "landscape while a canvas is allocated — use begin() "
                     "without a canvas to rotate that far."));
    return;
  }
  _tft->setRotation(r);
  LB_Canvas::setRotation(r);  /* keep the canvas's idea of rotation in step */
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
                                   "ILI9341", "ILI9488"};
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
  out.printf("Framebuffer: %s\n", !_fb ? "no (direct)"
                                 : _fbInPsram ? "yes (PSRAM)" : "yes (internal RAM)");
  // A customer whose screen misbehaves pastes this whole block into an AI
  // assistant. Carrying the URL means the assistant is handed the library's
  // real API along with the symptom, instead of guessing from TFT_eSPI.
  out.println(F("AI reference: https://raw.githubusercontent.com/"
                "Lonely-Binary/LonelyBinaryDisplay/main/llms.txt"));
  out.println(F("-------------------------------"));
}

void LB_Display::selfTest() {
  if (!_tft) return;
  const int16_t w = width(), h = height();

  // Everything below draws through LB_Canvas rather than through gfx(), which
  // is the point of the migration: this same body would run on VGA or on
  // e-paper. It is also the self-test, so if the canvas path is wrong on a
  // real panel, this is where it shows.
  //
  // Colours are lb_color_t, NOT the RGB565 names. Handing an lb_color_t to a
  // gfx() call truncates it silently - see LB_Colors.h.

  // 1. Solid colours - confirms the panel, the wiring and the colour order.
  const lb_color_t solids[] = {LB_RED, LB_GREEN, LB_BLUE};
  const char *names[] = {"RED", "GREEN", "BLUE"};
  for (int i = 0; i < 3; i++) {
    fillScreen(solids[i]);
    setTextColor(LB_WHITE);
    setTextSize(2);
    drawString(names[i], 4, h / 2 - 8);
    flush();
    delay(700);
  }

  // 2. Backlight sweep. Every panel in the range dims, so this runs on all of
  //    them - and it is the quickest way to spot a polarity mistake, because a
  //    panel wired the other way round sweeps backwards.
  if (_wiring.backlight >= 0) {
    fillScreen(LB_WHITE);
    setTextColor(LB_BLACK);
    setTextSize(1);
    drawString("Backlight sweep", 4, 4);
    flush();
    for (int v = 255; v >= 30; v -= 5) { backlight(v); delay(8); }
    for (int v = 30; v <= 255; v += 5) { backlight(v); delay(8); }
  }

  // 3. Colour bars + panel identity.
  fillScreen(LB_BLACK);
  const lb_color_t bars[] = {LB_RED, LB_GREEN, LB_BLUE, LB_YELLOW,
                             LB_MAGENTA, LB_CYAN, LB_WHITE, LB_GRAY};
  const int16_t barTop = h / 3;
  const int16_t barH = (h - barTop) / 8;
  for (int i = 0; i < 8; i++) {
    fillRect(0, barTop + i * barH, w, barH, bars[i]);
  }

  setTextColor(LB_WHITE);
  setTextSize(w >= 200 ? 2 : 1);
  drawString("LonelyBinary", 4, 6);
  setTextSize(1);
  drawString(_panel->name, 4, barTop - 22);
  char wh[24];
  snprintf(wh, sizeof(wh), "%dx%d", w, h);
  drawString(wh, 4, barTop - 12);
  flush();
}
