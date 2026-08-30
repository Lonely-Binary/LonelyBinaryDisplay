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
  switch (p->driver) {
    case LB_DRV_ST7735:
      return new Arduino_ST7735(_bus, LB_WIRING.rst, p->rotation, p->ips,
                                p->width, p->height,
                                p->colOff1, p->rowOff1, p->colOff2, p->rowOff2);
    case LB_DRV_ST7789:
      return new Arduino_ST7789(_bus, LB_WIRING.rst, p->rotation, p->ips,
                                p->width, p->height,
                                p->colOff1, p->rowOff1, p->colOff2, p->rowOff2);
    case LB_DRV_ST7796:
      return new Arduino_ST7796(_bus, LB_WIRING.rst, p->rotation, p->ips,
                                p->width, p->height,
                                p->colOff1, p->rowOff1, p->colOff2, p->rowOff2);
    case LB_DRV_NV3007:
      // The 2.79" is the same silicon as the 1.68" but needs its own
      // voltage/gamma table — without it the panel comes up looking wrong.
      if (p->initOps == LB_INIT_NV3007_279) {
        return new Arduino_NV3007(_bus, LB_WIRING.rst, p->rotation, p->ips,
                                  p->width, p->height,
                                  p->colOff1, p->rowOff1, p->colOff2, p->rowOff2,
                                  nv3007_279_init_operations,
                                  sizeof(nv3007_279_init_operations));
      }
      return new Arduino_NV3007(_bus, LB_WIRING.rst, p->rotation, p->ips,
                                p->width, p->height,
                                p->colOff1, p->rowOff1, p->colOff2, p->rowOff2);
  }
  return nullptr;
}

bool LB_Display::begin(bool useCanvas) {
  if (_gfx) return true;  // already up

  _bus = new Arduino_ESP32SPI(LB_WIRING.dc, LB_WIRING.cs, LB_WIRING.sclk,
                              LB_WIRING.mosi, GFX_NOT_DEFINED /* MISO unused */,
                              LB_WIRING.spiHost, true /* shared bus */);
  _driver = makeDriver();
  if (!_driver) return false;

  if (useCanvas) {
    // Canvas dimensions follow the panel's shipping rotation, which the driver
    // has already applied.
    _canvas = new Arduino_Canvas(_driver->width(), _driver->height(), _driver);
    // Arduino_Canvas::begin() brings up the output panel too.
    if (!_canvas->begin(_panel->spiHz)) {
      Serial.println(F("[LB_Display] canvas begin failed."));
      Serial.println(F("[LB_Display] A framebuffer needs width*height*2 bytes. "
                       "Set Tools > PSRAM to Enabled, or call begin() without "
                       "a canvas."));
      return false;
    }
    _gfx = _canvas;
  } else {
    if (!_driver->begin(_panel->spiHz)) {
      Serial.println(F("[LB_Display] panel begin failed — check the wiring, "
                       "DC in particular."));
      return false;
    }
    _gfx = _driver;
  }

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
  if (_gfx) _gfx->setRotation(r & 3);
}

// ─── Backlight ───────────────────────────────────────────────────────────────

void LB_Display::backlightBegin() {
  if (LB_WIRING.backlight < 0) return;
  if (_panel->blMode == LB_BL_PWM) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(LB_WIRING.backlight, LB_BL_PWM_FREQ, LB_BL_PWM_BITS);
#else
    ledcSetup(LB_BL_PWM_CHANNEL, LB_BL_PWM_FREQ, LB_BL_PWM_BITS);
    ledcAttachPin(LB_WIRING.backlight, LB_BL_PWM_CHANNEL);
#endif
  } else {
    pinMode(LB_WIRING.backlight, OUTPUT);
  }
  _blReady = true;
}

void LB_Display::backlight(uint8_t level) {
  if (!_blReady || LB_WIRING.backlight < 0) return;

  if (_panel->blMode == LB_BL_PWM) {
    // Active-low panels want a LOW duty cycle to be bright. Getting this
    // backwards is the classic "why is my screen dark at 255" bug — it is
    // decided by the panel table, not by the sketch.
    uint8_t duty = _panel->blActiveLow ? (uint8_t)(255 - level) : level;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(LB_WIRING.backlight, duty);
#else
    ledcWrite(LB_BL_PWM_CHANNEL, duty);
#endif
  } else {
    bool on = level > 0;
    digitalWrite(LB_WIRING.backlight, _panel->blActiveLow ? !on : on);
  }
}

// ─── Diagnostics ─────────────────────────────────────────────────────────────

void LB_Display::printInfo(Print &out) const {
  static const char *kDrivers[] = {"ST7735", "ST7789", "ST7796", "NV3007"};
  out.println(F("---- Lonely Binary Display ----"));
  out.printf("Panel      : %s (%s)\n", _panel->name, _panel->id);
  out.printf("Driver IC  : %s\n", kDrivers[_panel->driver]);
  out.printf("Resolution : %dx%d  (native %dx%d, rotation %d)\n",
             width(), height(), _panel->width, _panel->height, _panel->rotation);
  out.printf("SPI clock  : %ld Hz\n", (long)_panel->spiHz);
  out.printf("Backlight  : %s, active %s\n",
             _panel->blMode == LB_BL_PWM ? "PWM" : "on/off",
             _panel->blActiveLow ? "LOW" : "HIGH");
  out.printf("Board      : %s\n", LB_WIRING_NAME);
  out.printf("Pins       : CS=%d RST=%d DC=%d MOSI=%d SCLK=%d BL=%d\n",
             LB_WIRING.cs, LB_WIRING.rst, LB_WIRING.dc,
             LB_WIRING.mosi, LB_WIRING.sclk, LB_WIRING.backlight);
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

  // 2. Backlight sweep — only meaningful where the panel supports dimming.
  if (_panel->blMode == LB_BL_PWM) {
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
