// Sprout — the hacker scene, with an animatable creature.
//
// Static furniture (status bar, IRC log, matrix, laptop, prompt) is drawn once
// to the screen. The creature is composited into an off-screen sprite every
// frame and pushed, so breathing and blinking are flicker-free.
//
// The creature draws into any LovyanGFX canvas via drawCreature(), taking a
// pixel offset plus animation params (eye height, body bob, chest radius), so
// the same code serves the sprite. Board quirks: see the project memory.

#pragma once
#include <math.h>
#include "LGFX_Sprout.hpp"

namespace scene {

// ---- palette (RGB565) ----
inline uint16_t BG, HOODIE, HOODIE_HI, FACE, GREEN, GREEN_DIM, GREEN_HI,
                AMBER, AMBER_MID, AMBER_DIM, STATUS, CODE_DIM, LAPTOP_C, MATRIX, SCAN;

inline void initPalette(LGFX_Sprout& t) {
  BG        = t.color565(10, 14, 20);
  HOODIE    = t.color565(21, 41, 31);
  HOODIE_HI = t.color565(36, 74, 53);
  FACE      = t.color565(7, 16, 9);
  GREEN     = t.color565(57, 255, 106);
  GREEN_DIM = t.color565(43, 191, 106);
  GREEN_HI  = t.color565(201, 255, 222);
  AMBER     = t.color565(255, 180, 84);
  AMBER_MID = t.color565(185, 125, 52);
  AMBER_DIM = t.color565(110, 72, 28);
  STATUS    = t.color565(95, 211, 160);
  CODE_DIM  = t.color565(31, 143, 80);
  LAPTOP_C  = t.color565(14, 22, 32);
  MATRIX    = t.color565(19, 58, 40);
  SCAN      = t.color565(14, 26, 20);
}

constexpr int CX = 160;   // screen horizontal centre

enum EyeMode { EYE_NORMAL, EYE_WIDE };   // normal = round-rect (blinks); wide = startled
enum Mouth   { MOUTH_SMILE, MOUTH_GRIN, MOUTH_O };

// Creature sprite region (head + torso; lower body hides behind the laptop).
constexpr int SPR_X = 84, SPR_Y = 158, SPR_W = 152, SPR_H = 240;
constexpr int EYE_FULL = 34;   // open eye height

// ---- IRC-style speech log (3 lines, above the creature) ----
constexpr int LOG_X = 10, LOG_Y = 44, LOG_W = 300, LOG_H = 78, LOG_LINES = 3;
inline String logBuf[LOG_LINES];

inline void drawLog(LGFX_Sprout& t) {
  t.fillRect(LOG_X, LOG_Y, LOG_W, LOG_H, BG);
  t.setFont(&fonts::FreeMono9pt7b);
  t.setTextDatum(textdatum_t::top_left);
  for (int i = 0; i < LOG_LINES; i++) {
    if (logBuf[i].isEmpty()) continue;
    int y = LOG_Y + i * 25;
    t.setTextColor(GREEN, BG);
    t.drawString("<sprout>", LOG_X, y);
    t.setTextColor(STATUS, BG);
    t.drawString(logBuf[i].c_str(), LOG_X + 90, y);
  }
}

inline void say(LGFX_Sprout& t, const String& msg) {
  for (int i = 0; i < LOG_LINES - 1; i++) logBuf[i] = logBuf[i + 1];
  logBuf[LOG_LINES - 1] = msg.length() > 17 ? msg.substring(0, 17) : msg;
  drawLog(t);
}

inline void drawStatusBar(LGFX_Sprout& t, const char* clock, const char* wx = "") {
  t.fillRect(0, 0, 320, 34, BG);
  t.setFont(&fonts::FreeMono9pt7b);
  t.setTextColor(STATUS, BG);
  t.setTextDatum(textdatum_t::top_left);
  t.drawString(clock, 12, 10);
  if (wx && wx[0]) {
    t.setTextDatum(textdatum_t::top_right);
    t.setTextColor(CODE_DIM, BG);
    t.drawString(wx, 308, 10);
  }
}

// Is this touch on the creature (head + torso + laptop), not empty screen?
inline bool hitCreature(int x, int y) {
  return x >= 86 && x <= 234 && y >= 160 && y <= 444;
}

inline void drawMatrix(LGFX_Sprout& t) {
  for (int y = 150; y < 440; y += 40) t.drawFastHLine(0, y, 320, SCAN);
  t.setFont(&fonts::Font0);
  t.setTextColor(MATRIX, BG);
  const int xs[] = {14, 40, 284, 306};
  for (int i = 0; i < 4; i++)
    for (int k = 0; k < 5; k++)
      t.drawString(((i + k) & 1) ? "1" : "0", xs[i], 150 + k * 46 + (i * 17) % 40);
}

inline void drawLaptop(LGFX_Sprout& t) {
  t.fillSmoothRoundRect(CX - 44, 398, 88, 36, 5, LAPTOP_C);
  t.drawRoundRect(CX - 44, 398, 88, 36, 5, HOODIE_HI);
  t.fillRoundRect(CX - 36, 404, 72, 24, 2, FACE);
  t.drawFastHLine(CX - 30, 410, 30, GREEN);
  t.drawFastHLine(CX - 30, 416, 46, GREEN_DIM);
  t.drawFastHLine(CX - 30, 422, 22, CODE_DIM);
  t.fillSmoothRoundRect(CX - 52, 434, 104, 9, 2, LAPTOP_C);
  t.drawRoundRect(CX - 52, 434, 104, 9, 2, HOODIE_HI);
}

// Animated "typing" on the laptop screen — vary the code line widths and
// flash an amber cursor. Call periodically while hacking.
inline void typeCode(LGFX_Sprout& t, int step) {
  t.fillRoundRect(CX - 36, 404, 72, 24, 2, FACE);
  uint32_t r = step * 2654435761u;
  int w0 = 12 + (r >> 3) % 36;
  int w1 = 12 + (r >> 9) % 40;
  int w2 =  8 + (r >> 15) % 28;
  t.drawFastHLine(CX - 30, 410, w0, GREEN);
  t.drawFastHLine(CX - 30, 416, w1, GREEN_DIM);
  t.drawFastHLine(CX - 30, 422, w2, CODE_DIM);
  int cx = CX - 30 + ((step & 1) ? w1 : w0) + 3;   // cursor trails a line
  t.fillRect(cx, 415, 5, 8, (step & 1) ? AMBER : FACE);
}

inline int promptCursorX = 148;   // x where the cursor sits, after the prompt

// Shell prompt reflects his status: sprout@hacking:~$, sprout@online:~$, etc.
inline void drawPromptLine(LGFX_Sprout& t, const char* status) {
  t.fillRect(0, 452, 320, 24, BG);
  t.setFont(&fonts::FreeMono9pt7b);
  t.setTextColor(CODE_DIM, BG);
  t.setTextDatum(textdatum_t::top_left);
  String s = String("sprout@") + status + ":~$";
  t.drawString(s.c_str(), 12, 456);
  promptCursorX = 12 + t.textWidth(s.c_str()) + 3;
}

inline void drawCursor(LGFX_Sprout& t, bool on) {
  t.fillRect(promptCursorX, 456, 10, 15, on ? AMBER : BG);
}

// All static furniture (no creature — that comes from the sprite).
inline void drawBackground(LGFX_Sprout& t) {
  t.fillScreen(BG);
  drawMatrix(t);
  drawStatusBar(t, "--:--");
  drawLog(t);
  drawLaptop(t);
  drawPromptLine(t, "online");
}

// ---- the creature, drawn into canvas g at pixel offset ( dx, dy ) ----
//   eyeH    : current eye height (blink)
//   bodyRy  : vertical radius of the torso (breathing squash)
template <typename GFX>
inline void drawCreature(GFX& g, int dx, int dy, int eyeH, int bodyRy,
                         EyeMode eyeMode, Mouth mouth, uint16_t promptCol) {
  int cx = CX + dx;

  // torso
  g.fillEllipse(cx, 372 + dy, 74, bodyRy, HOODIE);
  g.drawEllipse(cx, 372 + dy, 74, bodyRy, HOODIE_HI);

  // hood + face recess
  g.fillEllipse(cx, 222 + dy, 62, 60, HOODIE);
  g.drawEllipse(cx, 222 + dy, 62, 60, HOODIE_HI);
  g.fillEllipse(cx, 230 + dy, 48, 46, FACE);

  // eyes
  if (eyeMode == EYE_WIDE) {
    // startled: big round eyes with a dark pupil
    g.fillCircle(cx - 20, 227 + dy, 14, GREEN);
    g.fillCircle(cx + 20, 227 + dy, 14, GREEN);
    g.fillCircle(cx - 20, 227 + dy, 5, FACE);
    g.fillCircle(cx + 20, 227 + dy, 5, FACE);
    g.fillCircle(cx - 24, 221 + dy, 3, GREEN_HI);
    g.fillCircle(cx + 16, 221 + dy, 3, GREEN_HI);
  } else {
    // normal: round-rects whose height is the blink; centred on y=227
    int ey = 227 - eyeH / 2 + dy;
    int r  = eyeH < 18 ? (eyeH < 4 ? 1 : eyeH / 2) : 9;
    g.fillSmoothRoundRect(cx - 30, ey, 20, eyeH, r, GREEN);
    g.fillSmoothRoundRect(cx + 10, ey, 20, eyeH, r, GREEN);
    if (eyeH > 16) {
      g.fillSmoothRoundRect(cx - 26, ey + 5, 7, 10, 3, GREEN_HI);
      g.fillSmoothRoundRect(cx + 14, ey + 5, 7, 10, 3, GREEN_HI);
    }
  }

  // mouth
  if (mouth == MOUTH_O) {
    g.fillCircle(cx, 264 + dy, 6, GREEN_DIM);
    g.fillCircle(cx, 264 + dy, 3, FACE);
  } else {
    int w = (mouth == MOUTH_GRIN) ? 18 : 12;
    int d = (mouth == MOUTH_GRIN) ? 9 : 6;
    g.drawWideLine(cx - w, 260 + dy, cx, 260 + d + dy, 3, GREEN_DIM);
    g.drawWideLine(cx, 260 + d + dy, cx + w, 260 + dy, 3, GREEN_DIM);
  }

  // drawstrings + LED tips
  g.drawWideLine(cx - 14, 282 + dy, cx - 18, 324 + dy, 3, GREEN_DIM);
  g.drawWideLine(cx + 14, 282 + dy, cx + 18, 324 + dy, 3, GREEN_DIM);
  g.fillCircle(cx - 18, 328 + dy, 4, GREEN);
  g.fillCircle(cx + 18, 328 + dy, 4, GREEN);

  // chest prompt (pulses amber while hacking)
  g.setFont(&fonts::FreeMonoBold9pt7b);
  g.setTextColor(promptCol, HOODIE);
  g.setTextDatum(textdatum_t::middle_center);
  g.drawString(">_", cx, 352 + dy);
}

}  // namespace scene
