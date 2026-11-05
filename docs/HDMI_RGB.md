# Lonely Binary ESP32-S3 RGB to HDMI - engineering notes

What the board is, how the library drives it, the numbers we measured, and why
the resolutions are what they are. The short version is in the README; this is
the long one.

```cpp
#include <LonelyBinaryDisplay.h>
LB_Display display(LB_HDMI_RGB);
```

## 1. What it is

An ESP32-S3 (N16R8: 16 MB flash, 8 MB octal PSRAM) on a board with a
**SiI9022A** HDMI transmitter (U1). The S3's LCD_CAM peripheral sends RGB565,
a pixel clock, DE, HSYNC and VSYNC; the SiI9022A turns that into TMDS for the
HDMI connector, and takes I2S audio on three more lines and puts it into the
same signal. Board: "ESP32-S3 RGB转HDMI V2", schematic and netlist 2026-11-04.

| | |
|---|---|
| Picture | RGB565, default 800 x 600 @ 50 Hz; others via `setResolution()` (section 4) |
| Frame buffer | two in PSRAM, RGB565 (800 x 600: 938 KB each) |
| Video pins | DE 45, PCLK 12, HSYNC 38, VSYNC 39; R0..R4 = 18, 17, 16, 15, 6; G0..G5 = 11, 10, 9, 46, 3, 8; B0..B4 = 48, 47, 21, 14, 13 |
| SiI9022A setup | I2C, SDA 5 / SCL 4, 7-bit address **0x3B** (CI2CA pulled high by R9; the chip's default is 0x39) |
| Audio | I2S: BCLK 42 -> SCK, WS 41 -> WS, DOUT 40 -> SD0. MCLK is not wired and not needed |
| Reset | the SiI9022A's RESET# is tied to the board's RST button through R2, with R8 as pull-up - **not to a GPIO** |
| Interrupt | INT is not connected |
| Serial | the native USB port: Tools > USB CDC On Boot > Enabled |

Only the top bits of each colour reach the chip (its D23-19, D15-10 and
D7-3); the rest of its 24 inputs float.

## 2. What begin() does

1. Starts the scan-out exactly as for any RGB panel (`LB_RgbScreen`): two
   frame buffers in PSRAM, bounce buffers in internal RAM.
2. Then `LB_HdmiSii9022::start()`: frees the I2C bus (a reset in the middle of
   a transfer can leave the chip holding SDA low), enters TPI mode (`0xC7 = 0`,
   device ID `0xB0` at `0x1B`), turns TMDS termination on, reads one byte of the
   monitor's EDID (some TVs do not start their receiver until a source has),
   writes the timing it is being fed (`0x00..0x07`), the input format
   (`0x08 = 0x70`: 1x clock, latched on the rising edge, RGB in and out), an
   all-zero AVI InfoFrame (no VIC: the monitor goes by the timing it measures),
   the DE generator and sync polarity (`0x60..0x69`), the audio format (I2S,
   SD0, 16-bit, 44.1 kHz, `0x1F..0x27` plus the audio InfoFrame), and finally
   TMDS on with an AV-mute / unmute.
3. Starts a background task (core 0, priority 1) that reads the hot-plug
   status every 200 ms.

The order and the values are those of the firmware that ran this board with
picture and sound on every monitor and TV it was tried on (QC
`esp32s3_hdmi_project`, `tests/esp32s3_hdmi_photo_music`). Two ordering
rules from the chip's programmer's reference that matter: the AVI InfoFrame
(`0x0C..0x19`) must be written before the DE generator, or writing `0x19`
clears it; and a 1 -> 0 change of `0x1A` bit 4 clears `0x08[3:0]`, so `0x08`
is written again after TMDS goes on.

## 3. Plugging and unplugging

Measured on the board before the library: unplugging and re-plugging the HDMI
cable sometimes drops the SiI9022A out of TPI mode (its device ID reads `0x00`
instead of `0xB0`) and the hot-plug line goes low. Nothing comes back by
itself. Since the chip has no GPIO reset and no interrupt line, the watcher
task does this every 200 ms:

- read `0x3D` (bit 2 hot-plug level, bit 3 receiver sense, bits 0/1 latched
  events) and `0x1B` (device ID);
- if the chip has lost TPI mode, or a monitor has just appeared, wait 500 ms
  and run the whole setup again (at most once every 3 s);
- `hdmiConnected()` returns the hot-plug level; `hdmiRestarts()` counts the
  setups.

Measured with the library: with no cable `0x3D = 0x00`, hot-plug low; cable in
and monitor on, `0x3D = 0x0C` within a second and one restart counted.

## 4. Resolutions

All measured on 2026-11-04 with a test sketch that steps through every entry:
test card on screen, a 60 x 60 box redrawn every 10 ms while it scans out, 4 s
each, `scanStats()` read before and after. **0 late frames in every one**, HDMI
connected throughout.

| Size | Total | Pixel clock | Clock source | Refresh | Frames in 4 s | Longest frame |
|---|---|---|---|---|---|---|
| 800 x 600 (default) | 1024 x 626 | 32 MHz | PLL160 / 5 | 49.9 Hz | 200 | 20.2 ms |
| 640 x 480 | 800 x 525 | 27.178 MHz | default | 64.7 Hz | 258 | 15.5 ms |
| 720 x 480 | 858 x 525 | 27 MHz | default | 59.9 Hz | 240 | 16.7 ms |
| 720 x 576 | 864 x 626 | 27 MHz | default | 49.9 Hz | 201 | 20.2 ms |
| 800 x 600 (56) | 1024 x 625 | 36 MHz | default | 56.2 Hz | 225 | 18.0 ms |
| 1024 x 768 | 1184 x 782 | 32 MHz | PLL160 / 5 | 34.6 Hz | 138 | 29.2 ms |
| 1280 x 720 | 1424 x 750 | 32 MHz | PLL160 / 5 | 30.0 Hz | 120 | 33.5 ms |
| 1366 x 768 | 1494 x 790 | 32 MHz | PLL160 / 5 | 27.1 Hz | 108 | 37.2 ms |
| 1280 x 800 | 1424 x 828 | 32 MHz | PLL160 / 5 | 27.1 Hz | 108 | 37.1 ms |

Seen on a monitor on 2026-11-05 with `Hdmi03_Resolutions` (BOOT through all
nine, twice round): **every one showed a picture**, the 36 MHz 800 x 600
included. Other monitors may still refuse the 27-35 Hz ones.

What `scanStats()` cannot see: it measures whole frames, not lines. On the VGA
board 36 MHz showed short streaks at the left of some lines with the frame
timing still perfect (see `VGA_800x600.md`). Check the 36 MHz entry with your
own eyes before relying on it.

**800 x 600 @ 50 is close to the edge too, and the 1 ms tick decides it.**
2026-11-05, `Hdmi09_Dashboard` at the default: the same streaks at the left
(stale lines from 20 rows up), static, on about **6 boots in 10** with the same
binary - clean or streaked from power-up to the next reset. The copy into the
bounce buffers (16 KB of PSRAM in 320 us) has 20-70 us to spare at 50 MB/s,
and with 1024 x 625 at 32 MHz a frame was exactly **20.000 ms**: twenty
FreeRTOS ticks (1 kHz, from the same crystal). So each tick interrupt fell on
the same refill every frame, at a place fixed by when the board was switched
on; where that was a refill with no time to spare, it was late in every frame.
One line more (1024 x **626**, 20.032 ms, 49.9 Hz) lets the ticks drift
through the frame: **1 boot in 10** showed streaks after that change. 720 x
576 (864 x 625 at 27 MHz, also exactly 20 ms) got the same extra line.

Rule for new timings: the frame time must not be a whole number of
milliseconds.

Also tried: bounce buffers of 20 lines instead of 10 - **worse** (streaks
higher up the screen, and some shaking); do not try that again. 640 x 480
(43.5 MB/s, 15.454 ms a frame) was clean in every test. So every example sets
`setResolution(800, 600)` with a commented-out `setResolution(640, 480)` under
it, the line to switch to if a monitor shows streaks or no picture.

**The default.** 800 x 600 @ 50 since 2026-11-05: every mode in the table showed on
the monitor, and this one has an integer pixel clock (PLL160 / 5) and the most
room on screen of the ones that nearly every monitor takes.

**640 x 480's clock.** 27.178 MHz is what the board has always been run at
(the timing works out to 64.7 Hz, not VESA's 60). It is kept because that is
what was seen to work on every monitor and TV so far; the SiI9022A is told the
real timing and the AVI InfoFrame carries no VIC.

**Why the big sizes are 27-35 Hz.** Two limits of the S3, both measured with
`tests/esp32s3_hdmi_res_scan3` in the QC project:

1. The scan-out cannot read PSRAM much faster than about 32-36 MHz of pixels
   (with a frame buffer, RGB565): 1024 x 768 @ 50 at 48 MHz shook, 1280 x 720
   at 60-80 MHz sent only 70-90 % of the lines.
2. LCD_CAM's vertical total is a 10-bit field: no more than 1024 lines, so
   1080p is out whatever the clock.

So the big sizes keep the clock at 32 MHz and give up refresh rate. Many
monitors take 30 Hz and below; some do not, and show "no signal" or "out of
range" (the monitor reads the timing from the signal; there is nothing in the
signal the library could change to make it accept).

**Horizontal blanking.** HDMI carries audio in "data islands" inside the
horizontal blank. Measured in the QC project with a 1 kHz test tone through
a microphone: 80 pixels of blanking gave mostly noise, 160 was clean; every
entry here has at least 128 (1366 x 768 has exactly 128), and with 128-144 the
SiI9022A's audio error flag (`0x3D` bit 4) stayed at 0. Do not shorten it.

## 5. Sound

The chip is set up for I2S, 16-bit samples, 44.1 kHz, stereo, on SD0. On the
S3 side that is `I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
I2S_SLOT_MODE_STEREO)` at 44100 Hz with the 16-bit sample in the top half of
each 32-bit word: the format ESP32-audioI2S uses, which is what ran on the
board. See `Hdmi07_Sound`: heard from the monitor's speakers on 2026-11-05,
clean, at 800 x 600.

In the QC project, the music stuttered when a large picture was copied from
flash to PSRAM while it played (the I2S DMA buffer ran dry); a bigger I2S
buffer (32 descriptors instead of 16) and copying in small pieces fixed it.
A sketch that plays music and redraws large areas should do the same.

## 6. Not yet

- `LB_HDMI_VGA` (the MS9288C board) has its table entry and timings, from its
  own test firmware, but has not been run through this library.
- No EDID parsing: the library does not ask the monitor which sizes it takes.
- The resolution is fixed at `begin()`; changing it means a restart
  (`Hdmi03_Resolutions` keeps the choice in RTC memory across `ESP.restart()`).
