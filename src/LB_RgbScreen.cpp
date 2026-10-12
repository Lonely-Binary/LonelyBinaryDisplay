#include "LB_RgbScreen.h"

#include <soc/soc_caps.h>
#if SOC_LCD_RGB_SUPPORTED
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Frame timing, kept by the vertical-blank interrupt: a frame that is not as long as the timing says was refilled late or restarted.
static volatile uint32_t s_frames = 0, s_off = 0, s_maxUs = 0, s_expectUs = 0;
static volatile int64_t s_lastUs = 0;
static bool IRAM_ATTR lb_rgb_on_vsync(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t *,
                                      void *ctx) {
  const int64_t now = esp_timer_get_time();
  if (s_lastUs && s_expectUs) {
    const uint32_t dt = (uint32_t)(now - s_lastUs);
    s_frames = s_frames + 1;
    const uint32_t tol = s_expectUs / 50;
    if (dt + tol < s_expectUs || dt > s_expectUs + tol) s_off = s_off + 1;
    if (dt > s_maxUs) s_maxUs = dt;
  }
  s_lastUs = now;
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR((SemaphoreHandle_t)ctx, &woken);
  return woken == pdTRUE;
}
#endif

LB_RgbScreen::~LB_RgbScreen() {
#if SOC_LCD_RGB_SUPPORTED
  if (_handle) esp_lcd_panel_del((esp_lcd_panel_handle_t)_handle);
  if (_vsync) vSemaphoreDelete((SemaphoreHandle_t)_vsync);
#endif
}

bool LB_RgbScreen::begin(int32_t, bool, bool) {
#if SOC_LCD_RGB_SUPPORTED
  const LB_RgbBoard *b = _panel->rgb;
  if (!b) return false;

  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = b->pclkHz;
  cfg.timings.h_res = _panel->width;
  cfg.timings.v_res = _panel->height;
  cfg.timings.hsync_pulse_width = b->hsPulse;
  cfg.timings.hsync_back_porch = b->hsBack;
  cfg.timings.hsync_front_porch = b->hsFront;
  cfg.timings.vsync_pulse_width = b->vsPulse;
  cfg.timings.vsync_back_porch = b->vsBack;
  cfg.timings.vsync_front_porch = b->vsFront;
  // Which way the sync pulses go (`syncIdleHigh`, panels.yaml). The 7-inch panel runs in DE mode and ignores them; the 4.3-inch one
  // takes its frame start from the vertical sync, and with the pulse the wrong way round it locked on to a different line on about
  // four boots in ten and stayed there until the next boot.
  cfg.timings.flags.hsync_idle_low = b->syncIdleHigh ? 0 : 1;
  cfg.timings.flags.vsync_idle_low = b->syncIdleHigh ? 0 : 1;
  cfg.timings.flags.pclk_active_neg = b->pclkActiveNeg;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  // `directScan` (panels.yaml): scanned straight out of PSRAM with ONE frame buffer: with two and no bounce buffers the picture
  // alternated between them and showed two pages on top of each other after a page change. LVGL then draws into the buffer on show
  // (small areas, so a little tearing at worst, as in the Arduino_GFX Sunton configurations).
  const bool direct = b->directScan;
  cfg.num_fbs = direct ? 1 : 2;
  // Bounce buffers: the scan-out DMA reads two small buffers in internal SRAM,
  // refilled from the PSRAM framebuffer by an interrupt. Without them the DMA
  // reads PSRAM directly, and when the CPU is busy in PSRAM too - LVGL blending
  // a translucent overlay over the whole 7" screen - it starves and the picture
  // tears. Ten lines each; the size must divide the frame evenly.
  // ESP-IDF wants the frame to be a whole number of bounce buffers, and the two buffers are used in turn, so the number of buffers
  // per frame must be EVEN (an odd count swaps their roles every frame and the picture jumps up and down: 272 / 16 = 17 did exactly
  // that). Each refill is also a deadline: the buffer is read out in (lines x line time) and the interrupt must have copied the next
  // one from PSRAM by then. A smaller panel has a shorter line (480 px at 8 MHz is 67 us against the 7-inch's 86 us), so eight lines
  // is little room there: the picture shows stale lines when Wi-Fi and the renderer fight over PSRAM. Take the smallest line count
  // of at least ten that divides the height into an even number of buffers (480 -> 10, as before; 272 -> 17, 1.1 ms); a height with
  // none up to 40 takes the largest below ten that does, and one with none at all goes without bounce buffers.
  uint16_t bounceLines = 0;
  for (uint16_t n = 10; n <= 40 && !bounceLines; n++) if (_panel->height % n == 0 && (_panel->height / n) % 2 == 0) bounceLines = n;
  for (uint16_t n = 9; n >= 2 && !bounceLines; n--) if (_panel->height % n == 0 && (_panel->height / n) % 2 == 0) bounceLines = n;
  // A directScan panel has no bounce buffers, as the widely used Sunton configurations: its 10 MHz clock needs 20 MB/s, which the PSRAM
  // has plenty of. The bounce-buffer scheme counts refills in software, and one missed refill at start-up left the picture shifted
  // until the next boot (about four boots in ten, measured on the board).
#ifdef LB_RGB_BOUNCE_LINES
  bounceLines = LB_RGB_BOUNCE_LINES;   // a build may force it (0 = none) to compare
#endif
  cfg.bounce_buffer_size_px = direct ? 0 : (size_t)_panel->width * bounceLines;
  cfg.dma_burst_size = 64;
  cfg.hsync_gpio_num = b->hsync;
  cfg.vsync_gpio_num = b->vsync;
  cfg.de_gpio_num = b->de;
  cfg.pclk_gpio_num = b->pclk;
  cfg.disp_gpio_num = -1;
  for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = b->data[i];
  cfg.flags.fb_in_psram = 1;

  esp_lcd_panel_handle_t h = nullptr;
  esp_err_t err = esp_lcd_new_rgb_panel(&cfg, &h);
  if (err == ESP_ERR_NO_MEM) {
    // Two buffers did not fit: one still works, it just may tear.
    cfg.num_fbs = 1;
    err = esp_lcd_new_rgb_panel(&cfg, &h);
  }
  if (err != ESP_OK) {
    Serial.printf("[LB_RgbScreen] esp_lcd_new_rgb_panel failed: %s%s\n", esp_err_to_name(err),
                  err == ESP_ERR_NO_MEM ? " - the framebuffer needs PSRAM: Tools > PSRAM > OPI PSRAM" : "");
    return false;
  }
  esp_lcd_panel_reset(h);
  // The buffers are cleared BEFORE the scan-out starts: clearing 500 KB of PSRAM while the first frames are being read out made the
  // start race with it.
  void *fb = nullptr, *fb2 = nullptr;
  if (cfg.num_fbs == 2) esp_lcd_rgb_panel_get_frame_buffer(h, 2, &fb, &fb2);
  else esp_lcd_rgb_panel_get_frame_buffer(h, 1, &fb);
  {
    const size_t cleared = (size_t)_panel->width * _panel->height * 2;
    if (fb) memset(fb, 0, cleared);
    if (fb2) memset(fb2, 0, cleared);
  }
  esp_lcd_panel_init(h);
  _handle = h;
  s_expectUs = (uint32_t)(1000000.0 * (b->hsPulse + b->hsBack + _panel->width + b->hsFront) * (b->vsPulse + b->vsBack + _panel->height + b->vsFront) / b->pclkHz);
  _fb = (uint16_t *)fb;
  _fb2 = (uint16_t *)fb2;
  sync(0, 0, _panel->width, _panel->height);

  _vsync = xSemaphoreCreateBinary();
  esp_lcd_rgb_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = lb_rgb_on_vsync;
  esp_lcd_rgb_panel_register_event_callbacks(h, &cbs, _vsync);
  return true;
#else
  Serial.println(F("[LB_RgbScreen] RGB panels need an ESP32-S3 (LCD_CAM). This chip has none."));
  return false;
#endif
}

void LB_RgbScreen::setRotation(uint8_t r) {
  if (r & 3) Serial.println(F("[LB_RgbScreen] RGB panels support rotation 0 only, for now."));
}

// Write the cache lines covering this rectangle back to PSRAM, where the scan-
// out reads them. Handing esp_lcd a pointer that lies inside its own framebuffer
// makes draw_bitmap do exactly that, and nothing else - no copy.
void LB_RgbScreen::sync(int16_t x, int16_t y, int16_t w, int16_t h) {
#if SOC_LCD_RGB_SUPPORTED
  if (!_handle || w <= 0 || h <= 0) return;
  esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)_handle, x, y, x + w, y + h,
                            _fb + (size_t)y * _panel->width + x);
#endif
}

void LB_RgbScreen::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (!_fb) return;
  const int16_t W = _panel->width, H = _panel->height;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > W) w = W - x;
  if (y + h > H) h = H - y;
  if (w <= 0 || h <= 0) return;
  for (int16_t r = 0; r < h; r++) {
    uint16_t *p = _fb + (size_t)(y + r) * W + x;
    for (int16_t c = 0; c < w; c++) p[c] = color;
  }
  sync(x, y, w, h);
}

void LB_RgbScreen::pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *px) {
  if (!_fb || !px) return;
  const int16_t W = _panel->width, H = _panel->height;
  int16_t sx = 0, sy = 0, cw = w, ch = h;
  if (x < 0) { sx = -x; cw += x; x = 0; }
  if (y < 0) { sy = -y; ch += y; y = 0; }
  if (x + cw > W) cw = W - x;
  if (y + ch > H) ch = H - y;
  if (cw <= 0 || ch <= 0) return;
  for (int16_t r = 0; r < ch; r++)
    memcpy(_fb + (size_t)(y + r) * W + x, px + (size_t)(sy + r) * w + sx, (size_t)cw * 2);
  sync(x, y, cw, ch);
}

// Hand the whole buffer to esp_lcd: it writes the cache back and scans that
// buffer out from the next frame. Wait for the vertical blank that starts it,
// so the caller may draw into the other buffer straight away.
void LB_RgbScreen::present(const uint16_t *fb) {
#if SOC_LCD_RGB_SUPPORTED
  if (!_handle || !fb) return;
  xSemaphoreTake((SemaphoreHandle_t)_vsync, 0);                 // forget an old blank
  esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)_handle, 0, 0, _panel->width, _panel->height, fb);
  xSemaphoreTake((SemaphoreHandle_t)_vsync, pdMS_TO_TICKS(100));
#endif
}

void LB_RgbScreen::flushFramebuffer(const uint16_t *fb) {
  // The canvas and LVGL draw straight into our framebuffer, so there is
  // nothing to copy - only the cache to write back.
  if (fb == _fb) sync(0, 0, _panel->width, _panel->height);
  else pushImage(0, 0, _panel->width, _panel->height, fb);
}

bool LB_RgbScreen::scanStats(uint32_t *frames, uint32_t *off, uint32_t *maxUs) {
#if SOC_LCD_RGB_SUPPORTED
  if (frames) *frames = s_frames;
  if (off) *off = s_off;
  if (maxUs) *maxUs = s_maxUs;
  return s_expectUs != 0;
#else
  (void)frames; (void)off; (void)maxUs; return false;
#endif
}
