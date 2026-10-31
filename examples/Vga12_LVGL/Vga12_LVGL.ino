/*
  Vga12_LVGL - a full widget dashboard with LVGL on the 800 x 600 screen.

  The earlier examples draw every pixel yourself. LVGL is the other way: you
  create objects (labels, gauges, charts, buttons) and describe where they go;
  LVGL draws them, and redraws only what changed.

  YOU WILL LEARN
    - the four lines that start LVGL here (no lv_conf.h, no flush callback)
    - how flex and grid layouts place objects so you never type a coordinate
    - widgets: arc gauge, bar, line chart, LED, spinner, labels
    - a timer that updates the widgets once a second

  NEEDS THE LIBRARY  Lonely Binary LVGL  (Library Manager, or the GitHub repo)
  Tools > Partition Scheme > "Huge APP", Tools > PSRAM > "OPI PSRAM".

  PAGES
    Press BOOT to switch between the two pages:
      1  SYSTEM   gauges for chip temperature and heap, PSRAM bar, live chart
      2  WIDGETS  a tour of animated widgets (this board has no touch)
    The readings on page 1 are real: the chip temperature and the memory in use.

  The screen is scanned out of PSRAM, so keep Wi-Fi off while it matters. LVGL
  draws straight into the screen buffer; LB_LVGL.loop() must run often.
*/
#include <LonelyBinaryDisplay.h>
#include <LonelyBinaryLVGL.h>

LB_Display display(LB_VGA_800X600);

static lv_obj_t *page1, *page2;
static lv_obj_t *tempArc, *tempLabel, *heapArc, *heapLabel, *psramBar, *psramLabel, *chart, *fpsLabel, *pageLabel;
static lv_chart_series_t *tempSeries;
static lv_obj_t *demoArc, *demoBar, *demoSlider, *demoLed;

static lv_obj_t *makeCard(lv_obj_t *parent, const char *title) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_add_style(card, &LB_Style::card, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *cap = lv_label_create(card);
  lv_obj_add_style(cap, &LB_Style::label, 0);
  lv_label_set_text(cap, title);
  return card;
}

// A ring gauge with its number in the middle.
static lv_obj_t *makeGauge(lv_obj_t *card, int size, int maxValue, lv_obj_t **label) {
  lv_obj_t *arc = lv_arc_create(card);
  lv_obj_set_size(arc, size, size);
  lv_arc_set_rotation(arc, 135);
  lv_arc_set_bg_angles(arc, 0, 270);
  lv_arc_set_range(arc, 0, maxValue);
  lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(arc, LB_Style::accent, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, LB_Style::muted, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 16, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 16, LV_PART_INDICATOR);
  *label = lv_label_create(arc);
  lv_obj_add_style(*label, &LB_Style::value, 0);
  lv_obj_set_style_text_font(*label, &lv_font_montserrat_40, 0);
  lv_obj_center(*label);
  return arc;
}

static void buildPage1(lv_obj_t *parent) {
  page1 = lv_obj_create(parent);
  lv_obj_remove_style_all(page1);
  lv_obj_set_size(page1, lv_pct(100), lv_pct(100));
  lv_obj_set_flex_flow(page1, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_gap(page1, 10, 0);

  lv_obj_t *c1 = makeCard(page1, "CHIP TEMPERATURE  C");
  lv_obj_set_size(c1, 246, 270);
  tempArc = makeGauge(c1, 200, 100, &tempLabel);

  lv_obj_t *c2 = makeCard(page1, "FREE HEAP  KB");
  lv_obj_set_size(c2, 246, 270);
  heapArc = makeGauge(c2, 200, 400, &heapLabel);

  lv_obj_t *c3 = makeCard(page1, "VIDEO");
  lv_obj_set_size(c3, 246, 270);
  fpsLabel = lv_label_create(c3);
  lv_obj_add_style(fpsLabel, &LB_Style::value, 0);
  lv_obj_set_style_text_font(fpsLabel, &lv_font_montserrat_48, 0);
  lv_label_set_text(fpsLabel, "--");
  lv_obj_t *spin = lv_spinner_create(c3);
  lv_spinner_set_anim_params(spin, 1400, 90);
  lv_obj_set_size(spin, 90, 90);
  lv_obj_set_style_arc_color(spin, LB_Style::accent, LV_PART_INDICATOR);

  lv_obj_t *c4 = makeCard(page1, "PSRAM IN USE");
  lv_obj_set_size(c4, 300, 210);
  psramBar = lv_bar_create(c4);
  lv_obj_set_size(psramBar, lv_pct(100), 22);
  lv_bar_set_range(psramBar, 0, 100);
  lv_obj_set_style_bg_color(psramBar, LB_Style::muted, LV_PART_MAIN);
  lv_obj_set_style_bg_color(psramBar, LB_Style::accent, LV_PART_INDICATOR);
  psramLabel = lv_label_create(c4);
  lv_obj_set_width(psramLabel, lv_pct(100));               // wrap inside the card instead of running out of it
  lv_label_set_long_mode(psramLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(psramLabel, LB_Style::text, 0);

  lv_obj_t *c5 = makeCard(page1, "CHIP TEMPERATURE - LAST 60 S");
  lv_obj_set_size(c5, 448, 210);
  chart = lv_chart_create(c5);
  lv_obj_set_size(chart, lv_pct(100), lv_pct(100));
  lv_obj_set_flex_grow(chart, 1);
  lv_chart_set_point_count(chart, 60);
  lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_div_line_count(chart, 4, 0);
  lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(chart, 0, 0);
  lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS);
  lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
  tempSeries = lv_chart_add_series(chart, LB_Style::accent, LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 20, 80);
  lv_chart_set_all_value(chart, tempSeries, (int32_t)temperatureRead());
}

static void buildPage2(lv_obj_t *parent) {
  page2 = lv_obj_create(parent);
  lv_obj_remove_style_all(page2);
  lv_obj_set_size(page2, lv_pct(100), lv_pct(100));
  lv_obj_set_flex_flow(page2, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_gap(page2, 10, 0);
  lv_obj_add_flag(page2, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *c1 = makeCard(page2, "ARC");
  lv_obj_set_size(c1, 246, 270);
  lv_obj_t *lbl;
  demoArc = makeGauge(c1, 190, 100, &lbl);
  lv_obj_delete(lbl);

  lv_obj_t *c2 = makeCard(page2, "BAR AND SLIDER");
  lv_obj_set_size(c2, 246, 270);
  demoBar = lv_bar_create(c2);
  lv_obj_set_size(demoBar, lv_pct(100), 24);
  lv_obj_set_style_bg_color(demoBar, LB_Style::muted, LV_PART_MAIN);
  lv_obj_set_style_bg_color(demoBar, LB_Style::warn, LV_PART_INDICATOR);
  demoSlider = lv_slider_create(c2);
  lv_obj_set_width(demoSlider, lv_pct(90));
  lv_obj_set_style_bg_color(demoSlider, LB_Style::accent, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(demoSlider, LB_Style::accent, LV_PART_KNOB);

  lv_obj_t *c3 = makeCard(page2, "LED AND SPINNER");
  lv_obj_set_size(c3, 246, 270);
  demoLed = lv_led_create(c3);
  lv_obj_set_size(demoLed, 60, 60);
  lv_led_set_color(demoLed, lv_color_hex(0x34D27B));
  lv_obj_t *sp = lv_spinner_create(c3);
  lv_obj_set_size(sp, 90, 90);
  lv_obj_set_style_arc_color(sp, LB_Style::warn, LV_PART_INDICATOR);

  lv_obj_t *c4 = makeCard(page2, "LABELS");
  lv_obj_set_size(c4, 300, 210);
  lv_obj_t *l1 = lv_label_create(c4);
  lv_obj_set_style_text_font(l1, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(l1, LB_Style::text, 0);
  lv_label_set_text(l1, "Montserrat 32");
  lv_obj_t *l2 = lv_label_create(c4);
  lv_obj_set_style_text_color(l2, LB_Style::muted, 0);
  lv_label_set_text(l2, LV_SYMBOL_OK "  LVGL symbols come with the fonts  " LV_SYMBOL_WIFI);

  lv_obj_t *c5 = makeCard(page2, "HOW THIS PAGE WORKS");
  lv_obj_set_size(c5, 448, 210);
  lv_obj_t *l3 = lv_label_create(c5);
  lv_obj_set_width(l3, lv_pct(100));
  lv_obj_set_style_text_color(l3, LB_Style::text, 0);
  lv_label_set_text(l3, "Every widget here was created with one call and placed by a layout. "
                        "A timer moves the values; LVGL redraws only what changed.");
}

static void onTimer(lv_timer_t *) {
  // Page 1: real readings.
  const float t = temperatureRead();
  lv_arc_set_value(tempArc, (int)t);
  lv_label_set_text_fmt(tempLabel, "%d", (int)t);
  LB_Style::setLevel(tempLabel, t > 70 ? LB_Style::LEVEL_CRIT : t > 55 ? LB_Style::LEVEL_WARN : LB_Style::LEVEL_OK);
  lv_chart_set_next_value(chart, tempSeries, (int32_t)t);

  const int heapKb = ESP.getFreeHeap() / 1024;
  lv_arc_set_value(heapArc, LV_MIN(heapKb, 400));
  lv_label_set_text_fmt(heapLabel, "%d", heapKb);

  const uint32_t ps = ESP.getPsramSize(), used = ps - ESP.getFreePsram();
  lv_bar_set_value(psramBar, ps ? (int)(used * 100ULL / ps) : 0, LV_ANIM_ON);
  lv_label_set_text_fmt(psramLabel, "%.1f of %.1f MB in use\n\nThe screen itself takes about 2 MB.", used / 1048576.0f, ps / 1048576.0f);

  static uint32_t lastFrames = 0, lastMs = 0;
  uint32_t frames = 0, late = 0, maxUs = 0;
  display.scanStats(&frames, &late, &maxUs);
  if (lastMs) lv_label_set_text_fmt(fpsLabel, "%lu fps", (unsigned long)((frames - lastFrames) * 1000 / (millis() - lastMs)));
  lastFrames = frames;
  lastMs = millis();

  // Page 2: motion for the demo widgets.
  static int n = 0;
  n++;
  const int wave = 50 + (int)(48 * sinf(n * 0.5f));
  lv_arc_set_value(demoArc, wave);
  lv_bar_set_value(demoBar, wave, LV_ANIM_ON);
  lv_slider_set_value(demoSlider, 100 - wave, LV_ANIM_ON);
  if (n % 2) lv_led_on(demoLed); else lv_led_off(demoLed);
}

void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);
  display.begin(true);
  if (!LB_LVGL.begin(display)) {
    Serial.println("LVGL failed to start");
    return;
  }

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  // Keep clear of the edges: a monitor may crop the first lines and the last columns.
  lv_obj_set_style_pad_top(scr, 26, 0);
  lv_obj_set_style_pad_bottom(scr, 16, 0);
  lv_obj_set_style_pad_hor(scr, 18, 0);
  lv_obj_set_style_pad_row(scr, 10, 0);

  lv_obj_t *head = lv_obj_create(scr);
  lv_obj_remove_style_all(head);
  lv_obj_set_size(head, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *title = lv_label_create(head);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(title, LB_Style::text, 0);
  lv_label_set_text(title, LV_SYMBOL_HOME "  Lonely Binary VGA");
  pageLabel = lv_label_create(head);
  lv_obj_set_style_text_color(pageLabel, LB_Style::muted, 0);
  lv_label_set_text(pageLabel, "1 / 2   BOOT = next page");

  lv_obj_t *body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  lv_obj_set_width(body, lv_pct(100));
  lv_obj_set_flex_grow(body, 1);
  buildPage1(body);
  buildPage2(body);

  lv_timer_create(onTimer, 1000, nullptr);
  onTimer(nullptr);
}

void loop() {
  // BOOT flips the page.
  static bool wasDown = false;
  static bool second = false;
  const bool down = digitalRead(0) == LOW;
  if (down && !wasDown) {
    second = !second;
    if (second) { lv_obj_add_flag(page1, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(page2, LV_OBJ_FLAG_HIDDEN); }
    else        { lv_obj_add_flag(page2, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(page1, LV_OBJ_FLAG_HIDDEN); }
    lv_label_set_text(pageLabel, second ? "2 / 2   BOOT = next page" : "1 / 2   BOOT = next page");
  }
  wasDown = down;

  LB_LVGL.loop();
}
