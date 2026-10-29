/*
  TapWatchLVGL - a smart-watch for the Tap series, built on LVGL.

  Anti-aliased dials and rings, real fonts, and a tile view that follows your
  finger and snaps. One sketch for all the watch panels: change the one panel
  line below (LB_TAP_128_CTP, the 1.28 inch round; LB_TAP_150_CTP, the 1.5 inch
  240 x 320; LB_TAP_196_CTP, the 1.96 inch 240 x 296). Everything is laid out
  from the screen size.

  The clock is in the middle of a cross of pages. Swipe to move between them:

                       swipe DOWN
                      [ SETTINGS ]        brightness, face, 12/24 h
                           |
     swipe RIGHT                          swipe LEFT
     [ STOPWATCH ] --- [ CLOCK ] --- [ DATE ]
                           |
                      [ APPS ]            torch, sparkle
                       swipe UP

  On the clock page:   double tap   sleeps the screen (touch to wake)
  In an app:           double tap   leaves the app
  The screen dims by itself after IDLE_DIM_MS and wakes on touch.

  TIME. By default the clock starts from the moment this sketch was compiled -
  nothing to set up, but it restarts from that moment on every reset, so it is
  only right for a demo. For the real time, uncomment USE_NTP below, fill in your
  Wi-Fi name and password and your time zone, and the watch sets itself over
  the internet a few seconds after power-up, then switches Wi-Fi off.
*/
#include <LonelyBinaryDisplay.h>
#include <LonelyBinaryLVGL.h>
#include <LB_TouchIT7259.h>   // the touch drivers for the Tap panels: including one registers it
#include <LB_TouchCST816.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>

// ── Time source ─────────────────────────────────────────────────────────────
// #define USE_NTP                       // <- uncomment to set the time over Wi-Fi

#ifdef USE_NTP
  #include <WiFi.h>
  static const char *WIFI_SSID = "your-network-name";
  static const char *WIFI_PASS = "your-password";
  static const long  GMT_OFFSET_S = 0;        // seconds east of UTC: UTC+8 is 8 * 3600
  static const int   DST_OFFSET_S = 0;        // 3600 while daylight saving is on
#endif

// ── Tuning ──────────────────────────────────────────────────────────────────
static const int32_t  SPI_HZ      = 40000000;   // SPI panels only (not the 1.96); drop to 20000000 (or 10000000) if the picture tears or shows noise
static const uint32_t IDLE_DIM_MS = 15000;      // dim after this long without a touch
static const uint8_t  DIM_LEVEL   = 25;         // backlight while dimmed, 0-255

LB_Display display(LB_TAP_196_CTP);
//LB_Display display(LB_TAP_128_CTP);
//LB_Display display(LB_TAP_150_CTP);

static int W, H, D;     // screen size, and the smaller side (the dial's room); set in setup()

// Lonely Binary brand colours.
#define C_TEAL  lv_color_hex(0x14898F)
#define C_RED   lv_color_hex(0xD12428)
#define C_WHITE lv_color_white()
#define C_DIM   lv_color_hex(0x5A6064)

// ── time ────────────────────────────────────────────────────────────────────

struct Now { tm t; float sec; };   // sec includes the fraction, for a sweeping hand

static const char *DAYS[]   = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
static const char *MONTHS[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

static Now readClock() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  Now n;
  localtime_r(&tv.tv_sec, &n.t);
  n.sec = n.t.tm_sec + tv.tv_usec / 1e6f;
  return n;
}

// Start from the moment this sketch was compiled.
static void setTimeFromCompile() {
  char mon[4];
  int d, y, H, M, S;
  sscanf(__DATE__, "%3s %d %d", mon, &d, &y);
  sscanf(__TIME__, "%d:%d:%d", &H, &M, &S);
  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon = (strstr(months, mon) - months) / 3;
  t.tm_mday = d;
  t.tm_hour = H; t.tm_min = M; t.tm_sec = S;
  t.tm_isdst = -1;
  timeval tv = {mktime(&t), 0};
  settimeofday(&tv, nullptr);
}

#ifdef USE_NTP
static bool ntpStarted = false, ntpSynced = false;

static void ntpBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

// Call from loop(): waits for Wi-Fi, asks for the time, then switches Wi-Fi off.
static void ntpTick() {
  static uint32_t last = 0;
  if (ntpSynced || millis() - last < 1000) return;
  last = millis();
  if (WiFi.status() != WL_CONNECTED) return;
  if (!ntpStarted) {
    configTime(GMT_OFFSET_S, DST_OFFSET_S, "pool.ntp.org", "time.google.com");
    ntpStarted = true;
  }
  tm t;
  if (getLocalTime(&t, 10) && t.tm_year >= 120) {   // 2020 or later: the answer came in
    ntpSynced = true;
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
  }
}
static const char *timeSource() { return ntpSynced ? "TIME: NTP" : "TIME: WAITING..."; }
#else
static const char *timeSource() { return "TIME: COMPILED"; }
#endif

// ── state ───────────────────────────────────────────────────────────────────

static uint8_t face = 0;                 // 0 analog, 1 digital, 2 rings
static bool    h24 = true;
static uint8_t userBright = 255;
static float   bl = 255;
static bool    manualSleep = false;
static bool    torchOn = false;

static lv_obj_t *tv, *clockTile;
static lv_obj_t *faceBox[3];
static lv_obj_t *guard, *torch, *sparkle;

// ── small helpers ───────────────────────────────────────────────────────────

static void noClick(lv_obj_t *o) {
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) noClick(lv_obj_get_child(o, i));
}

static lv_obj_t *label(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t c,
                       int x = 0, int y = 0) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, c, 0);
  lv_obj_align(l, LV_ALIGN_CENTER, x, y);
  return l;
}

// Only touch a label when its text changed: LVGL redraws a label every time it is set.
static void setText(lv_obj_t *l, const char *txt) {
  if (strcmp(lv_label_get_text(l), txt) != 0) lv_label_set_text(l, txt);
}

static lv_obj_t *roundButton(lv_obj_t *parent, int size, lv_color_t bg, const char *txt,
                             const lv_font_t *font, int x, int y, lv_event_cb_t cb) {
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_set_size(b, size, size);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, bg, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_align(b, LV_ALIGN_CENTER, x, y);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  return b;
}

// A thin ring that shows a fraction of a turn from 12 o'clock.
static lv_obj_t *ringArc(lv_obj_t *parent, int size, int width, lv_color_t color, lv_color_t track,
                         int32_t maxValue) {
  lv_obj_t *a = lv_arc_create(parent);
  lv_obj_set_size(a, size, size);
  lv_obj_center(a);
  lv_arc_set_rotation(a, 270);
  lv_arc_set_bg_angles(a, 0, 360);
  lv_arc_set_range(a, 0, maxValue);
  lv_obj_remove_style(a, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(a, track, LV_PART_MAIN);
  lv_obj_set_style_arc_color(a, color, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
  return a;
}

static lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, W, H);
  lv_obj_center(o);
  return o;
}

// ── clock faces ─────────────────────────────────────────────────────────────

// Analog
static lv_obj_t *aScale, *aHour, *aMin, *aSec, *aDate;
// Digital
static lv_obj_t *dTime, *dAmPm, *dDate, *dArc;
// Rings
static lv_obj_t *rHour, *rMin, *rSec, *rTime, *rDate;

static void buildAnalog(lv_obj_t *parent) {
  static const char *numerals[] = {"12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "", nullptr};
  aScale = lv_scale_create(parent);
  lv_obj_set_size(aScale, D - 8, D - 8);
  lv_obj_center(aScale);
  lv_scale_set_mode(aScale, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_range(aScale, 0, 3600);              // 60 per second, so the hand can sweep
  lv_scale_set_total_tick_count(aScale, 61);
  lv_scale_set_major_tick_every(aScale, 5);
  lv_scale_set_angle_range(aScale, 360);
  lv_scale_set_rotation(aScale, 270);               // 12 o'clock at the top
  lv_scale_set_label_show(aScale, true);
  lv_scale_set_text_src(aScale, numerals);
  lv_obj_set_style_length(aScale, 5, LV_PART_ITEMS);
  lv_obj_set_style_length(aScale, 11, LV_PART_INDICATOR);
  lv_obj_set_style_line_width(aScale, 1, LV_PART_ITEMS);
  lv_obj_set_style_line_width(aScale, 3, LV_PART_INDICATOR);
  lv_obj_set_style_line_color(aScale, C_TEAL, LV_PART_ITEMS);
  lv_obj_set_style_line_color(aScale, C_WHITE, LV_PART_INDICATOR);
  lv_obj_set_style_text_color(aScale, C_WHITE, LV_PART_INDICATOR);
  lv_obj_set_style_text_font(aScale, &lv_font_montserrat_14, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(aScale, 0, LV_PART_MAIN);

  // Inside the dial, not in the bands above and below it on a tall screen: the
  // glass bezel hides the edges of those.
  label(parent, "LONELY BINARY", &lv_font_montserrat_10, C_TEAL, 0, -50);
  aDate = label(parent, "", &lv_font_montserrat_18, C_WHITE, 0, 52);

  // Hands last, so they sit on top of the text.
  aHour = lv_line_create(aScale);
  aMin  = lv_line_create(aScale);
  aSec  = lv_line_create(aScale);
  lv_obj_set_style_line_width(aHour, 7, 0);  lv_obj_set_style_line_color(aHour, C_WHITE, 0);
  lv_obj_set_style_line_width(aMin, 5, 0);   lv_obj_set_style_line_color(aMin, C_WHITE, 0);
  lv_obj_set_style_line_width(aSec, 2, 0);   lv_obj_set_style_line_color(aSec, C_RED, 0);
  lv_obj_set_style_line_rounded(aHour, true, 0);
  lv_obj_set_style_line_rounded(aMin, true, 0);
  lv_obj_set_style_line_rounded(aSec, true, 0);

  lv_obj_t *cap = lv_obj_create(parent);
  lv_obj_remove_style_all(cap);
  lv_obj_set_size(cap, 14, 14);
  lv_obj_center(cap);
  lv_obj_set_style_radius(cap, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(cap, C_RED, 0);
}

static void buildDigital(lv_obj_t *parent) {
  dArc = ringArc(parent, D - 8, 6, C_TEAL, lv_color_hex(0x1A2024), 600);
  label(parent, "LONELY BINARY", &lv_font_montserrat_10, C_DIM, 0, -62);
  dTime = label(parent, "", &lv_font_montserrat_48, C_WHITE, 0, -8);
  dAmPm = label(parent, "", &lv_font_montserrat_14, C_TEAL, 0, 28);
  dDate = label(parent, "", &lv_font_montserrat_20, C_TEAL, 0, 56);
}

static void buildRings(lv_obj_t *parent) {
  const lv_color_t track = lv_color_hex(0x181C20);
  rSec  = ringArc(parent, D - 8, 12, lv_color_hex(0x2DD4E8), track, 600);
  rMin  = ringArc(parent, D - 36, 12, lv_color_hex(0x34D27B), track, 600);
  rHour = ringArc(parent, D - 64, 12, lv_color_hex(0xF2705B), track, 600);
  rTime = label(parent, "", &lv_font_montserrat_40, C_WHITE, 0, -8);
  rDate = label(parent, "", &lv_font_montserrat_14, lv_color_hex(0x2DD4E8), 0, 28);
}

static void applyFace() {
  for (int i = 0; i < 3; i++) {
    if (i == face) lv_obj_remove_flag(faceBox[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(faceBox[i], LV_OBJ_FLAG_HIDDEN);
  }
}

static void updateClock(const Now &n) {
  int h12 = n.t.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  const int hShow = h24 ? n.t.tm_hour : h12;
  char b[24];

  if (face == 0) {
    lv_scale_set_line_needle_value(aScale, aHour, 56, (int32_t)(((n.t.tm_hour % 12) * 60 + n.t.tm_min) * 5));
    lv_scale_set_line_needle_value(aScale, aMin, 84, (int32_t)((n.t.tm_min + n.sec / 60.0f) * 60));
    lv_scale_set_line_needle_value(aScale, aSec, 98, (int32_t)(n.sec * 60));
    snprintf(b, sizeof b, "%s %d", MONTHS[n.t.tm_mon], n.t.tm_mday);
    setText(aDate, b);
  } else if (face == 1) {
    const bool colon = ((int)(n.sec * 2)) % 2 == 0;
    snprintf(b, sizeof b, h24 ? "%02d%c%02d" : "%d%c%02d", hShow, colon ? ':' : ' ', n.t.tm_min);
    setText(dTime, b);
    setText(dAmPm, h24 ? "" : (n.t.tm_hour < 12 ? "AM" : "PM"));
    snprintf(b, sizeof b, "%.3s %d %s", DAYS[n.t.tm_wday], n.t.tm_mday, MONTHS[n.t.tm_mon]);
    setText(dDate, b);
    lv_arc_set_value(dArc, (int32_t)(n.sec * 10));
  } else {
    lv_arc_set_value(rSec, (int32_t)(n.sec * 10));
    lv_arc_set_value(rMin, (int32_t)((n.t.tm_min + n.sec / 60.0f) * 10));
    lv_arc_set_value(rHour, (int32_t)((((n.t.tm_hour % 12) * 60 + n.t.tm_min) / 720.0f) * 600));
    snprintf(b, sizeof b, h24 ? "%02d:%02d" : "%d:%02d", hShow, n.t.tm_min);
    setText(rTime, b);
    snprintf(b, sizeof b, "%.3s %d", DAYS[n.t.tm_wday], n.t.tm_mday);
    setText(rDate, b);
  }
}

// ── date page ───────────────────────────────────────────────────────────────

static lv_obj_t *iYear, *iDay, *iWeek, *iMonth, *iUp;

static void buildInfo(lv_obj_t *t) {
  iYear  = ringArc(t, D - 8, 6, C_TEAL, lv_color_hex(0x1A2024), 366);
  iWeek  = label(t, "", &lv_font_montserrat_20, C_TEAL, 0, -64);
  iDay   = label(t, "", &lv_font_montserrat_48, C_WHITE, 0, -14);
  iMonth = label(t, "", &lv_font_montserrat_24, C_WHITE, 0, 38);
  iUp    = label(t, "", &lv_font_montserrat_14, C_DIM, 0, 76);
}

static void updateInfo(const Now &n) {
  char b[24];
  setText(iWeek, DAYS[n.t.tm_wday]);
  snprintf(b, sizeof b, "%d", n.t.tm_mday);
  setText(iDay, b);
  snprintf(b, sizeof b, "%s %d", MONTHS[n.t.tm_mon], 1900 + n.t.tm_year);
  setText(iMonth, b);
  const uint32_t up = millis() / 1000;
  snprintf(b, sizeof b, "UP %lu:%02lu:%02lu", (unsigned long)(up / 3600), (unsigned long)((up / 60) % 60), (unsigned long)(up % 60));
  setText(iUp, b);
  lv_arc_set_value(iYear, n.t.tm_yday + 1);
}

// ── stopwatch page ──────────────────────────────────────────────────────────

static lv_obj_t *swMain, *swCent, *swGo;
static bool     swRunning = false;
static uint32_t swStart = 0, swAccum = 0;

static uint32_t swElapsed() { return swAccum + (swRunning ? millis() - swStart : 0); }

static void onSwReset(lv_event_t *) { swRunning = false; swAccum = 0; }
static void onSwGo(lv_event_t *) {
  if (swRunning) { swAccum += millis() - swStart; swRunning = false; }
  else { swStart = millis(); swRunning = true; }
}

static void buildStop(lv_obj_t *t) {
  label(t, "STOPWATCH", &lv_font_montserrat_12, C_TEAL, 0, -90);
  swMain = label(t, "00:00", &lv_font_montserrat_48, C_WHITE, 0, -42);
  swCent = label(t, ".00", &lv_font_montserrat_24, C_DIM, 0, -2);
  roundButton(t, 52, lv_color_hex(0x3C4044), LV_SYMBOL_REFRESH, &lv_font_montserrat_20, -42, 58, onSwReset);
  swGo = roundButton(t, 52, lv_color_hex(0x14823C), LV_SYMBOL_PLAY, &lv_font_montserrat_20, 42, 58, onSwGo);
}

static void updateStop() {
  const uint32_t ms = swElapsed();
  char b[12];
  snprintf(b, sizeof b, "%02lu:%02lu", (unsigned long)((ms / 60000) % 100), (unsigned long)((ms / 1000) % 60));
  setText(swMain, b);
  snprintf(b, sizeof b, ".%02lu", (unsigned long)((ms / 10) % 100));
  setText(swCent, b);
  lv_obj_set_style_text_color(swCent, swRunning ? C_TEAL : C_DIM, 0);
  lv_obj_set_style_bg_color(swGo, swRunning ? C_RED : lv_color_hex(0x14823C), 0);
  lv_label_set_text(lv_obj_get_child(swGo, 0), swRunning ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

// ── settings page ───────────────────────────────────────────────────────────

static lv_obj_t *setFace, *set24, *setSrc;

static const char *faceName() { return face == 0 ? "ANALOG" : face == 1 ? "DIGITAL" : "RINGS"; }

static void onBright(lv_event_t *e) {
  userBright = (uint8_t)lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
}
static void onFace(lv_event_t *) {
  face = (face + 1) % 3;
  applyFace();
  lv_label_set_text(lv_obj_get_child(setFace, 0), faceName());
}
static void on24(lv_event_t *) {
  h24 = !h24;
  lv_label_set_text(lv_obj_get_child(set24, 0), h24 ? "24H" : "12H");
}

static void buildSettings(lv_obj_t *t) {
  label(t, "SETTINGS", &lv_font_montserrat_12, C_TEAL, 0, -88);
  label(t, "BRIGHTNESS", &lv_font_montserrat_12, C_DIM, 0, -56);
  lv_obj_t *s = lv_slider_create(t);
  lv_obj_set_size(s, 150, 10);
  lv_obj_align(s, LV_ALIGN_CENTER, 0, -30);
  lv_slider_set_range(s, 10, 255);
  lv_slider_set_value(s, userBright, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s, C_TEAL, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s, C_WHITE, LV_PART_KNOB);
  lv_obj_add_event_cb(s, onBright, LV_EVENT_VALUE_CHANGED, nullptr);
  setFace = roundButton(t, 60, lv_color_hex(0x0F6A70), faceName(), &lv_font_montserrat_12, -42, 30, onFace);
  set24   = roundButton(t, 60, lv_color_hex(0x0F6A70), "24H", &lv_font_montserrat_14, 42, 30, on24);
  setSrc  = label(t, timeSource(), &lv_font_montserrat_12, C_DIM, 0, 82);
}

// ── apps page ───────────────────────────────────────────────────────────────

static void onTorch(lv_event_t *) { torchOn = true; lv_obj_remove_flag(torch, LV_OBJ_FLAG_HIDDEN); }
static void onSparkle(lv_event_t *) { lv_obj_remove_flag(sparkle, LV_OBJ_FLAG_HIDDEN); }

static void buildApps(lv_obj_t *t) {
  label(t, "APPS", &lv_font_montserrat_12, C_TEAL, 0, -88);
  roundButton(t, 68, lv_color_hex(0x3C4044), LV_SYMBOL_CHARGE, &lv_font_montserrat_28, -40, -8, onTorch);
  roundButton(t, 68, lv_color_hex(0x3C4044), "*", &lv_font_montserrat_40, 40, -4, onSparkle);
  label(t, "TORCH", &lv_font_montserrat_12, C_WHITE, -40, 40);
  label(t, "SPARKLE", &lv_font_montserrat_12, C_WHITE, 40, 40);
}

// ── full-screen apps and the wake guard ─────────────────────────────────────

static void leaveApp(lv_event_t *) {
  torchOn = false;
  lv_obj_add_flag(torch, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(sparkle, LV_OBJ_FLAG_HIDDEN);
}

struct Dot { lv_obj_t *o; float x, y, vx, vy, life; };
static Dot dots[28];
static uint8_t nextDot = 0;

static void onSparklePress(lv_event_t *) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  static uint16_t hue = 0;
  hue = (hue + 9) % 360;
  for (int i = 0; i < 2; i++) {
    Dot &d = dots[nextDot];
    nextDot = (nextDot + 1) % (sizeof dots / sizeof dots[0]);
    const float a = random(0, 6283) / 1000.0f, v = 0.4f + random(0, 150) / 100.0f;
    d.x = p.x; d.y = p.y; d.vx = cosf(a) * v; d.vy = sinf(a) * v; d.life = 1.0f;
    lv_obj_set_style_bg_color(d.o, lv_color_hsv_to_rgb((hue + random(-20, 21) + 360) % 360, 85, 100), 0);
    lv_obj_remove_flag(d.o, LV_OBJ_FLAG_HIDDEN);
  }
}

static void sparkleStep(lv_timer_t *) {
  for (Dot &d : dots) {
    if (lv_obj_has_flag(d.o, LV_OBJ_FLAG_HIDDEN)) continue;
    d.life -= 0.04f;
    if (d.life <= 0) { lv_obj_add_flag(d.o, LV_OBJ_FLAG_HIDDEN); continue; }
    d.x += d.vx * 2; d.y += d.vy * 2; d.vx *= 0.96f; d.vy *= 0.96f;
    const int s = 3 + (int)(d.life * 9);
    lv_obj_set_size(d.o, s, s);
    lv_obj_set_pos(d.o, (int)d.x - s / 2, (int)d.y - s / 2);
    lv_obj_set_style_bg_opa(d.o, (lv_opa_t)(d.life * 255), 0);
  }
}

static void onGuard(lv_event_t *) {          // the touch that wakes the screen does nothing else
  manualSleep = false;
  lv_display_trigger_activity(nullptr);
  lv_obj_add_flag(guard, LV_OBJ_FLAG_HIDDEN);
}
static void onClockDouble(lv_event_t *) { manualSleep = true; }

static lv_obj_t *fullScreen(lv_color_t bg) {
  lv_obj_t *o = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, W, H);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(o, bg, 0);
  lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

// ── ui update ───────────────────────────────────────────────────────────────

static void uiStep(lv_timer_t *) {
  const Now n = readClock();
  updateClock(n);
  updateInfo(n);
  updateStop();
}

// ── main ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  display.setSpiHz(SPI_HZ);
  if (!display.begin(true)) {
    Serial.println("Display did not start - check the wiring.");
    while (true) delay(1000);
  }
  if (!display.touch()) Serial.println("No touch: the touch controller did not answer on SDA/SCL.");
  LB_LVGL.begin(display);
  W = display.width();
  H = display.height();
  D = min(W, H);

  setTimeFromCompile();     // a sensible start; NTP replaces it when enabled
#ifdef USE_NTP
  ntpBegin();
#endif

  lv_obj_t *scr = lv_screen_active();
  tv = lv_tileview_create(scr);
  lv_obj_set_style_bg_color(tv, lv_color_black(), 0);
  lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);

  // A cross of tiles: clock in the middle, a page in each direction.
  clockTile = lv_tileview_add_tile(tv, 1, 1, LV_DIR_ALL);
  lv_obj_t *info  = lv_tileview_add_tile(tv, 2, 1, LV_DIR_LEFT);
  lv_obj_t *stop  = lv_tileview_add_tile(tv, 0, 1, LV_DIR_RIGHT);
  lv_obj_t *sett  = lv_tileview_add_tile(tv, 1, 0, LV_DIR_BOTTOM);
  lv_obj_t *apps  = lv_tileview_add_tile(tv, 1, 2, LV_DIR_TOP);
  lv_obj_t *tiles[] = {clockTile, info, stop, sett, apps};
  for (lv_obj_t *t : tiles) {
    lv_obj_set_style_pad_all(t, 0, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  }

  for (int i = 0; i < 3; i++) faceBox[i] = box(clockTile);
  buildAnalog(faceBox[0]);
  buildDigital(faceBox[1]);
  buildRings(faceBox[2]);
  for (int i = 0; i < 3; i++) noClick(faceBox[i]);   // so a double tap reaches the tile
  applyFace();
  lv_obj_add_event_cb(clockTile, onClockDouble, LV_EVENT_DOUBLE_CLICKED, nullptr);

  buildInfo(info);
  buildStop(stop);
  buildSettings(sett);
  buildApps(apps);

  lv_tileview_set_tile_by_index(tv, 1, 1, LV_ANIM_OFF);

  // Full-screen apps, above everything.
  torch = fullScreen(C_WHITE);
  lv_obj_t *tl = label(torch, "DOUBLE TAP TO EXIT", &lv_font_montserrat_12, lv_color_black(), 0, H / 2 - 20);
  (void)tl;
  lv_obj_add_event_cb(torch, leaveApp, LV_EVENT_DOUBLE_CLICKED, nullptr);

  sparkle = fullScreen(lv_color_black());
  label(sparkle, "DOUBLE TAP TO EXIT", &lv_font_montserrat_12, C_DIM, 0, H / 2 - 20);
  for (Dot &d : dots) {
    d.o = lv_obj_create(sparkle);
    lv_obj_remove_style_all(d.o);
    lv_obj_set_style_radius(d.o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(d.o, LV_OPA_COVER, 0);
    lv_obj_add_flag(d.o, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(d.o, LV_OBJ_FLAG_CLICKABLE);
  }
  lv_obj_add_event_cb(sparkle, onSparklePress, LV_EVENT_PRESSING, nullptr);
  lv_obj_add_event_cb(sparkle, leaveApp, LV_EVENT_DOUBLE_CLICKED, nullptr);
  lv_timer_create(sparkleStep, 30, nullptr);

  guard = fullScreen(lv_color_black());
  lv_obj_set_style_bg_opa(guard, LV_OPA_TRANSP, 0);
  lv_obj_add_event_cb(guard, onGuard, LV_EVENT_PRESSED, nullptr);

  lv_timer_create(uiStep, 33, nullptr);
}

void loop() {
#ifdef USE_NTP
  ntpTick();
  static bool wasSynced = false;
  if (ntpSynced && !wasSynced) { wasSynced = true; lv_label_set_text(setSrc, timeSource()); }
#endif

  const uint32_t t0 = micros();
  LB_LVGL.loop();
  static uint32_t worst = 0, last = millis(), fpsAt = millis(), loops = 0;
  worst = max(worst, micros() - t0);
  loops++;

  // Dimming: after a while without a touch, or on a double tap on the clock.
  const uint32_t now = millis();
  const float dt = min(0.08f, (now - last) / 1000.0f);
  last = now;
  const bool dimmed = !torchOn && (manualSleep || lv_display_get_inactive_time(nullptr) > IDLE_DIM_MS);
  if (dimmed) lv_obj_remove_flag(guard, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(guard, LV_OBJ_FLAG_HIDDEN);
  float target = dimmed ? (manualSleep ? 2 : min<float>(userBright, DIM_LEVEL)) : userBright;
  if (torchOn) target = 255;
  bl += (target - bl) * min(1.0f, dt * 6.0f);
  static int lastBl = -1;
  if ((int)bl != lastBl) { lastBl = (int)bl; display.backlight(lastBl); }

  if (now - fpsAt >= 5000) {
    Serial.printf("loop %lu/s, slowest %lu us, heap %lu\n", (unsigned long)(loops * 1000 / (now - fpsAt)),
                  (unsigned long)worst, (unsigned long)ESP.getFreeHeap());
    loops = 0; worst = 0; fpsAt = now;
  }
}
