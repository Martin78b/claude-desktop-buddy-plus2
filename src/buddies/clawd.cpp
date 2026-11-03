#include "../buddy.h"
#include "../buddy_common.h"
#include "../hal.h"
#include <string.h>

extern TFT_eSprite spr;

// Clawd — the orange pixel critter from Claude Code's startup banner.
// Unlike the ASCII species this one is drawn as pixel art with filled
// rects (buddyFillPx), on a 16x9 grid of 4px cells at 1x:
//
//   ..XXXXXXXXXXXX..   rows 0-6: body (cols 2-13)
//   ..XXXXXXXXXXXX..   eyes: cols 4 and 11, rows 2-3
//   ..XX.XXXXXX.XX..   arms: cols 0-1 / 14-15, rows 3-4
//   XXXX.XXXXXX.XXXX
//   XXXXXXXXXXXXXXXX
//   ..XXXXXXXXXXXX..
//   ..XXXXXXXXXXXX..
//   ...X.X....X.X...   rows 7-8: legs (cols 3, 5, 10, 12)
//   ...X.X....X.X...
//
// Every pose is a small parameter set, so the seven states below are
// just sequences of Pose values plus particle overlays.

namespace clawd {

static const uint16_t ORANGE = 0xDBAA;   // #D97757
static const uint16_t BLUSH  = 0xFB2C;   // soft pink
static const int U = 4;                  // grid cell, 1x px
static const int GROUND = 9 * U;         // feet rest here (dy=0)

enum Eyes : uint8_t { E_OPEN, E_LOOK_L, E_LOOK_R, E_BLINK, E_CLOSED, E_HAPPY,
                      E_BIG, E_DOWN, E_X, E_HEART, E_WINK };
enum Arm  : uint8_t { A_NORM, A_UP, A_HIGH, A_DOWN, A_TYPE, A_OUT };
enum Legs : uint8_t { L_STAND, L_WALK_A, L_WALK_B, L_SPLAY, L_TAP };

struct Pose {
  uint8_t eyes, armL, armR, legs;
  int8_t  dx, dy;          // 1x px; dy>0 sinks (crouch/loaf), dy<0 jumps
  bool    blush;
};

static inline int cx(int col) { return (col - 8) * U; }

static void cell(int col, int row, int ox, int oy, uint16_t c) {
  buddyFillPx(cx(col) + ox, row * U + oy, U, U, c);
}

static void drawArm(bool left, uint8_t a, int ox, int oy) {
  // Mirror: left arm uses cols 0-1, right arm cols 14-15.
  int outer = left ? 0 : 15, inner = left ? 1 : 14;
  switch (a) {
    case A_NORM: for (int r = 3; r <= 4; r++) { cell(outer, r, ox, oy, ORANGE); cell(inner, r, ox, oy, ORANGE); } break;
    case A_UP:   cell(inner, 3, ox, oy, ORANGE); cell(inner, 2, ox, oy, ORANGE);
                 cell(outer, 2, ox, oy, ORANGE); cell(outer, 1, ox, oy, ORANGE); break;
    case A_HIGH: cell(inner, 2, ox, oy, ORANGE); cell(outer, 1, ox, oy, ORANGE);
                 cell(outer, 0, ox, oy, ORANGE); cell(outer, -1, ox, oy, ORANGE); break;
    case A_DOWN: cell(inner, 4, ox, oy, ORANGE); cell(inner, 5, ox, oy, ORANGE);
                 cell(outer, 5, ox, oy, ORANGE); cell(outer, 6, ox, oy, ORANGE); break;
    case A_TYPE: cell(inner, 4, ox, oy, ORANGE); cell(inner, 5, ox, oy, ORANGE); break;
    case A_OUT:  cell(inner, 3, ox, oy, ORANGE); cell(outer, 3, ox, oy, ORANGE);
                 cell(outer, 2, ox, oy, ORANGE); break;
  }
}

static void drawEye(int col, uint8_t e, int ox, int oy, bool rightEye) {
  int ex = cx(col) + ox, ey = 2 * U + oy;     // eye cell top-left (4x8)
  const uint16_t K = BUDDY_BG;
  switch (e) {
    case E_OPEN:   buddyFillPx(ex, ey, 4, 8, K); break;
    case E_LOOK_L: buddyFillPx(ex - 4, ey, 4, 8, K); break;
    case E_LOOK_R: buddyFillPx(ex + 4, ey, 4, 8, K); break;
    case E_BLINK:  buddyFillPx(ex, ey + 4, 4, 3, K); break;
    case E_CLOSED: buddyFillPx(ex - 2, ey + 5, 8, 2, K); break;
    case E_DOWN:   buddyFillPx(ex, ey + 3, 4, 6, K); break;
    case E_BIG:    buddyFillPx(ex - 1, ey - 1, 6, 10, K); break;
    case E_HAPPY:  // ^
      buddyFillPx(ex - 2, ey + 4, 2, 3, K);
      buddyFillPx(ex,     ey + 1, 4, 3, K);
      buddyFillPx(ex + 4, ey + 4, 2, 3, K);
      break;
    case E_X:      // X from 2px dots
      buddyFillPx(ex - 1, ey,     2, 2, K); buddyFillPx(ex + 3, ey,     2, 2, K);
      buddyFillPx(ex + 1, ey + 2, 2, 3, K);
      buddyFillPx(ex - 1, ey + 5, 2, 2, K); buddyFillPx(ex + 3, ey + 5, 2, 2, K);
      break;
    case E_HEART:  // tiny pink heart
      buddyFillPx(ex - 2, ey + 1, 3, 2, BUDDY_HEART); buddyFillPx(ex + 3, ey + 1, 3, 2, BUDDY_HEART);
      buddyFillPx(ex - 2, ey + 3, 8, 2, BUDDY_HEART);
      buddyFillPx(ex - 1, ey + 5, 6, 1, BUDDY_HEART);
      buddyFillPx(ex,     ey + 6, 4, 1, BUDDY_HEART);
      buddyFillPx(ex + 1, ey + 7, 2, 1, BUDDY_HEART);
      break;
    case E_WINK:
      if (rightEye) buddyFillPx(ex - 2, ey + 5, 8, 2, K);
      else          buddyFillPx(ex, ey, 4, 8, K);
      break;
  }
}

static void drawLegs(uint8_t l, int ox, int dy) {
  // On the ground the legs shrink as the body sinks; airborne they hang
  // at full length and move with the body.
  int len = dy < 0 ? 2 * U : (2 * U - dy);
  if (len <= 0) return;
  int top = 7 * U + dy;
  static const int COLS[4] = { 3, 5, 10, 12 };
  for (int i = 0; i < 4; i++) {
    int c = COLS[i], h = len, x = cx(c) + ox;
    if (l == L_SPLAY) { x += (i < 2 ? -U : U) * (i == 0 || i == 3 ? 1 : 0); h = U; }
    if (l == L_WALK_A && (i == 1 || i == 3)) h = len - U;
    if (l == L_WALK_B && (i == 0 || i == 2)) h = len - U;
    if (l == L_TAP && i == 3) h = len - U;
    if (h > 0) buddyFillPx(x, top, U, h, ORANGE);
  }
}

static void draw(const Pose& p) {
  int ox = p.dx, oy = p.dy;
  buddyFillPx(cx(2) + ox, oy, 12 * U, 7 * U, ORANGE);       // body
  drawArm(true,  p.armL, ox, oy);
  drawArm(false, p.armR, ox, oy);
  drawLegs(p.legs, ox, oy);
  drawEye(4,  p.eyes, ox, oy, false);
  drawEye(11, p.eyes, ox, oy, true);
  if (p.blush) {
    buddyFillPx(cx(3) + ox, 4 * U + oy + 2, 4, 2, BLUSH);
    buddyFillPx(cx(12) + ox, 4 * U + oy + 2, 4, 2, BLUSH);
  }
}

// Small pixel heart, 7x6 px at 1x, top-left at (dx, dy) sprite-relative.
static void pixelHeart(int dx, int dy, uint16_t c) {
  buddyFillPx(dx,     dy,     3, 2, c); buddyFillPx(dx + 4, dy, 3, 2, c);
  buddyFillPx(dx,     dy + 2, 7, 2, c);
  buddyFillPx(dx + 1, dy + 4, 5, 1, c);
  buddyFillPx(dx + 2, dy + 5, 3, 1, c);
  buddyFillPx(dx + 3, dy + 6, 1, 1, c);
}

// ─── SLEEP ───  ~14s cycle: loafs down, breathes, rolls an eye open
static void doSleep(uint32_t t) {
  static const Pose P[] = {
    { E_CLOSED, A_DOWN, A_DOWN, L_STAND, 0, 8, false },   // 0 loaf
    { E_CLOSED, A_DOWN, A_DOWN, L_STAND, 0, 7, false },   // 1 inhale
    { E_CLOSED, A_TYPE, A_TYPE, L_STAND, 0, 8, false },   // 2 arms tucked
    { E_WINK,   A_DOWN, A_DOWN, L_STAND, 0, 8, false },   // 3 one eye peeks
    { E_CLOSED, A_DOWN, A_DOWN, L_STAND, 0, 8, true  },   // 4 dreaming (blush)
  };
  static const uint8_t SEQ[] = { 0,1,0,1,0,1, 2,2,0,1, 0,1,4,4,4,1, 0,1,0,3,0, 1,0,1,2,2 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  draw(P[SEQ[beat]]);

  int p1 = t % 12, p2 = (t + 5) % 12, p3 = (t + 9) % 12;
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 22 + p1, BUDDY_Y_OVERLAY + 18 - p1 * 2);
  buddyPrint("z");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 28 + p2, BUDDY_Y_OVERLAY + 14 - p2);
  buddyPrint("Z");
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 16 + p3 / 2, BUDDY_Y_OVERLAY + 10 - p3 / 2);
  buddyPrint("z");
}

// ─── IDLE ───  ~38s cycle: blinks, glances, little walks, a wave, a hop
static void doIdle(uint32_t t) {
  static const Pose P[] = {
    { E_OPEN,   A_NORM, A_NORM, L_STAND,  0,  0, false },  // 0 rest
    { E_BLINK,  A_NORM, A_NORM, L_STAND,  0,  0, false },  // 1 blink
    { E_LOOK_L, A_NORM, A_NORM, L_STAND,  0,  0, false },  // 2 look left
    { E_LOOK_R, A_NORM, A_NORM, L_STAND,  0,  0, false },  // 3 look right
    { E_LOOK_L, A_NORM, A_NORM, L_WALK_A,-1,  0, false },  // 4 step left
    { E_LOOK_L, A_NORM, A_NORM, L_WALK_B,-2,  0, false },  // 5 step left
    { E_LOOK_R, A_NORM, A_NORM, L_WALK_A, 1,  0, false },  // 6 step right
    { E_LOOK_R, A_NORM, A_NORM, L_WALK_B, 2,  0, false },  // 7 step right
    { E_HAPPY,  A_NORM, A_UP,   L_STAND,  0,  0, false },  // 8 wave up
    { E_HAPPY,  A_NORM, A_OUT,  L_STAND,  0,  0, false },  // 9 wave out
    { E_OPEN,   A_NORM, A_NORM, L_STAND,  0,  2, false },  // 10 crouch
    { E_OPEN,   A_UP,   A_UP,   L_SPLAY,  0, -4, false },  // 11 hop
    { E_OPEN,   A_NORM, A_NORM, L_TAP,    0,  0, false },  // 12 foot tap
    { E_DOWN,   A_NORM, A_NORM, L_STAND,  0,  0, false },  // 13 look down
  };
  static const uint8_t SEQ[] = {
    0,0,0,1,0,2,0,3,0,
    12,0,12,0,
    4,5,4,5,0, 6,7,6,7,0,
    8,9,8,9,0,
    0,13,13,0,1,
    10,11,10,0,
    2,3,0,1,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  draw(P[SEQ[beat]]);
}

// ─── BUSY ───  typing furiously, Claude-style spinner + dot ticker
static void doBusy(uint32_t t) {
  static const Pose P[] = {
    { E_DOWN,   A_TYPE, A_NORM, L_STAND, 0, 0, false },   // 0 tap L
    { E_DOWN,   A_NORM, A_TYPE, L_STAND, 0, 0, false },   // 1 tap R
    { E_DOWN,   A_TYPE, A_TYPE, L_STAND, 0, 1, false },   // 2 both
    { E_LOOK_R, A_NORM, A_UP,   L_STAND, 0, 0, false },   // 3 thinking
    { E_BLINK,  A_TYPE, A_TYPE, L_STAND, 0, 1, false },   // 4 focus blink
  };
  static const uint8_t SEQ[] = { 0,1,0,1,0,1,2, 0,1,0,1,4, 3,3, 0,1,0,1,2,2 };
  uint8_t beat = (t / 2) % sizeof(SEQ);    // faster: frantic typing
  draw(P[SEQ[beat]]);

  // Spinner glyph cycling like Claude Code's status line.
  static const char* const SPIN[] = { ".", "+", "x", "*", "x", "+" };
  buddySetColor(ORANGE);
  buddySetCursor(BUDDY_X_CENTER + 26, BUDDY_Y_OVERLAY + 8);
  buddyPrint(SPIN[t % 6]);

  static const char* const DOTS[] = { ".  ", ".. ", "...", " ..", "  .", "   " };
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER - 34, BUDDY_Y_OVERLAY + 14);
  buddyPrint(DOTS[(t / 2) % 6]);
}

// ─── ATTENTION ───  wide eyes, arms flailing, bouncing on its toes
static void doAttention(uint32_t t) {
  static const Pose P[] = {
    { E_BIG,    A_UP,   A_UP,   L_STAND, 0,  0, false },  // 0 alarm
    { E_BIG,    A_HIGH, A_UP,   L_STAND, 0, -2, false },  // 1 wave L
    { E_BIG,    A_UP,   A_HIGH, L_STAND, 0, -2, false },  // 2 wave R
    { E_LOOK_L, A_NORM, A_HIGH, L_TAP,   0,  0, false },  // 3 "over here"
    { E_LOOK_R, A_HIGH, A_NORM, L_TAP,   0,  0, false },  // 4 "over here"
    { E_BIG,    A_HIGH, A_HIGH, L_SPLAY, 0, -5, false },  // 5 jump
  };
  static const uint8_t SEQ[] = { 0,1,2,1,2,0, 5,0, 3,3,4,4, 1,2,5,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  Pose p = P[pose];
  if (pose == 0) p.dx = (t & 1) ? 1 : -1;  // shiver
  draw(p);

  if ((t / 2) & 1) {
    buddySetColor(BUDDY_YEL);
    buddySetCursor(BUDDY_X_CENTER - 4, BUDDY_Y_OVERLAY - 4);
    buddyPrint("!");
  }
  if ((t / 3) & 1) {
    buddySetColor(BUDDY_RED);
    buddySetCursor(BUDDY_X_CENTER + 10, BUDDY_Y_OVERLAY);
    buddyPrint("!");
  }
}

// ─── CELEBRATE ───  crouch, launch, airborne with arms high, land, repeat
static void doCelebrate(uint32_t t) {
  static const Pose P[] = {
    { E_HAPPY, A_NORM, A_NORM, L_STAND, 0,   3, false },  // 0 crouch
    { E_HAPPY, A_UP,   A_UP,   L_SPLAY, 0,  -6, false },  // 1 launch
    { E_HAPPY, A_HIGH, A_HIGH, L_SPLAY, 0, -12, false },  // 2 apex
    { E_HAPPY, A_HIGH, A_HIGH, L_STAND, 0,   0, true  },  // 3 landed pose
    { E_WINK,  A_NORM, A_HIGH, L_STAND, 0,   0, true  },  // 4 wink + fist
    { E_HAPPY, A_HIGH, A_HIGH, L_WALK_A,-2,  0, false },  // 5 dance L
    { E_HAPPY, A_HIGH, A_HIGH, L_WALK_B, 2,  0, false },  // 6 dance R
  };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 0,1,2,1,0, 5,6,5,6, 3,3,4,4 };
  uint8_t beat = (t / 2) % sizeof(SEQ);
  draw(P[SEQ[beat]]);

  static const uint16_t cols[] = { BUDDY_YEL, BUDDY_HEART, BUDDY_CYAN, ORANGE, BUDDY_GREEN };
  for (int i = 0; i < 6; i++) {
    int phase = (t * 2 + i * 11) % 22;
    int x = BUDDY_X_CENTER - 36 + i * 14;
    int y = BUDDY_Y_OVERLAY - 6 + phase;
    if (y > BUDDY_Y_BASE + 20 || y < 0) continue;
    buddySetColor(cols[i % 5]);
    buddySetCursor(x, y);
    buddyPrint(((i + (int)(t / 2)) & 1) ? "*" : "+");
  }
}

// ─── DIZZY ───  X eyes, swaying, stars orbiting, eventually flops
static void doDizzy(uint32_t t) {
  static const Pose P[] = {
    { E_X,      A_DOWN, A_UP,   L_WALK_A, -2, 0, false },  // 0 sway L
    { E_X,      A_UP,   A_DOWN, L_WALK_B,  2, 0, false },  // 1 sway R
    { E_CLOSED, A_OUT,  A_OUT,  L_STAND,   0, 1, false },  // 2 steady...
    { E_X,      A_DOWN, A_DOWN, L_STAND,   0, 6, false },  // 3 flop
  };
  static const uint8_t SEQ[] = { 0,1,0,1, 2, 0,1,0,1, 3,3,3, 2 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  draw(P[SEQ[beat]]);

  static const int8_t OX[] = { 0, 6, 9, 6, 0, -6, -9, -6 };
  static const int8_t OY[] = { -5, -3, 0, 3, 5, 3, 0, -3 };
  uint8_t p1 = t % 8, p2 = (t + 4) % 8, p3 = (t + 2) % 8;
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + OX[p1] - 2, BUDDY_Y_OVERLAY + 10 + OY[p1]);
  buddyPrint("*");
  buddySetColor(BUDDY_YEL);
  buddySetCursor(BUDDY_X_CENTER + OX[p2] - 2, BUDDY_Y_OVERLAY + 10 + OY[p2]);
  buddyPrint("*");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + OX[p3] - 2, BUDDY_Y_OVERLAY + 10 + OY[p3]);
  buddyPrint("o");
}

// ─── HEART ───  heart eyes, blushing, swaying, pixel hearts floating up
static void doHeart(uint32_t t) {
  static const Pose P[] = {
    { E_HEART, A_NORM, A_NORM, L_STAND, 0,  0, true },   // 0 smitten
    { E_HEART, A_NORM, A_NORM, L_STAND, 0, -1, true },   // 1 bob
    { E_HAPPY, A_TYPE, A_TYPE, L_STAND,-2,  0, true },   // 2 shy sway L
    { E_HAPPY, A_TYPE, A_TYPE, L_STAND, 2,  0, true },   // 3 shy sway R
    { E_CLOSED,A_UP,   A_UP,   L_STAND, 0,  0, true },   // 4 hug the air
  };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3,2,3, 0,1,0,1, 4,4, 2,3 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  draw(P[SEQ[beat]]);

  // Pixel hearts rise from behind the head. Sprite-relative coords, so
  // negative dy is above the body top.
  for (int i = 0; i < 3; i++) {
    int phase = (t + i * 6) % 18;
    int dy = -4 - phase;
    if (dy < -22) continue;
    int dx = -22 + i * 18 + (((phase / 3) & 1) ? 1 : -1);
    pixelHeart(dx, dy, BUDDY_HEART);
  }
}

}  // namespace clawd

extern const Species CLAWD_SPECIES = {
  "clawd",
  clawd::ORANGE,
  { clawd::doSleep, clawd::doIdle, clawd::doBusy, clawd::doAttention,
    clawd::doCelebrate, clawd::doDizzy, clawd::doHeart }
};
