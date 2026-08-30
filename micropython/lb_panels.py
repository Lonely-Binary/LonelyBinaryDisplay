"""GENERATED FROM panels.yaml BY tools/gen_panels.py — DO NOT EDIT"""

# Wiring is per-MCU, not per-panel — the whole range shares one breakout.
WIRING = {
    "esp32s3": {"cs": 10, "rst": 42, "dc": 2, "mosi": 11, "sclk": 12, "backlight": 41, "spi_id": 1, "psram": True},
    "esp32": {"cs": 15, "rst": 4, "dc": 2, "mosi": 23, "sclk": 18, "backlight": 32, "spi_id": 2, "psram": False},
}

# The MicroPython driver takes a single xstart/ystart, so the offset pair
# matching each panel's default rotation is pre-selected here.

TFT_096 = {
    "id": "tft_096",
    "name": "0.96 inch",
    "cls": "ST7735",
    "width": 80,
    "height": 160,
    "rotation": 0,
    "xstart": 24,
    "ystart": 0,
    "bgr": True,
    "invert": False,
    "flip_x": False,
    "flip_y": False,
    "baudrate": 20000000,
    "bl_active_low": True,
}

TFT_18 = {
    "id": "tft_18",
    "name": "1.8 inch",
    "cls": "ST7735",
    "width": 128,
    "height": 160,
    "rotation": 0,
    "xstart": 0,
    "ystart": 0,
    "bgr": False,
    "invert": False,
    "flip_x": True,
    "flip_y": True,
    "baudrate": 20000000,
    "bl_active_low": False,
}

TFT_20 = {
    "id": "tft_20",
    "name": "2.0 inch",
    "cls": "ST7789",
    "width": 240,
    "height": 320,
    "rotation": 0,
    "xstart": 0,
    "ystart": 0,
    "bgr": False,
    "invert": True,
    "flip_x": False,
    "flip_y": False,
    "baudrate": 40000000,
    "bl_active_low": False,
}

TFT_24 = {
    "id": "tft_24",
    "name": "2.4 inch",
    "cls": "ST7789",
    "width": 240,
    "height": 320,
    "rotation": 0,
    "xstart": 0,
    "ystart": 0,
    "bgr": False,
    "invert": True,
    "flip_x": False,
    "flip_y": False,
    "baudrate": 40000000,
    "bl_active_low": False,
}

TFT_28 = {
    "id": "tft_28",
    "name": "2.8 inch",
    "cls": "ST7789",
    "width": 240,
    "height": 320,
    "rotation": 0,
    "xstart": 0,
    "ystart": 0,
    "bgr": False,
    "invert": True,
    "flip_x": False,
    "flip_y": False,
    "baudrate": 40000000,
    "bl_active_low": False,
}

TFT_35 = {
    "id": "tft_35",
    "name": "3.5 inch",
    "cls": "ST7796",
    "width": 320,
    "height": 480,
    "rotation": 0,
    "xstart": 0,
    "ystart": 0,
    "bgr": True,
    "invert": True,
    "flip_x": False,
    "flip_y": False,
    "baudrate": 40000000,
    "bl_active_low": False,
}

NARROW_114 = {
    "id": "narrow_114",
    "name": "1.14 inch",
    "cls": "ST7789",
    "width": 135,
    "height": 240,
    "rotation": 1,
    "xstart": 53,
    "ystart": 40,
    "bgr": False,
    "invert": True,
    "flip_x": False,
    "flip_y": False,
    "baudrate": 8000000,
    "bl_active_low": True,
}

NARROW_168 = {
    "id": "narrow_168",
    "name": "1.68 inch",
    "cls": "NV3007_168",
    "width": 142,
    "height": 428,
    "rotation": 1,
    "xstart": 14,
    "ystart": 0,
    "bgr": False,
    "invert": False,
    "flip_x": False,
    "flip_y": False,
    "baudrate": 8000000,
    "bl_active_low": True,
}

NARROW_19 = {
    "id": "narrow_19",
    "name": "1.9 inch",
    "cls": "ST7789",
    "width": 170,
    "height": 320,
    "rotation": 1,
    "xstart": 35,
    "ystart": 0,
    "bgr": False,
    "invert": True,
    "flip_x": False,
    "flip_y": False,
    "baudrate": 8000000,
    "bl_active_low": True,
}

NARROW_225 = {
    "id": "narrow_225",
    "name": "2.25 inch",
    "cls": "ST7789",
    "width": 76,
    "height": 284,
    "rotation": 1,
    "xstart": 82,
    "ystart": 18,
    "bgr": False,
    "invert": False,
    "flip_x": False,
    "flip_y": False,
    "baudrate": 8000000,
    "bl_active_low": True,
}

NARROW_279 = {
    "id": "narrow_279",
    "name": "2.79 inch",
    "cls": "NV3007_279",
    "width": 142,
    "height": 428,
    "rotation": 1,
    "xstart": 14,
    "ystart": 0,
    "bgr": False,
    "invert": False,
    "flip_x": False,
    "flip_y": False,
    "baudrate": 20000000,
    "bl_active_low": True,
}

PANELS = {
    "tft_096": TFT_096,
    "tft_18": TFT_18,
    "tft_20": TFT_20,
    "tft_24": TFT_24,
    "tft_28": TFT_28,
    "tft_35": TFT_35,
    "narrow_114": NARROW_114,
    "narrow_168": NARROW_168,
    "narrow_19": NARROW_19,
    "narrow_225": NARROW_225,
    "narrow_279": NARROW_279,
}
