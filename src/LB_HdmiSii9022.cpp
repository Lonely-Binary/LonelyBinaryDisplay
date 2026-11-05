#include "LB_HdmiSii9022.h"

#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

LB_RgbBoard s_board = {};
uint16_t s_w = 0, s_h = 0;
uint8_t s_addr = 0x3B;
bool s_ok = false;                 // the last I2C transfer was acknowledged
volatile bool s_hpd = false;
volatile uint32_t s_restarts = 0;
TaskHandle_t s_task = nullptr;

void tpiWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(s_addr);
  Wire.write(reg);
  Wire.write(val);
  s_ok = Wire.endTransmission() == 0;
}

uint8_t tpiRead(uint8_t reg) {
  Wire.beginTransmission(s_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) { s_ok = false; return 0; }
  Wire.requestFrom((int)s_addr, 1);
  s_ok = Wire.available() > 0;
  return s_ok ? Wire.read() : 0;
}

void tpiWriteBlock(uint8_t reg, const uint8_t *data, uint8_t len) {
  Wire.beginTransmission(s_addr);
  Wire.write(reg);
  for (uint8_t i = 0; i < len; i++) Wire.write(data[i]);
  s_ok = Wire.endTransmission() == 0;
}

void tpiIndexedWrite(uint8_t page, uint8_t offset, uint8_t value) {
  tpiWrite(0xBC, page);
  tpiWrite(0xBD, offset);
  tpiWrite(0xBE, value);
}

// A reset of the S3 in the middle of a transfer can leave the SiI9022A holding
// SDA low. Clock it out (up to nine pulses), then send a STOP.
void busRecovery(int8_t sda, int8_t scl) {
  pinMode(scl, OUTPUT_OPEN_DRAIN);
  pinMode(sda, INPUT_PULLUP);
  digitalWrite(scl, HIGH);
  delayMicroseconds(10);
  for (uint8_t i = 0; i < 9 && digitalRead(sda) == LOW; i++) {
    digitalWrite(scl, LOW);  delayMicroseconds(5);
    digitalWrite(scl, HIGH); delayMicroseconds(5);
  }
  pinMode(sda, OUTPUT_OPEN_DRAIN);
  digitalWrite(sda, LOW);  delayMicroseconds(5);
  digitalWrite(scl, HIGH); delayMicroseconds(5);
  digitalWrite(sda, HIGH); delayMicroseconds(5);
}

// Read one byte of the monitor's EDID through the chip's DDC pass-through:
// some TVs do not start their HDMI receiver until a source has done so.
void touchEdid() {
  tpiWrite(0x1A, 0x04);   // request the DDC bus
  bool granted = false;
  for (uint8_t i = 0; i < 20 && !granted; i++) { delay(5); granted = tpiRead(0x1A) & 0x02; }
  if (!granted) { tpiWrite(0x1A, 0x00); return; }
  tpiWrite(0x1A, 0x06);
  Wire.beginTransmission(0x50);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission(false) == 0) {
    Wire.requestFrom(0x50, 1);
    while (Wire.available()) Wire.read();
  }
  Wire.beginTransmission(s_addr);   // release the bus; no ACK is normal here
  Wire.write((uint8_t)0x1A);
  Wire.write((uint8_t)0x00);
  Wire.endTransmission();
  delay(2);
  if (tpiRead(0x1A) & 0x06) tpiWrite(0x1A, 0x00);
}

bool configure() {
  // Enter TPI mode (0xC7 = 0) and check the device ID. Right after power-up
  // the chip may take a few tries.
  uint8_t id = 0;
  for (uint8_t attempt = 0; attempt < 8; attempt++) {
    tpiWrite(0xC7, 0x00);
    delay(10);
    id = tpiRead(0x1B);
    if (s_ok && id == 0xB0) break;
    delay(100);
  }
  if (!s_ok || id != 0xB0) return false;

  tpiWrite(0xBC, 0x01);   // TMDS termination on: indexed page 1, 0x82 bit 0 (as the reference driver)
  tpiWrite(0xBD, 0x82);
  tpiWrite(0xBE, tpiRead(0xBE) | 0x01);
  touchEdid();

  const LB_RgbBoard &b = s_board;
  const uint32_t ht = s_w + b.hsPulse + b.hsBack + b.hsFront;
  const uint32_t vt = s_h + b.vsPulse + b.vsBack + b.vsFront;
  const uint16_t pclk10k = (uint16_t)((b.pclkHz + 5000) / 10000);
  const uint16_t vfreq = (uint16_t)((b.pclkHz + ht * vt / 2) / (ht * vt));
  const uint8_t vmode[8] = {(uint8_t)pclk10k, (uint8_t)(pclk10k >> 8), (uint8_t)vfreq, (uint8_t)(vfreq >> 8),
                            (uint8_t)ht, (uint8_t)(ht >> 8), (uint8_t)vt, (uint8_t)(vt >> 8)};
  tpiWriteBlock(0x00, vmode, 8);

  tpiWrite(0x1A, 0x11);   // TMDS off while we change things, HDMI mode
  tpiWrite(0x1E, 0x00);   // power state D0
  // Input bus: 1x pixel clock, latched on the rising edge (the S3 changes data
  // on the rising edge too: pclk_active_neg = false). RGB in and out.
  tpiWrite(0x08, 0x70);
  tpiWrite(0x0B, 0x00);
  tpiWrite(0x09, 0x00);
  tpiWrite(0x0A, 0x00);

  // AVI InfoFrame, all zero: RGB, no VIC - the monitor goes by the timing it
  // measures. Must be written before the DE generator (0x60..), or writing 0x19
  // clears it.
  {
    uint8_t avi[14] = {0};
    uint16_t sum = 0x82 + 0x02 + 13;
    avi[0] = (uint8_t)(0x100 - (sum & 0xFF));
    tpiWriteBlock(0x0C, avi, 14);
  }

  // The chip's own DE generator: where the picture starts and how big it is,
  // plus the sync polarity (bit 4 / 5 set = negative).
  tpiWrite(0x60, 0x00);
  {
    const uint16_t deDly = b.hsPulse + b.hsBack;
    const uint8_t deTop = (uint8_t)(b.vsPulse + b.vsBack);
    const uint8_t neg = b.syncIdleHigh ? 0x30 : 0x00;
    const uint8_t de[8] = {(uint8_t)deDly, (uint8_t)(((deDly >> 8) & 0x03) | neg | 0x40), (uint8_t)(deTop & 0x7F), 0,
                           (uint8_t)s_w, (uint8_t)((s_w >> 8) & 0x0F), (uint8_t)s_h, (uint8_t)((s_h >> 8) & 0x07)};
    tpiWriteBlock(0x62, de, 8);
  }

  // Audio: I2S on SD0, 16-bit, 44.1 kHz, stereo; muted while it is set up.
  tpiWrite(0x26, 0x90);
  tpiWrite(0x20, 0x88);
  tpiWrite(0x1F, 0x80);
  tpiWrite(0x21, 0x00);
  tpiWrite(0x22, 0x00);
  tpiWrite(0x23, 0x00);
  tpiWrite(0x24, 0x00);
  tpiWrite(0x25, 0x02);
  tpiWrite(0x27, 0x50);
  {
    uint8_t ai[10] = {0x09, 0x09};   // audio InfoFrame: 2 channels, 16-bit, 44.1 kHz
    uint16_t sum = 0x84 + 0x01 + 0x0A;
    for (uint8_t i = 0; i < 10; i++) sum += ai[i];
    tpiWrite(0xBF, 0xC2);
    tpiWrite(0xC0, 0x84);
    tpiWrite(0xC1, 0x01);
    tpiWrite(0xC2, 0x0A);
    tpiWrite(0xC3, (uint8_t)(0x100 - (sum & 0xFF)));
    for (uint8_t i = 0; i < 10; i++) tpiWrite(0xC4 + i, ai[i]);
  }
  tpiIndexedWrite(0x02, 0x24, 0x02);
  tpiWrite(0x26, 0x80);   // audio on

  // TMDS on, then a clean AV-mute / unmute so the receiver resynchronises.
  tpiWrite(0x1A, 0x01);
  tpiWrite(0x08, 0x70);   // a 1 -> 0 change of 0x1A bit 4 clears 0x08[3:0]
  tpiWrite(0x1A, 0x09);
  delay(50);
  tpiWrite(0x1A, 0x01);
  tpiWrite(0x3C, 0x03);   // latch hot-plug / receiver-sense events in 0x3D
  tpiWrite(0x3D, 0x03);
  return s_ok;
}

void watcher(void *) {
  bool prevHpd = true, prevRsen = true;
  uint32_t lastSetup = millis();
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(200));
    const uint8_t st = tpiRead(0x3D);
    if (!s_ok) continue;
    const bool hpd = st & 0x04, rsen = st & 0x08;
    const bool lost = tpiRead(0x1B) != 0xB0;     // back out of TPI mode: the chip lost its setup
    const bool rising = (hpd && !prevHpd) || (rsen && !prevRsen);
    const bool event = st & 0x03;
    prevHpd = hpd;
    prevRsen = rsen;
    s_hpd = hpd;
    if (event) tpiWrite(0x3D, st & 0x03);
    if ((lost || (hpd && (rising || event))) && millis() - lastSetup > 3000) {
      vTaskDelay(pdMS_TO_TICKS(500));   // let the monitor settle
      if (configure()) s_restarts = s_restarts + 1;
      lastSetup = millis();
    }
  }
}

}  // namespace

namespace LB_HdmiSii9022 {

bool start(const LB_RgbBoard &board, uint16_t width, uint16_t height) {
  s_board = board;
  s_w = width;
  s_h = height;
  s_addr = board.i2cAddr ? board.i2cAddr : 0x3B;
  busRecovery(board.i2cSda, board.i2cScl);
  Wire.begin(board.i2cSda, board.i2cScl, 100000);
  delay(300);
  if (!configure()) {
    Serial.printf("[LB_HdmiSii9022] no answer from the SiI9022A at 0x%02X (SDA %d, SCL %d)\n", s_addr, board.i2cSda,
                  board.i2cScl);
    return false;
  }
  s_hpd = tpiRead(0x3D) & 0x04;
  if (!s_task) xTaskCreatePinnedToCore(watcher, "lb_hdmi", 3072, nullptr, 1, &s_task, 0);
  return true;
}

bool connected() { return s_hpd; }

uint32_t restarts() { return s_restarts; }

}  // namespace LB_HdmiSii9022
