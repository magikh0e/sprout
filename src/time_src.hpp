// Sprout — time + day/night source.
//
// hourOfDay() returns 0..24. sleepiness(h) returns 0 (wide awake) .. 1 (deep
// night) with smooth dusk/dawn ramps. In demo mode the day runs fast so the
// behaviour is visible immediately; otherwise it uses NTP (if Wi-Fi creds are
// set) or a free-running offline clock.

#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

#if !DEMO_DAYNIGHT
#include <WiFi.h>
#endif

namespace timesrc {

inline bool haveReal = false;

inline void begin() {
#if !DEMO_DAYNIGHT
  if (strlen(WIFI_SSID) > 0) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(200);
    if (WiFi.status() == WL_CONNECTED) {
      configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
      struct tm tm;
      haveReal = getLocalTime(&tm, 5000);
    }
  }
#endif
}

inline float hourOfDay() {
#if DEMO_DAYNIGHT
  return (millis() % DEMO_DAY_MS) / (float)DEMO_DAY_MS * 24.0f;
#else
  if (haveReal) {
    struct tm tm;
    if (getLocalTime(&tm, 0)) return tm.tm_hour + tm.tm_min / 60.0f;
  }
  return fmodf(12.0f + millis() / 3600000.0f, 24.0f);   // offline, real speed
#endif
}

// 0 = awake, 1 = asleep, with 2-hour dusk (21-23) and dawn (05-07) ramps.
inline float sleepiness(float h) {
  if (h >= 7 && h < 21) return 0.0f;
  if (h >= 21 && h < 23) return (h - 21) / 2.0f;
  if (h >= 5 && h < 7)  return 1.0f - (h - 5) / 2.0f;
  return 1.0f;
}

inline void clockStr(float h, char* out) {
  int hh = (int)h;
  int mm = (int)((h - hh) * 60) % 60;
  sprintf(out, "%02d:%02d", hh, mm);
}

}  // namespace timesrc
