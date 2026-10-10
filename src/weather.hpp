// Sprout — current weather from Open-Meteo (free, no API key).
//
// fetch() pulls the current temperature + WMO weather code for the configured
// lat/lon and maps it to a coarse condition. Blocking (~1-2 s) but called
// rarely, so the brief hitch is fine on a desk pet.

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "settings.hpp"

namespace weather {

enum Cond { CLEAR, CLOUDS, FOG, RAIN, STORM, SNOW, UNKNOWN };

inline bool valid = false;
inline int  tempF = 0;
inline Cond cond  = UNKNOWN;

inline Cond fromCode(int c) {
  if (c == 0 || c == 1)               return CLEAR;
  if (c == 2 || c == 3)               return CLOUDS;
  if (c == 45 || c == 48)             return FOG;
  if (c >= 51 && c <= 67)             return RAIN;
  if (c >= 71 && c <= 77)             return SNOW;
  if (c >= 80 && c <= 82)             return RAIN;
  if (c == 85 || c == 86)             return SNOW;
  if (c >= 95)                        return STORM;
  return UNKNOWN;
}

inline const char* word(Cond c) {
  switch (c) {
    case CLEAR:  return "clear";
    case CLOUDS: return "cloudy";
    case FOG:    return "fog";
    case RAIN:   return "rain";
    case STORM:  return "storm";
    case SNOW:   return "snow";
    default:     return "";
  }
}

// Short IRC-style remark (<=17 chars) for a condition, "" to stay quiet.
inline const char* line(Cond c) {
  switch (c) {
    case CLEAR:  return "sunny out :)";
    case CLOUDS: return "grey skies today";
    case FOG:    return "foggy.. spooky";
    case RAIN:   return "rain. cozy >_";
    case STORM:  return "storm! i love it";
    case SNOW:   return "snow?! in HI?!";
    default:     return "";
  }
}

// Returns true if the condition changed.
inline bool fetch() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();                      // skip cert check (public data)
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + settings::lat +
               "&longitude=" + settings::lon +
               "&current=temperature_2m,weather_code&temperature_unit=fahrenheit";

  bool changed = false;
  if (http.begin(client, url)) {
    if (http.GET() == 200) {
      StaticJsonDocument<768> doc;
      if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
        auto cur = doc["current"];
        tempF = (int)lround((double)cur["temperature_2m"]);
        Cond nc = fromCode((int)cur["weather_code"]);
        changed = (nc != cond) || !valid;
        cond = nc;
        valid = true;
        Serial.printf("[wx] %dF %s\n", tempF, word(cond));
      }
    }
    http.end();
  }
  return changed;
}

// "72F clear" for the status bar, or "" if no data yet.
inline void statusStr(char* out, size_t n) {
  if (valid) snprintf(out, n, "%dF %s", tempF, word(cond));
  else       out[0] = '\0';
}

}  // namespace weather
