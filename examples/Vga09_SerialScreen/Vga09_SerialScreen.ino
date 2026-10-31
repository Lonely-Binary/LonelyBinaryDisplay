/*
  Vga09_SerialScreen - everything you print to Serial also appears on the
  monitor, in a 100 x 37 character terminal. Anything you TYPE into the Serial
  Monitor appears too.

  YOU WILL LEARN
    - how to make your own print target: a class that inherits from Print gets
      println(), printf() and print() of numbers for free
    - a scrolling text buffer (a ring of lines)
    - repainting only the rows whose text changed

  WHAT IT IS GOOD FOR
    A debug screen with no PC attached: put the board on a monitor with a USB
    power supply and read what your sketch is doing. Replace the demo lines in
    loop() with your own term.printf(...) calls.

  USE IT
    1  Upload, open the Serial Monitor at 115200 baud, line ending "Newline".
    2  Type a line and press Enter - it appears on the monitor.
    3  Lines containing ERROR are red, WARN yellow, OK green.
*/
#include <LonelyBinaryDisplay.h>

LB_Display display(LB_VGA_800X600);

static const int COLS = 100;          // 800 / 8
static const int ROWS = 37;           // 600 / 16 (the last 8 pixels stay black)
static const lb_color_t BG = LB_RGB(4, 8, 12);

class Terminal : public Print {
 public:
  char text[ROWS][COLS + 1];
  lb_color_t color[ROWS];
  int head = 0;                       // the row being written
  int col = 0;
  bool dirty = true;

  void clear() {
    for (int r = 0; r < ROWS; r++) { text[r][0] = 0; color[r] = LB_WHITE; }
    head = 0; col = 0; dirty = true;
  }

  size_t write(uint8_t c) override {
    if (c == '\r') return 1;
    if (c == '\n' || col >= COLS) {
      // Pick the colour from the finished line, then start the next one.
      const char *t = text[head];
      color[head] = strstr(t, "ERROR") ? LB_RGB(255, 90, 80)
                  : strstr(t, "WARN")  ? LB_RGB(255, 200, 60)
                  : strstr(t, "OK")    ? LB_RGB(80, 230, 130)
                                       : LB_RGB(220, 226, 235);
      head = (head + 1) % ROWS;
      text[head][0] = 0;
      col = 0;
      dirty = true;
      if (c == '\n') return 1;
    }
    if (c >= 32 && c < 127) {         // ASCII only here; see Vga04_Text for UTF-8
      text[head][col++] = (char)c;
      text[head][col] = 0;
      dirty = true;
    }
    return 1;
  }

  // What is on the glass now, row by row, so only rows that changed are painted.
  char shown[ROWS][COLS + 1] = {};
  lb_color_t shownColor[ROWS] = {};

  // Paint the rows, oldest at the top, the line being typed at the bottom.
  // A row whose text and colour are the same as before is left alone: while the
  // screen is filling up only the new line is drawn, and typing redraws one row.
  void draw() {
    display.setFont(&LB_Font8x16);
    display.setTextSize(1);
    for (int r = 0; r < ROWS; r++) {
      const int idx = (head + 1 + r) % ROWS;
      const lb_color_t c = idx == head ? LB_WHITE : color[idx];
      if (r < ROWS - 1 && c == shownColor[r] && strcmp(text[idx], shown[r]) == 0) continue;
      char line[COLS + 1];
      memset(line, ' ', COLS);
      line[COLS] = 0;
      memcpy(line, text[idx], strlen(text[idx]));
      display.setTextColor(c, BG);
      display.drawString(line, 0, r * 16);
      strcpy(shown[r], text[idx]);
      shownColor[r] = c;
    }
    // A block cursor at the end of the current line (the last row is always repainted, which erases the old one).
    display.fillRect(col * 8, (ROWS - 1) * 16 + 2, 8, 13, LB_RGB(80, 230, 130));
    display.flush();
    dirty = false;
  }
};

static Terminal term;

void setup() {
  Serial.begin(115200);
  if (!display.begin()) {
    Serial.println("VGA did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  display.fillScreen(BG);
  term.clear();

  term.println("Lonely Binary ESP32-S3 VGA 800x600");
  term.println("OK  display started");
  term.printf("OK  %d x %d, %d x %d characters\n", display.width(), display.height(), COLS, ROWS);
  term.printf("    CPU %d MHz, flash %lu MB, PSRAM %lu MB\n", getCpuFrequencyMhz(),
              (unsigned long)(ESP.getFlashChipSize() >> 20), (unsigned long)(ESP.getPsramSize() >> 20));
  term.println("    Type in the Serial Monitor and it appears here.");
  term.println();
}

void loop() {
  // Anything typed in the Serial Monitor is echoed to the screen.
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n') term.println(); else if (c != '\r') term.write(c);
  }

  // A demo log line every 1.5 s. Delete this block and use your own term.printf().
  static uint32_t last = 0;
  static int n = 0;
  if (millis() - last > 1500) {
    last = millis();
    n++;
    if (n % 7 == 0)      term.printf("[%6lu ms] ERROR  sensor %d did not answer\n", (unsigned long)millis(), n % 3);
    else if (n % 4 == 0) term.printf("[%6lu ms] WARN   free heap is %lu KB\n", (unsigned long)millis(), (unsigned long)(ESP.getFreeHeap() / 1024));
    else if (n % 5 == 0) term.printf("[%6lu ms] OK     reading %d stored\n", (unsigned long)millis(), n);
    else                 term.printf("[%6lu ms] temp %.1f C   uptime %lu s\n", (unsigned long)millis(), temperatureRead(), (unsigned long)(millis() / 1000));
  }

  // Redraw at most 20 times a second, and only when the text changed.
  static uint32_t lastDraw = 0;
  if (term.dirty && millis() - lastDraw > 50) {
    lastDraw = millis();
    term.draw();
  }
}
