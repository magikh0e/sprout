// Sprout — Step 5: day/night + hacking mode on the CYD 3.5".
//
// Behaviour:
//   Day   : idle (breathe + blink). Left alone a while -> slips into hacking
//           mode (focused squint, leans at the laptop, types, amber LED).
//           Poke -> happy; rapid pokes -> startled.
//   Night : always hacking (night-owl). Disturb his session -> annoyed.
//
// Time comes from time_src.hpp (demo-fast by default; NTP when Wi-Fi creds are
// set in config.h). Board quirks: backlight 27, RGB active-LOW G=17/B=16,
// speaker 26, touch calibrated. See project memory.

#include <Arduino.h>
#include <math.h>
#include "LGFX_Sprout.hpp"
#include "scene.hpp"
#include "time_src.hpp"
#include "store.hpp"
#include "weather.hpp"

#define TOUCH_TEST 0   // 1 = bare init + touch poll only (isolation); 0 = full app

LGFX_Sprout tft;
LGFX_Sprite creature(&tft);

constexpr int PIN_RGB_R = 4;
constexpr int PIN_RGB_G = 17;
constexpr int PIN_RGB_B = 16;
constexpr int PIN_SPK   = 26;
constexpr int SPK_CH    = 0;
constexpr uint32_t HACK_AFTER = 60000;   // daytime: only hacks after a minute left alone

void rgbWrite(bool r, bool g, bool b) {
  digitalWrite(PIN_RGB_R, r ? LOW : HIGH);
  digitalWrite(PIN_RGB_G, g ? LOW : HIGH);
  digitalWrite(PIN_RGB_B, b ? LOW : HIGH);
}
void chirp(int f1, int f2) {
  ledcWriteTone(SPK_CH, f1); delay(35);
  ledcWriteTone(SPK_CH, f2); delay(35);
  ledcWriteTone(SPK_CH, 0);
}

// --- chatter (all <=17 chars for the IRC line) ---
const char* idleLines[]    = {"nothing to see ;)", "stay frosty", "sipping coffee", "all quiet here"};
const char* happyLines[]   = {"hey! :)", "boop detected", "that tickles", "hi there o/"};
const char* hackLines[]    = {"compiling...", "nmap -sV lan", "sudo make me", "0xdeadbeef",
                              "access: ok", "bruteforce...", "// TODO: sleep", "> in the matrix"};
const char* disturbLines[] = {"i'm working!", "do not disturb", "shhh, hacking", "quit it >:(",
                              "mid-exploit!", "lost my shell >:("};
const int idleCount    = sizeof(idleLines)    / sizeof(idleLines[0]);
const int happyCount   = sizeof(happyLines)   / sizeof(happyLines[0]);
const int hackCount    = sizeof(hackLines)    / sizeof(hackLines[0]);
const int disturbCount = sizeof(disturbLines) / sizeof(disturbLines[0]);

// --- state ---
enum Mood { IDLE, HAPPY, STARTLED, HACKING };
uint32_t lastFrame = 0, lastCursor = 0, nextSay = 0, nextType = 0;
uint32_t nextBlink = 0, blinkStart = 0, lastInteraction = 0;
bool blinking = false, cursorOn = true, wasTouched = false;
int sayIdx = 0, typeStep = 0;

Mood     mood = IDLE, prevMood = IDLE;
uint32_t reactStart = 0, reactDur = 0, pokeWindow = 0, rgbRevertAt = 0;
int      pokeCount = 0;
float    sleepy = 0.0f;         // 0 day .. 1 deep night
char     lastClock[6] = "";
int      curBrightness = -1;
int      lastHacking = -1;      // status-bar indicator state (-1 = unset)
char     wxStr[16] = "";        // weather string for the status bar
uint32_t nextWx = 0;            // next weather fetch

bool isNight()   { return sleepy > 0.5f; }
bool inSession() { return mood == HACKING || isNight(); }   // hacking "zone"

void onPoke() {
  uint32_t now = millis();
  lastInteraction = now;

  store::addPoke();

  if (inSession()) {
    // disturbing the hacking session -> annoyed, even a single poke at night
    mood = STARTLED; reactStart = now; reactDur = 850;
    scene::say(tft, disturbLines[random(disturbCount)]);
    rgbWrite(true, false, false); rgbRevertAt = now + 500;
    chirp(1500, 700);
    return;
  }

  // daytime idle: gentle boop, rapid pokes escalate
  pokeCount = (now < pokeWindow) ? pokeCount + 1 : 1;
  pokeWindow = now + 900;
  if (pokeCount >= 3) {
    mood = STARTLED; reactStart = now; reactDur = 850;
    scene::say(tft, disturbLines[random(disturbCount)]);
    rgbWrite(true, false, false); rgbRevertAt = now + 500;
    chirp(1500, 700);
  } else {
    mood = HAPPY; reactStart = now; reactDur = 650;
    scene::say(tft, happyLines[random(happyCount)]);
    rgbWrite(true, true, true); rgbRevertAt = now + 350;
    chirp(1400, 2200);
  }
}

void setup() {
  Serial.begin(115200);
  delay(150);
  randomSeed(micros());

  pinMode(PIN_RGB_R, OUTPUT);
  pinMode(PIN_RGB_G, OUTPUT);
  pinMode(PIN_RGB_B, OUTPUT);
  ledcSetup(SPK_CH, 2000, 8);
  ledcAttachPin(PIN_SPK, SPK_CH);
  ledcWriteTone(SPK_CH, 0);

  tft.init();
  tft.setRotation(0);
  uint16_t touchCal[8] = {3628, 3681, 3566, 308, 588, 3560, 504, 381};
  tft.setTouchCalibrate(touchCal);

#if TOUCH_TEST
  tft.setBrightness(200);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("TOUCH TEST - mash me", 20, 40, 4);
  Serial.println("[touchtest] ready");
  return;
#endif

  store::begin();
  scene::initPalette(tft);
  scene::drawBackground(tft);
  if (store::boots <= 1) {
    scene::say(tft, "hey, i'm sprout");
  } else {
    scene::say(tft, ("back! boot #" + String(store::boots)).c_str());
  }

  creature.setColorDepth(16);
  if (!creature.createSprite(scene::SPR_W, scene::SPR_H))
    Serial.println("[Sprout] sprite alloc failed");

  timesrc::begin();
  if (weather::fetch()) {                    // first pull
    weather::statusStr(wxStr, sizeof(wxStr));
    scene::say(tft, weather::line(weather::cond));
  }
  nextWx = millis() + 1200000UL;             // refresh every 20 min
  lastInteraction = millis();
  nextBlink = millis() + random(2500, 6000);
  nextSay   = millis() + random(270000, 330000);   // ~1 random line / 5 min
  Serial.println("[Sprout] alive");
}

void loop() {
  uint32_t now = millis();

#if TOUCH_TEST
  int32_t x, y;
  if (tft.getTouch(&x, &y)) tft.fillCircle(x, y, 5, TFT_CYAN);   // calibrated
  delay(8);
  return;
#endif

  // time + day/night
  float hour = timesrc::hourOfDay();
  sleepy = timesrc::sleepiness(hour);

  // --- touch (only on his body) ---
  int32_t tx, ty;
  bool touched = tft.getTouch(&tx, &ty);
  if (touched && !wasTouched && scene::hitCreature(tx, ty)) onPoke();
  wasTouched = touched;

  // --- RGB flash revert ---
  if (rgbRevertAt && now >= rgbRevertAt) rgbRevertAt = 0;

  // --- resolve mood when no transient reaction is playing ---
  bool reacting = (mood == HAPPY || mood == STARTLED) && now < reactStart + reactDur;
  if (!reacting) {
    if (isNight())                                  mood = HACKING;
    else if (now - lastInteraction > HACK_AFTER)    mood = HACKING;
    else                                            mood = IDLE;
  }

  // entering hacking announces itself once
  if (mood == HACKING && prevMood != HACKING) {
    scene::say(tft, isNight() ? "> night shift" : "> getting to work");
    nextType = now;
  }
  if (mood != prevMood) Serial.printf("[mood] %d (hour=%.1f sleepy=%.2f)\n", mood, hour, sleepy);
  prevMood = mood;

  // --- idle LED (when not flashing) ---
  if (!rgbRevertAt) {
    if (mood == HACKING) rgbWrite(true, true, false);   // amber
    else                 rgbWrite(false, true, false);  // green
  }

  // --- backlight: a touch dimmer at night ---
  int bright = 200 - (int)(sleepy * 70);
  if (bright != curBrightness) { tft.setBrightness(bright); curBrightness = bright; }

  // --- weather refresh (every 20 min); comment on a change ---
  if (now >= nextWx) {
    nextWx = now + 1200000UL;
    if (weather::fetch()) {
      weather::statusStr(wxStr, sizeof(wxStr));
      lastClock[0] = '\0';                       // force status redraw
      if (mood != STARTLED) scene::say(tft, weather::line(weather::cond));
    }
  }

  // --- status bar: live clock + weather + hacking/online indicator ---
  char clk[6]; timesrc::clockStr(hour, clk);
  int hacking = inSession() ? 1 : 0;
  if (strcmp(clk, lastClock) != 0 || hacking != lastHacking) {
    strcpy(lastClock, clk);
    lastHacking = hacking;
    scene::drawStatusBar(tft, clk, hacking, wxStr);
  }

  // --- laptop typing while hacking ---
  if (mood == HACKING && now >= nextType) {
    scene::typeCode(tft, typeStep++);
    nextType = now + 180;
  }

  // --- render ~33 fps ---
  if (now - lastFrame >= 33) {
    lastFrame = now;

    int period = 3600 + (int)(sleepy * 1500);   // breathe a little slower at night
    float breath = sinf((now % period) / (float)period * 2.0f * PI);
    int bob    = -(int)roundf(breath * 2.0f);
    int bodyRy = 62 + (int)roundf(breath * 2.0f);
    int eyeH   = scene::EYE_FULL;
    scene::EyeMode eyeMode = scene::EYE_NORMAL;
    scene::Mouth   mouth   = scene::MOUTH_SMILE;
    uint16_t promptCol = scene::GREEN;

    if (mood == IDLE) {
      if (!blinking && now >= nextBlink) { blinking = true; blinkStart = now; }
      if (blinking) {
        float p = (now - blinkStart) / 150.0f;
        if (p >= 1.0f) { blinking = false; nextBlink = now + random(2500, 6000); }
        else { float o = fabsf(1.0f - 2.0f * p); eyeH = max(3, (int)(scene::EYE_FULL * o)); }
      }
    } else if (mood == HACKING) {
      eyeH = 7;            // narrow focused slit (clearly != idle's tall eyes)
      bob  += 3;           // lean toward the laptop
      float ph = fabsf(sinf((now % 1200) / 1200.0f * PI));   // ~0.6 Hz pulse
      promptCol = ph > 0.66f ? scene::AMBER
                : ph > 0.33f ? scene::AMBER_MID : scene::AMBER_DIM;
    } else {               // HAPPY / STARTLED reaction
      float p = (now - reactStart) / (float)reactDur;
      float jolt = sinf(p * PI);
      if (mood == HAPPY) { bob -= (int)(jolt * 12); mouth = scene::MOUTH_GRIN; }
      else { bob -= (int)(jolt * 7); bodyRy += 3; eyeMode = scene::EYE_WIDE; mouth = scene::MOUTH_O; }
    }

    creature.fillSprite(scene::BG);
    for (int y = 190; y < scene::SPR_Y + scene::SPR_H; y += 40)
      creature.drawFastHLine(0, y - scene::SPR_Y, scene::SPR_W, scene::SCAN);
    scene::drawCreature(creature, -scene::SPR_X, -scene::SPR_Y + bob,
                        eyeH, bodyRy, eyeMode, mouth, promptCol);
    creature.pushSprite(scene::SPR_X, scene::SPR_Y);
  }

  // --- terminal cursor ---
  if (now - lastCursor >= 500) {
    lastCursor = now; cursorOn = !cursorOn;
    scene::drawCursor(tft, cursorOn);
  }

  store::flush(now);

  // --- random chatter: ~1 line every 5 min, flavoured by mood ---
  if (now >= nextSay && !reacting) {
    if (mood == HACKING) {
      scene::say(tft, hackLines[random(hackCount)]);
    } else if (mood == IDLE) {
      int pick = random(3);
      if (pick == 0 && weather::valid && weather::line(weather::cond)[0])
        scene::say(tft, weather::line(weather::cond));
      else if (pick == 1 && store::pokes > 0)
        scene::say(tft, ("pokes: " + String(store::pokes)).c_str());
      else {
        scene::say(tft, idleLines[sayIdx]);
        sayIdx = (sayIdx + 1) % idleCount;
      }
    }
    nextSay = now + random(270000, 330000);
  }
}
