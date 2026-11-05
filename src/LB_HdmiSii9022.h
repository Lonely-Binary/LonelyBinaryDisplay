#pragma once
/*
 * LB_HdmiSii9022 - the SiI9022A HDMI transmitter on an RGB board (LB_HDMI_RGB).
 *
 * The S3 scans RGB565 out with a pixel clock and DE, exactly as for a bare RGB
 * panel; the SiI9022A turns that into HDMI. What it needs from software is a
 * one-time setup over I2C (its "TPI" register interface): the timing it is
 * being fed, the input format, the audio format, and then "TMDS on".
 * LB_RgbScreen::begin() calls start() once the scan-out is running.
 *
 * Two things the board does not do for us:
 *   - Its RESET pin is tied to the board's RST button, not to a GPIO. A sketch
 *     that restarts (ESP.restart(), an upload) leaves the chip as it was, so
 *     start() always sets it up from scratch.
 *   - Unplugging the HDMI cable, or a TV powering up late, can drop the chip
 *     out of TPI mode. Its interrupt pin is not wired, so a small background
 *     task polls the hot-plug status every 200 ms and sets the chip up again
 *     when a monitor appears or the chip has lost its setup.
 *
 * The register sequence is the one that ran on the board with picture and
 * sound on the monitors and TVs it was tried on (QC esp32s3_hdmi_project,
 * tests/esp32s3_hdmi_photo_music). Register meanings: "SiI9022A/SiI9024A TPI
 * Programmer's Reference" (SiI-PR-1064-A).
 *
 * Audio: the chip is set up for I2S, 16-bit, 44.1 kHz, stereo, on its SD0
 * input. Send exactly that on the board's I2S pins (display.wiringRgb().i2sBclk
 * / i2sWs / i2sDout) and it comes out of the TV. See examples/Hdmi07_Sound.
 */
#include <Arduino.h>
#include "LB_Panels.h"

namespace LB_HdmiSii9022 {

// Set the chip up for this board at this resolution and start the hot-plug
// watcher. width/height and the timings in `board` are the ones the scan-out
// runs at. Returns false if the chip did not answer on I2C.
bool start(const LB_RgbBoard &board, uint16_t width, uint16_t height);

// A monitor is plugged in and powered (the HDMI hot-plug line is high).
bool connected();

// How many times the chip has been set up again since start() (a cable
// plugged back in, a TV switched on, the chip having lost its setup).
uint32_t restarts();

}  // namespace LB_HdmiSii9022
