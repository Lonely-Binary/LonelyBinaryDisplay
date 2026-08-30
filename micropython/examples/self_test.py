"""
self_test.py - does my screen and wiring work?

Solid colours, a backlight sweep (on panels that dim) and a colour-bar page
with the panel's identity, then the same details on the REPL.

TO USE A DIFFERENT SCREEN, CHANGE ONE LINE - the import below.

    TFT_096   TFT_18   TFT_20   TFT_24   TFT_28   TFT_35
    NARROW_114   NARROW_168   NARROW_19   NARROW_225   NARROW_279

Everything else (driver class, resolution, offsets, SPI clock, backlight
polarity) comes from the panel table. The GPIOs are picked at runtime from the
chip this is running on, so the same script runs on a classic ESP32 and an
ESP32-S3 unmodified.

Copy to the board (rename to main.py to run it at boot):
    mpremote mip install github:Lonely-Binary/LonelyBinaryDisplay/micropython
    mpremote cp examples/self_test.py :main.py
"""

from lb_display import LBDisplay
from lb_panels import TFT_24            # <- the ONLY line that changes

d = LBDisplay(TFT_24)
d.begin()
d.print_info()
d.self_test()
