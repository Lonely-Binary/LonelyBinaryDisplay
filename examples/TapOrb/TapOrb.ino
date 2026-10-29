/*
  TapOrb - a touch toy for the Tap panels (1.28, 1.5 and 1.96 inch).

  A glowing orb sits in the middle of a dial. Put a finger on the glass and:

    - the orb chases your finger and sprays a trail of colour-cycling sparks
    - the bezel ticks nearest your finger light up, so the ring "looks at" you
    - touching down sends a ripple across the glass
    - TAP        a pulse and a small burst
    - DOUBLE TAP a shockwave from the centre, and the whole palette turns
    - FLICK      sparks fly off the way you flicked, and the palette steps on

  Lift your finger and the orb drifts home. Left alone, a comet circles the
  bezel.

  Change the panel line below to run it on another Tap panel. Taps, double taps
  and flicks are worked out here from the finger's path, so it needs nothing but
  a touch controller. Everything is kept inside a circle, the shape of the 1.28.
*/
#include <LonelyBinaryDisplay.h>
#include <LB_TouchIT7259.h>
#include <LB_TouchCST816.h>
#include <math.h>

// SPI panels only: 40 MHz makes the animation smoother. If the picture tears or
// shows noise on your wiring, drop it back to 20000000.
static const int32_t SPI_HZ = 40000000;

LB_Display display(LB_TAP_196_CTP);
//LB_Display display(LB_TAP_128_CTP);
//LB_Display display(LB_TAP_150_CTP);

static int CX, CY, R;           // the middle and the radius of the circle we draw in; set in setup()
static const int TICKS = 60;

static float baseHue = 200;     // degrees; the palette everything is drawn from
static float huePhase = 0;      // runs on, so the trail cycles through colours

// ── colour ──────────────────────────────────────────────────────────────────

static lb_color_t hsv(float h, float s, float v) {
  h = fmodf(h, 360.0f);
  if (h < 0) h += 360.0f;
  if (v < 0) v = 0;
  if (v > 1) v = 1;
  const float c = v * s, x = c * (1 - fabsf(fmodf(h / 60.0f, 2.0f) - 1)), m = v - c;
  float r = 0, g = 0, b = 0;
  switch ((int)(h / 60.0f)) {
    case 0:  r = c; g = x; break;
    case 1:  r = x; g = c; break;
    case 2:  g = c; b = x; break;
    case 3:  g = x; b = c; break;
    case 4:  r = x; b = c; break;
    default: r = c; b = x; break;
  }
  return LB_RGB((uint8_t)((r + m) * 255), (uint8_t)((g + m) * 255), (uint8_t)((b + m) * 255));
}

// ── sparks ──────────────────────────────────────────────────────────────────

struct Spark { float x, y, vx, vy, life, hue; };
static Spark sparks[110];
static uint8_t nextSpark = 0;

static void spawn(float x, float y, float vx, float vy, float hue) {
  Spark &s = sparks[nextSpark];
  nextSpark = (nextSpark + 1) % (sizeof sparks / sizeof sparks[0]);
  s = {x, y, vx, vy, 1.0f, hue};
}

static void burst(float x, float y, int n, float speed, float hue) {
  for (int i = 0; i < n; i++) {
    const float a = random(0, 6283) / 1000.0f;
    const float v = speed * (0.4f + random(0, 100) / 100.0f);
    spawn(x, y, cosf(a) * v, sinf(a) * v, hue + random(-25, 26));
  }
}

// ── ripples ─────────────────────────────────────────────────────────────────

struct Ripple { float x, y, r, maxR, hue; bool on; };
static Ripple ripples[6];
static uint8_t nextRipple = 0;

static void ripple(float x, float y, float maxR, float hue) {
  ripples[nextRipple] = {x, y, 4, maxR, hue, true};
  nextRipple = (nextRipple + 1) % (sizeof ripples / sizeof ripples[0]);
}

// ── scene ───────────────────────────────────────────────────────────────────

static float orbX = 0, orbY = 0;   // eased towards the finger, or home (set in setup())
static float glow = 0;               // 0 idle .. 1 finger down, eased
static float fingerAngle = 0;

static void drawBezel(float t) {
  const float comet = fmodf(t * 70.0f, 360.0f);   // degrees, idle highlight
  for (int i = 0; i < TICKS; i++) {
    const float deg = i * (360.0f / TICKS);
    const float rad = (deg - 90.0f) * DEG_TO_RAD;

    // The comet: brightest at its head, a tail behind it.
    float behind = comet - deg;
    if (behind < 0) behind += 360.0f;
    float v = 0.14f + (behind < 70.0f ? 0.55f * (1 - behind / 70.0f) : 0) * (1 - glow);

    // The finger: ticks within 55 degrees of it light up.
    float d = fabsf(fmodf(deg - fingerAngle + 540.0f, 360.0f) - 180.0f);
    if (d < 55.0f) {
      const float k = 1 - d / 55.0f;
      v += glow * k * k * 0.9f;
    }

    const bool major = (i % 5) == 0;
    const int r0 = (major ? 100 : 106) * R / 120, r1 = 117 * R / 120;
    const float c = cosf(rad), s = sinf(rad);
    display.drawLine(CX + c * r0, CY + s * r0, CX + c * r1, CY + s * r1,
                     hsv(baseHue + deg * 0.25f, major ? 0.55f : 0.8f, v));
  }
}

static void drawOrb(float t) {
  const float pulse = 0.5f + 0.5f * sinf(t * 2.4f);
  const float radius = (13 + 3 * pulse + 7 * glow) * R / 120.0f;
  const float hue = baseHue + huePhase * 0.3f;
  // Glow: wide dim discs first, a bright core last.
  for (int i = 6; i >= 0; i--) {
    const float f = i / 6.0f;
    const float v = (0.10f + 0.9f * (1 - f)) * (0.55f + 0.45f * pulse * (1 - glow) + 0.45f * glow);
    display.fillCircle((int)orbX, (int)orbY, (int)(radius * (1 + f * 1.5f)),
                       hsv(hue + f * 25, 0.9f - 0.5f * (1 - f) * (1 - f), v));
  }
}

static void drawText() {
  const float v = 0.7f * (1 - glow);
  if (v < 0.05f) return;
  const lb_color_t c = hsv(baseHue, 0.35f, v);
  display.setTextColor(c);
  display.setTextSize(1);
  const char *a = "LONELY BINARY", *b = "TOUCH  ME";
  display.drawString(a, CX - display.textWidth(a) / 2, CY - R * 62 / 120);
  display.drawString(b, CX - display.textWidth(b) / 2, CY + R * 56 / 120);
}

static void drawSparksAndRipples(float dt) {
  for (Ripple &r : ripples) {
    if (!r.on) continue;
    r.r += dt * 150.0f;
    if (r.r >= r.maxR) { r.on = false; continue; }
    const float a = 1 - r.r / r.maxR;
    display.drawCircle((int)r.x, (int)r.y, (int)r.r, hsv(r.hue, 0.7f, a));
    display.drawCircle((int)r.x, (int)r.y, (int)r.r - 2, hsv(r.hue, 0.8f, a * 0.5f));
  }
  for (Spark &s : sparks) {
    if (s.life <= 0) continue;
    s.life -= dt * 0.95f;
    s.x += s.vx * dt * 60.0f;
    s.y += s.vy * dt * 60.0f;
    s.vx *= 0.97f;
    s.vy *= 0.97f;
    const float dx = s.x - CX, dy = s.y - CY;
    if (s.life <= 0 || dx * dx + dy * dy > (R - 4) * (R - 4)) { s.life = 0; continue; }
    display.fillCircle((int)s.x, (int)s.y, 1 + (int)(s.life * 2.2f), hsv(s.hue, 0.85f, s.life));
  }
}

// ── main ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  display.setSpiHz(SPI_HZ);
  if (!display.begin(true)) {
    Serial.println("Display did not start - check the wiring.");
    while (true) delay(1000);
  }
  display.printInfo();
  if (!display.touch())
    Serial.println("No touch: the touch controller did not answer on SDA/SCL.");
  CX = display.width() / 2;
  CY = display.height() / 2;
  R = min(display.width(), display.height()) / 2;
  orbX = CX; orbY = CY;
}

// What the finger just did, worked out from its path.
enum Gesture : uint8_t { G_NONE, G_TAP, G_DOUBLE_TAP, G_FLICK_UP, G_FLICK_RIGHT, G_FLICK_DOWN, G_FLICK_LEFT };

void loop() {
  static uint32_t last = millis(), fpsAt = millis(), frames = 0;
  static bool wasTouching = false;
  static int16_t px = 0, py = 0, lx = 0, ly = 0, sx = 0, sy = 0;
  static uint32_t downAt = 0, lastTapAt = 0;
  static float fvx = 0, fvy = 0;                // finger speed, px/s

  const uint32_t now = millis();
  const float dt = min(0.08f, (now - last) / 1000.0f);
  const float t = now / 1000.0f;
  last = now;
  huePhase += dt * 80.0f;

  LB_Touch *touch = display.touch();
  int16_t x = 0, y = 0;
  const bool touching = touch && touch->getTouch(&x, &y);
  Gesture gesture = G_NONE;

  if (touching) {
    if (!wasTouching) {                       // finger down
      px = lx = sx = x; py = ly = sy = y;
      downAt = now;
      fvx = fvy = 0;
      ripple(x, y, 80 * R / 120.0f, baseHue + huePhase);
    }
    const float dx = x - px, dy = y - py;
    fvx = 0.6f * fvx + 0.4f * (x - lx) / max(dt, 0.001f);
    fvy = 0.6f * fvy + 0.4f * (y - ly) / max(dt, 0.001f);
    const int n = 1 + min(5, (int)(sqrtf(dx * dx + dy * dy) / 3));
    for (int i = 0; i < n; i++) {
      const float a = random(0, 6283) / 1000.0f, v = 0.3f + random(0, 120) / 100.0f;
      spawn(orbX, orbY, cosf(a) * v + dx * 0.12f, sinf(a) * v + dy * 0.12f,
            baseHue + huePhase + random(-20, 21));
    }
    fingerAngle = atan2f(y - CY, x - CX) * RAD_TO_DEG + 90.0f;   // 0 = top, like the ticks
    if (fingerAngle < 0) fingerAngle += 360.0f;
    px = lx = x; py = ly = y;
  } else if (wasTouching) {                    // finger up: what was it?
    const float moved = sqrtf((float)((lx - sx) * (lx - sx) + (ly - sy) * (ly - sy)));
    if (now - downAt < 300 && moved < 14) {
      if (now - lastTapAt < 350) { gesture = G_DOUBLE_TAP; lastTapAt = 0; }
      else { gesture = G_TAP; lastTapAt = now; }
    } else if (moved > 25 && sqrtf(fvx * fvx + fvy * fvy) > 350) {
      if (fabsf(fvx) > fabsf(fvy)) gesture = fvx > 0 ? G_FLICK_RIGHT : G_FLICK_LEFT;
      else gesture = fvy > 0 ? G_FLICK_DOWN : G_FLICK_UP;
    }
  }
  wasTouching = touching;

  switch (gesture) {
    case G_TAP:
      ripple(lx, ly, 100 * R / 120.0f, baseHue + huePhase + 60);
      burst(lx, ly, 14, 2.2f, baseHue + huePhase);
      break;
    case G_DOUBLE_TAP:
      baseHue += 120;
      ripple(CX, CY, 150 * R / 120.0f, baseHue);
      ripple(CX, CY, 110 * R / 120.0f, baseHue + 40);
      burst(lx, ly, 40, 3.2f, baseHue);
      break;
    case G_FLICK_UP:    baseHue += 45; for (int i = 0; i < 30; i++) spawn(lx, ly, random(-80, 81) / 40.0f, -2.0f - random(0, 300) / 100.0f, baseHue + random(-20, 21)); break;
    case G_FLICK_DOWN:  baseHue += 45; for (int i = 0; i < 30; i++) spawn(lx, ly, random(-80, 81) / 40.0f,  2.0f + random(0, 300) / 100.0f, baseHue + random(-20, 21)); break;
    case G_FLICK_LEFT:  baseHue += 45; for (int i = 0; i < 30; i++) spawn(lx, ly, -2.0f - random(0, 300) / 100.0f, random(-80, 81) / 40.0f, baseHue + random(-20, 21)); break;
    case G_FLICK_RIGHT: baseHue += 45; for (int i = 0; i < 30; i++) spawn(lx, ly,  2.0f + random(0, 300) / 100.0f, random(-80, 81) / 40.0f, baseHue + random(-20, 21)); break;
    default: break;
  }

  // Ease the orb and the glow towards where the finger is, or home.
  const float tx = touching ? x : CX, ty = touching ? y : CY;
  orbX += (tx - orbX) * min(1.0f, dt * 14.0f);
  orbY += (ty - orbY) * min(1.0f, dt * 14.0f);
  glow += ((touching ? 1.0f : 0.0f) - glow) * min(1.0f, dt * 8.0f);

  display.fillScreen(LB_BLACK);
  drawSparksAndRipples(dt);
  drawBezel(t);
  drawOrb(t);
  drawText();
  display.flush();

  frames++;
  if (now - fpsAt >= 5000) {
    Serial.printf("%lu fps\n", (unsigned long)(frames * 1000 / (now - fpsAt)));
    frames = 0;
    fpsAt = now;
  }
}
