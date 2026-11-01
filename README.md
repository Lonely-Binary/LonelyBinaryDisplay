# Lonely Binary Display

One line of code per screen. Every Lonely Binary SPI display module shares the
same 15-pin FPC and the same breakout, so swapping a panel is already a
hardware no-op — this library makes it a software no-op too.

```cpp
LB_Display display(LB_TFT_24);   // ← the only line that changes
```

That constant resolves the driver IC, resolution, inversion, column/row offsets,
SPI clock and backlight polarity. The GPIOs follow the board you picked in
**Tools ▸ Board** — classic ESP32 or ESP32-S3, no edits either way.

Arduino/C++ **and** MicroPython, from one shared panel table.

> **Writing your code with an AI?** Give it this first:
> `https://raw.githubusercontent.com/Lonely-Binary/LonelyBinaryDisplay/main/llms.txt`
>
> That is [`llms.txt`](llms.txt) — the whole API in one page, written for an
> assistant rather than for you. Without it the model falls back on the
> `TFT_eSPI` and `Adafruit_ST7789` code it was trained on, which is confidently
> wrong for these panels and reads like a dead screen.

> **Now on sale: the Lonely Binary ESP32-S3 VGA 800 x 600.** A VGA output
> board that plugs into any monitor, sold together with an ESP32-S3 (N16R8).
> `LB_Display display(LB_VGA_800X600);` and the same drawing calls as every
> other display here. [Jump to it](#vga-800-x-600-esp32-s3) - twelve examples
> take you from `Hello` to a game and an LVGL dashboard.
>
> **VGA requires PSRAM to be enabled.** In the Arduino IDE set
> **Tools ▸ PSRAM ▸ OPI PSRAM** before you upload. Without it the screen stays
> blank.

---

## Supported panels

| Panel | Arduino constant | MicroPython name | Screen size | Driver IC | Bus | Touch | Backlight |
|---|---|---|---|---|---|---|---|
| 0.96 inch | `LB_TFT_096` | `TFT_096` | 80 x 160 | ST7735 | SPI 20 MHz | — | active low |
| 1.8 inch | `LB_TFT_18` | `TFT_18` | 128 x 160 | ST7735 | SPI 20 MHz | — | active high |
| 2.0 inch | `LB_TFT_20` | `TFT_20` | 240 x 320 | ST7789 | SPI 40 MHz | — | active high |
| 2.4 inch | `LB_TFT_24` | `TFT_24` | 240 x 320 | ST7789 | SPI 40 MHz | — | active high |
| 2.8 inch | `LB_TFT_28` | `TFT_28` | 240 x 320 | ST7789 | SPI 40 MHz | — | active high |
| 3.5 inch | `LB_TFT_35` | `TFT_35` | 320 x 480 | ST7796 | SPI 40 MHz | — | active high |
| 1.14 inch | `LB_NARROW_114` | `NARROW_114` | 240 x 135 | ST7789 | SPI 8 MHz | — | active low |
| 1.68 inch | `LB_NARROW_168` | `NARROW_168` | 428 x 142 | NV3007 | SPI 8 MHz | — | active low |
| 1.9 inch | `LB_NARROW_19` | `NARROW_19` | 320 x 170 | ST7789 | SPI 8 MHz | — | active low |
| 2.25 inch | `LB_NARROW_225` | `NARROW_225` | 284 x 76 | ST7789 | SPI 8 MHz | — | active low |
| 2.79 inch | `LB_NARROW_279` | `NARROW_279` | 428 x 142 | NV3007 | SPI 20 MHz | — | active low |
| 3.92 inch square, capacitive touch | `LB_SQUARE_392_CTP` | not supported | 320 x 320 | ILI9488 | 8-bit parallel | GT911 (capacitive) | active high |
| 1.28 inch round, capacitive touch | `LB_TAP_128_CTP` | not supported | 240 x 240 | GC9A01 | SPI 20 MHz | IT7259 (capacitive) | active high |
| 1.5 inch, capacitive touch | `LB_TAP_150_CTP` | not supported | 240 x 320 | GC9306 | SPI 10 MHz | CST816 (capacitive) | active high |
| 1.96 inch, capacitive touch | `LB_TAP_196_CTP` | not supported | 240 x 296 | GC9307 | 8-bit parallel | CST816 (capacitive) | active high |
| 2.13 inch reflective LCD | `LB_RLCD_213` | not supported | 250 x 122 | ST7305 | SPI 20 MHz | — | active high |
| 2.9 inch reflective LCD | `LB_RLCD_290` | not supported | 384 x 168 | ST7305 | SPI 20 MHz | — | active high |
| 4.2 inch reflective LCD | `LB_RLCD_420` | not supported | 400 x 300 | ST7305 | SPI 20 MHz | — | active high |
| 1.8 inch, resistive touch | `LB_RTP_180` | `RTP_180` | 160 x 128 | ST7735 | SPI 20 MHz | NS2009 (resistive) | active low |
| 2.4 inch, resistive touch | `LB_RTP_240` | `RTP_240` | 320 x 240 | ST7789 | SPI 40 MHz | NS2009 (resistive) | active low |
| 2.8 inch, resistive touch | `LB_RTP_280` | `RTP_280` | 320 x 240 | ST7789 | SPI 40 MHz | NS2009 (resistive) | active low |
| 4.3 inch, resistive touch | `LB_RTP_430` | not supported | 480 x 272 | NV3041A | SPI 20 MHz | NS2009 (resistive) | active low |
| 3.5 inch, resistive touch | `LB_RTP_350` | not supported | 320 x 480 | ST7796 | 8-bit parallel | NS2009 (resistive) | active high |
| 0.85 inch square | `LB_SQUARE_085` | `SQUARE_085` | 128 x 128 | ST7735 | SPI 20 MHz | — | active low |
| 1.33 inch square | `LB_SQUARE_133` | `SQUARE_133` | 240 x 240 | ST7789 | SPI 20 MHz | — | active low |
| 1.54 inch square | `LB_SQUARE_154` | `SQUARE_154` | 240 x 240 | ST7789 | SPI 20 MHz | — | active low |
| 2.0 inch square | `LB_SQUARE_200` | not supported | 320 x 240 | ILI9342 | SPI 40 MHz | — | active low |
| 2.31 inch square | `LB_SQUARE_231` | not supported | 320 x 240 | ILI9342 | SPI 40 MHz | — | active low |
| 3.92 inch square | `LB_SQUARE_392` | `SQUARE_392` | 320 x 320 | ST7796 | SPI 40 MHz | — | active high |
| VGA 800 x 600, with ESP32-S3 | `LB_VGA_800X600` | not supported | 800 x 600 | RGB | 16-bit RGB (S3 only) | — | active high |

### Third-party screens

Not products — screens other people sell, which the library can drive. They are
named by controller and the size the sketch sees, so they cannot be mistaken
for the table above. Each carries its own pins; override them with `setWiring()`.

| Panel | Arduino constant | MicroPython name | Screen size | Driver IC | Bus | Touch | Backlight |
|---|---|---|---|---|---|---|---|
| RGB | `LB_SUNTON_4827S043R` | not supported | 480 x 272 | RGB | 16-bit RGB (S3 only) | XPT2046 (resistive) | active high |
| RGB | `LB_SUNTON_8048S070C` | not supported | 800 x 480 | RGB | 16-bit RGB (S3 only) | GT911 (capacitive) | active high |
| ILI9341 | `LB_ILI9341_240X320` | not supported | 240 x 320 | ILI9341 | SPI 40 MHz | — | active high |
| ILI9327 | `LB_ILI9327_400X240` | not supported | 400 x 240 | ILI9327 | 8-bit parallel | — | active high |
| ILI9486 | `LB_ILI9486_480X320` | not supported | 480 x 320 | ILI9486 | SPI 20 MHz | — | active high |
| ILI9341 | `LB_ILI9341_320X240` | not supported | 320 x 240 | ILI9341 | SPI 40 MHz | — | active high |
| ST7305 | `LB_ST7305_400X300` | not supported | 400 x 300 | ST7305 | SPI 10 MHz | — | active high |

Their pins are in `panels.yaml`. The ILI9486 module's resistive touch is not
supported: its XPT2046 never answered on the pins we were given.

**Every panel dims.** `backlight(0..255)` works the same on all of them — the
library always drives the backlight with PWM, so full-on is just `255`. Whether
the panel is wired active-low is the library's problem, not yours.

MicroPython uses the same names without the `LB_` prefix (`TFT_24`,
`NARROW_19`, …).

Pick by the size printed on your display. That is the whole decision — the
driver IC, resolution, offsets, colour order and backlight polarity all follow
from it, and there is never more than one entry for a size.

---

## VGA 800 x 600 (ESP32-S3)

> ## ⚠️ PSRAM MUST BE ENABLED
>
> **The VGA display will not work unless PSRAM is turned on.** The whole
> 800 x 600 picture lives in a framebuffer in PSRAM (about 960 KB, far more
> than the ESP32-S3's internal RAM), so with PSRAM off `begin()` returns
> `false`, prints that the framebuffer needs PSRAM, and the monitor shows
> nothing.
>
> **Arduino IDE:** Tools ▸ **PSRAM ▸ OPI PSRAM**  (the board's N16R8 module has
> 8 MB of octal PSRAM, so *OPI*, not *QSPI*).
> **PlatformIO:** use an `N16R8` board definition and add
> `-DBOARD_HAS_PSRAM` and `board_build.arduino.memory_type = qio_opi`.
>
> Set this first. It is the most common reason a new VGA board "does nothing".

**Available now.** The Lonely Binary VGA board is sold together with an
ESP32-S3 (N16R8: 16 MB flash, 8 MB octal PSRAM). It turns 16 GPIO lines into
the three analog colour signals of a VGA connector with a resistor ladder and
sends an **800 x 600 @ 50 Hz** picture, 65,536 colours, to any VGA
monitor (a 32 MHz pixel clock: the standard 56 Hz timing needs 36 MHz, which
leaves the memory reads that feed the screen no margin and shows as short
streaks at the left of some lines). There is no controller and no screen memory on the board: the
ESP32-S3 sends every pixel of every frame, forever, out of a framebuffer in
PSRAM.

```cpp
#include <LonelyBinaryDisplay.h>

LB_Display display(LB_VGA_800X600);

void setup() {
  display.begin();
  display.fillScreen(LB_BLACK);
  display.setTextColor(LB_WHITE);
  display.setTextSize(8);
  display.drawString("Hello, VGA!", 40, 40);
  display.flush();
}

void loop() {}
```

### Board settings (Tools menu)

| Setting | Value | If you get it wrong |
|---|---|---|
| Board | **ESP32S3 Dev Module** | The pins are those of the ESP32-S3 |
| PSRAM | **OPI PSRAM** (**required**, never "Disabled") | `begin()` returns false, says the framebuffer needs PSRAM, and the monitor stays blank |
| USB CDC On Boot | **Disabled** | `Serial` goes to native USB and nothing appears on the UART |
| Partition Scheme | **Huge APP** | Only the photo and LVGL examples are large enough to need it |

### What to know before you draw

- **Call `flush()`** after drawing. The pixels are written through the CPU
  cache, and `flush()` is what pushes them to the PSRAM the monitor reads.
- **Drawing is slow, and that is physics, not a bug.** The picture being sent to the
  monitor uses most of the PSRAM bandwidth, so drawing into it manages only 2
  to 5 million pixels a second: a full-screen fill takes about 0.23 s, a
  full-screen JPEG about 1.3 s. Redraw what changed, not the screen. Every
  animation example does this.
- **Keep Wi-Fi and Bluetooth off while the picture matters.** The radio and
  the screen share the PSRAM bus, and the picture shakes while the radio is
  on. Turn it on, fetch what you need, turn it off.
- **Draw what changed; never wipe a region and redraw it.** The monitor reads
  the screen while you write, so a wiped region blinks. Draw the fixed parts
  once and update text with `setTextColor(fg, bg)`. The examples do this.
- **Keep important things about 24 pixels from the top and 16 from the sides**;
  monitors crop a picture that is not a standard preset.
- **Rotation 0 only**, no touch, and the only button is BOOT (GPIO 0).
  MicroPython is not supported on this board yet.
- **Is the picture healthy?** `display.scanStats(&frames, &late, &maxUs)`
  reports frames sent and *late* frames. A healthy picture is 50 frames a
  second and 0 late. (The frame rate alone proves nothing: the picture can be
  broken and still count 50. It also cannot see the streaks described above: it
  measures the length of a frame, not of a line.)

If the monitor shows no picture, or a shifted or cropped one, press its AUTO
button; and run `Vga02_ColorBars`, which tells a broken wiring line from a
monitor problem.

**Why 50 Hz and not the standard 56 Hz?** The screen is fed by a CPU copy from
PSRAM that must finish within 10 line times; at the standard 36 MHz a cold
copy takes about as long as the budget, so some lines arrive late and show as
short streaks at the left. At 32 MHz there is margin and the picture is clean.
The measurements, and everything else about the board, are in
[`docs/VGA_800x600.md`](docs/VGA_800x600.md).

### Examples

Start at the top and go down: each one adds one idea to the ones before.
**File ▸ Examples ▸ Lonely Binary Display ▸ Vga01_Hello** and so on.

| Example | You build | You learn |
|---|---|---|
| `Vga01_Hello` | Text in the middle of the screen | The four lines every sketch starts with; `flush()` |
| `Vga02_ColorBars` | Colour bars, ramps and 16 wiring patches | Self-test: find a bad data line in a minute. **Run this first.** |
| `Vga03_Shapes` | Every shape on a 100 px grid | The coordinate system; `draw*` vs `fill*` |
| `Vga04_Text` | Two fonts, sizes 1-8, colours, alignment | `setTextSize`, `textWidth`, text on a background |
| `Vga05_Bounce` | Eight bouncing balls | The animation loop: erase, move, draw; frame timing |
| `Vga06_Photo` | A photo slideshow | `drawJpg`, scaling, and `make_photos.py` for your own pictures |
| `Vga07_Clock` | Analog and digital clock | `sin`/`cos`, thick lines, repainting one area a second |
| `Vga08_Chart` | A live scrolling chart and bars | Mapping values to pixels; plotting a sensor |
| `Vga09_SerialScreen` | `Serial` on the monitor, 100 x 37 characters | Your own print target; a scrolling text buffer |
| `Vga10_Hop` | A one-button jumping game (BOOT) | A game loop; gravity; collisions; high score |
| `Vga11_Dashboard` | A status board for the ESP32-S3 | Tiles, big numbers, bars, sparklines |
| `Vga12_LVGL` | A full widget dashboard, two pages | LVGL: layouts, gauges, charts (needs *Lonely Binary LVGL*) |

Put your own sensor into `Vga08_Chart` or `Vga11_Dashboard` and you have a
useful display; put your own pictures into `Vga06_Photo` and you have a photo
frame.

### Pins

The pins are fixed by the board and need no setup. They are
in `panels.yaml` (entry `vga_800x600`): HSYNC GPIO 2, VSYNC GPIO 1,
R0..R4 = 7, 15, 16, 38, 39; G0..G5 = 17, 18, 8, 3, 9, 10;
B0..B4 = 11, 12, 13, 14, 21. UART0 (the serial port) is GPIO 43 / 44.

## Arduino

Install **Lonely Binary Display** from Library Manager (it pulls in
*Lonely Binary GFX*, the drawing API), then
**File ▸ Examples ▸ Lonely Binary Display**.

```cpp
#include <LonelyBinaryDisplay.h>

LB_Display display(LB_TFT_24);

void setup() {
  display.begin();                    // backlight comes on at full

  display.fillScreen(LB_BLACK);
  display.setTextColor(LB_WHITE);
  display.setTextSize(2);
  display.drawString("Hello", 10, 10);
  display.flush();                    // no-op without a framebuffer; always call it
}

void loop() {}
```

`LB_Display` *is* the drawing surface — the same `LB_Canvas` API that
Lonely Binary VGA and e-paper use, so a function written against
`LB_Canvas &` runs on all of them. The panel driver is the library's own;
nothing else is needed.

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

Every example already does this: its top lists each GPIO it uses as a
`#define PIN_...` line, and `setup()` passes them to `setWiring()` before
`begin()`. To match your board, change those numbers - nothing else.
See also **File ▸ Examples ▸ Lonely Binary Display ▸ CustomPins**.

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

Both are single register writes, so they work at any time — before `begin()`
(the panel then comes up that way) or after it, on every controller — and a
test sketch can sweep them live.

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

### API

| | |
|---|---|
| `begin(bool useCanvas = false)` | Bring up SPI, panel and backlight. `true` asks for a framebuffer (flicker-free redraws, and what LVGL wants); if there is no room it says so and draws direct instead, so it is safe to ask for. `hasCanvas()` reports what you got. |
| Drawing | `fillScreen`, `fillRect`, `drawLine`, `drawCircle`, `drawString`, `drawJpg`, … — the Lonely Binary GFX API, colours as `LB_RED` etc. |
| `pushImage(x, y, w, h, px)` | A block of raw RGB565 pixels. Into the framebuffer if there is one, else straight to the panel. |
| `framebuffer()` | The RGB565 framebuffer after `begin(true)`, or `nullptr`. |
| `flush()` | Push the framebuffer. No-op without a canvas, so always safe to call. |
| `backlight(0..255)` | PWM on every panel. Active-high vs active-low is decided by the panel table, not by you. |
| `setWiring(pins)` | Your own GPIOs. Before `begin()`. |
| `setSpiHz(hz)` | Override the panel's SPI clock. Before `begin()`; `0` restores the default. |
| `setColorOrder(order)` | `COLOR_AUTO` / `COLOR_RGB` / `COLOR_BGR`. Any time. |
| `setInverted(bool)` | Set the panel's inversion. Any time. |
| `setBacklightActiveLow(bool)` | Flip the backlight polarity. Any time. |
| `setRotation(0..3)` | Applies the correct offset pair for portrait vs landscape. With a framebuffer, only 0 ↔ 2 or 1 ↔ 3 (the buffer keeps its shape). |
| `width()` / `height()` | Current size, rotation included. |
| `selfTest()` | Colour bars, backlight sweep, panel identity. |
| `printInfo()` | The same details on Serial. |

Drawing calls take `LB_BLACK`, `LB_RED`, … (`lb_color_t`, from
Lonely Binary GFX). The bare names `BLACK`, `RED`, … are raw RGB565 values, for
`pushImage()` and `framebuffer()` only — never pass one to a drawing call, or
the other way round. Define `LB_NO_LEGACY_COLORS` to suppress the bare names.

### Touch

A touch panel — the constants ending `_CTP` (capacitive) or `_RTP` (resistive)
— brings up its touch controller in `begin()`, and `setRotation()` keeps the
touch coordinates turned with the picture. Include the driver header to enable
it:

```cpp
#include <LonelyBinaryDisplay.h>
#include <LB_TouchGT911.h>              // the controller on LB_SQUARE_392_CTP

LB_Display display(LB_SQUARE_392_CTP);

void loop() {
  int16_t x, y;
  if (display.touch() && display.touch()->getTouch(&x, &y))
    display.fillCircle(x, y, 3, LB_RED);
}
```

`touch()` is `nullptr` on a panel without touch, or if the controller did not
answer (`begin()` says which on Serial). The Lonely Binary LVGL library
includes the driver header itself and wires touch into LVGL, so an LVGL sketch
needs neither line.

Why the include: the GT911 is on I2C through `Wire`, and Arduino links every
library that any file of this one includes. Keeping the driver header-only and
opt-in means a sketch without touch does not carry `Wire` (about 22 KB).

The driver also works on its own, for touch on your own wiring:

```cpp
LB_TouchGT911 touch(21 /* SDA */, 22 /* SCL */, 18 /* INT */, 19 /* RST */);
touch.begin();          // false = the chip did not answer
touch.setRotation(0);   // same value as display.setRotation()
```

| | |
|---|---|
| `begin()` | Reset the chip (when RST is wired) and find it at 0x5D or 0x14. The resolution comes from the chip's own configuration, so there is nothing to calibrate. |
| `getTouch(&x, &y)` | The first finger. `false` when nothing is touching. |
| `read(points, max)` | Every finger (GT911: up to 5), as `LB_TouchPoint { x, y, size, id }`. Returns how many. |
| `setRotation(0..3)` | Keep it in step with the display, and the points follow the picture. |
| `setOrientation(swapXY, flipX, flipY)` | How the touch layer sits relative to display rotation 0. The panel table carries this; only for a panel it does not know. |

The 1.28" round Tap panel (`LB_TAP_128_CTP`) uses the IT7259 instead:
`#include <LB_TouchIT7259.h>`, one finger, 0-239 in both axes, plus the chip's own
gestures through `takeGesture()` (tap, double tap, flick). Its I2C pins are part
of the wiring set (classic ESP32: SDA 27, SCL 21; ESP32-S3: SDA 4, SCL 6, INT 5, RST 7); see `examples/TapOrb`.

The 1.5" Tap panel (`LB_TAP_150_CTP`, 240 x 320) uses the CST816D:
`#include <LB_TouchCST816.h>`, one finger, mapped to pixels with the raw edge
values in the panel table. The driver never writes to the chip (the vendor found
that stops it scanning). Try it with `examples/TapCheck`.

The 1.96" Tap panel (`LB_TAP_196_CTP`, 240 x 296, GC9307, 8-bit parallel) also uses the
CST816D. It is wired on its own pins, not the breakout: D0-D7 on GPIO 5, 6, 4, 7,
15, 1, 10, 11 (the FPC numbers them D7..D0), WR 2, DC 42, CS 41, touch SDA 13 /
SCL 12, and no reset, backlight or interrupt pins. Not yet run through this
library; `examples/TapCheck` is the first thing to try.

The resistive RTP series (`LB_RTP_180`, `LB_RTP_240`, `LB_RTP_280`, `LB_RTP_430`: 1.8, 2.4,
2.8 and 4.3 inch) reads an NS2009 over I2C: `#include <LB_TouchNS2009.h>`. A resistive layer
needs a firm press (a stylus or a fingernail) and reports raw numbers, not pixels, so every layer is
calibrated: `LB_TouchCalibration.h` shows four crosses, fits the layer from four presses, applies
the result and can keep it in the chip's flash. `examples/RtpCheck` is the first power-on test
(calibration, backlight, colours, edges, a backlight page) and `examples/RtpShowcase` a touch
show-reel (paint, whack-a-mole, a colour mixer). The 4.3 inch panel's backlight is held to 50 %
in the examples: at full brightness it makes the dev board's LDO too hot.

Measured on the 3.92" square (`LB_SQUARE_392_CTP`), classic ESP32, SDA 21
SCL 22 INT 18 RST 19: the GT911 reports 320 x 320 in the same orientation as
display rotation 0. All four rotations were checked on hardware, 5/5 targets
hit in each.

### The square series (8-bit parallel)

The square panels are driven over an 8-bit parallel bus rather than SPI, on a
breakout of their own, so their pins are a separate set — `LB_WIRING_PAR8`,
picked automatically by the panel constant. Override it the same way as the
SPI wiring:

```cpp
LB_WiringPar8 pins = LB_WIRING_PAR8;
pins.rst = -1;                // panel reset not wired on this board
display.setWiring(pins);
```

Default pins (proposed — the breakout is not final yet):

| | D0–D7 | WR | DC | RST | Backlight | Touch SDA / SCL / INT / RST |
|---|---|---|---|---|---|---|
| ESP32-S3 | 4, 5, 6, 7, 15, 16, 17, 18 | 8 | 9 | 42 | 41 | 1 / 2 / 40 / 39 |
| classic ESP32 | 5, 17, 16, 15, 13, 26, 14, 27 | 4 | 23 | 33 | 32 | 21 / 22 / 18 / 19 |

CS is tied low and RD high on the board. The data lines, WR and DC are all below
GPIO 32 on purpose: the bus sets a whole byte with one register write, and pins
above 31 would need a second.

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

The setup calls mirror the Arduino ones: `begin()`, `backlight()`,
`set_rotation()`, `width()`, `height()`, `self_test()`, `print_info()`. Drawing
is different for now: on MicroPython it is on the object `gfx()` returns, not
on the display itself.

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
| `LB_ILI9341_240X320` | ILI9341 | 240 × 320 | measured on one panel; rotations 1–3 and the 40 MHz clock untried |

Yours will not necessarily match. ILI9341 boards vary in colour order,
inversion and backlight polarity, and the glass in front of the controller is
sometimes smaller than its 240 × 320 of RAM, which needs offsets we cannot
guess. Expect to reach for
[`setColorOrder()` / `setInverted()` / `setBacklightActiveLow()`](#when-the-colours-come-out-wrong),
and `setSpiHz()` if the image tears. If you get one working, the numbers are
welcome.

---

## License

MIT. Brought to you by Lonely Binary — this library exists because of the
wonderful customers who buy our products and keep supporting our open-source
work. Thank you!
