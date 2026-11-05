/*
  Hdmi07_Sound - sound through the HDMI cable: a tune out of the TV's speakers,
  the notes on the screen.

  YOU WILL LEARN
    - HDMI carries sound as well as the picture: the ESP32-S3 sends it to the
      SiI9022A on three I2S lines, and the chip packs it into the HDMI signal
    - what I2S wants: a stream of samples, left and right, 44,100 a second
    - how a note is made: a sine wave at the note's frequency, faded in and
      out so it does not click
    - the I2S pins come with the board: display.wiringRgb().i2sBclk / i2sWs /
      i2sDout, no numbers to look up

  WHAT YOU SHOULD HEAR AND SEE
    "Ode to Joy", over and over, from the monitor's or TV's speakers. Each note
    lights up on a little keyboard on the screen as it plays. A monitor without
    speakers stays silent - try a TV, or a monitor with a headphone socket.

  THE FORMAT IS FIXED
    begin() sets the SiI9022A up for 16-bit stereo at 44.1 kHz, standard I2S
    (Philips). Send exactly that. The samples go out in 32-bit slots, so each
    16-bit sample is shifted to the top of a 32-bit word - see playNote().

  This example uses only the ESP32's own I2S driver, no extra library. For
  MP3s, the ESP32-audioI2S library (schreibfaul1) plays to the same pins:
  audio.setPinout(bclk, ws, dout) with the three pins from wiringRgb().

  BOARD SETTINGS: as Hdmi01_Hello.
*/
#include <LonelyBinaryDisplay.h>
#include <driver/i2s_std.h>
#include <math.h>

// ---- GPIO wiring. Every pin this sketch uses is here - change the numbers to match
// ---- your board; nothing below refers to a GPIO by number. -1 = not connected.
#define PIN_DE     45   // to the SiI9022A HDMI chip
#define PIN_PCLK   12
#define PIN_HSYNC  38
#define PIN_VSYNC  39
#define PIN_B0     48   // blue  - 5 bits
#define PIN_B1     47
#define PIN_B2     21
#define PIN_B3     14
#define PIN_B4     13
#define PIN_G0     11   // green - 6 bits
#define PIN_G1     10
#define PIN_G2      9
#define PIN_G3     46
#define PIN_G4      3
#define PIN_G5      8
#define PIN_R0     18   // red   - 5 bits
#define PIN_R1     17
#define PIN_R2     16
#define PIN_R3     15
#define PIN_R4      6
#define PIN_SDA     5   // I2C to the SiI9022A (its setup)
#define PIN_SCL     4
#define PIN_I2S_BCLK 42 // I2S to the SiI9022A (the sound): bit clock
#define PIN_I2S_WS   41 //   left/right (word select)
#define PIN_I2S_DOUT 40 //   data

LB_Display display(LB_HDMI_RGB);
static i2s_chan_handle_t tx = nullptr;

static const int RATE = 44100;              // what the SiI9022A is set up for
static const float VOLUME = 0.25f;          // 0..1 - a pure sine is loud

// ---- The tune ---------------------------------------------------------------
// Notes of one octave and the next, C4 to C5, and the tune as note numbers.
static const char *NOTE_NAME[8] = {"C", "D", "E", "F", "G", "A", "B", "C"};
static const float NOTE_HZ[8] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f, 440.00f, 493.88f, 523.25f};
struct Step { int8_t note; uint8_t beats; };   // note -1 = rest; a beat is BEAT_MS
static const Step TUNE[] = {
  {2,1},{2,1},{3,1},{4,1},{4,1},{3,1},{2,1},{1,1},{0,1},{0,1},{1,1},{2,1},{2,2},{1,1},{1,2},{-1,1},
  {2,1},{2,1},{3,1},{4,1},{4,1},{3,1},{2,1},{1,1},{0,1},{0,1},{1,1},{2,1},{1,2},{0,1},{0,2},{-1,2},
};
static const int BEAT_MS = 360;

// ---- Sound out --------------------------------------------------------------
static bool soundBegin(int8_t bclk, int8_t ws, int8_t dout) {
  i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  chan.auto_clear = true;                   // silence, not the last buffer, when we stop sending
  if (i2s_new_channel(&chan, &tx, nullptr) != ESP_OK) return false;
  i2s_std_config_t cfg = {};
  cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RATE);
  // Standard I2S, stereo, 32-bit slots: the format the SiI9022A is set up for.
  cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;      // the board does not wire MCLK; the chip does not need it
  cfg.gpio_cfg.bclk = (gpio_num_t)bclk;
  cfg.gpio_cfg.ws = (gpio_num_t)ws;
  cfg.gpio_cfg.dout = (gpio_num_t)dout;
  cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(tx, &cfg) != ESP_OK) return false;
  return i2s_channel_enable(tx) == ESP_OK;
}

// One note (or a rest, hz = 0): a sine wave with a short fade in and out.
static void playNote(float hz, int ms) {
  static int32_t buf[256 * 2];              // 256 frames, left + right
  const int total = RATE * ms / 1000, fade = RATE / 100;   // 10 ms fades
  static float phase = 0;
  for (int done = 0; done < total;) {
    const int n = min(256, total - done);
    for (int i = 0; i < n; i++) {
      const int k = done + i;
      float env = 1.0f;
      if (k < fade) env = (float)k / fade;
      else if (k > total - fade) env = (float)(total - k) / fade;
      const float v = hz > 0 ? sinf(phase) * VOLUME * env : 0.0f;
      phase += 2.0f * (float)M_PI * hz / RATE;
      if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
      const int32_t s = (int32_t)(v * 32767.0f) << 16;   // 16-bit sample at the top of the 32-bit slot
      buf[2 * i] = s;                       // left
      buf[2 * i + 1] = s;                   // right
    }
    size_t written;
    i2s_channel_write(tx, buf, (size_t)n * 8, &written, portMAX_DELAY);   // waits while the buffer is full
    done += n;
  }
}

// ---- Picture ----------------------------------------------------------------
static int keyX0, keyW, keyY, keyH;

static void drawKey(int note, bool lit) {
  const int x = keyX0 + note * keyW;
  display.fillRect(x + 2, keyY, keyW - 4, keyH, lit ? LB_RGB(20, 137, 143) : LB_WHITE);
  display.setTextSize(3);
  display.setTextColor(lit ? LB_WHITE : LB_BLACK);
  display.drawString(NOTE_NAME[note], x + (keyW - display.textWidth(NOTE_NAME[note])) / 2, keyY + keyH - 60);
  display.flush();
}

void setup() {
  Serial.begin(115200);

  LB_WiringRgb pins = display.wiringRgb();   // the board's default, then yours
  pins.de = PIN_DE;        pins.pclk = PIN_PCLK;
  pins.hsync = PIN_HSYNC;  pins.vsync = PIN_VSYNC;
  pins.i2cSda = PIN_SDA;   pins.i2cScl = PIN_SCL;
  pins.i2sBclk = PIN_I2S_BCLK;  pins.i2sWs = PIN_I2S_WS;  pins.i2sDout = PIN_I2S_DOUT;
  const int8_t dataPins[16] = {PIN_B0, PIN_B1, PIN_B2, PIN_B3, PIN_B4,
                              PIN_G0, PIN_G1, PIN_G2, PIN_G3, PIN_G4, PIN_G5,
                              PIN_R0, PIN_R1, PIN_R2, PIN_R3, PIN_R4};
  for (int i = 0; i < 16; i++) pins.data[i] = dataPins[i];
  display.setWiring(pins);

  display.setResolution(800, 600);      // B: 800 x 600 @ 50 Hz, the default
  // display.setResolution(640, 480);   // A: 640 x 480 @ 64.7 Hz - use this if the monitor shows no picture or streaks at the left
  if (!display.begin()) {
    Serial.println("HDMI did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  const LB_WiringRgb &p = display.wiringRgb();
  if (!soundBegin(p.i2sBclk, p.i2sWs, p.i2sDout)) Serial.println("I2S did not start");

  const int W = display.width(), H = display.height();
  display.fillScreen(LB_RGB(8, 12, 18));
  display.setTextSize(3);
  display.setTextColor(LB_WHITE);
  const char *title = "Ode to Joy";
  display.drawString(title, (W - display.textWidth(title)) / 2, 24);
  display.setTextSize(1);
  display.setTextColor(LB_LIGHTGRAY);
  const char *hint = "Sound comes out of the monitor's or TV's speakers.";
  display.drawString(hint, (W - display.textWidth(hint)) / 2, 80);

  keyW = (W - 40) / 8;
  keyX0 = (W - keyW * 8) / 2;
  keyY = H / 3;
  keyH = H / 2;
  for (int n = 0; n < 8; n++) drawKey(n, false);
}

void loop() {
  if (!tx) { delay(1000); return; }
  for (const Step &s : TUNE) {
    if (s.note >= 0) drawKey(s.note, true);
    playNote(s.note >= 0 ? NOTE_HZ[s.note] : 0, s.beats * BEAT_MS - 40);
    if (s.note >= 0) drawKey(s.note, false);
    playNote(0, 40);                         // a short gap between notes
  }
}
