# Lonely Binary Display

One line of code per screen. Every Lonely Binary SPI display module shares the
same 15-pin FPC and the same breakout, so swapping a panel is already a
hardware no-op — this library makes it a software no-op too.

```cpp
LB_Display display(LB_TFT_24);   // ← the only line that changes
```

That constant resolves the driver IC, resolution, IPS flag, column/row offsets,
SPI clock and backlight polarity. The GPIOs follow the board you picked in
**Tools ▸ Board** — classic ESP32 or ESP32-S3, no edits either way.

Arduino/C++ **and** MicroPython, from one shared panel table.

---

## Supported panels

| Constant | Panel | Resolution | Driver | Backlight |
|---|---|---|---|---|
| `LB_TFT_096` | 0.96" | 80 × 160 | ST7735 | on/off, **active low** |
| `LB_TFT_18` | 1.8" | 128 × 160 | ST7735 | on/off |
| `LB_TFT_20` | 2.0" | 240 × 320 | ST7789 | on/off |
| `LB_TFT_24` | 2.4" | 240 × 320 | ST7789 | on/off |
| `LB_TFT_28` | 2.8" | 240 × 320 | ST7789 | on/off |
| `LB_TFT_35` | 3.5" | 320 × 480 | ST7796 | on/off |
| `LB_NARROW_114` | 1.14" | 135 × 240 | ST7789 | PWM, active low |
| `LB_NARROW_168` | 1.68" | 142 × 428 | NV3007 | PWM, active low |
| `LB_NARROW_19` | 1.9" | 170 × 320 | ST7789 | PWM, active low |
| `LB_NARROW_225` | 2.25" | 76 × 284 | ST7789 | PWM, active low |
| `LB_NARROW_279` | 2.79" | 142 × 428 | NV3007 | PWM, active low |

MicroPython uses the same names without the `LB_` prefix (`TFT_24`,
`NARROW_19`, …).

---

## Arduino

Install **Lonely Binary Display** from Library Manager (it pulls in
*GFX Library for Arduino*), then **File ▸ Examples ▸ Lonely Binary Display**.

```cpp
#include <LonelyBinaryDisplay.h>

LB_Display display(LB_TFT_24);

void setup() {
  display.begin();
  display.backlight(255);

  auto *gfx = display.gfx();          // note: auto — see below
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(10, 10);
  gfx->print("Hello");
}

void loop() {}
```

### Always write `auto *gfx`

On a TFT, `gfx()` returns an `Arduino_GFX`. On an e-paper panel it returns a
`GxEPD2_GFX`. The two share **no base class**, but their drawing methods have
identical names — so one body of code compiles against both, as long as the
variable is declared `auto`. Spelling the type out by hand forks every sketch
the day you plug in an e-paper module.

### API

| | |
|---|---|
| `begin(bool useCanvas = false)` | Bring up SPI, panel and backlight. `true` allocates a PSRAM framebuffer (flicker-free redraws, and what LVGL wants). |
| `gfx()` | The drawing surface. |
| `flush()` | Push the framebuffer. No-op without a canvas, so always safe to call. |
| `backlight(0..255)` | On/off vs PWM and active-high vs active-low are decided by the panel table, not by you. |
| `setRotation(0..3)` | Applies the correct offset pair for portrait vs landscape. |
| `width()` / `height()` | Current size, rotation included. |
| `selfTest()` | Colour bars, backlight sweep, panel identity. |
| `printInfo()` | The same details on Serial. |

Colour names (`BLACK`, `RED`, …) are provided for you — GFX Library 1.6.5
renamed them to `RGB565_*`, and the shim lives here instead of in every sketch.
Prefixed `LB_BLACK` / `LB_RED` / … are always available; define
`LB_NO_LEGACY_COLORS` to suppress the bare names.

---

## MicroPython

Runs on **stock MicroPython** from micropython.org — no custom firmware build.

```bash
mpremote mip install github:Lonely-Binary/LonelyBinaryDisplay/micropython
```

```python
from lb_display import LBDisplay, BLACK, WHITE
from lb_panels import TFT_24          # ← the only line that changes

d = LBDisplay(TFT_24)
d.begin()
d.backlight(255)

tft = d.gfx()
tft.fill(BLACK)
tft.text("Hello", 10, 10, WHITE, scale=2)
```

The API mirrors the Arduino one: `begin()`, `gfx()`, `backlight()`,
`set_rotation()`, `width()`, `height()`, `self_test()`, `print_info()`.

`nv3007.py` is only imported when a NV3007 panel is selected, so the other nine
panels don't need it on the board.

---

## Adding a panel

[`panels.yaml`](panels.yaml) is the single source of truth. Both language
bindings are generated from it:

```
panels.yaml ──▶ src/LB_Panels.h        (Arduino)
            └─▶ micropython/lb_panels.py
```

Add one entry, then:

```bash
python3 tools/gen_panels.py
```

No code changes. Never hand-edit the generated files — CI runs
`gen_panels.py --check` and fails the build if they are stale, which is what
keeps the C++ and Python tables from quietly disagreeing.

---

## License

MIT. Brought to you by Lonely Binary — this library exists because of the
wonderful customers who buy our products and keep supporting our open-source
work. Thank you!
