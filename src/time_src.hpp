// Sprout — time + day/night source, driven by runtime settings.
//
// connectWifi() and startNtp() are split so a web config can be fetched in
// between (after Wi-Fi, before the clock is set). hourOfDay() returns 0..24;
// sleepiness(h) ramps 0 (awake) .. 1 (deep night) with dusk/dawn transitions.

#pragma once
#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#include "settings.hpp"

namespace timesrc {

inline bool haveReal = false;

inline void connectWifi() {
  if (settings::demo || settings::ssid.isEmpty()) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(settings::ssid.c_str(), settings::pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(200);
  if (WiFi.status() == WL_CONNECTED) WiFi.setSleep(false);   // reliable scans
  Serial.printf("[time] wifi %s\n", WiFi.status() == WL_CONNECTED ? "ok" : "failed");
}

inline void startNtp() {
  if (settings::demo || WiFi.status() != WL_CONNECTED) return;
  configTzTime(settings::tz.c_str(), "pool.ntp.org", "time.nist.gov");   // DST-aware
  struct tm tm;
  haveReal = getLocalTime(&tm, 8000);
  Serial.printf("[time] ntp=%d\n", haveReal);
}

inline float hourOfDay() {
  if (settings::demo)
    return (millis() % settings::dayMs) / (float)settings::dayMs * 24.0f;
  if (haveReal) {
    struct tm tm;
    if (getLocalTime(&tm, 0)) return tm.tm_hour + tm.tm_min / 60.0f;
  }
  return fmodf(12.0f + millis() / 3600000.0f, 24.0f);   // offline, real speed
}

inline float sleepiness(float h) {
  if (h >= 7 && h < 21) return 0.0f;
  if (h >= 21 && h < 23) return (h - 21) / 2.0f;
  if (h >= 5 && h < 7)  return 1.0f - (h - 5) / 2.0f;
  return 1.0f;
}

inline void clockStr(float h, char* out) {
  int hh = (int)h;
  int mm = (int)((h - hh) * 60) % 60;
  if (settings::hour24) {
    sprintf(out, "%02d:%02d", hh, mm);
  } else {
    int h12 = hh % 12; if (h12 == 0) h12 = 12;
    sprintf(out, "%d:%02d%c", h12, mm, hh < 12 ? 'a' : 'p');
  }
}

}  // namespace timesrc
