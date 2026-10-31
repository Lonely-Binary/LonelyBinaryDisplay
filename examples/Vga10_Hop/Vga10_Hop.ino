/*
  Vga10_Hop - a one-button game. Press BOOT to jump the obstacles.

  YOU WILL LEARN
    - a game loop: read the button, update the world, draw, wait for the frame
    - gravity: speed changes a little each frame, position changes by speed
    - collision between rectangles
    - keeping a high score
    - difficulty that rises with the score

  HOW TO PLAY
    BOOT jumps. Hold it a little longer for a higher jump. Hit an obstacle and
    the game ends; press BOOT to play again.

  The picture is redrawn by wiping and redrawing only the player, the
  obstacles and the score - exactly as in Vga05_Bounce.
*/
#include <LonelyBinaryDisplay.h>

LB_Display display(LB_VGA_800X600);

static const int GROUND = 470;                 // y of the ground line
static const int PX = 140, PSIZE = 44;            // player x and size
static const lb_color_t BG    = LB_RGB(10, 14, 22);
static const lb_color_t TEAL  = LB_RGB(20, 137, 143);
static const lb_color_t BRICK = LB_RGB(230, 90, 70);

static float py, pv;                           // player top y, vertical speed
static int score, best;
static float speed;                            // world speed in pixels per frame
static const int NOBS = 3;
static float ox[NOBS];                         // obstacle x
static int ow[NOBS], oh[NOBS];                 // obstacle size
static bool alive;
static int W;

static void waitFrame() {
  uint32_t f0, f1, d;
  display.scanStats(&f0, &d, &d);
  const uint32_t t0 = millis();
  do { delay(1); display.scanStats(&f1, &d, &d); } while (f1 == f0 && millis() - t0 < 40);
}

static bool pressed() { return digitalRead(0) == LOW; }

static void newObstacle(int i, float after) {
  ow[i] = 24 + random(0, 3) * 12;
  oh[i] = 40 + random(0, 4) * 14;
  ox[i] = after + 320 + random(0, 260) + speed * 14;
}

static void drawScore() {
  char s[40];
  snprintf(s, sizeof s, "score %05d   best %05d", score, best);
  display.setTextSize(3);
  display.setTextColor(LB_WHITE, BG);
  display.drawString(s, 40, 30);
}

static void drawPlayer(float y, bool erase) {
  const int iy = (int)y;
  if (erase) { display.fillRect(PX - 2, iy - 2, PSIZE + 4, PSIZE + 4, BG); return; }
  display.fillRoundRect(PX, iy, PSIZE, PSIZE, 8, TEAL);
  display.fillRect(PX + 26, iy + 10, 8, 12, LB_WHITE);        // an eye
  display.fillRect(PX + 30, iy + 14, 4, 6, LB_BLACK);
}

static void newGame() {
  display.fillScreen(BG);
  display.fillRect(0, GROUND, W, 4, LB_GRAY);
  for (int x = 0; x < W; x += 40) display.fillRect(x, GROUND + 12, 18, 3, LB_DARKGRAY);   // sleepers
  score = 0;
  speed = 6;
  py = GROUND - PSIZE;
  pv = 0;
  float after = 0;
  for (int i = 0; i < NOBS; i++) { newObstacle(i, after); after = ox[i]; }
  alive = true;
  drawScore();
}

void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);
  if (!display.begin()) {
    Serial.println("VGA did not start. Is Tools > PSRAM set to OPI PSRAM?");
    return;
  }
  W = display.width();
  randomSeed(esp_random());
  newGame();
  display.setTextSize(4);
  display.setTextColor(LB_WHITE, BG);
  display.drawString("Press BOOT to jump", 190, 250);
  display.flush();
  while (!pressed()) delay(10);
  while (pressed()) delay(10);
  display.fillRect(0, 230, W, 80, BG);
}

void loop() {
  waitFrame();

  if (!alive) {
    display.setTextSize(5);
    display.setTextColor(BRICK, BG);
    display.drawString("GAME OVER", 250, 200);
    display.setTextSize(2);
    display.setTextColor(LB_WHITE, BG);
    display.drawString("press BOOT to play again", 270, 280);
    display.flush();
    while (pressed()) delay(10);
    while (!pressed()) delay(10);
    while (pressed()) delay(10);
    newGame();
    return;
  }

  // 1 - wipe what moves.
  drawPlayer(py, true);
  for (int i = 0; i < NOBS; i++)
    display.fillRect((int)ox[i] - 2, GROUND - oh[i] - 2, ow[i] + 12, oh[i] + 2, BG);

  // 2 - update. Jump from the ground; holding the button gives a lighter pull.
  const bool onGround = py >= GROUND - PSIZE;
  if (pressed() && onGround) pv = -13.5f;
  pv += pressed() && pv < 0 ? 0.55f : 1.0f;     // gravity, softer while rising with the button held
  py += pv;
  if (py > GROUND - PSIZE) { py = GROUND - PSIZE; pv = 0; }

  for (int i = 0; i < NOBS; i++) {
    ox[i] -= speed;
    if (ox[i] + ow[i] < 0) {
      float far = 0;
      for (int k = 0; k < NOBS; k++) far = max(far, ox[k]);
      newObstacle(i, max(far, (float)W));
      score++;
      best = max(best, score);
      speed += 0.12f;                            // a little faster each time
      drawScore();
    }
    // Collision: do the rectangles overlap? (the player a little smaller than drawn, to be kind)
    if (PX + PSIZE - 6 > ox[i] && PX + 6 < ox[i] + ow[i] && py + PSIZE - 4 > GROUND - oh[i]) alive = false;
  }

  // 3 - draw.
  for (int i = 0; i < NOBS; i++)
    if (ox[i] < W) display.fillRect((int)ox[i], GROUND - oh[i], min(ow[i], W - (int)ox[i]), oh[i], BRICK);
  display.fillRect(0, GROUND, W, 4, LB_GRAY);
  drawPlayer(py, false);
  display.flush();
}
