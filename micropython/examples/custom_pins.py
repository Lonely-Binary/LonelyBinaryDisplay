"""
custom_pins.py - using a Lonely Binary panel on YOUR wiring.

By default the pins come from the Lonely Binary breakout for the chip this runs
on. If the display is on your own PCB, or the default GPIOs are already taken,
override them.

THE RULE: start from the default and change only what differs. Copying it keeps
the right spi_id for your chip - SPI bus numbering is not the same on an
ESP32-S3 (SPI(1) = FSPI) as on a classic ESP32 (SPI(2) = VSPI).

The panel constant does not change: driver, resolution, offsets, colour order
and backlight polarity belong to the SCREEN, not to the wiring.
"""

import lb_panels
from lb_display import LBDisplay, detect_board
from lb_panels import TFT_24

# Start from the wiring for the chip we are running on...
pins = dict(lb_panels.WIRING[detect_board()])

# ...then change only what is different on your hardware.
pins["cs"] = 5
pins["dc"] = 16
pins["rst"] = 17
pins["mosi"] = 23
pins["sclk"] = 18

# Backlight: a GPIO for dimming, or -1 if the board has no backlight pin
# (hard-wired on, or driven by your own circuit). With -1, backlight() does
# nothing instead of poking a stray pin.
pins["backlight"] = 4

# Longer traces or jumper wires may not survive the panel's rated clock.
# A scrambled or half-drawn image means: lower this.
# pins is not where the clock lives - override it on the panel copy instead:
panel = dict(TFT_24)
# panel["baudrate"] = 20_000_000

d = LBDisplay(panel, wiring=pins)
d.begin()
d.print_info()     # reports the pins in use and marks them as custom
d.self_test()
