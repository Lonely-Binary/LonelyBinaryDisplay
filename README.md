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
| `LB_TFT_096` | 0.96" | 80 × 160 | ST7735 | **active low** |
| `LB_TFT_18` | 1.8" | 128 × 160 | ST7735 | active high |
| `LB_TFT_20` | 2.0" | 240 × 320 | ST7789 | active high |
| `LB_TFT_24` | 2.4" | 240 × 320 | ST7789 | active high |
| `LB_TFT_28` | 2.8" | 240 × 320 | ST7789 | active high |
| `LB_TFT_35` | 3.5" | 320 × 480 | ST7796 | active high |
| `LB_NARROW_114` | 1.14" | 135 × 240 | ST7789 | active low |
| `LB_NARROW_168` | 1.68" | 142 × 428 | NV3007 | active low |
| `LB_NARROW_19` | 1.9" | 170 × 320 | ST7789 | active low |
| `LB_NARROW_225` | 2.25" | 76 × 284 | ST7789 | active low |
| `LB_NARROW_279` | 2.79" | 142 × 428 | NV3007 | active low |

**Every panel dims.** `backlight(0..255)` works the same on all of them — the
library always drives the backlight with PWM, so full-on is just `255`. Whether
the panel is wired active-low is the library's problem, not yours.

MicroPython uses the same names without the `LB_` prefix (`TFT_24`,
`NARROW_19`, …).

Pick by the size printed on your display. That is the whole decision — the
driver IC, resolution, offsets, colour order and backlight polarity all follow
from it, and there is never more than one entry for a size.

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

### Using your own wiring

The default GPIOs are the Lonely Binary breakout for the board you picked in
**Tools ▸ Board**. On your own PCB, or on a dev board where those pins are
taken, override them — **starting from the default**:

```cpp
LB_Display display(LB_TFT_24);

void setup() {
  LB_Wiring pins = LB_WIRING;   // the kit wiring for this board
  pins.cs   = 5;                // ...change only what differs
  pins.dc   = 16;
  pins.rst  = 17;
  pins.backlight = 4;           // or -1 if your board has no backlight pin

  display.setWiring(pins);      // must be before begin()
  display.begin();
}
```

Copy `LB_WIRING` rather than filling an `LB_Wiring` from scratch: the `spiHost`
field is a bus number whose meaning differs per chip — `VSPI` does not even
exist as a name on the ESP32-S3 — so starting from the default gives you a bus
that is already right for the MCU you are compiling for.

The **panel constant does not change.** Driver IC, resolution, offsets, colour
order and backlight polarity belong to the screen; only the GPIOs belong to
your board.

Long ribbon extensions or jumper wires sometimes will not take the panel's
rated clock — a scrambled or half-drawn image is the symptom:

```cpp
display.setSpiHz(20000000);     // before begin(); 0 restores the default
```

`printInfo()` prints the pins actually in use and marks them as custom, which
makes a wiring mistake obvious in the serial log.

See **File ▸ Examples ▸ Lonely Binary Display ▸ CustomPins**.

### When the colours come out wrong

The panel table already carries the right colour order and inversion for every
screen we sell, so you should never need this. You will need it for a bare
panel bought elsewhere, or if a new batch of glass is wired differently from
the one we characterised.

**Diagnose it first.** Fill the screen with pure red, then green, then blue,
and leave a black bar somewhere. Then read the symptom off this table:

| What you see | What is wrong | Fix |
|---|---|---|
| Red shows **blue**, blue shows **red**, black is still black | Colour order | `setColorOrder()` |
| Everything looks like a photo negative — black comes out white | Inversion | `setInverted()` |
| Red → **yellow**, green → **magenta**, blue → **cyan** | **Both** | both calls |

That third row is the confusing one, so it is worth knowing on sight: it is not
some exotic third failure, it is simply inversion *and* a channel swap stacked
on top of each other. Invert red and you get cyan; swap cyan's channels and you
get yellow.

```cpp
LB_Display display(LB_TFT_18);

void setup() {
  display.begin();

  display.setColorOrder(LB_Display::COLOR_BGR);
  display.setInverted(true);
}
```

Both are live register writes, so a test sketch can sweep them. The one
exception is **ST7735**, where Arduino_GFX takes the colour order as a
constructor argument — there `setColorOrder()` has to come *before* `begin()`,
and it returns `false` if you call it too late rather than failing quietly.

A third knob belongs with these two, for the same reason: the backlight can be
wired either way round, and getting it wrong is easy to misread because the
whole brightness scale simply runs backwards — dark at 255, bright at 0.

```cpp
display.setBacklightActiveLow(false);
```

`printInfo()` reports both, and marks either as `(forced)` when you have
overridden the table:

```
Colour     : BGR (forced), inverted (forced)
```

If you find a wrong value for a panel **we sell**, please tell us rather than
working around it in your sketch — it belongs in `panels.yaml`, where both the
Arduino and MicroPython sides pick it up.

### Always write `auto *gfx`

On a TFT, `gfx()` returns an `Arduino_GFX`. On an e-paper panel it returns a
`GxEPD2_GFX`. The two share **no base class**, but their drawing methods have
identical names — so one body of code compiles against both, as long as the
variable is declared `auto`. Spelling the type out by hand forks every sketch
the day you plug in an e-paper module.

### API

| | |
|---|---|
| `begin(bool useCanvas = false)` | Bring up SPI, panel and backlight. `true` asks for a framebuffer (flicker-free redraws, and what LVGL wants); if there is no room it says so and draws direct instead, so it is safe to ask for. `hasCanvas()` reports what you got. |
| `gfx()` | The drawing surface. |
| `flush()` | Push the framebuffer. No-op without a canvas, so always safe to call. |
| `backlight(0..255)` | PWM on every panel. Active-high vs active-low is decided by the panel table, not by you. |
| `setWiring(pins)` | Your own GPIOs. Before `begin()`. |
| `setSpiHz(hz)` | Override the panel's SPI clock. Before `begin()`; `0` restores the default. |
| `setColorOrder(order)` | `COLOR_AUTO` / `COLOR_RGB` / `COLOR_BGR`. Any time, except on ST7735 where it must precede `begin()`. Returns `false` if it could not be applied. |
| `setInverted(bool)` | Flip the panel's inversion. Any time. |
| `setBacklightActiveLow(bool)` | Flip the backlight polarity. Any time. |
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

Colour order and inversion live on the panel, so override them by copying it:

```python
panel = dict(TFT_24)
panel["bgr"] = True             # red and blue swapped
panel["invert"] = False         # black coming out white
d = LBDisplay(panel)
```

Your own wiring, same rule — copy the default, change what differs:

```python
import lb_panels
from lb_display import LBDisplay, detect_board
from lb_panels import TFT_24

pins = dict(lb_panels.WIRING[detect_board()])
pins["cs"] = 5
pins["backlight"] = -1          # no backlight pin on this board

d = LBDisplay(TFT_24, wiring=pins)
d.begin()
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

## Other panels (not sold by Lonely Binary)

**If you bought a display from us, you do not need this section** — yours is in
the table at the top of this page, and it is the only 2.8 inch, the only 1.9
inch, and so on. Nothing below competes with it.

These are panels we do **not** stock and do **not** support. They are listed
because the library happens to drive them and it costs nothing to say so. They
are named by **controller and resolution** rather than by size, precisely so
they cannot be mistaken for a product:

| Constant | Controller | Resolution | Status |
|---|---|---|---|
| `LB_ILI9341_240X320` | ILI9341 | 240 × 320 | **unverified — every value is a guess** |

Unverified means exactly that: nobody has run it. Expect to need
[`setColorOrder()` / `setInverted()` / `setBacklightActiveLow()`](#when-the-colours-come-out-wrong)
and quite possibly `setSpiHz()`. If you get one working, the numbers are
welcome.

---

## License

MIT. Brought to you by Lonely Binary — this library exists because of the
wonderful customers who buy our products and keep supporting our open-source
work. Thank you!
