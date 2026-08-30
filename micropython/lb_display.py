"""
lb_display.py - one-line setup for every Lonely Binary SPI display.

Every panel in the range plugs into the same 15-pin FPC breakout, so swapping a
screen is a hardware no-op. This makes it a software no-op too:

    from lb_display import LBDisplay, RED, WHITE
    from lb_panels import TFT_24            # <- the ONLY line that changes

    d = LBDisplay(TFT_24)
    d.begin()
    d.backlight(255)

    tft = d.gfx()
    tft.fill(RED)
    tft.text("Hello", 10, 10, WHITE, scale=2)

The panel constant carries the driver class, resolution, rotation, x/y offset,
colour order, inversion, SPI clock and backlight polarity. The GPIOs come from
the same table and follow the chip this runs on - classic ESP32 or ESP32-S3,
detected at runtime.

Mirrors the Arduino API of the same library: LB_Display / begin() / gfx() /
backlight() / set_rotation() / self_test() / print_info().

Files to copy to the board:
    lb_display.py  lb_panels.py  st77xx.py   (+ nv3007.py for 1.68 / 2.79 inch)

Or install with mip:
    mpremote mip install github:Lonely-Binary/LonelyBinaryDisplay/micropython

MIT License - Lonely Binary
"""

import os
import time
from machine import Pin, SPI, PWM

import st77xx
import lb_panels

# Colour names, re-exported so a sketch needs one import.
BLACK = st77xx.BLACK
WHITE = st77xx.WHITE
RED = st77xx.RED
GREEN = st77xx.GREEN
BLUE = st77xx.BLUE
YELLOW = st77xx.YELLOW
MAGENTA = st77xx.MAGENTA
CYAN = st77xx.CYAN


def detect_board():
    """'esp32s3' or 'esp32', from the chip this is running on."""
    return "esp32s3" if "ESP32S3" in os.uname().machine else "esp32"


class LBDisplay:
    def __init__(self, panel, board=None):
        self.panel = panel
        self.board = board or detect_board()
        self.wiring = lb_panels.WIRING[self.board]
        self.tft = None
        self.spi = None
        self._pwm = None
        self._bl = None

    # -- bring-up ------------------------------------------------------------

    def begin(self):
        p, w = self.panel, self.wiring

        self.spi = SPI(w["spi_id"], baudrate=p["baudrate"],
                       polarity=0, phase=0,
                       sck=Pin(w["sclk"]), mosi=Pin(w["mosi"]), miso=None)

        kw = dict(dc=Pin(w["dc"]), cs=Pin(w["cs"]), rst=Pin(w["rst"]),
                  width=p["width"], height=p["height"],
                  rotation=p["rotation"],
                  xstart=p["xstart"], ystart=p["ystart"],
                  bgr=p["bgr"], invert=p["invert"],
                  flip_x=p["flip_x"], flip_y=p["flip_y"])

        cls = p["cls"]
        if cls.startswith("NV3007"):
            # Only imported when a NV3007 panel is actually selected, so the
            # other nine panels never need nv3007.py on the board.
            import nv3007
            self.tft = getattr(nv3007, cls)(self.spi, **kw)
        else:
            self.tft = getattr(st77xx, cls)(self.spi, **kw)

        self._backlight_begin()
        self.backlight(255)
        return self.tft

    def gfx(self):
        """The drawing surface. Same name as the Arduino API."""
        return self.tft

    # -- backlight -----------------------------------------------------------

    def _backlight_begin(self):
        pin = self.wiring["backlight"]
        if self.panel["bl_pwm"]:
            self._pwm = PWM(Pin(pin), freq=5000)
        else:
            self._bl = Pin(pin, Pin.OUT)

    def backlight(self, level):
        """0 = off, 255 = full. Active-low wiring and PWM vs on/off are decided
        by the panel table, never by the caller - which is the whole point.
        Getting this backwards is the classic 'why is my screen dark at 255'."""
        level = max(0, min(255, int(level)))
        if self._pwm is not None:
            duty = 255 - level if self.panel["bl_active_low"] else level
            self._pwm.duty_u16(duty * 257)
        elif self._bl is not None:
            on = 1 if level > 0 else 0
            self._bl.value((1 - on) if self.panel["bl_active_low"] else on)

    # -- geometry ------------------------------------------------------------

    def set_rotation(self, r):
        self.tft.set_rotation(r & 3)

    def width(self):
        return self.tft.width if self.tft else self.panel["width"]

    def height(self):
        return self.tft.height if self.tft else self.panel["height"]

    # -- diagnostics ---------------------------------------------------------

    def print_info(self):
        p, w = self.panel, self.wiring
        print("---- Lonely Binary Display ----")
        print("Panel      : {} ({})".format(p["name"], p["id"]))
        print("Driver     : {}".format(p["cls"]))
        print("Resolution : {}x{}  (native {}x{}, rotation {})".format(
            self.width(), self.height(), p["width"], p["height"], p["rotation"]))
        print("SPI clock  : {} Hz  (SPI{})".format(p["baudrate"], w["spi_id"]))
        print("Backlight  : {}, active {}".format(
            "PWM" if p["bl_pwm"] else "on/off",
            "LOW" if p["bl_active_low"] else "HIGH"))
        print("Board      : {}".format(self.board))
        print("Pins       : CS={} RST={} DC={} MOSI={} SCLK={} BL={}".format(
            w["cs"], w["rst"], w["dc"], w["mosi"], w["sclk"], w["backlight"]))
        print("-------------------------------")

    def self_test(self):
        """Solid colours, a backlight sweep (dimmable panels) and a colour-bar
        page - the 'does my wiring work' check."""
        t = self.tft
        w, h = self.width(), self.height()

        for color, name in ((RED, "RED"), (GREEN, "GREEN"), (BLUE, "BLUE")):
            t.fill(color)
            t.text(name, 4, h // 2 - 8, WHITE, bg=color, scale=2)
            time.sleep_ms(700)

        if self.panel["bl_pwm"]:
            t.fill(WHITE)
            t.text("Backlight sweep", 4, 4, BLACK, bg=WHITE)
            for v in range(255, 29, -5):
                self.backlight(v)
                time.sleep_ms(8)
            for v in range(30, 256, 5):
                self.backlight(v)
                time.sleep_ms(8)

        t.fill(BLACK)
        bars = (RED, GREEN, BLUE, YELLOW, MAGENTA, CYAN, WHITE, 0x8410)
        top = h // 3
        bar_h = (h - top) // 8
        for i, c in enumerate(bars):
            t.fill_rect(0, top + i * bar_h, w, bar_h, c)

        t.text("LonelyBinary", 4, 6, WHITE, scale=2 if w >= 200 else 1)
        t.text(self.panel["name"], 4, top - 22, WHITE)
        t.text("{}x{}".format(w, h), 4, top - 12, WHITE)
